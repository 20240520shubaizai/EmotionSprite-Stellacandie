#pragma once
#include "../Records.h"
#include <QList>
class DreamRepository { public: virtual ~DreamRepository()=default;virtual qint64 addDream(const DreamRecord&)=0;virtual QList<DreamRecord>loadDreams(int=200)const=0;
 virtual bool hasDreamForDate(const QDate&)const=0;virtual bool markDreamOpened(qint64,const QDateTime&)=0;virtual bool setDreamFavorite(qint64,bool)=0;
 virtual bool saveDreamRealityEcho(qint64,const QString&)=0;virtual bool setDreamDisclosure(qint64,int,const QDateTime&)=0;};
