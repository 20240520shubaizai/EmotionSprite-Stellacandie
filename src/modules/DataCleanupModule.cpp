#include "DataCleanupModule.h"
#include <QDateTime>
#include <QSettings>

DataCleanupModule::DataCleanupModule(DataRepository*r,QObject*p):FeatureModule(p),m_repository(r)
{
    m_enabled=QSettings().value(QStringLiteral("modules/dataCleanupEnabled"),true).toBool();refresh();m_timer.setInterval(6*60*60*1000);connect(&m_timer,&QTimer::timeout,this,[this]{if(m_enabled)runMaintenance();});m_timer.start();QTimer::singleShot(12000,this,[this]{if(m_enabled)runMaintenance();});
}
void DataCleanupModule::setEnabled(bool e){if(e==m_enabled)return;m_enabled=e;QSettings().setValue(QStringLiteral("modules/dataCleanupEnabled"),e);emit enabledChanged(e);emit changed();}
void DataCleanupModule::refresh()
{
    if (!m_repository) return;
    const QList<MemoryRecord> loaded = m_repository->loadManagedMemories(true);
    m_items.clear();
    m_items.reserve(loaded.size());
    for (const auto &record : loaded) m_items.append(record);
}
QString DataCleanupModule::summary()const{int active=0,sleeping=0,archived=0,deleted=0,locked=0;for(const auto&m:m_items){if(m.locked)locked++;if(m.deletedAt.isValid()||m.memoryState=="deleted")deleted++;else if(m.memoryState=="sleeping")sleeping++;else if(m.memoryState=="archived")archived++;else active++;}return QStringLiteral("活跃 %1　沉睡 %2　归档 %3　回收区 %4　锁定 %5").arg(active).arg(sleeping).arg(archived).arg(deleted).arg(locked);}
QStringList DataCleanupModule::memoryItems()const{QStringList out;for(const auto&m:m_items){QString state=m.deletedAt.isValid()?QStringLiteral("回收区"):m.memoryState=="sleeping"?QStringLiteral("沉睡"):m.memoryState=="archived"?QStringLiteral("归档"):QStringLiteral("活跃");out<<QStringLiteral("%1|%2|%3|%4|%5|%6").arg(m.id).arg(m.locked?1:0).arg(state,m.subject,m.content).arg(m.importance);}return out;}
bool DataCleanupModule::runMaintenance(){if(!m_repository)return false;const int count=m_repository->runMemoryLifecycleMaintenance(QDateTime::currentDateTime());m_lastResult=count?QStringLiteral("本次安全整理了 %1 条记忆；没有物理删除任何长期记忆。").arg(count):QStringLiteral("检查完成，目前没有需要迁移生命周期的记忆。");refresh();emit changed();return true;}
bool DataCleanupModule::toggleLock(int row){if(row<0||row>=m_items.size())return false;const auto&m=m_items.at(row);if(m.deletedAt.isValid())return false;const bool ok=m_repository->updateMemoryGovernance(m.id,m.memoryState,!m.locked,m.expiresAt);if(ok){refresh();emit changed();}return ok;}
bool DataCleanupModule::setState(int row,const QString&s){if(row<0||row>=m_items.size()||!QStringList{"active","sleeping","archived"}.contains(s))return false;const auto&m=m_items.at(row);if(m.deletedAt.isValid()||m.locked&&s!="active")return false;const bool ok=m_repository->updateMemoryGovernance(m.id,s,m.locked,m.expiresAt);if(ok){refresh();emit changed();}return ok;}
bool DataCleanupModule::restore(int row){if(row<0||row>=m_items.size())return false;const bool ok=m_repository->restoreMemory(m_items.at(row).id);if(ok){refresh();emit changed();}return ok;}
bool DataCleanupModule::softDelete(int row){if(row<0||row>=m_items.size()||m_items.at(row).locked)return false;const bool ok=m_repository->softDeleteMemory(m_items.at(row).id);if(ok){refresh();emit changed();}return ok;}
