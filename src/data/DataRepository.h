#pragma once

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>

struct ChatMessageRecord {
    qint64 id = 0; QString sender; QString text; QDateTime createdAt;
};

struct PetStateRecord {
    int mood = 60; int energy = 70; int health = 80; int closeness = 20;
    int boredom = 10; int neglect = 0; int curiosity = 25; int irritation = 0;
    QDateTime lastInteraction; QDateTime lastIllness;
    QString healthPhase = QStringLiteral("healthy");
    QString condition;
    int recoveryProgress = 0;
    int fullness = 45;
    QDateTime lastDigestionAt;
    QDateTime illnessStartedAt;
    QDateTime phaseChangedAt;
    QDate lastIllnessCheckDate;
};

struct DiaryEntryRecord {
    qint64 id = 0;
    QString uuid;
    QDate entryDate;
    QString content;
    QDateTime createdAt;
    QDateTime updatedAt;
    QString syncStatus = QStringLiteral("pending");
};

struct DiaryStickerRecord {
    qint64 id=0; QDate entryDate; QString emoji; QString label;
    int xPercent=10; int yPercent=10; int rotation=0;
};

struct MemoryRecord {
    qint64 id = 0; QString uuid; QString category; QString subject; QString content;
    int importance = 50; double confidence = 0.8; QString nextQuestion;
    QDateTime createdAt; QDateTime updatedAt; QDateTime lastUsedAt;
    int useCount = 0; QString syncStatus = QStringLiteral("pending");
    QString memoryState = QStringLiteral("active");
    bool locked = false;
    QDateTime expiresAt; QDateTime archivedAt; QDateTime deletedAt;
    QString retention = QStringLiteral("long_term");
    QString governanceReason;
};

struct ReminderRecord {
    qint64 id = 0; QString type; QDateTime scheduledAt; QString status; QString payload;
};
struct CommitmentRecord { qint64 id=0; QString description; QDateTime dueAt; QString status; QDateTime createdAt; };
struct CognitiveRecord {
    qint64 id=0; QString uuid; QString recordType; QString subject; QString sourceText;
    QDateTime scheduledAt; QDateTime eventEndAt; QDateTime followUpAt; QDateTime expiresAt;
    QString status=QStringLiteral("planned"); bool explicitRequest=false;
    int deliveryPriority=50; int memoryImportance=30; int followUpCount=0; int maxFollowUps=0;
    QString followUpPolicy=QStringLiteral("none"); qint64 reminderId=0;
    QDateTime createdAt; QDateTime updatedAt;
};
struct SnackInventoryRecord { qint64 id=0; QString uuid; QString snackType; QString snackName; QString emoji; int quantity=0; int nutrition=0; QDateTime updatedAt; };
struct SnackCatalogRecord { QString snackType; QString snackName; QString emoji; int eatenCount=0; int preference=50; int consecutiveCount=0; QDateTime firstUnlockedAt; QDateTime lastEatenAt; QString nickname; };
struct SummaryRecord { qint64 id=0;QString title;QString sourceName;QString mode;QString content;QDateTime createdAt; };
struct DreamRecord {
    qint64 id=0; QString uuid; QDate dreamDate; QString title; QString content;
    QString mood; QString dreamType; QStringList symbols; QString color;
    QString realityHint; QString continuationKey; QStringList memoryIds;
    QDateTime createdAt; QDateTime openedAt; bool favorite=false;
    QString realityEcho; QDateTime echoCreatedAt;
    int disclosureLevel=0; QDateTime disclosedAt;
};
struct MorningLollipopRecord {
    qint64 id=0; QString uuid; QDate giftDate; QString flavorId; QString flavorName;
    QString category; QString emoji; QString color; QString rarity;
    QDateTime plannedAt; QDateTime actualAt; QString status=QStringLiteral("planned");
    QString greeting; QString greetingFingerprint; QString generationSource;
    QString delayReason; bool viewed=false; bool favorite=false;
    QString acquisitionType=QStringLiteral("regular"); QString themeTags; QString weatherSnapshot;
    QString story; QString memorialKey; QString shape=QStringLiteral("round"); QString pattern=QStringLiteral("swirl");
    QDateTime createdAt; QDateTime updatedAt;
};

