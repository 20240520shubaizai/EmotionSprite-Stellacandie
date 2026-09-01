#pragma once
#include "../Records.h"
#include <QList>
class MemoryRepository { public: virtual ~MemoryRepository()=default;
 virtual QList<MemoryRecord>loadMemories()const=0;virtual bool upsertMemory(const MemoryRecord&)=0;virtual bool touchMemory(qint64)=0;virtual bool softDeleteMemory(qint64)=0;
 virtual QList<MemoryRecord>loadManagedMemories(bool includeDeleted=true)const=0;virtual bool updateMemoryGovernance(qint64 id,const QString& lifecycleState,bool pinned,const QDateTime& archivedAt=QDateTime())=0;
 virtual bool restoreMemory(qint64)=0;virtual int runMemoryLifecycleMaintenance(const QDateTime&)=0;virtual bool updateMemoryContent(qint64,const QString&,const QString&)=0;
 virtual int forgetTopic(const QString&)=0;virtual QStringList forgottenTopics()const=0;virtual bool ensureEntity(const QString&,const QString&)=0;
 virtual bool linkMemoryToEntity(const QString&,const QString&,const QString&)=0;virtual QStringList entityNames()const=0;virtual int removeTimeBoundMemories()=0;};
