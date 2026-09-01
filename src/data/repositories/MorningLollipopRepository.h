#pragma once
#include "../Records.h"
#include <QList>
class MorningLollipopRepository { public: virtual ~MorningLollipopRepository()=default;
 virtual bool upsertMorningLollipop(const MorningLollipopRecord&)=0;virtual MorningLollipopRecord loadMorningLollipop(const QDate&)const=0;
 virtual QList<MorningLollipopRecord>loadMorningLollipops(int limit=120)const=0;virtual bool setMorningLollipopFavorite(qint64,bool)=0;
 virtual bool markMorningLollipopViewed(qint64)=0;virtual bool hasMorningLollipopMemorial(const QString&)const=0;
 virtual QList<CognitiveRecord>loadCognitiveRecords(const QStringList& states=QStringList())const=0;
 virtual QList<ReminderRecord>loadDueReminders(const QDateTime&,int limit=20)const=0;virtual PetStateRecord loadPetState()const=0;};
