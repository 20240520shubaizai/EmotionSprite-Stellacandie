#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QFile>
#include <QMap>
#include <QUuid>

#include "src/StorageService.h"

class StorageRepositoryTests : public QObject
{
    Q_OBJECT
private slots:
    void freshDatabaseAndDomainRepositories();
    void migrationIsIdempotent();
    void atomicCognitiveReminderRollsBack();
    void outboxRetryPreservesLocalDataAndBacksOff();
    void stage1DatabaseCopyMigratesWithoutDataLoss();
    void agentMutationCommitIsAtomicAndIdempotent();
};

static QMap<QString,qint64> tableCounts(const QString &path, const QStringList &tables)
{
    const QString name=QStringLiteral("count-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QMap<QString,qint64> result;
    { QSqlDatabase db=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),name);db.setDatabaseName(path);
      if(db.open()) for(const QString &table:tables){QSqlQuery q(db);if(q.exec(QStringLiteral("SELECT count(*) FROM %1").arg(table))&&q.next())result[table]=q.value(0).toLongLong();} db.close(); }
    QSqlDatabase::removeDatabase(name); return result;
}

void StorageRepositoryTests::freshDatabaseAndDomainRepositories()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    StorageService storage(dir.filePath(QStringLiteral("fresh.db")));
    QVERIFY2(storage.initialize(), qPrintable(storage.lastError()));
    QCOMPARE(storage.schemaVersion(), 24);

    ConversationRepository *conversation = &storage;
    QVERIFY(conversation->addMessage(QStringLiteral("user"), QStringLiteral("hello")).id > 0);
    QCOMPARE(conversation->loadRecentMessages(5).size(), 1);

    MemoryRepository *memory = &storage;
    MemoryRecord item; item.category=QStringLiteral("person"); item.subject=QStringLiteral("friend"); item.content=QStringLiteral("met today");
    QVERIFY(memory->upsertMemory(item)); QVERIFY(!memory->loadMemories().isEmpty());

    DiaryRepository *diary = &storage;
    QVERIFY(diary->saveDiaryEntry(QDate(2026,8,18), QStringLiteral("test diary")));
    QCOMPARE(diary->loadDiaryEntries().size(), 1);

    PetStateRepository *pet = &storage;
    PetStateRecord state=pet->loadPetState(); state.mood=77; QVERIFY(pet->savePetState(state)); QCOMPARE(pet->loadPetState().mood,77);

    ReminderRepository *reminders = &storage;
    QVERIFY(reminders->addReminder(QStringLiteral("test"),QDateTime::currentDateTime(),QStringLiteral("payload"))>0);
    QVERIFY(!reminders->loadDueReminders(QDateTime::currentDateTime().addSecs(1)).isEmpty());
    SyncRepository *sync=&storage;QVERIFY(!sync->syncMasterEnabled());QVERIFY(!sync->syncEnabled(QStringLiteral("reminder")));QVERIFY(sync->loadPendingOutbox().isEmpty());
    QVERIFY(sync->setSyncEnabled(QStringLiteral("reminder"),true));QVERIFY(sync->loadPendingOutbox().isEmpty());QVERIFY(sync->setSyncMasterEnabled(true));QVERIFY(!sync->loadPendingOutbox().isEmpty());QVERIFY(!sync->syncDeviceId().isEmpty());
    const auto memoryRows=memory->loadMemories();QVERIFY(!memoryRows.isEmpty());QVERIFY(sync->setSyncEnabled(QStringLiteral("memory"),true));
    QVERIFY(sync->setEntitySyncPrivacy(QStringLiteral("memory"),memoryRows.first().uuid,QStringLiteral("normal")));
    bool memoryQueued=false;for(const auto&o:sync->loadPendingOutbox())if(o.entityType==QStringLiteral("memory"))memoryQueued=true;QVERIFY(memoryQueued);
    QVERIFY(memory->softDeleteMemory(memoryRows.first().id));QVERIFY(memory->loadMemories().isEmpty());
    const auto deletedRows=memory->loadManagedMemories(true);QCOMPARE(deletedRows.first().memoryState,QStringLiteral("deleted"));QVERIFY(deletedRows.first().deletedAt.isValid());
    bool deleteQueued=false;for(const auto&o:sync->loadPendingOutbox())if(o.entityUuid==memoryRows.first().uuid&&o.operation==QStringLiteral("delete"))deleteQueued=true;QVERIFY(deleteQueued);
    QVERIFY(memory->restoreMemory(memoryRows.first().id));bool restoreQueued=false;for(const auto&o:sync->loadPendingOutbox())if(o.entityUuid==memoryRows.first().uuid&&o.operation==QStringLiteral("restore"))restoreQueued=true;QVERIFY(restoreQueued);
    QVERIFY(sync->setEntitySyncPrivacy(QStringLiteral("memory"),memoryRows.first().uuid,QStringLiteral("secret")));
    QVERIFY(sync->setSyncEnabled(QStringLiteral("settings"),true));
    QVERIFY(sync->setSyncSetting(QStringLiteral("theme"),QStringLiteral("warm")));
    QVERIFY(!sync->setSyncSetting(QStringLiteral("api_key"),QStringLiteral("must-not-sync")));

    DreamRepository *dreams=&storage; DreamRecord dream; dream.dreamDate=QDate(2026,8,18);dream.title=QStringLiteral("star");dream.content=QStringLiteral("warm");dream.mood=QStringLiteral("warm");dream.dreamType=QStringLiteral("random");dream.color=QStringLiteral("#E7C7D5");
    QVERIFY(dreams->addDream(dream)>0); QCOMPARE(dreams->loadDreams(5).size(),1);

    CollectionRepository *collection=&storage;
    QVERIFY(collection->addSnackToInventory(QStringLiteral("txt"),QStringLiteral("cookie"),QStringLiteral("C"),2));
    QCOMPARE(collection->loadSnackInventory().size(),1);

    MorningLollipopRepository *lollipops=&storage; MorningLollipopRecord candy;
    candy.giftDate=QDate(2026,8,18);candy.flavorId=QStringLiteral("peach");candy.flavorName=QStringLiteral("peach");
    candy.category=QStringLiteral("sweet");candy.emoji=QStringLiteral("L");candy.color=QStringLiteral("#FFC0CB");
    candy.rarity=QStringLiteral("common");candy.plannedAt=QDateTime::currentDateTime();
    QVERIFY(lollipops->upsertMorningLollipop(candy));QVERIFY(lollipops->loadMorningLollipop(candy.giftDate).id>0);
}

