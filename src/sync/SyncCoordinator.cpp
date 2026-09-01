#include "SyncCoordinator.h"

#include "../agent/AgentClient.h"
#include "../data/repositories/SyncRepository.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QSettings>
#include "../AiCredentialStore.h"

SyncCoordinator::SyncCoordinator(SyncRepository *repository,AgentClient *client,QObject *parent)
    :QObject(parent),m_repository(repository),m_client(client)
{
    m_timer.setInterval(5000);
    connect(&m_timer,&QTimer::timeout,this,[this]{refreshStatus();flush();});
    connect(m_client,&AgentClient::stateChanged,this,[this]{if(m_client->available())flush();});
    connect(m_client,&AgentClient::stateChanged,this,&SyncCoordinator::statusChanged);
    connect(m_client,&AgentClient::syncBatchFinished,this,&SyncCoordinator::handleSuccess);
    connect(m_client,&AgentClient::syncBatchFailed,this,&SyncCoordinator::handleFailure);
    connect(m_client,&AgentClient::syncConflictsFinished,this,[this](const QJsonArray&rows){m_conflictItems.clear();m_conflictIds.clear();for(const auto&item:rows){const auto row=item.toObject();m_conflictIds<<row.value("id").toVariant().toLongLong();m_conflictItems<<QStringLiteral("%1 · %2 · %3").arg(row.value("entity_type").toString(),row.value("entity_uuid").toString().left(12),row.value("created_at").toString());}emit statusChanged();});
    connect(m_client,&AgentClient::syncConflictResolved,this,[this]{refreshConflicts();refreshStatus();});
    connect(m_client,&AgentClient::syncControlFailed,this,[this](const QString&code){m_lastError=code;emit statusChanged();});
    connect(m_client,&AgentClient::cloudExportReady,this,[this](const QJsonObject&data){const QString dir=QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);const QString path=QDir(dir).filePath(QStringLiteral("情绪精灵_云端数据导出_%1.json").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss")));QFile file(path);if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(data).toJson(QJsonDocument::Indented));m_lastOperation=QStringLiteral("云端数据已导出：%1").arg(path);}else m_lastOperation=QStringLiteral("导出文件写入失败");emit statusChanged();emit cloudExportReady(data);});
    connect(m_client,&AgentClient::cloudDeletionFinished,this,[this](int count){m_lastOperation=QStringLiteral("已删除 %1 条云端实体，本地数据未改变").arg(count);refreshConflicts();emit statusChanged();emit cloudDeletionFinished(count);});
}

void SyncCoordinator::start(){m_timer.start();refreshStatus();flush();}
void SyncCoordinator::stop(){m_timer.stop();}

bool SyncCoordinator::masterEnabled()const{return m_repository&&m_repository->syncMasterEnabled();}
bool SyncCoordinator::settingsEnabled()const{return m_repository&&m_repository->syncEnabled(QStringLiteral("settings"));}
bool SyncCoordinator::petStateEnabled()const{return m_repository&&m_repository->syncEnabled(QStringLiteral("pet_state"));}
bool SyncCoordinator::memoryEnabled()const{return m_repository&&m_repository->syncEnabled(QStringLiteral("memory"));}
bool SyncCoordinator::reminderEnabled()const{return m_repository&&m_repository->syncEnabled(QStringLiteral("reminder"));}
QString SyncCoordinator::deviceId()const{return m_repository?m_repository->syncDeviceId():QString();}
QString SyncCoordinator::cloudUrl()const{return QSettings().value(QStringLiteral("sync/cloudUrl")).toString();}
QString SyncCoordinator::cloudConnectionStatus()const{return m_client?m_client->statusText():QStringLiteral("未配置");}
bool SyncCoordinator::cloudConfigured()const{return !cloudUrl().isEmpty()&&!AiCredentialStore::loadSyncToken().isEmpty();}
void SyncCoordinator::setMasterEnabled(bool enabled){if(enabled&&!cloudConfigured()){m_lastOperation=QStringLiteral("请先配置并连接 HTTPS 同步服务");emit statusChanged();return;}if(m_repository&&m_repository->setSyncMasterEnabled(enabled)){refreshStatus();if(enabled)flush();}}
void SyncCoordinator::setCategoryEnabled(const QString&type,bool enabled){if(m_repository&&m_repository->setSyncEnabled(type,enabled)){refreshStatus();if(enabled)flush();}}
void SyncCoordinator::refreshStatus(){if(!m_repository)return;const auto status=m_repository->syncStatus();m_pendingCount=status.value("pending_count").toInt();m_lastSuccess=status.value("last_success_at").toString();m_lastError=status.value("last_error").toString();emit statusChanged();}
void SyncCoordinator::refreshConflicts(){if(m_client)m_client->fetchSyncConflicts();}
void SyncCoordinator::resolveConflict(int row,const QString&choice){if(m_client&&row>=0&&row<m_conflictIds.size())m_client->resolveSyncConflict(m_conflictIds.at(row),choice);}
void SyncCoordinator::exportCloudData(){if(m_client)m_client->requestCloudExport();}
void SyncCoordinator::deleteCloudData(const QString&confirmation){if(m_client)m_client->requestCloudDeletion(confirmation);}
void SyncCoordinator::saveCloudConfiguration(const QString&url,const QString&token){const QUrl endpoint(url.trimmed());if(!endpoint.isValid()||endpoint.scheme()!=QStringLiteral("https")){m_lastOperation=QStringLiteral("正式同步地址必须使用 HTTPS");emit statusChanged();return;}QSettings().setValue(QStringLiteral("sync/cloudUrl"),endpoint.toString(QUrl::RemoveQuery|QUrl::RemoveFragment));if(!token.isEmpty()&&!AiCredentialStore::saveSyncToken(token)){m_lastOperation=QStringLiteral("同步令牌保存失败");emit statusChanged();return;}if(m_client)m_client->configure(endpoint,AiCredentialStore::loadSyncToken());m_lastOperation=QStringLiteral("同步服务配置已保存，正在检查连接");emit statusChanged();}
void SyncCoordinator::clearCloudConfiguration(){QSettings().remove(QStringLiteral("sync/cloudUrl"));AiCredentialStore::clearSyncToken();if(m_client)m_client->configure(QUrl(),QString());m_repository->setSyncMasterEnabled(false);m_lastOperation=QStringLiteral("同步服务配置已清除，总开关已关闭");refreshStatus();}
void SyncCoordinator::testCloudConnection(){if(m_client)m_client->configure(QUrl(cloudUrl()),AiCredentialStore::loadSyncToken());}

void SyncCoordinator::flush()
{
    if(!m_repository||!m_client||!m_client->available()||!m_inFlight.isEmpty())return;
    m_inFlight=m_repository->loadPendingOutbox(50);
    if(m_inFlight.isEmpty())return;
    QJsonArray events;
    QList<SyncOutboxRecord> validRows;
    for(const auto &row:m_inFlight){
        QJsonParseError error;
        const QJsonObject payload=QJsonDocument::fromJson(row.payload.toUtf8(),&error).object();
        if(error.error!=QJsonParseError::NoError){m_repository->markOutboxRetry(row.id,QStringLiteral("invalid_local_payload"));continue;}
        events.append(QJsonObject{{QStringLiteral("idempotency_key"),row.idempotencyKey},
                                  {QStringLiteral("user_id"),row.userId},
                                  {QStringLiteral("entity_type"),row.entityType},
                                  {QStringLiteral("entity_uuid"),row.entityUuid},
                                  {QStringLiteral("revision"),row.revision},
                                  {QStringLiteral("operation"),row.operation},
                                  {QStringLiteral("privacy_level"),row.privacyLevel},
                                  {QStringLiteral("payload"),payload}});
        validRows.append(row);
    }
    m_inFlight=validRows;
    if(events.isEmpty()){m_inFlight.clear();return;}
    m_client->submitSyncBatch(events);
}

void SyncCoordinator::handleSuccess(const QJsonArray &results)
{
    const int count=qMin(results.size(),m_inFlight.size());
    for(int i=0;i<count;++i){
        const QString status=results.at(i).toObject().value(QStringLiteral("status")).toString();
        if(status==QStringLiteral("applied")||status==QStringLiteral("duplicate")||status==QStringLiteral("ignored")||status==QStringLiteral("stale_ignored")||status==QStringLiteral("conflict_copy_created"))
            m_repository->markOutboxDelivered(m_inFlight.at(i).id);
        else
            m_repository->markOutboxRetry(m_inFlight.at(i).id,QStringLiteral("server_rejected"));
    }
    for(int i=count;i<m_inFlight.size();++i)m_repository->markOutboxRetry(m_inFlight.at(i).id,QStringLiteral("missing_result"));
    m_inFlight.clear();
    refreshStatus();
}

void SyncCoordinator::handleFailure(const QString &errorCode)
{
    for(const auto &row:m_inFlight)m_repository->markOutboxRetry(row.id,errorCode);
    m_inFlight.clear();
    refreshStatus();
}
