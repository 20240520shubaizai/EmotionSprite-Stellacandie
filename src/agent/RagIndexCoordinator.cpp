#include "RagIndexCoordinator.h"
#include "AgentClient.h"
#include "../StorageService.h"

#include <QJsonArray>
#include <QJsonObject>

namespace {
QString iso(const QDateTime &value){return (value.isValid()?value:QDateTime::currentDateTimeUtc()).toUTC().toString(Qt::ISODateWithMs);}
QString ragPrivacy(const QString &value){if(value==QStringLiteral("secret")||value==QStringLiteral("local_only"))return QStringLiteral("secret");if(value==QStringLiteral("private"))return QStringLiteral("sensitive");return QStringLiteral("normal");}
QJsonObject baseDocument(const QString&id,const QString&source,const QString&fact,const QString&subject,const QString&content)
{return{{"record_id",id},{"source_type",source},{"fact_type",fact},{"subject",subject},{"content",content}};}
}

RagIndexCoordinator::RagIndexCoordinator(StorageService *storage,AgentClient *client,QObject *parent)
    :QObject(parent),m_storage(storage),m_client(client)
{
    m_debounce.setSingleShot(true);m_debounce.setInterval(50);
    connect(&m_debounce,&QTimer::timeout,this,&RagIndexCoordinator::rebuildNow);
    connect(client,&AgentClient::stateChanged,this,[this]{if(m_client->available())scheduleRebuild();});
    connect(client,&AgentClient::requestFinished,this,[this](const QString&id,const QJsonObject&){if(id==m_requestId)m_requestId.clear();});
    connect(client,&AgentClient::requestFailed,this,[this](const QString&id,const QString&,const QString&){if(id==m_requestId)m_requestId.clear();});
}

void RagIndexCoordinator::scheduleRebuild(){m_debounce.start();}
void RagIndexCoordinator::rebuildNow()
{
    if(!m_client||!m_client->available()||!m_requestId.isEmpty())return;
    m_requestId=m_client->submit(QStringLiteral("rag_rebuild_v1"),QJsonObject{{QStringLiteral("documents"),documents()}},120000,QStringLiteral("background"));
}

QJsonArray RagIndexCoordinator::documents()const
{
    QJsonArray out;
    QJsonObject bible=baseDocument(QStringLiteral("bible:core"),QStringLiteral("personality_bible"),QStringLiteral("personality_rule"),
        QStringLiteral("核心人格与关系边界"),QStringLiteral("Stellacandie是独立、好奇、有小脾气的平等陪伴型猫精灵，不与用户建立主仆关系；事实不确定时必须承认不确定，禁止凭空捏造用户经历。"));
    bible.insert("importance",100);bible.insert("confidence",1.0);bible.insert("proactive_allowed",true);bible.insert("revision",1);out.append(bible);
    for(const auto&m:m_storage->loadManagedMemories(false)){
        if(m.deletedAt.isValid())continue;const QString source=m.category==QStringLiteral("event")?QStringLiteral("event"):QStringLiteral("user_memory");
        QJsonObject d=baseDocument(QStringLiteral("memory:")+m.uuid,source,source==QStringLiteral("event")?QStringLiteral("temporary_event"):QStringLiteral("user_statement"),m.subject,m.content);
        d.insert("recorded_at",iso(m.createdAt));d.insert("importance",m.importance);d.insert("confidence",m.confidence);d.insert("use_count",m.useCount);
        d.insert("status",m.memoryState);if(m.expiresAt.isValid())d.insert("expires_at",iso(m.expiresAt));d.insert("proactive_allowed",m.memoryState==QStringLiteral("active"));
        d.insert("privacy_level",ragPrivacy(m.privacyLevel));d.insert("revision",m.syncRevision);out.append(d);
    }
    for(const auto&r:m_storage->loadCognitiveRecords()){
        const QString source=r.recordType==QStringLiteral("reminder")?QStringLiteral("reminder"):QStringLiteral("event");
        QJsonObject d=baseDocument(QStringLiteral("cognitive:")+r.uuid,source,QStringLiteral("temporary_event"),r.subject,r.sourceText.isEmpty()?r.subject:r.sourceText);
        d.insert("recorded_at",iso(r.createdAt));d.insert("importance",r.memoryImportance);d.insert("confidence",r.explicitRequest?1.0:.75);d.insert("status",r.status);
        if(r.expiresAt.isValid())d.insert("expires_at",iso(r.expiresAt));d.insert("proactive_allowed",r.followUpCount<r.maxFollowUps||r.explicitRequest);
        d.insert("explicit_request",r.explicitRequest);d.insert("revision",qMax(0,r.followUpCount));out.append(d);
    }
    for(const auto&entry:m_storage->loadDiaryEntries()){
        QJsonObject d=baseDocument(QStringLiteral("diary:")+entry.uuid,QStringLiteral("shared_experience"),QStringLiteral("shared_experience"),entry.entryDate.toString(Qt::ISODate),entry.content);
        d.insert("recorded_at",iso(entry.createdAt));d.insert("importance",40);d.insert("confidence",1.0);d.insert("proactive_allowed",false);out.append(d);
    }
    for(const auto&dream:m_storage->loadDreams()){
        QJsonObject d=baseDocument(QStringLiteral("dream:")+dream.uuid,QStringLiteral("shared_experience"),QStringLiteral("shared_experience"),dream.title,dream.content);
        d.insert("recorded_at",iso(dream.createdAt));d.insert("importance",30);d.insert("confidence",1.0);d.insert("proactive_allowed",false);out.append(d);
    }
    return out;
}