class DataRepository
{
public:
    virtual ~DataRepository() = default;
    virtual QList<ChatMessageRecord> loadRecentMessages(int limit = 100) const = 0;
    virtual ChatMessageRecord addMessage(const QString &sender, const QString &text) = 0;
    virtual PetStateRecord loadPetState() const = 0;
    virtual bool savePetState(const PetStateRecord &state) = 0;
    virtual bool saveDiaryEntry(const QDate &date, const QString &content) = 0;
    virtual QList<DiaryEntryRecord> loadDiaryEntries() const = 0;
    virtual bool addDiarySticker(const DiaryStickerRecord &sticker) = 0;
    virtual QList<DiaryStickerRecord> loadDiaryStickers(const QDate &date) const = 0;
    virtual QList<MemoryRecord> loadMemories() const = 0;
    virtual bool upsertMemory(const MemoryRecord &memory) = 0;
    virtual bool touchMemory(qint64 id) = 0;
    virtual bool softDeleteMemory(qint64 id) = 0;
    virtual QList<MemoryRecord> loadManagedMemories(bool includeDeleted=true) const = 0;
    virtual bool updateMemoryGovernance(qint64 id,const QString &state,bool locked,const QDateTime &expiresAt=QDateTime()) = 0;
    virtual bool restoreMemory(qint64 id) = 0;
    virtual int runMemoryLifecycleMaintenance(const QDateTime &now) = 0;
    virtual bool updateMemoryContent(qint64 id, const QString &content, const QString &nextQuestion) = 0;
    virtual int forgetTopic(const QString &topic) = 0;
    virtual QStringList forgottenTopics() const = 0;
    virtual bool ensureEntity(const QString &name, const QString &type) = 0;
    virtual bool linkMemoryToEntity(const QString &category, const QString &subject, const QString &entityName) = 0;
    virtual QStringList entityNames() const = 0;
    virtual qint64 addReminder(const QString &type, const QDateTime &scheduledAt, const QString &payload) = 0;
    virtual QList<ReminderRecord> loadDueReminders(const QDateTime &now, int limit = 20) const = 0;
    virtual bool updateReminderStatus(qint64 id, const QString &status) = 0;
    virtual int cancelReminders(const QString &typePrefix, const QString &payloadContains = QString()) = 0;
    virtual int deliveredReminderCount(const QDate &date, const QString &typePrefix = QString()) const = 0;
    virtual qint64 addCommitment(const QString &description, const QDateTime &dueAt) = 0;
    virtual QList<CommitmentRecord> loadActiveCommitments() const = 0;
    virtual bool updateCommitmentStatus(qint64 id, const QString &status) = 0;
    virtual bool updateCommitmentDue(qint64 id, const QDateTime &dueAt) = 0;
    virtual qint64 addCognitiveRecord(const CognitiveRecord &record) = 0;
    virtual QList<CognitiveRecord> loadCognitiveRecords(const QStringList &statuses = {}) const = 0;
    virtual bool updateCognitiveRecord(qint64 id,const QString &status,const QDateTime &scheduledAt=QDateTime(),
                                       const QDateTime &followUpAt=QDateTime(),int followUpCount=-1,qint64 reminderId=-1) = 0;
    virtual QList<CognitiveRecord> loadDueCognitiveFollowUps(const QDateTime &now,int limit=20) const = 0;
    virtual int archiveExpiredCognitiveRecords(const QDateTime &now) = 0;
    virtual int removeTimeBoundMemories() = 0;
    virtual bool addSnackToInventory(const QString &type,const QString &name,const QString &emoji,int nutrition) = 0;
    virtual QList<SnackInventoryRecord> loadSnackInventory() const = 0;
    virtual bool consumeSnackInventory(qint64 id) = 0;
    virtual QList<SnackCatalogRecord> loadSnackCatalog() const = 0;
    virtual SnackCatalogRecord recordSnackEaten(const QString &type,const QString &name,const QString &emoji) = 0;
    virtual bool addSnackHistory(const QString &eventType,const QString &type,const QString &name,const QString &sourceName,qint64 sourceSize,const QString &safetyLevel) = 0;
    virtual QStringList loadSnackHistory(int limit=30) const = 0;
    virtual qint64 addSummary(const QString &title,const QString &sourceName,const QString &mode,const QString &content) = 0;
    virtual QList<SummaryRecord> loadSummaries(int limit=50) const = 0;
    virtual bool deleteSummary(qint64 id) = 0;
    virtual qint64 addDream(const DreamRecord &dream) = 0;
    virtual QList<DreamRecord> loadDreams(int limit=200) const = 0;
    virtual bool hasDreamForDate(const QDate &date) const = 0;
    virtual bool markDreamOpened(qint64 id,const QDateTime &openedAt) = 0;
    virtual bool setDreamFavorite(qint64 id,bool favorite) = 0;
    virtual bool saveDreamRealityEcho(qint64 id,const QString &echo) = 0;
    virtual bool setDreamDisclosure(qint64 id,int level,const QDateTime &at) = 0;
    virtual bool upsertMorningLollipop(const MorningLollipopRecord &record) = 0;
    virtual MorningLollipopRecord loadMorningLollipop(const QDate &date) const = 0;
    virtual QList<MorningLollipopRecord> loadMorningLollipops(int limit=120) const = 0;
    virtual bool setMorningLollipopFavorite(qint64 id,bool favorite) = 0;
    virtual bool markMorningLollipopViewed(qint64 id) = 0;
    virtual bool hasMorningLollipopMemorial(const QString &memorialKey) const = 0;
};