void StorageRepositoryTests::migrationIsIdempotent()
{
    QTemporaryDir dir; const QString path=dir.filePath(QStringLiteral("repeat.db"));
    { StorageService first(path); QVERIFY2(first.initialize(),qPrintable(first.lastError())); QCOMPARE(first.schemaVersion(),24); }
    { StorageService second(path); QVERIFY2(second.initialize(),qPrintable(second.lastError())); QCOMPARE(second.schemaVersion(),24); }
}

void StorageRepositoryTests::atomicCognitiveReminderRollsBack()
{
    QTemporaryDir dir; const QString path=dir.filePath(QStringLiteral("atomic.db"));
    StorageService storage(path); QVERIFY(storage.initialize());
    CognitiveRecord record; record.recordType=QStringLiteral("meeting"); record.subject=QStringLiteral("atomic"); record.explicitRequest=true;
    // NULL scheduled_at violates the reminder table, so both inserts must roll back.
    QCOMPARE(storage.addCognitiveReminderAtomic(record,QString(),QDateTime(),QStringLiteral("x")),qint64(0));
    QCOMPARE(storage.loadCognitiveRecords().size(),0);
}

void StorageRepositoryTests::outboxRetryPreservesLocalDataAndBacksOff()
{
    QTemporaryDir dir;const QString path=dir.filePath(QStringLiteral("retry.db"));
    {StorageService storage(path);QVERIFY(storage.initialize());QVERIFY(storage.setSyncEnabled(QStringLiteral("reminder"),true));QVERIFY(storage.setSyncMasterEnabled(true));
     const qint64 reminderId=storage.addReminder(QStringLiteral("retry-test"),QDateTime::currentDateTime(),QStringLiteral("local payload"));QVERIFY(reminderId>0);
     const auto pending=storage.loadPendingOutbox();QCOMPARE(pending.size(),1);QVERIFY(storage.markOutboxRetry(pending.first().id,QStringLiteral("network_error")));
     QVERIFY(storage.loadPendingOutbox().isEmpty());QCOMPARE(storage.loadDueReminders(QDateTime::currentDateTime().addSecs(1)).size(),1);}
    {StorageService reopened(path);QVERIFY(reopened.initialize());QVERIFY(reopened.syncMasterEnabled());QVERIFY(reopened.syncEnabled(QStringLiteral("reminder")));QCOMPARE(reopened.loadDueReminders(QDateTime::currentDateTime().addSecs(1)).size(),1);}
}

