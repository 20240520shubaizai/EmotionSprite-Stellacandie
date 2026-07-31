#pragma once

#include "data/DataRepository.h"

class StorageService final : public DataRepository
{
public:
    StorageService();
    ~StorageService();

    bool initialize();
    QString databasePath() const;
    QString lastError() const;

    QList<ChatMessageRecord> loadRecentMessages(int limit = 100) const override;
    ChatMessageRecord addMessage(const QString &sender, const QString &text) override;
    bool saveDiaryEntry(const QDate &date, const QString &content) override;
    QList<DiaryEntryRecord> loadDiaryEntries() const override;
    bool addDiarySticker(const DiaryStickerRecord &sticker) override;
    QList<DiaryStickerRecord> loadDiaryStickers(const QDate &date) const override;
    QList<MemoryRecord> loadMemories() const override;
    bool upsertMemory(const MemoryRecord &memory) override;
    bool touchMemory(qint64 id) override;
    bool softDeleteMemory(qint64 id) override;
    QList<MemoryRecord> loadManagedMemories(bool includeDeleted=true) const override;
    bool updateMemoryGovernance(qint64 id,const QString &state,bool locked,const QDateTime &expiresAt=QDateTime()) override;
    bool restoreMemory(qint64 id) override;
    int runMemoryLifecycleMaintenance(const QDateTime &now) override;
    bool updateMemoryContent(qint64 id, const QString &content, const QString &nextQuestion) override;
    int forgetTopic(const QString &topic) override;
    QStringList forgottenTopics() const override;
    bool ensureEntity(const QString &name, const QString &type) override;
    bool linkMemoryToEntity(const QString &category, const QString &subject, const QString &entityName) override;
    QStringList entityNames() const override;
    qint64 addReminder(const QString &type, const QDateTime &scheduledAt, const QString &payload) override;
    QList<ReminderRecord> loadDueReminders(const QDateTime &now, int limit = 20) const override;
    bool updateReminderStatus(qint64 id, const QString &status) override;
    int cancelReminders(const QString &typePrefix, const QString &payloadContains = QString()) override;
    int deliveredReminderCount(const QDate &date, const QString &typePrefix = QString()) const override;
    qint64 addCommitment(const QString &description, const QDateTime &dueAt) override;
    QList<CommitmentRecord> loadActiveCommitments() const override;
    bool updateCommitmentStatus(qint64 id, const QString &status) override;
    bool updateCommitmentDue(qint64 id, const QDateTime &dueAt) override;
    qint64 addCognitiveRecord(const CognitiveRecord &record) override;
    QList<CognitiveRecord> loadCognitiveRecords(const QStringList &statuses={}) const override;
    bool updateCognitiveRecord(qint64 id,const QString &status,const QDateTime &scheduledAt=QDateTime(),const QDateTime &followUpAt=QDateTime(),int followUpCount=-1,qint64 reminderId=-1) override;
    QList<CognitiveRecord> loadDueCognitiveFollowUps(const QDateTime &now,int limit=20) const override;
    int archiveExpiredCognitiveRecords(const QDateTime &now) override;
    int removeTimeBoundMemories() override;
    bool addSnackToInventory(const QString &type,const QString &name,const QString &emoji,int nutrition) override;
    QList<SnackInventoryRecord> loadSnackInventory() const override;
    bool consumeSnackInventory(qint64 id) override;
    QList<SnackCatalogRecord> loadSnackCatalog() const override;
    SnackCatalogRecord recordSnackEaten(const QString &type,const QString &name,const QString &emoji) override;
    bool addSnackHistory(const QString &eventType,const QString &type,const QString &name,const QString &sourceName,qint64 sourceSize,const QString &safetyLevel) override;
    QStringList loadSnackHistory(int limit=30) const override;
    qint64 addSummary(const QString &title,const QString &sourceName,const QString &mode,const QString &content) override;
    QList<SummaryRecord> loadSummaries(int limit=50) const override;
    bool deleteSummary(qint64 id) override;
    qint64 addDream(const DreamRecord &dream) override;
    QList<DreamRecord> loadDreams(int limit=200) const override;
    bool hasDreamForDate(const QDate &date) const override;
    bool markDreamOpened(qint64 id,const QDateTime &openedAt) override;
    bool setDreamFavorite(qint64 id,bool favorite) override;
    bool saveDreamRealityEcho(qint64 id,const QString &echo) override;
    bool setDreamDisclosure(qint64 id,int level,const QDateTime &at) override;
    bool upsertMorningLollipop(const MorningLollipopRecord &record) override;
    MorningLollipopRecord loadMorningLollipop(const QDate &date) const override;
    QList<MorningLollipopRecord> loadMorningLollipops(int limit=120) const override;
    bool setMorningLollipopFavorite(qint64 id,bool favorite) override;
    bool markMorningLollipopViewed(qint64 id) override;
    bool hasMorningLollipopMemorial(const QString &memorialKey) const override;
    PetStateRecord loadPetState() const override;
    bool savePetState(const PetStateRecord &state) override;

private:
    bool createSchema();
    bool columnExists(const QString &table, const QString &column) const;

    QString m_connectionName;
    QString m_databasePath;
    QString m_lastError;
};
