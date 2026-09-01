#pragma once
#include "../Records.h"
#include <QList>
class CollectionRepository { public: virtual ~CollectionRepository()=default;
 virtual bool addSnackToInventory(const QString&,const QString&,const QString&,int)=0;virtual QList<SnackInventoryRecord>loadSnackInventory()const=0;virtual bool consumeSnackInventory(qint64)=0;
 virtual QList<SnackCatalogRecord>loadSnackCatalog()const=0;virtual SnackCatalogRecord recordSnackEaten(const QString&,const QString&,const QString&)=0;
 virtual bool addSnackHistory(const QString&,const QString&,const QString&,const QString&,qint64,const QString&)=0;virtual QStringList loadSnackHistory(int=30)const=0;
 virtual qint64 addSummary(const QString&,const QString&,const QString&,const QString&)=0;virtual QList<SummaryRecord>loadSummaries(int=50)const=0;virtual bool deleteSummary(qint64)=0;
 virtual bool upsertMorningLollipop(const MorningLollipopRecord&)=0;virtual MorningLollipopRecord loadMorningLollipop(const QDate&)const=0;virtual QList<MorningLollipopRecord>loadMorningLollipops(int=120)const=0;
 virtual bool setMorningLollipopFavorite(qint64,bool)=0;virtual bool markMorningLollipopViewed(qint64)=0;virtual bool hasMorningLollipopMemorial(const QString&)const=0;};
