#pragma once
#include "../Records.h"
#include <QList>
#include <QJsonObject>
class SyncRepository{public:virtual~SyncRepository()=default;
virtual bool setSyncMasterEnabled(bool)=0;virtual bool syncMasterEnabled()const=0;
virtual bool setSyncEnabled(const QString&,bool)=0;virtual bool syncEnabled(const QString&)const=0;
virtual QString syncDeviceId()const=0;virtual QJsonObject syncStatus()const=0;
virtual bool setSyncSetting(const QString&,const QString&)=0;virtual bool setEntitySyncPrivacy(const QString&,const QString&,const QString&)=0;
virtual QList<SyncOutboxRecord> loadPendingOutbox(int limit=50)const=0;virtual bool markOutboxDelivered(qint64)=0;virtual bool markOutboxRetry(qint64,const QString&)=0;};
