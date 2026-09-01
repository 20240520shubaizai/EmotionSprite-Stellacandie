#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QStringList>

struct SyncMetadata {
    QString uuid;
    QString userId = QStringLiteral("local-single-user");
    int syncRevision = 0;
    QString syncStatus = QStringLiteral("pending");
    QString privacyLevel = QStringLiteral("normal");
    QDateTime deletedAt;
};

struct ChatMessageRecord { qint64 id=0; QString sender; QString text; QDateTime createdAt; };
struct PetStateRecord {
    int mood=60,energy=70,health=80,closeness=20,boredom=10,neglect=0,curiosity=25,irritation=0;
    QDateTime lastInteraction,lastIllness; QString healthPhase=QStringLiteral("healthy"),condition;
    int recoveryProgress=0,fullness=45; QDateTime lastDigestionAt,illnessStartedAt,phaseChangedAt; QDate lastIllnessCheckDate;
};
struct DiaryEntryRecord { qint64 id=0;QString uuid;QDate entryDate;QString content;QDateTime createdAt,updatedAt;QString syncStatus=QStringLiteral("pending"); };
struct DiaryStickerRecord { qint64 id=0;QDate entryDate;QString emoji,label;int xPercent=10,yPercent=10,rotation=0; };
struct MemoryRecord {
    qint64 id=0;QString uuid,category,subject,content;int importance=50;double confidence=.8;QString nextQuestion;
    QDateTime createdAt,updatedAt,lastUsedAt;int useCount=0;QString syncStatus=QStringLiteral("pending"),memoryState=QStringLiteral("active");
    bool locked=false;QDateTime expiresAt,archivedAt,deletedAt;QString retention=QStringLiteral("long_term"),governanceReason;
    int syncRevision=0;QString privacyLevel=QStringLiteral("normal");
};
struct ReminderRecord { qint64 id=0;QString type;QDateTime scheduledAt;QString status,payload; };
struct CommitmentRecord { qint64 id=0;QString description;QDateTime dueAt;QString status;QDateTime createdAt; };
struct CognitiveRecord {
    qint64 id=0;QString uuid,recordType,subject,sourceText;QDateTime scheduledAt,eventEndAt,followUpAt,expiresAt;
    QString status=QStringLiteral("planned");bool explicitRequest=false;int deliveryPriority=50,memoryImportance=30,followUpCount=0,maxFollowUps=0;
    QString followUpPolicy=QStringLiteral("none");qint64 reminderId=0;QDateTime createdAt,updatedAt;
};
struct SnackInventoryRecord { qint64 id=0;QString uuid,snackType,snackName,emoji;int quantity=0,nutrition=0;QDateTime updatedAt; };
struct SnackCatalogRecord { QString snackType,snackName,emoji;int eatenCount=0,preference=50,consecutiveCount=0;QDateTime firstUnlockedAt,lastEatenAt;QString nickname; };
struct SummaryRecord { qint64 id=0;QString title,sourceName,mode,content;QDateTime createdAt; };
struct DreamRecord {
    qint64 id=0;QString uuid;QDate dreamDate;QString title,content,mood,dreamType;QStringList symbols;QString color,realityHint,continuationKey;
    QStringList memoryIds;QDateTime createdAt,openedAt;bool favorite=false;QString realityEcho;QDateTime echoCreatedAt;int disclosureLevel=0;QDateTime disclosedAt;
};
struct MorningLollipopRecord {
    qint64 id=0;QString uuid;QDate giftDate;QString flavorId,flavorName,category,emoji,color,rarity;QDateTime plannedAt,actualAt;
    QString status=QStringLiteral("planned"),greeting,greetingFingerprint,generationSource,delayReason;bool viewed=false,favorite=false;
    QString acquisitionType=QStringLiteral("regular"),themeTags,weatherSnapshot,story,memorialKey,shape=QStringLiteral("round"),pattern=QStringLiteral("swirl");
    QDateTime createdAt,updatedAt;
};
struct SyncOutboxRecord {qint64 id=0;QString idempotencyKey,userId,entityType,entityUuid,operation,privacyLevel,payload;int revision=0,retryCount=0;QDateTime createdAt,nextAttemptAt;};
