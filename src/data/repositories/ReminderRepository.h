#pragma once
#include "../Records.h"
#include <QList>
class ReminderRepository { public: virtual ~ReminderRepository()=default;
 virtual qint64 addReminder(const QString&,const QDateTime&,const QString&)=0;virtual QList<ReminderRecord>loadDueReminders(const QDateTime&,int limit=20)const=0;
 virtual bool updateReminderStatus(qint64,const QString&)=0;virtual int cancelReminders(const QString&,const QString& payloadContains=QString())=0;virtual int deliveredReminderCount(const QDate&,const QString& type=QString())const=0;
 virtual qint64 addCommitment(const QString&,const QDateTime&)=0;virtual QList<CommitmentRecord>loadActiveCommitments()const=0;virtual bool updateCommitmentStatus(qint64,const QString&)=0;
 virtual bool updateCommitmentDue(qint64,const QDateTime&)=0;virtual qint64 addCognitiveRecord(const CognitiveRecord&)=0;virtual QList<CognitiveRecord>loadCognitiveRecords(const QStringList& states=QStringList())const=0;
 virtual bool updateCognitiveRecord(qint64 id,const QString& state,const QDateTime& dueAt=QDateTime(),const QDateTime& nextFollowUpAt=QDateTime(),int followUpCount=-1,qint64 linkedReminderId=-1)=0;
 virtual qint64 addCognitiveReminderAtomic(const CognitiveRecord& record,const QString& reminderType,const QDateTime& scheduledAt,const QString& payload)=0;
 virtual QList<CognitiveRecord>loadDueCognitiveFollowUps(const QDateTime&,int limit=20)const=0;virtual int archiveExpiredCognitiveRecords(const QDateTime&)=0;};