void StorageRepositoryTests::stage1DatabaseCopyMigratesWithoutDataLoss()
{
    const QString source=qEnvironmentVariable("EMOTION_STAGE1_DB");
    if(source.isEmpty() || !QFile::exists(source)) QSKIP("Stage 1 database fixture is unavailable");
    QTemporaryDir dir; const QString copy=dir.filePath(QStringLiteral("stage1-copy.db")); QVERIFY(QFile::copy(source,copy));
    const QStringList tables{QStringLiteral("messages"),QStringLiteral("memories"),QStringLiteral("diary_entries"),QStringLiteral("reminders"),QStringLiteral("cognitive_records"),QStringLiteral("dreams"),QStringLiteral("morning_lollipops")};
    const auto before=tableCounts(copy,tables);
    { StorageService migrated(copy); QVERIFY2(migrated.initialize(),qPrintable(migrated.lastError())); QCOMPARE(migrated.schemaVersion(),24); }
    QCOMPARE(tableCounts(copy,tables),before);
    { StorageService repeated(copy); QVERIFY2(repeated.initialize(),qPrintable(repeated.lastError())); QCOMPARE(repeated.schemaVersion(),24); }
    QCOMPARE(tableCounts(copy,tables),before);
    const QString name=QStringLiteral("verify-old-copy");
    { QSqlDatabase db=QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),name);db.setDatabaseName(copy);QVERIFY(db.open());QSqlQuery q(db);
      QVERIFY(q.exec(QStringLiteral("PRAGMA integrity_check")));QVERIFY(q.next());QCOMPARE(q.value(0).toString(),QStringLiteral("ok"));
      QVERIFY(q.exec(QStringLiteral("PRAGMA foreign_key_check")));QVERIFY(!q.next());db.close(); }
    QSqlDatabase::removeDatabase(name);
}

void StorageRepositoryTests::agentMutationCommitIsAtomicAndIdempotent()
{
    QTemporaryDir dir;StorageService storage(dir.filePath(QStringLiteral("agent-commit.db")));QVERIFY(storage.initialize());
    const QJsonArray proposals{
        QJsonObject{{"kind","reminder"},{"permission","local_write"},{"confidence",.95},{"payload",QJsonObject{{"type","agent.reminder"},{"subject",QStringLiteral("换枕头")},{"scheduled_at","2026-08-22T09:00:00+08:00"}}}},
        QJsonObject{{"kind","memory_candidate"},{"permission","local_write"},{"confidence",.9},{"payload",QJsonObject{{"content",QStringLiteral("我喜欢橘猫")}}}}};
    const auto failed=storage.commitAgentTurn(QStringLiteral("rollback-id"),QStringLiteral("trace-a"),QStringLiteral("记住啦"),proposals,true);
    QVERIFY(!failed.value("committed").toBool());QVERIFY(storage.loadRecentMessages().isEmpty());QVERIFY(storage.loadMemories().isEmpty());QVERIFY(storage.loadDueReminders(QDateTime(QDate(2030,1,1),QTime(0,0))).isEmpty());
    const auto first=storage.commitAgentTurn(QStringLiteral("stable-id"),QStringLiteral("trace-b"),QStringLiteral("记住啦"),proposals,false);QVERIFY(first.value("committed").toBool());QCOMPARE(first.value("record_ids").toArray().size(),2);
    const auto replay=storage.commitAgentTurn(QStringLiteral("stable-id"),QStringLiteral("trace-b"),QStringLiteral("重复投递"),proposals,false);QVERIFY(replay.value("idempotent_replay").toBool());QCOMPARE(storage.loadRecentMessages().size(),1);QCOMPARE(storage.loadMemories().size(),1);QCOMPARE(storage.loadDueReminders(QDateTime(QDate(2030,1,1),QTime(0,0))).size(),1);
    {StorageService reopened(storage.databasePath());QVERIFY(reopened.initialize());const auto replayAfterRestart=reopened.commitAgentTurn(QStringLiteral("stable-id"),QStringLiteral("trace-b"),QStringLiteral("重复投递"),proposals,false);QVERIFY(replayAfterRestart.value("idempotent_replay").toBool());QCOMPARE(reopened.loadRecentMessages().size(),1);}
}

QTEST_MAIN(StorageRepositoryTests)
#include "StorageRepositoryTests.moc"
