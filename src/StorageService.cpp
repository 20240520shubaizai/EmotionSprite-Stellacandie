#include "StorageService.h"
#include "data/SchemaMigrator.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUuid>

StorageService::StorageService()
    : StorageService(QString())
{
}

StorageService::StorageService(const QString &databasePathOverride)
    : m_connectionName(QStringLiteral("emotion-sprite-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))),
      m_databasePathOverride(databasePathOverride)
{
}

StorageService::~StorageService()
{
    {
        QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
        if (database.isValid()) {
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool StorageService::initialize()
{
    const QString dataDirectory = m_databasePathOverride.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : QFileInfo(m_databasePathOverride).absolutePath();
    if (!QDir().mkpath(dataDirectory)) {
        m_lastError = QStringLiteral("无法创建数据目录：%1").arg(dataDirectory);
        return false;
    }

    m_databasePath = m_databasePathOverride.isEmpty()
        ? QDir(dataDirectory).filePath(QStringLiteral("emotion_sprite.db")) : m_databasePathOverride;
    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    database.setDatabaseName(m_databasePath);
    if (!database.open()) {
        m_lastError = database.lastError().text();
        return false;
    }

    {
        QSqlQuery pragma(database);
        if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
            m_lastError = pragma.lastError().text();
            return false;
        }
        pragma.finish();
        if (!pragma.exec(QStringLiteral("PRAGMA busy_timeout = 5000"))) {
            m_lastError = pragma.lastError().text();
            return false;
        }
        pragma.finish();
        if (!pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"))) {
            m_lastError = pragma.lastError().text();
            return false;
        }
        // journal_mode returns one result row. It must be consumed and the
        // query finalized before starting the schema transaction.
        pragma.next();
        pragma.finish();
    }
    if (!createSchema()) return false;
    return SchemaMigrator::migrate(database, &m_lastError);
}

QString StorageService::databasePath() const
{
    return m_databasePath;
}

QString StorageService::lastError() const
{
    return m_lastError;
}

int StorageService::schemaVersion() const
{
    return SchemaMigrator::currentVersion(QSqlDatabase::database(m_connectionName));
}

QList<ChatMessageRecord> StorageService::loadRecentMessages(int limit) const
{
    QList<ChatMessageRecord> messages;
    QSqlDatabase database = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "SELECT id, sender, text, created_at FROM ("
        "SELECT id, sender, text, created_at FROM messages "
        "ORDER BY id DESC LIMIT :limit) ORDER BY id ASC"));
    query.bindValue(QStringLiteral(":limit"), limit);
    if (!query.exec()) {
        return messages;
    }

    while (query.next()) {
        messages.append({query.value(0).toLongLong(),
                         query.value(1).toString(),
                         query.value(2).toString(),
                         QDateTime::fromString(query.value(3).toString(), Qt::ISODateWithMs)});
    }
    return messages;
}

ChatMessageRecord StorageService::addMessage(const QString &sender, const QString &text)
{
    ChatMessageRecord record;
    record.sender = sender;
    record.text = text;
    record.createdAt = QDateTime::currentDateTime();

    QSqlDatabase database = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
        "INSERT INTO messages(conversation_id, sender, text, created_at) "
        "VALUES(1, :sender, :text, :created_at)"));
    query.bindValue(QStringLiteral(":sender"), sender);
    query.bindValue(QStringLiteral(":text"), text);
    query.bindValue(QStringLiteral(":created_at"), record.createdAt.toString(Qt::ISODateWithMs));
    if (query.exec()) {
        record.id = query.lastInsertId().toLongLong();
    }
    return record;
}

QJsonObject StorageService::commitAgentTurn(const QString &requestId,const QString &traceId,const QString &assistantText,
                                            const QJsonArray &proposals,bool injectFailure)
{
    QSqlDatabase db=QSqlDatabase::database(m_connectionName);QJsonObject result{{"schema_version","mutation_commit_v1"}};
    if(requestId.trimmed().isEmpty()||assistantText.trimmed().isEmpty()){result.insert("error","invalid_commit_input");return result;}
    QSqlQuery prior(db);prior.prepare(QStringLiteral("SELECT result_json FROM agent_mutation_commits WHERE request_id=:id"));prior.bindValue(":id",requestId);
    if(prior.exec()&&prior.next()){auto cached=QJsonDocument::fromJson(prior.value(0).toByteArray()).object();cached.insert("idempotent_replay",true);return cached;}
    if(!db.transaction()){result.insert("error",db.lastError().text());return result;}
    auto fail=[&](const QString&e){db.rollback();return QJsonObject{{"schema_version","mutation_commit_v1"},{"committed",false},{"error",e}};};
    QSqlQuery message(db);message.prepare(QStringLiteral("INSERT INTO messages(conversation_id,sender,text,created_at,uuid,user_id,sync_status,privacy_level) VALUES(1,'pet',:text,:at,lower(hex(randomblob(16))),'local-single-user','pending','private')"));
    message.bindValue(":text",assistantText);message.bindValue(":at",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));if(!message.exec())return fail(message.lastError().text());
    const qint64 messageId=message.lastInsertId().toLongLong();QJsonArray ids;int step=0;
    for(const auto &value:proposals){const QJsonObject proposal=value.toObject();const QString kind=proposal.value("kind").toString();const QJsonObject payload=proposal.value("payload").toObject();
        if(proposal.value("permission").toString("local_write")!=QStringLiteral("local_write"))continue;
        if(injectFailure&&++step==2)return fail(QStringLiteral("injected_second_step_failure"));
        if(kind==QStringLiteral("reminder")){
            const QDateTime due=QDateTime::fromString(payload.value("scheduled_at").toString(),Qt::ISODateWithMs);if(!due.isValid())return fail(QStringLiteral("invalid_reminder_time"));
            QSqlQuery q(db);q.prepare(QStringLiteral("INSERT INTO reminders(uuid,reminder_type,scheduled_at,status,payload,created_at,updated_at,user_id,sync_status,privacy_level) VALUES(lower(hex(randomblob(16))),:type,:due,'pending',:payload,:now,:now,'local-single-user','pending','private')"));
            q.bindValue(":type",payload.value("type").toString("agent.reminder"));q.bindValue(":due",due.toString(Qt::ISODateWithMs));q.bindValue(":payload",payload.value("subject").toString(payload.value("source_text").toString()));q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));if(!q.exec())return fail(q.lastError().text());ids.append(QJsonObject{{"kind",kind},{"id",q.lastInsertId().toLongLong()}});
        }else if(kind==QStringLiteral("memory_candidate")){
            const QString content=payload.value("content").toString().trimmed();if(content.isEmpty())return fail(QStringLiteral("empty_memory"));QSqlQuery q(db);q.prepare(QStringLiteral("INSERT INTO memories(uuid,category,subject,content,importance,confidence,created_at,updated_at,sync_status,memory_state,privacy_level,user_id) VALUES(lower(hex(randomblob(16))),'agent_explicit',:subject,:content,80,:confidence,:now,:now,'pending','active','private','local-single-user') ON CONFLICT(category,subject) DO UPDATE SET content=excluded.content,updated_at=excluded.updated_at,sync_revision=sync_revision+1"));q.bindValue(":subject",content.left(80));q.bindValue(":content",content);q.bindValue(":confidence",proposal.value("confidence").toDouble(.8));q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));if(!q.exec())return fail(q.lastError().text());ids.append(QJsonObject{{"kind",kind},{"id",q.lastInsertId().toLongLong()}});
        }else if(kind==QStringLiteral("state_delta")){
            QSqlQuery q(db);q.prepare(QStringLiteral("UPDATE pet_state SET energy=MAX(0,MIN(100,energy+:energy)),mood=MAX(0,MIN(100,mood+:mood)),updated_at=:now,sync_status='pending',sync_revision=sync_revision+1 WHERE id=1"));q.bindValue(":energy",payload.value("energy").toInt());q.bindValue(":mood",payload.value("valence").toInt());q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));if(!q.exec())return fail(q.lastError().text());ids.append(QJsonObject{{"kind",kind},{"id",1}});
        }
    }
    result.insert("committed",true);result.insert("message_id",messageId);result.insert("record_ids",ids);result.insert("idempotent_replay",false);
    const QByteArray proposalBytes=QJsonDocument(proposals).toJson(QJsonDocument::Compact);QSqlQuery receipt(db);receipt.prepare(QStringLiteral("INSERT INTO agent_mutation_commits(request_id,trace_id,proposal_hash,result_json,created_at) VALUES(:id,:trace,:hash,:result,:at)"));receipt.bindValue(":id",requestId);receipt.bindValue(":trace",traceId);receipt.bindValue(":hash",QString::fromLatin1(QCryptographicHash::hash(proposalBytes,QCryptographicHash::Sha256).toHex()));receipt.bindValue(":result",QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact)));receipt.bindValue(":at",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));if(!receipt.exec())return fail(receipt.lastError().text());if(!db.commit())return fail(db.lastError().text());return result;
}

bool StorageService::saveDiaryEntry(const QDate &date, const QString &content)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO diary_entries(entry_date, content, created_at, uuid, updated_at, sync_status) "
        "VALUES(:date, :content, :created, lower(hex(randomblob(16))), :created, 'pending') "
        "ON CONFLICT(entry_date) DO UPDATE SET content=excluded.content, updated_at=excluded.updated_at, "
        "deleted_at=NULL, sync_status='pending', sync_revision=sync_revision+1"));
    query.bindValue(QStringLiteral(":date"), date.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":content"), content);
    query.bindValue(QStringLiteral(":created"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    return query.exec();
}

QList<DiaryEntryRecord> StorageService::loadDiaryEntries() const
{
    QList<DiaryEntryRecord> entries;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral(
            "SELECT id, uuid, entry_date, content, created_at, updated_at, sync_status "
            "FROM diary_entries WHERE deleted_at IS NULL ORDER BY entry_date DESC, id DESC"))) return entries;
    while (query.next()) {
        entries.append({query.value(0).toLongLong(), query.value(1).toString(),
            QDate::fromString(query.value(2).toString(), Qt::ISODate), query.value(3).toString(),
            QDateTime::fromString(query.value(4).toString(), Qt::ISODateWithMs),
            QDateTime::fromString(query.value(5).toString(), Qt::ISODateWithMs), query.value(6).toString()});
    }
    return entries;
}

bool StorageService::addDiarySticker(const DiaryStickerRecord &sticker)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("INSERT OR IGNORE INTO diary_stickers(entry_date,emoji,label,x_percent,y_percent,rotation,created_at) VALUES(:date,:emoji,:label,:x,:y,:rotation,:created)"));
    query.bindValue(QStringLiteral(":date"),sticker.entryDate.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":emoji"),sticker.emoji.left(12));
    query.bindValue(QStringLiteral(":label"),sticker.label.left(30));
    query.bindValue(QStringLiteral(":x"),qBound(0,sticker.xPercent,85));
    query.bindValue(QStringLiteral(":y"),qBound(0,sticker.yPercent,85));
    query.bindValue(QStringLiteral(":rotation"),qBound(-20,sticker.rotation,20));
    query.bindValue(QStringLiteral(":created"),QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    return query.exec();
}

QList<DiaryStickerRecord> StorageService::loadDiaryStickers(const QDate &date) const
{
    QList<DiaryStickerRecord> result;QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral("SELECT id,entry_date,emoji,label,x_percent,y_percent,rotation FROM diary_stickers WHERE entry_date=:date ORDER BY id LIMIT 4"));
    query.bindValue(QStringLiteral(":date"),date.toString(Qt::ISODate));
    if(query.exec())while(query.next())result.append({query.value(0).toLongLong(),QDate::fromString(query.value(1).toString(),Qt::ISODate),query.value(2).toString(),query.value(3).toString(),query.value(4).toInt(),query.value(5).toInt(),query.value(6).toInt()});
    return result;
}

QList<MemoryRecord> StorageService::loadMemories() const
{
    QList<MemoryRecord> result;QSqlQuery q(QSqlDatabase::database(m_connectionName));if(!q.exec(QStringLiteral("SELECT id,uuid,category,subject,content,importance,confidence,next_question,created_at,updated_at,last_used_at,use_count,sync_status,sync_revision,privacy_level FROM memories WHERE deleted_at IS NULL AND memory_state='active' ORDER BY importance DESC,updated_at DESC")))return result;while(q.next()){MemoryRecord m;m.id=q.value(0).toLongLong();m.uuid=q.value(1).toString();m.category=q.value(2).toString();m.subject=q.value(3).toString();m.content=q.value(4).toString();m.importance=q.value(5).toInt();m.confidence=q.value(6).toDouble();m.nextQuestion=q.value(7).toString();m.createdAt=QDateTime::fromString(q.value(8).toString(),Qt::ISODateWithMs);m.updatedAt=QDateTime::fromString(q.value(9).toString(),Qt::ISODateWithMs);m.lastUsedAt=QDateTime::fromString(q.value(10).toString(),Qt::ISODateWithMs);m.useCount=q.value(11).toInt();m.syncStatus=q.value(12).toString();m.syncRevision=q.value(13).toInt();m.privacyLevel=q.value(14).toString();result.append(m);}return result;
}

bool StorageService::upsertMemory(const MemoryRecord &m)
{
    for (const QString &blocked : forgottenTopics()) {
        if ((!blocked.isEmpty()) && (m.subject.contains(blocked, Qt::CaseInsensitive)
            || m.content.contains(blocked, Qt::CaseInsensitive))) return false;
    }
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("INSERT INTO memories(uuid,category,subject,content,importance,confidence,next_question,created_at,updated_at,sync_status,memory_state,locked,expires_at,retention,governance_reason,privacy_level) "
        "VALUES(lower(hex(randomblob(16))),:category,:subject,:content,:importance,:confidence,:question,:now,:now,'pending',:state,:locked,:expires,:retention,:reason,:privacy) "
        "ON CONFLICT(category,subject) DO UPDATE SET content=excluded.content,importance=MAX(memories.importance,excluded.importance),"
        "confidence=excluded.confidence,next_question=excluded.next_question,updated_at=excluded.updated_at,deleted_at=NULL,memory_state='active',locked=MAX(memories.locked,excluded.locked),expires_at=excluded.expires_at,retention=excluded.retention,governance_reason=excluded.governance_reason,"
        "privacy_level=excluded.privacy_level,sync_status='pending',sync_revision=sync_revision+1"));
    q.bindValue(":category",m.category); q.bindValue(":subject",m.subject); q.bindValue(":content",m.content);
    q.bindValue(":importance",m.importance); q.bindValue(":confidence",m.confidence); q.bindValue(":question",m.nextQuestion);
    q.bindValue(":state",m.memoryState);q.bindValue(":locked",m.locked?1:0);q.bindValue(":expires",m.expiresAt.isValid()?m.expiresAt.toString(Qt::ISODateWithMs):QVariant());q.bindValue(":retention",m.retention);q.bindValue(":reason",m.governanceReason);q.bindValue(":privacy",m.privacyLevel);
    q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs)); return q.exec();
}

bool StorageService::touchMemory(qint64 id)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("UPDATE memories SET last_used_at=:now,use_count=use_count+1 WHERE id=:id"));
    q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs)); q.bindValue(":id",id); return q.exec();
}

bool StorageService::softDeleteMemory(qint64 id)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("UPDATE memories SET deleted_at=:now,memory_state='deleted',updated_at=:now,sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id AND deleted_at IS NULL"));
    q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec()&&q.numRowsAffected()==1;
}

QList<MemoryRecord> StorageService::loadManagedMemories(bool includeDeleted)const
{
    QList<MemoryRecord> result;QSqlQuery q(QSqlDatabase::database(m_connectionName));QString sql=QStringLiteral("SELECT id,uuid,category,subject,content,importance,confidence,next_question,created_at,updated_at,last_used_at,use_count,sync_status,memory_state,locked,expires_at,archived_at,deleted_at,retention,governance_reason,sync_revision,privacy_level FROM memories");if(!includeDeleted)sql+=QStringLiteral(" WHERE deleted_at IS NULL");sql+=QStringLiteral(" ORDER BY locked DESC,importance DESC,updated_at DESC");if(!q.exec(sql))return result;while(q.next()){MemoryRecord m;m.id=q.value(0).toLongLong();m.uuid=q.value(1).toString();m.category=q.value(2).toString();m.subject=q.value(3).toString();m.content=q.value(4).toString();m.importance=q.value(5).toInt();m.confidence=q.value(6).toDouble();m.nextQuestion=q.value(7).toString();m.createdAt=QDateTime::fromString(q.value(8).toString(),Qt::ISODateWithMs);m.updatedAt=QDateTime::fromString(q.value(9).toString(),Qt::ISODateWithMs);m.lastUsedAt=QDateTime::fromString(q.value(10).toString(),Qt::ISODateWithMs);m.useCount=q.value(11).toInt();m.syncStatus=q.value(12).toString();m.memoryState=q.value(13).toString();m.locked=q.value(14).toBool();m.expiresAt=QDateTime::fromString(q.value(15).toString(),Qt::ISODateWithMs);m.archivedAt=QDateTime::fromString(q.value(16).toString(),Qt::ISODateWithMs);m.deletedAt=QDateTime::fromString(q.value(17).toString(),Qt::ISODateWithMs);m.retention=q.value(18).toString();m.governanceReason=q.value(19).toString();m.syncRevision=q.value(20).toInt();m.privacyLevel=q.value(21).toString();result.append(m);}return result;
}
bool StorageService::updateMemoryGovernance(qint64 id,const QString&state,bool locked,const QDateTime&expiresAt){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE memories SET memory_state=:state,locked=:locked,expires_at=:expires,archived_at=CASE WHEN :state='archived' THEN :now ELSE archived_at END,updated_at=:now,sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id AND deleted_at IS NULL"));q.bindValue(":state",state);q.bindValue(":locked",locked?1:0);q.bindValue(":expires",expiresAt.isValid()?expiresAt.toString(Qt::ISODateWithMs):QVariant());q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec()&&q.numRowsAffected()==1;}
bool StorageService::restoreMemory(qint64 id){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE memories SET deleted_at=NULL,memory_state='active',updated_at=:now,sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id"));q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec()&&q.numRowsAffected()==1;}
int StorageService::runMemoryLifecycleMaintenance(const QDateTime&now){int changed=0;const QList<MemoryRecord> items=loadManagedMemories(false);for(const MemoryRecord&m:items){if(m.locked)continue;if(m.expiresAt.isValid()&&m.expiresAt<now){if(softDeleteMemory(m.id))changed++;continue;}const QDateTime reference=m.lastUsedAt.isValid()?m.lastUsedAt:(m.updatedAt.isValid()?m.updatedAt:m.createdAt);if(!reference.isValid())continue;const qint64 days=reference.daysTo(now);if(m.memoryState==QStringLiteral("active")&&m.importance<70&&days>=90){if(updateMemoryGovernance(m.id,QStringLiteral("sleeping"),false,m.expiresAt))changed++;}else if(m.memoryState==QStringLiteral("sleeping")&&days>=180){if(updateMemoryGovernance(m.id,QStringLiteral("archived"),false,m.expiresAt))changed++;}}return changed;}

bool StorageService::updateMemoryContent(qint64 id, const QString &content, const QString &nextQuestion)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("UPDATE memories SET content=:content,next_question=:question,updated_at=:now,"
        "sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id AND deleted_at IS NULL"));
    q.bindValue(":content",content);q.bindValue(":question",nextQuestion);
    q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec();
}

int StorageService::forgetTopic(const QString &topic)
{
    const QString key=topic.trimmed();if(key.isEmpty())return 0;QSqlDatabase db=QSqlDatabase::database(m_connectionName);
    if(!db.transaction())return 0;QSqlQuery q(db);const QString now=QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    q.prepare(QStringLiteral("INSERT INTO forgotten_topics(topic,created_at,sync_status) VALUES(:topic,:now,'pending') "
        "ON CONFLICT(topic) DO UPDATE SET created_at=excluded.created_at,sync_status='pending'"));q.bindValue(":topic",key);q.bindValue(":now",now);
    if(!q.exec()){db.rollback();return 0;}
    q.prepare(QStringLiteral("UPDATE memories SET deleted_at=:now,memory_state='deleted',updated_at=:now,sync_status='pending',sync_revision=sync_revision+1 "
        "WHERE deleted_at IS NULL AND (subject LIKE :pattern OR content LIKE :pattern)"));q.bindValue(":now",now);q.bindValue(":pattern",QStringLiteral("%%1%").arg(key));
    if(!q.exec()){db.rollback();return 0;}const int count=q.numRowsAffected();
    q.prepare(QStringLiteral("UPDATE story_threads SET status='forgotten',updated_at=:now WHERE topic LIKE :pattern OR summary LIKE :pattern"));q.bindValue(":now",now);q.bindValue(":pattern",QStringLiteral("%%1%").arg(key));if(!q.exec()){db.rollback();return 0;}
    q.prepare(QStringLiteral("UPDATE reminders SET status='cancelled',updated_at=datetime('now'),sync_revision=sync_revision+1,sync_status='pending' WHERE payload LIKE :pattern"));q.bindValue(":pattern",QStringLiteral("%%1%").arg(key));if(!q.exec()){db.rollback();return 0;}
    q.prepare(QStringLiteral("UPDATE memory_entities SET deleted_at=:now,sync_status='pending',sync_revision=sync_revision+1 "
        "WHERE deleted_at IS NULL AND name LIKE :pattern"));q.bindValue(":now",now);q.bindValue(":pattern",QStringLiteral("%%1%").arg(key));if(!q.exec()){db.rollback();return 0;}
    q.prepare(QStringLiteral("DELETE FROM memory_relations WHERE memory_id IN (SELECT id FROM memories WHERE deleted_at IS NOT NULL) "
        "OR entity_id IN (SELECT id FROM memory_entities WHERE deleted_at IS NOT NULL)"));if(!q.exec()){db.rollback();return 0;}
    return db.commit()?count:0;
}

QStringList StorageService::forgottenTopics() const
{
    QStringList result;QSqlQuery q(QSqlDatabase::database(m_connectionName));
    if(q.exec(QStringLiteral("SELECT topic FROM forgotten_topics ORDER BY created_at DESC")))while(q.next())result<<q.value(0).toString();return result;
}

bool StorageService::ensureEntity(const QString &name,const QString &type)
{
    if(name.trimmed().isEmpty())return false;QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("INSERT INTO memory_entities(uuid,entity_type,name,created_at,updated_at,sync_status) "
        "VALUES(lower(hex(randomblob(16))),:type,:name,:now,:now,'pending') ON CONFLICT(name) DO UPDATE SET updated_at=excluded.updated_at"));
    q.bindValue(":type",type);q.bindValue(":name",name.trimmed());q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));return q.exec();
}

bool StorageService::linkMemoryToEntity(const QString &category,const QString &subject,const QString &entityName)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO memory_relations(memory_id,entity_id,relation_type,created_at) "
        "SELECT m.id,e.id,'mentions',:now FROM memories m,memory_entities e "
        "WHERE m.category=:category AND m.subject=:subject AND e.name=:name AND m.deleted_at IS NULL"));
    q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":category",category);q.bindValue(":subject",subject);q.bindValue(":name",entityName);return q.exec();
}

QStringList StorageService::entityNames() const
{
    QStringList result;QSqlQuery q(QSqlDatabase::database(m_connectionName));if(q.exec(QStringLiteral("SELECT name FROM memory_entities WHERE deleted_at IS NULL ORDER BY length(name) DESC")))while(q.next())result<<q.value(0).toString();return result;
}

qint64 StorageService::addReminder(const QString &type,const QDateTime &scheduledAt,const QString &payload)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("INSERT INTO reminders(uuid,reminder_type,scheduled_at,status,payload,created_at,updated_at) "
        "VALUES(lower(hex(randomblob(16))),:type,:at,'pending',:payload,:now,:now)"));
    const QString now=QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    q.bindValue(":type",type);q.bindValue(":at",scheduledAt.toString(Qt::ISODateWithMs));q.bindValue(":payload",payload);q.bindValue(":now",now);
    return q.exec()?q.lastInsertId().toLongLong():0;
}

QList<ReminderRecord> StorageService::loadDueReminders(const QDateTime &now,int limit) const
{
    QList<ReminderRecord> result;QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("SELECT id,reminder_type,scheduled_at,status,payload FROM reminders "
        "WHERE status='pending' AND scheduled_at<=:now ORDER BY scheduled_at,id LIMIT :limit"));
    q.bindValue(":now",now.toString(Qt::ISODateWithMs));q.bindValue(":limit",limit);
    if(q.exec())while(q.next()){ReminderRecord r;r.id=q.value(0).toLongLong();r.type=q.value(1).toString();
        r.scheduledAt=QDateTime::fromString(q.value(2).toString(),Qt::ISODateWithMs);r.status=q.value(3).toString();r.payload=q.value(4).toString();result<<r;}return result;
}

bool StorageService::updateReminderStatus(qint64 id,const QString &status)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral(
        "UPDATE reminders SET status=:status,delivered_at=CASE WHEN :status='delivered' THEN :now ELSE delivered_at END,updated_at=:now,sync_revision=sync_revision+1,sync_status='pending' WHERE id=:id"));
    q.bindValue(":status",status);q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec();
}
int StorageService::cancelReminders(const QString&typePrefix,const QString&payloadContains){QSqlQuery q(QSqlDatabase::database(m_connectionName));QString sql=QStringLiteral(
    "UPDATE reminders SET status='cancelled',updated_at=:now,sync_revision=sync_revision+1,sync_status='pending' WHERE status='pending' AND reminder_type LIKE :type");if(!payloadContains.isEmpty())sql+=QStringLiteral(" AND payload LIKE :payload");q.prepare(sql);
    q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":type",typePrefix+QStringLiteral("%"));if(!payloadContains.isEmpty())q.bindValue(":payload",QStringLiteral("%%1%").arg(payloadContains));return q.exec()?q.numRowsAffected():0;}

int StorageService::deliveredReminderCount(const QDate &date,const QString &typePrefix) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));QString sql=QStringLiteral(
        "SELECT count(*) FROM reminders WHERE status='delivered' AND delivered_at>=:start AND delivered_at<:end");
    if(!typePrefix.isEmpty())sql+=QStringLiteral(" AND reminder_type LIKE :prefix");q.prepare(sql);
    q.bindValue(":start",QDateTime(date.startOfDay()).toString(Qt::ISODateWithMs));q.bindValue(":end",QDateTime(date.addDays(1).startOfDay()).toString(Qt::ISODateWithMs));
    if(!typePrefix.isEmpty())q.bindValue(":prefix",typePrefix+QStringLiteral("%"));return q.exec()&&q.next()?q.value(0).toInt():0;
}

qint64 StorageService::addCommitment(const QString &description,const QDateTime &dueAt){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral(
    "INSERT INTO commitments(description,due_at,status,created_at,updated_at) VALUES(:text,:due,'active',:now,:now)"));const QString now=QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    q.bindValue(":text",description);q.bindValue(":due",dueAt.toString(Qt::ISODateWithMs));q.bindValue(":now",now);return q.exec()?q.lastInsertId().toLongLong():0;}
QList<CommitmentRecord> StorageService::loadActiveCommitments()const{QList<CommitmentRecord> out;QSqlQuery q(QSqlDatabase::database(m_connectionName));if(q.exec(QStringLiteral(
    "SELECT id,description,due_at,status,created_at FROM commitments WHERE status='active' ORDER BY due_at,id")))while(q.next()){CommitmentRecord c;c.id=q.value(0).toLongLong();c.description=q.value(1).toString();c.dueAt=QDateTime::fromString(q.value(2).toString(),Qt::ISODateWithMs);c.status=q.value(3).toString();c.createdAt=QDateTime::fromString(q.value(4).toString(),Qt::ISODateWithMs);out<<c;}return out;}
bool StorageService::updateCommitmentStatus(qint64 id,const QString&status){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral(
    "UPDATE commitments SET status=:status,updated_at=:now WHERE id=:id"));q.bindValue(":status",status);q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec();}
bool StorageService::updateCommitmentDue(qint64 id,const QDateTime&dueAt){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral(
    "UPDATE commitments SET due_at=:due,updated_at=:now WHERE id=:id AND status='active'"));q.bindValue(":due",dueAt.toString(Qt::ISODateWithMs));q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec();}

qint64 StorageService::addCognitiveRecord(const CognitiveRecord &r)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral("INSERT INTO cognitive_records(uuid,record_type,subject,source_text,scheduled_at,event_end_at,follow_up_at,expires_at,status,explicit_request,delivery_priority,memory_importance,follow_up_count,max_follow_ups,follow_up_policy,reminder_id,created_at,updated_at,sync_status) VALUES(lower(hex(randomblob(16))),:type,:subject,:source,:scheduled,:end,:follow,:expires,:status,:explicit,:priority,:importance,:count,:max,:policy,:reminder,:now,:now,'pending')"));
    const QString now=QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    q.bindValue(":type",r.recordType);q.bindValue(":subject",r.subject.trimmed());q.bindValue(":source",r.sourceText);
    q.bindValue(":scheduled",r.scheduledAt.isValid()?r.scheduledAt.toString(Qt::ISODateWithMs):QVariant());
    q.bindValue(":end",r.eventEndAt.isValid()?r.eventEndAt.toString(Qt::ISODateWithMs):QVariant());
    q.bindValue(":follow",r.followUpAt.isValid()?r.followUpAt.toString(Qt::ISODateWithMs):QVariant());
    q.bindValue(":expires",r.expiresAt.isValid()?r.expiresAt.toString(Qt::ISODateWithMs):QVariant());
    q.bindValue(":status",r.status);q.bindValue(":explicit",r.explicitRequest?1:0);q.bindValue(":priority",r.deliveryPriority);q.bindValue(":importance",r.memoryImportance);
    q.bindValue(":count",r.followUpCount);q.bindValue(":max",r.maxFollowUps);q.bindValue(":policy",r.followUpPolicy);q.bindValue(":reminder",r.reminderId>0?QVariant(r.reminderId):QVariant());q.bindValue(":now",now);
    return q.exec()?q.lastInsertId().toLongLong():0;
}

qint64 StorageService::addCognitiveReminderAtomic(const CognitiveRecord &record,
                                                   const QString &reminderType,
                                                   const QDateTime &scheduledAt,
                                                   const QString &payload)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.transaction()) return 0;
    const qint64 cognitiveId = addCognitiveRecord(record);
    if (cognitiveId <= 0) { db.rollback(); return 0; }
    if (reminderType.trimmed().isEmpty() || !scheduledAt.isValid()) { db.rollback(); return 0; }
    const qint64 reminderId = addReminder(reminderType, scheduledAt, payload);
    if (reminderId <= 0
        || !updateCognitiveRecord(cognitiveId, record.status, scheduledAt, record.followUpAt,
                                  record.followUpCount, reminderId)) {
        db.rollback(); return 0;
    }
    if (!db.commit()) { db.rollback(); return 0; }
    return cognitiveId;
}

QList<CognitiveRecord> StorageService::loadCognitiveRecords(const QStringList &statuses) const
{
    QList<CognitiveRecord> out;QSqlQuery q(QSqlDatabase::database(m_connectionName));
    QString sql=QStringLiteral("SELECT id,uuid,record_type,subject,source_text,scheduled_at,event_end_at,follow_up_at,expires_at,status,explicit_request,delivery_priority,memory_importance,follow_up_count,max_follow_ups,follow_up_policy,reminder_id,created_at,updated_at FROM cognitive_records");
    if(!statuses.isEmpty()){QStringList marks;for(int i=0;i<statuses.size();++i)marks<<QStringLiteral(":s%1").arg(i);sql+=QStringLiteral(" WHERE status IN (")+marks.join(',')+QLatin1Char(')');}
    sql+=QStringLiteral(" ORDER BY updated_at DESC,id DESC");q.prepare(sql);for(int i=0;i<statuses.size();++i)q.bindValue(QStringLiteral(":s%1").arg(i),statuses.at(i));
    if(q.exec())while(q.next()){CognitiveRecord r;r.id=q.value(0).toLongLong();r.uuid=q.value(1).toString();r.recordType=q.value(2).toString();r.subject=q.value(3).toString();r.sourceText=q.value(4).toString();r.scheduledAt=QDateTime::fromString(q.value(5).toString(),Qt::ISODateWithMs);r.eventEndAt=QDateTime::fromString(q.value(6).toString(),Qt::ISODateWithMs);r.followUpAt=QDateTime::fromString(q.value(7).toString(),Qt::ISODateWithMs);r.expiresAt=QDateTime::fromString(q.value(8).toString(),Qt::ISODateWithMs);r.status=q.value(9).toString();r.explicitRequest=q.value(10).toBool();r.deliveryPriority=q.value(11).toInt();r.memoryImportance=q.value(12).toInt();r.followUpCount=q.value(13).toInt();r.maxFollowUps=q.value(14).toInt();r.followUpPolicy=q.value(15).toString();r.reminderId=q.value(16).toLongLong();r.createdAt=QDateTime::fromString(q.value(17).toString(),Qt::ISODateWithMs);r.updatedAt=QDateTime::fromString(q.value(18).toString(),Qt::ISODateWithMs);out<<r;}return out;
}

bool StorageService::updateCognitiveRecord(qint64 id,const QString &status,const QDateTime &scheduled,const QDateTime &follow,int count,qint64 reminderId)
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE cognitive_records SET status=:status,scheduled_at=CASE WHEN :scheduled='' THEN scheduled_at ELSE :scheduled END,follow_up_at=CASE WHEN :follow='' THEN follow_up_at ELSE :follow END,follow_up_count=CASE WHEN :count<0 THEN follow_up_count ELSE :count END,reminder_id=CASE WHEN :reminder<0 THEN reminder_id ELSE :reminder END,updated_at=:now,sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id"));
    q.bindValue(":status",status);q.bindValue(":scheduled",scheduled.isValid()?scheduled.toString(Qt::ISODateWithMs):QString());q.bindValue(":follow",follow.isValid()?follow.toString(Qt::ISODateWithMs):QString());q.bindValue(":count",count);q.bindValue(":reminder",reminderId);q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec()&&q.numRowsAffected()==1;
}

QList<CognitiveRecord> StorageService::loadDueCognitiveFollowUps(const QDateTime &now,int limit) const
{
    QList<CognitiveRecord> all=loadCognitiveRecords({QStringLiteral("awaiting_followup")}),out;for(const auto&r:all)if(r.followUpAt.isValid()&&r.followUpAt<=now&&r.followUpCount<r.maxFollowUps){out<<r;if(out.size()>=limit)break;}return out;
}
int StorageService::archiveExpiredCognitiveRecords(const QDateTime &now){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE cognitive_records SET status='archived',updated_at=:now,sync_status='pending',sync_revision=sync_revision+1 WHERE status NOT IN ('archived','completed','cancelled') AND expires_at IS NOT NULL AND expires_at<:now"));q.bindValue(":now",now.toString(Qt::ISODateWithMs));if(!q.exec())return 0;const int changed=q.numRowsAffected();QSqlQuery cleanup(QSqlDatabase::database(m_connectionName));cleanup.exec(QStringLiteral("UPDATE reminders SET status='expired',updated_at=datetime('now'),sync_revision=sync_revision+1,sync_status='pending' WHERE status='pending' AND id IN (SELECT reminder_id FROM cognitive_records WHERE status='archived' AND reminder_id IS NOT NULL)"));return changed;}
int StorageService::removeTimeBoundMemories(){QSqlQuery q(QSqlDatabase::database(m_connectionName));return q.exec(QStringLiteral("UPDATE memories SET memory_state='archived',archived_at=datetime('now'),governance_reason='migrated_time_bound_event',sync_status='pending',sync_revision=sync_revision+1 WHERE deleted_at IS NULL AND memory_state='active' AND category='event' AND (content LIKE '%开会%' OR content LIKE '%提醒%' OR content LIKE '%明天%' OR content LIKE '%后天%' OR content LIKE '%几点%')"))?q.numRowsAffected():0;}

bool StorageService::addSnackToInventory(const QString&type,const QString&name,const QString&emoji,int nutrition){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral(
    "INSERT INTO snack_inventory(uuid,snack_type,snack_name,emoji,quantity,nutrition,created_at,updated_at,sync_status) VALUES(lower(hex(randomblob(16))),:type,:name,:emoji,1,:nutrition,:now,:now,'pending') "
    "ON CONFLICT(snack_type) DO UPDATE SET quantity=quantity+1,nutrition=excluded.nutrition,updated_at=excluded.updated_at,sync_status='pending',sync_revision=sync_revision+1"));const QString now=QDateTime::currentDateTime().toString(Qt::ISODateWithMs);q.bindValue(":type",type);q.bindValue(":name",name);q.bindValue(":emoji",emoji);q.bindValue(":nutrition",nutrition);q.bindValue(":now",now);return q.exec();}
QList<SnackInventoryRecord> StorageService::loadSnackInventory()const{QList<SnackInventoryRecord> out;QSqlQuery q(QSqlDatabase::database(m_connectionName));if(q.exec(QStringLiteral("SELECT id,uuid,snack_type,snack_name,emoji,quantity,nutrition,updated_at FROM snack_inventory WHERE quantity>0 ORDER BY updated_at DESC")))while(q.next())out.append({q.value(0).toLongLong(),q.value(1).toString(),q.value(2).toString(),q.value(3).toString(),q.value(4).toString(),q.value(5).toInt(),q.value(6).toInt(),QDateTime::fromString(q.value(7).toString(),Qt::ISODateWithMs)});return out;}
bool StorageService::consumeSnackInventory(qint64 id){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE snack_inventory SET quantity=quantity-1,updated_at=:now,sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id AND quantity>0"));q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec()&&q.numRowsAffected()==1;}
QList<SnackCatalogRecord> StorageService::loadSnackCatalog()const{QList<SnackCatalogRecord> out;QSqlQuery q(QSqlDatabase::database(m_connectionName));if(q.exec(QStringLiteral("SELECT snack_type,snack_name,emoji,eaten_count,preference,consecutive_count,first_unlocked_at,last_eaten_at,nickname FROM snack_catalog ORDER BY eaten_count DESC,first_unlocked_at")))while(q.next())out.append({q.value(0).toString(),q.value(1).toString(),q.value(2).toString(),q.value(3).toInt(),q.value(4).toInt(),q.value(5).toInt(),QDateTime::fromString(q.value(6).toString(),Qt::ISODateWithMs),QDateTime::fromString(q.value(7).toString(),Qt::ISODateWithMs),q.value(8).toString()});return out;}
SnackCatalogRecord StorageService::recordSnackEaten(const QString&type,const QString&name,const QString&emoji){const int base=type=="image"?82:type=="audio"?86:type=="video"?74:type=="document"?66:type=="code"?58:type=="archive"?48:62;const QString now=QDateTime::currentDateTime().toString(Qt::ISODateWithMs);QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral(
    "INSERT INTO snack_catalog(snack_type,snack_name,emoji,eaten_count,preference,consecutive_count,first_unlocked_at,last_eaten_at,updated_at,sync_status) VALUES(:type,:name,:emoji,1,:pref,1,:now,:now,:now,'pending') "
    "ON CONFLICT(snack_type) DO UPDATE SET snack_name=excluded.snack_name,emoji=excluded.emoji,eaten_count=eaten_count+1,consecutive_count=CASE WHEN julianday(excluded.last_eaten_at)-julianday(last_eaten_at)<1 THEN consecutive_count+1 ELSE 1 END,last_eaten_at=excluded.last_eaten_at,updated_at=excluded.updated_at,sync_status='pending',sync_revision=sync_revision+1"));q.bindValue(":type",type);q.bindValue(":name",name);q.bindValue(":emoji",emoji);q.bindValue(":pref",base);q.bindValue(":now",now);q.exec();QSqlQuery s(QSqlDatabase::database(m_connectionName));s.prepare(QStringLiteral("SELECT snack_type,snack_name,emoji,eaten_count,preference,consecutive_count,first_unlocked_at,last_eaten_at,nickname FROM snack_catalog WHERE snack_type=:type"));s.bindValue(":type",type);if(s.exec()&&s.next())return{s.value(0).toString(),s.value(1).toString(),s.value(2).toString(),s.value(3).toInt(),s.value(4).toInt(),s.value(5).toInt(),QDateTime::fromString(s.value(6).toString(),Qt::ISODateWithMs),QDateTime::fromString(s.value(7).toString(),Qt::ISODateWithMs),s.value(8).toString()};return{};}
bool StorageService::addSnackHistory(const QString&eventType,const QString&type,const QString&name,const QString&sourceName,qint64 sourceSize,const QString&safety){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("INSERT INTO snack_history(uuid,event_type,snack_type,snack_name,source_name,source_size,safety_level,created_at,sync_status) VALUES(lower(hex(randomblob(16))),:event,:type,:name,:source,:size,:safety,:now,'pending')"));q.bindValue(":event",eventType);q.bindValue(":type",type);q.bindValue(":name",name);q.bindValue(":source",sourceName);q.bindValue(":size",sourceSize);q.bindValue(":safety",safety);q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));return q.exec();}
QStringList StorageService::loadSnackHistory(int limit)const{QStringList out;QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("SELECT created_at,event_type,snack_name,source_name FROM snack_history ORDER BY id DESC LIMIT :limit"));q.bindValue(":limit",limit);if(q.exec())while(q.next()){const QString event=q.value(1).toString()=="stored"?QStringLiteral("放入零食袋"):QStringLiteral("吃掉");out<<QStringLiteral("%1 · %2%3 · %4").arg(QDateTime::fromString(q.value(0).toString(),Qt::ISODateWithMs).toString("MM-dd HH:mm"),event,q.value(2).toString(),q.value(3).toString());}return out;}
qint64 StorageService::addSummary(const QString&t,const QString&s,const QString&m,const QString&c){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("INSERT INTO ai_summaries(uuid,title,source_name,summary_mode,content,created_at,sync_status) VALUES(lower(hex(randomblob(16))),:title,:source,:mode,:content,:now,'pending')"));q.bindValue(":title",t);q.bindValue(":source",s);q.bindValue(":mode",m);q.bindValue(":content",c);q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));return q.exec()?q.lastInsertId().toLongLong():0;}
QList<SummaryRecord> StorageService::loadSummaries(int limit)const{QList<SummaryRecord>out;QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("SELECT id,title,source_name,summary_mode,content,created_at FROM ai_summaries WHERE deleted_at IS NULL ORDER BY id DESC LIMIT :limit"));q.bindValue(":limit",limit);if(q.exec())while(q.next())out.append({q.value(0).toLongLong(),q.value(1).toString(),q.value(2).toString(),q.value(3).toString(),q.value(4).toString(),QDateTime::fromString(q.value(5).toString(),Qt::ISODateWithMs)});return out;}
bool StorageService::deleteSummary(qint64 id){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE ai_summaries SET deleted_at=:now,sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id"));q.bindValue(":now",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec()&&q.numRowsAffected()==1;}

qint64 StorageService::addDream(const DreamRecord&d){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("INSERT OR IGNORE INTO dreams(uuid,dream_date,title,content,mood,dream_type,symbols,color,reality_hint,continuation_key,memory_ids,created_at,sync_status) VALUES(lower(hex(randomblob(16))),:date,:title,:content,:mood,:type,:symbols,:color,:hint,:continuation,:memory_ids,:created,'pending')"));q.bindValue(":date",d.dreamDate.toString(Qt::ISODate));q.bindValue(":title",d.title);q.bindValue(":content",d.content);q.bindValue(":mood",d.mood);q.bindValue(":type",d.dreamType);q.bindValue(":symbols",d.symbols.join(QStringLiteral("\n")));q.bindValue(":color",d.color);q.bindValue(":hint",d.realityHint);q.bindValue(":continuation",d.continuationKey);q.bindValue(":memory_ids",d.memoryIds.join(QLatin1Char(',')));q.bindValue(":created",(d.createdAt.isValid()?d.createdAt:QDateTime::currentDateTime()).toString(Qt::ISODateWithMs));return q.exec()?q.lastInsertId().toLongLong():0;}
QList<DreamRecord> StorageService::loadDreams(int limit)const{QList<DreamRecord>out;QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("SELECT id,uuid,dream_date,title,content,mood,dream_type,symbols,color,reality_hint,continuation_key,memory_ids,created_at,opened_at,favorite,reality_echo,echo_created_at,disclosure_level,disclosed_at FROM dreams WHERE deleted_at IS NULL ORDER BY dream_date DESC,id DESC LIMIT :limit"));q.bindValue(":limit",limit);if(q.exec())while(q.next()){DreamRecord d;d.id=q.value(0).toLongLong();d.uuid=q.value(1).toString();d.dreamDate=QDate::fromString(q.value(2).toString(),Qt::ISODate);d.title=q.value(3).toString();d.content=q.value(4).toString();d.mood=q.value(5).toString();d.dreamType=q.value(6).toString();d.symbols=q.value(7).toString().split(QLatin1Char('\n'),Qt::SkipEmptyParts);d.color=q.value(8).toString();d.realityHint=q.value(9).toString();d.continuationKey=q.value(10).toString();d.memoryIds=q.value(11).toString().split(QLatin1Char(','),Qt::SkipEmptyParts);d.createdAt=QDateTime::fromString(q.value(12).toString(),Qt::ISODateWithMs);d.openedAt=QDateTime::fromString(q.value(13).toString(),Qt::ISODateWithMs);d.favorite=q.value(14).toBool();d.realityEcho=q.value(15).toString();d.echoCreatedAt=QDateTime::fromString(q.value(16).toString(),Qt::ISODateWithMs);d.disclosureLevel=q.value(17).toInt();d.disclosedAt=QDateTime::fromString(q.value(18).toString(),Qt::ISODateWithMs);out.append(d);}return out;}
bool StorageService::hasDreamForDate(const QDate&date)const{QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("SELECT 1 FROM dreams WHERE dream_date=:date AND deleted_at IS NULL LIMIT 1"));q.bindValue(":date",date.toString(Qt::ISODate));return q.exec()&&q.next();}
bool StorageService::markDreamOpened(qint64 id,const QDateTime&at){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE dreams SET opened_at=COALESCE(opened_at,:at),sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id"));q.bindValue(":at",at.toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec()&&q.numRowsAffected()==1;}
bool StorageService::setDreamFavorite(qint64 id,bool favorite){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE dreams SET favorite=:favorite,sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id"));q.bindValue(":favorite",favorite?1:0);q.bindValue(":id",id);return q.exec()&&q.numRowsAffected()==1;}
bool StorageService::saveDreamRealityEcho(qint64 id,const QString&echo){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE dreams SET reality_echo=:echo,echo_created_at=:at,sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id"));q.bindValue(":echo",echo.left(2000));q.bindValue(":at",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec()&&q.numRowsAffected()==1;}
bool StorageService::setDreamDisclosure(qint64 id,int level,const QDateTime&at){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE dreams SET disclosure_level=:level,disclosed_at=:at,sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id"));q.bindValue(":level",qBound(0,level,3));q.bindValue(":at",at.toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec()&&q.numRowsAffected()==1;}

bool StorageService::upsertMorningLollipop(const MorningLollipopRecord&r){QSqlDatabase db=QSqlDatabase::database(m_connectionName);QSqlQuery q(db);q.prepare(QStringLiteral("INSERT INTO morning_lollipops(uuid,gift_date,flavor_id,flavor_name,category,emoji,color,rarity,planned_at,actual_at,status,greeting,greeting_fingerprint,generation_source,delay_reason,viewed,favorite,created_at,updated_at,sync_status) VALUES(lower(hex(randomblob(16))),:date,:fid,:name,:cat,:emoji,:color,:rarity,:planned,:actual,:status,:greeting,:fp,:source,:reason,:viewed,:favorite,:created,:updated,'pending') ON CONFLICT(gift_date) DO UPDATE SET flavor_id=excluded.flavor_id,flavor_name=excluded.flavor_name,category=excluded.category,emoji=excluded.emoji,color=excluded.color,rarity=excluded.rarity,planned_at=excluded.planned_at,actual_at=excluded.actual_at,status=excluded.status,greeting=excluded.greeting,greeting_fingerprint=excluded.greeting_fingerprint,generation_source=excluded.generation_source,delay_reason=excluded.delay_reason,viewed=excluded.viewed,favorite=excluded.favorite,updated_at=excluded.updated_at,sync_status='pending',sync_revision=sync_revision+1"));const auto now=QDateTime::currentDateTime().toString(Qt::ISODateWithMs);const QString date=r.giftDate.toString(Qt::ISODate);q.bindValue(":date",date);q.bindValue(":fid",r.flavorId);q.bindValue(":name",r.flavorName);q.bindValue(":cat",r.category);q.bindValue(":emoji",r.emoji);q.bindValue(":color",r.color);q.bindValue(":rarity",r.rarity);q.bindValue(":planned",r.plannedAt.toString(Qt::ISODateWithMs));q.bindValue(":actual",r.actualAt.isValid()?QVariant(r.actualAt.toString(Qt::ISODateWithMs)):QVariant());q.bindValue(":status",r.status);q.bindValue(":greeting",r.greeting);q.bindValue(":fp",r.greetingFingerprint);q.bindValue(":source",r.generationSource);q.bindValue(":reason",r.delayReason);q.bindValue(":viewed",r.viewed);q.bindValue(":favorite",r.favorite);q.bindValue(":created",r.createdAt.isValid()?r.createdAt.toString(Qt::ISODateWithMs):now);q.bindValue(":updated",now);if(!q.exec())return false;QSqlQuery m(db);m.prepare(QStringLiteral("INSERT INTO lollipop_metadata(gift_date,acquisition_type,theme_tags,weather_snapshot,story,memorial_key,shape,pattern) VALUES(:date,:type,:tags,:weather,:story,:key,:shape,:pattern) ON CONFLICT(gift_date) DO UPDATE SET acquisition_type=excluded.acquisition_type,theme_tags=excluded.theme_tags,weather_snapshot=excluded.weather_snapshot,story=excluded.story,memorial_key=excluded.memorial_key,shape=excluded.shape,pattern=excluded.pattern"));m.bindValue(":date",date);m.bindValue(":type",r.acquisitionType);m.bindValue(":tags",r.themeTags);m.bindValue(":weather",r.weatherSnapshot);m.bindValue(":story",r.story);m.bindValue(":key",r.memorialKey);m.bindValue(":shape",r.shape);m.bindValue(":pattern",r.pattern);return m.exec();}
static MorningLollipopRecord readLollipop(QSqlQuery&q){MorningLollipopRecord r;r.id=q.value(0).toLongLong();r.uuid=q.value(1).toString();r.giftDate=QDate::fromString(q.value(2).toString(),Qt::ISODate);r.flavorId=q.value(3).toString();r.flavorName=q.value(4).toString();r.category=q.value(5).toString();r.emoji=q.value(6).toString();r.color=q.value(7).toString();r.rarity=q.value(8).toString();r.plannedAt=QDateTime::fromString(q.value(9).toString(),Qt::ISODateWithMs);r.actualAt=QDateTime::fromString(q.value(10).toString(),Qt::ISODateWithMs);r.status=q.value(11).toString();r.greeting=q.value(12).toString();r.greetingFingerprint=q.value(13).toString();r.generationSource=q.value(14).toString();r.delayReason=q.value(15).toString();r.viewed=q.value(16).toBool();r.favorite=q.value(17).toBool();r.createdAt=QDateTime::fromString(q.value(18).toString(),Qt::ISODateWithMs);r.updatedAt=QDateTime::fromString(q.value(19).toString(),Qt::ISODateWithMs);return r;}
static void loadLollipopMeta(QSqlDatabase db,MorningLollipopRecord&r){if(!r.giftDate.isValid())return;QSqlQuery m(db);m.prepare(QStringLiteral("SELECT acquisition_type,theme_tags,weather_snapshot,story,memorial_key,shape,pattern FROM lollipop_metadata WHERE gift_date=:date"));m.bindValue(":date",r.giftDate.toString(Qt::ISODate));if(m.exec()&&m.next()){r.acquisitionType=m.value(0).toString();r.themeTags=m.value(1).toString();r.weatherSnapshot=m.value(2).toString();r.story=m.value(3).toString();r.memorialKey=m.value(4).toString();r.shape=m.value(5).toString();r.pattern=m.value(6).toString();}}
MorningLollipopRecord StorageService::loadMorningLollipop(const QDate&date)const{QSqlDatabase db=QSqlDatabase::database(m_connectionName);QSqlQuery q(db);q.prepare(QStringLiteral("SELECT id,uuid,gift_date,flavor_id,flavor_name,category,emoji,color,rarity,planned_at,actual_at,status,greeting,greeting_fingerprint,generation_source,delay_reason,viewed,favorite,created_at,updated_at FROM morning_lollipops WHERE gift_date=:date"));q.bindValue(":date",date.toString(Qt::ISODate));MorningLollipopRecord r;if(q.exec()&&q.next())r=readLollipop(q);loadLollipopMeta(db,r);return r;}
QList<MorningLollipopRecord> StorageService::loadMorningLollipops(int limit)const{QList<MorningLollipopRecord>out;QSqlDatabase db=QSqlDatabase::database(m_connectionName);QSqlQuery q(db);q.prepare(QStringLiteral("SELECT id,uuid,gift_date,flavor_id,flavor_name,category,emoji,color,rarity,planned_at,actual_at,status,greeting,greeting_fingerprint,generation_source,delay_reason,viewed,favorite,created_at,updated_at FROM morning_lollipops ORDER BY gift_date DESC,id DESC LIMIT :limit"));q.bindValue(":limit",limit);if(q.exec())while(q.next()){auto r=readLollipop(q);loadLollipopMeta(db,r);out<<r;}return out;}
bool StorageService::setMorningLollipopFavorite(qint64 id,bool favorite){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE morning_lollipops SET favorite=:v,updated_at=:at,sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id"));q.bindValue(":v",favorite);q.bindValue(":at",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec();}
bool StorageService::markMorningLollipopViewed(qint64 id){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE morning_lollipops SET viewed=1,updated_at=:at,sync_status='pending',sync_revision=sync_revision+1 WHERE id=:id"));q.bindValue(":at",QDateTime::currentDateTime().toString(Qt::ISODateWithMs));q.bindValue(":id",id);return q.exec();}
bool StorageService::hasMorningLollipopMemorial(const QString&key)const{if(key.isEmpty())return false;QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("SELECT 1 FROM lollipop_metadata WHERE memorial_key=:key LIMIT 1"));q.bindValue(":key",key);return q.exec()&&q.next();}

PetStateRecord StorageService::loadPetState() const
{
    PetStateRecord state;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral(
            "SELECT mood, energy, health, closeness, boredom, neglect, curiosity, irritation, last_interaction, last_illness, "
            "health_phase, condition, recovery_progress, illness_started_at, phase_changed_at, last_illness_check_date, fullness, last_digestion_at "
            "FROM pet_state WHERE id=1"))
        || !query.next()) {
        return state;
    }

    state.mood = query.value(0).toInt();
    state.energy = query.value(1).toInt();
    state.health = query.value(2).toInt();
    state.closeness = query.value(3).toInt();
    state.boredom = query.value(4).toInt();
    state.neglect = query.value(5).toInt();
    state.curiosity = query.value(6).toInt();
    state.irritation = query.value(7).toInt();
    state.lastInteraction = QDateTime::fromString(query.value(8).toString(), Qt::ISODateWithMs);
    state.lastIllness = QDateTime::fromString(query.value(9).toString(), Qt::ISODateWithMs);
    state.healthPhase = query.value(10).toString();
    state.condition = query.value(11).toString();
    state.recoveryProgress = query.value(12).toInt();
    state.illnessStartedAt = QDateTime::fromString(query.value(13).toString(), Qt::ISODateWithMs);
    state.phaseChangedAt = QDateTime::fromString(query.value(14).toString(), Qt::ISODateWithMs);
    state.lastIllnessCheckDate = QDate::fromString(query.value(15).toString(), Qt::ISODate);
    state.fullness = query.value(16).toInt();
    state.lastDigestionAt = QDateTime::fromString(query.value(17).toString(), Qt::ISODateWithMs);
    return state;
}

bool StorageService::savePetState(const PetStateRecord &state)
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "UPDATE pet_state SET mood=:mood, energy=:energy, health=:health, closeness=:closeness, sync_revision=sync_revision+1,sync_status='pending', "
        "boredom=:boredom, neglect=:neglect, curiosity=:curiosity, irritation=:irritation, last_interaction=:last_interaction, "
        "last_illness=:last_illness, health_phase=:health_phase, condition=:condition, recovery_progress=:recovery_progress, "
        "illness_started_at=:illness_started_at, phase_changed_at=:phase_changed_at, last_illness_check_date=:last_illness_check_date, fullness=:fullness, last_digestion_at=:last_digestion_at, "
        "updated_at=:updated_at WHERE id=1"));
    query.bindValue(QStringLiteral(":mood"), state.mood);
    query.bindValue(QStringLiteral(":energy"), state.energy);
    query.bindValue(QStringLiteral(":health"), state.health);
    query.bindValue(QStringLiteral(":closeness"), state.closeness);
    query.bindValue(QStringLiteral(":boredom"), state.boredom);
    query.bindValue(QStringLiteral(":neglect"), state.neglect);
    query.bindValue(QStringLiteral(":curiosity"), state.curiosity);
    query.bindValue(QStringLiteral(":irritation"), state.irritation);
    query.bindValue(QStringLiteral(":last_interaction"), state.lastInteraction.toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":last_illness"), state.lastIllness.toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":health_phase"), state.healthPhase);
    query.bindValue(QStringLiteral(":condition"), state.condition);
    query.bindValue(QStringLiteral(":recovery_progress"), state.recoveryProgress);
    query.bindValue(QStringLiteral(":illness_started_at"), state.illnessStartedAt.toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":phase_changed_at"), state.phaseChangedAt.toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":last_illness_check_date"), state.lastIllnessCheckDate.toString(Qt::ISODate));
    query.bindValue(QStringLiteral(":fullness"), state.fullness);
    query.bindValue(QStringLiteral(":last_digestion_at"), state.lastDigestionAt.toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":updated_at"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    return query.exec();
}

bool StorageService::setSyncEnabled(const QString&type,bool enabled){static const QStringList allowed{QStringLiteral("settings"),QStringLiteral("pet_state"),QStringLiteral("memory"),QStringLiteral("reminder")};if(!allowed.contains(type))return false;QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("INSERT INTO sync_preferences(entity_type,enabled,updated_at) VALUES(:type,:enabled,:now) ON CONFLICT(entity_type) DO UPDATE SET enabled=excluded.enabled,updated_at=excluded.updated_at"));q.bindValue(":type",type);q.bindValue(":enabled",enabled?1:0);q.bindValue(":now",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));return q.exec();}
bool StorageService::syncEnabled(const QString&type)const{QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("SELECT enabled FROM sync_preferences WHERE entity_type=:type"));q.bindValue(":type",type);return q.exec()&&q.next()&&q.value(0).toBool();}
bool StorageService::setSyncMasterEnabled(bool enabled){QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("UPDATE sync_runtime SET master_enabled=:enabled,updated_at=:now WHERE id=1"));q.bindValue(":enabled",enabled?1:0);q.bindValue(":now",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));return q.exec()&&q.numRowsAffected()==1;}
bool StorageService::syncMasterEnabled()const{QSqlQuery q(QSqlDatabase::database(m_connectionName));return q.exec(QStringLiteral("SELECT master_enabled FROM sync_runtime WHERE id=1"))&&q.next()&&q.value(0).toBool();}
QString StorageService::syncDeviceId()const{QSqlQuery q(QSqlDatabase::database(m_connectionName));return q.exec(QStringLiteral("SELECT device_id FROM sync_runtime WHERE id=1"))&&q.next()?q.value(0).toString():QString();}
QJsonObject StorageService::syncStatus()const{QJsonObject out{{QStringLiteral("master_enabled"),syncMasterEnabled()},{QStringLiteral("device_id"),syncDeviceId()}};QSqlQuery q(QSqlDatabase::database(m_connectionName));if(q.exec(QStringLiteral("SELECT SUM(CASE WHEN status IN ('pending','retry') THEN 1 ELSE 0 END),COALESCE(MAX(CASE WHEN status='retry' THEN last_error_code END),''),(SELECT COALESCE(last_success_at,'') FROM sync_runtime WHERE id=1) FROM sync_outbox"))&&q.next()){out.insert(QStringLiteral("pending_count"),q.value(0).toInt());out.insert(QStringLiteral("last_error"),q.value(1).toString());out.insert(QStringLiteral("last_success_at"),q.value(2).toString());}return out;}
bool StorageService::setSyncSetting(const QString&key,const QString&value){if(key.isEmpty()||key.size()>128||key.contains(QStringLiteral("secret"),Qt::CaseInsensitive)||key.contains(QStringLiteral("token"),Qt::CaseInsensitive)||key.contains(QStringLiteral("key"),Qt::CaseInsensitive)||key.contains(QStringLiteral("path"),Qt::CaseInsensitive))return false;QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("INSERT INTO sync_settings(setting_key,setting_value,updated_at) VALUES(:key,:value,:now) ON CONFLICT(setting_key) DO UPDATE SET setting_value=excluded.setting_value,sync_revision=sync_settings.sync_revision+1,sync_status='pending',updated_at=excluded.updated_at"));q.bindValue(":key",key);q.bindValue(":value",value);q.bindValue(":now",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));return q.exec();}
bool StorageService::setEntitySyncPrivacy(const QString&type,const QString&uuid,const QString&privacy){if(!QStringList{QStringLiteral("normal"),QStringLiteral("private"),QStringLiteral("secret"),QStringLiteral("local_only")}.contains(privacy)||type!=QStringLiteral("memory"))return false;QSqlDatabase db=QSqlDatabase::database(m_connectionName);if(!db.transaction())return false;QSqlQuery q(db);q.prepare(QStringLiteral("UPDATE memories SET privacy_level=:privacy,sync_revision=sync_revision+1,sync_status='pending' WHERE uuid=:uuid"));q.bindValue(":privacy",privacy);q.bindValue(":uuid",uuid);if(!(q.exec()&&q.numRowsAffected()==1)){db.rollback();return false;}if(privacy==QStringLiteral("secret")||privacy==QStringLiteral("local_only")){QSqlQuery block(db);block.prepare(QStringLiteral("UPDATE sync_outbox SET status='blocked',last_error_code='privacy_forbidden' WHERE entity_type='memory' AND entity_uuid=:uuid AND status IN ('pending','retry')"));block.bindValue(":uuid",uuid);if(!block.exec()){db.rollback();return false;}QSqlQuery erase(db);erase.prepare(QStringLiteral("INSERT OR IGNORE INTO sync_outbox(idempotency_key,user_id,entity_type,entity_uuid,operation,revision,privacy_level,payload,created_at) SELECT uuid||':'||sync_revision||':privacy-delete',user_id,'memory',uuid,'delete',sync_revision,'normal','{}',datetime('now') FROM memories WHERE uuid=:uuid"));erase.bindValue(":uuid",uuid);if(!erase.exec()){db.rollback();return false;}}return db.commit();}
QList<SyncOutboxRecord> StorageService::loadPendingOutbox(int limit)const{QList<SyncOutboxRecord>out;if(!syncMasterEnabled())return out;QSqlQuery q(QSqlDatabase::database(m_connectionName));q.prepare(QStringLiteral("SELECT o.id,o.idempotency_key,o.user_id,o.entity_type,o.entity_uuid,o.operation,o.revision,o.privacy_level,o.payload,o.created_at,o.retry_count,o.next_attempt_at FROM sync_outbox o JOIN sync_preferences p ON p.entity_type=o.entity_type AND p.enabled=1 WHERE o.status IN ('pending','retry') AND o.privacy_level NOT IN ('secret','local_only') AND (o.next_attempt_at IS NULL OR datetime(o.next_attempt_at)<=datetime(:now)) ORDER BY o.id LIMIT :limit"));q.bindValue(":now",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));q.bindValue(":limit",limit);if(q.exec())while(q.next()){SyncOutboxRecord r;r.id=q.value(0).toLongLong();r.idempotencyKey=q.value(1).toString();r.userId=q.value(2).toString();r.entityType=q.value(3).toString();r.entityUuid=q.value(4).toString();r.operation=q.value(5).toString();r.revision=q.value(6).toInt();r.privacyLevel=q.value(7).toString();r.payload=q.value(8).toString();r.createdAt=QDateTime::fromString(q.value(9).toString(),Qt::ISODateWithMs);r.retryCount=q.value(10).toInt();r.nextAttemptAt=QDateTime::fromString(q.value(11).toString(),Qt::ISODateWithMs);out<<r;}return out;}
bool StorageService::markOutboxDelivered(qint64 id){QSqlDatabase db=QSqlDatabase::database(m_connectionName);QSqlQuery q(db);q.prepare(QStringLiteral("UPDATE sync_outbox SET status='delivered',last_error_code=NULL WHERE id=:id"));q.bindValue(":id",id);if(!(q.exec()&&q.numRowsAffected()==1))return false;QSqlQuery runtime(db);runtime.prepare(QStringLiteral("UPDATE sync_runtime SET last_success_at=:now,last_error_code=NULL,updated_at=:now WHERE id=1"));runtime.bindValue(":now",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));return runtime.exec();}
bool StorageService::markOutboxRetry(qint64 id,const QString&error){QSqlDatabase db=QSqlDatabase::database(m_connectionName);QSqlQuery q(db);q.prepare(QStringLiteral("UPDATE sync_outbox SET status='retry',retry_count=retry_count+1,last_error_code=:error,next_attempt_at=datetime('now','+'||MIN(86400,60*(1 << MIN(retry_count,10)))||' seconds') WHERE id=:id"));q.bindValue(":error",error.left(64));q.bindValue(":id",id);if(!(q.exec()&&q.numRowsAffected()==1))return false;QSqlQuery runtime(db);runtime.prepare(QStringLiteral("UPDATE sync_runtime SET last_error_code=:error,updated_at=:now WHERE id=1"));runtime.bindValue(":error",error.left(64));runtime.bindValue(":now",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));return runtime.exec();}

bool StorageService::createSchema()
{
    QSqlDatabase database = QSqlDatabase::database(m_connectionName);
    if (!database.transaction()) {
        m_lastError = database.lastError().text();
        return false;
    }

    const QStringList statements{
        QStringLiteral("CREATE TABLE IF NOT EXISTS schema_info(version INTEGER NOT NULL)"),
        QStringLiteral("INSERT INTO schema_info(version) SELECT 0 WHERE NOT EXISTS(SELECT 1 FROM schema_info)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS settings(key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS conversations(id INTEGER PRIMARY KEY, title TEXT, created_at TEXT NOT NULL)"),
        QStringLiteral("INSERT OR IGNORE INTO conversations(id, title, created_at) VALUES(1, '默认会话', datetime('now'))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS messages("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, conversation_id INTEGER NOT NULL DEFAULT 1, "
                       "sender TEXT NOT NULL CHECK(sender IN ('user','pet','system')), text TEXT NOT NULL, "
                       "created_at TEXT NOT NULL, FOREIGN KEY(conversation_id) REFERENCES conversations(id))"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_messages_conversation ON messages(conversation_id, id)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS memories("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, category TEXT NOT NULL, subject TEXT NOT NULL DEFAULT '', content TEXT NOT NULL, "
                       "importance INTEGER NOT NULL DEFAULT 50, confidence REAL NOT NULL DEFAULT 0.8, next_question TEXT, created_at TEXT NOT NULL, "
                       "updated_at TEXT, last_used_at TEXT, use_count INTEGER NOT NULL DEFAULT 0, uuid TEXT, deleted_at TEXT, "
                       "sync_status TEXT NOT NULL DEFAULT 'pending', sync_revision INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS story_threads("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, topic TEXT NOT NULL, summary TEXT NOT NULL, "
                       "next_question TEXT, status TEXT NOT NULL DEFAULT 'open', updated_at TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS forgotten_topics("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, topic TEXT NOT NULL UNIQUE, created_at TEXT NOT NULL, "
                       "sync_status TEXT NOT NULL DEFAULT 'pending', sync_revision INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS memory_entities("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, uuid TEXT NOT NULL UNIQUE, entity_type TEXT NOT NULL, name TEXT NOT NULL UNIQUE, "
                       "created_at TEXT NOT NULL, updated_at TEXT NOT NULL, deleted_at TEXT, sync_status TEXT NOT NULL DEFAULT 'pending', sync_revision INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS memory_relations("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, memory_id INTEGER NOT NULL, entity_id INTEGER NOT NULL, relation_type TEXT NOT NULL, created_at TEXT NOT NULL, "
                       "UNIQUE(memory_id,entity_id,relation_type), FOREIGN KEY(memory_id) REFERENCES memories(id), FOREIGN KEY(entity_id) REFERENCES memory_entities(id))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS pet_state("
                       "id INTEGER PRIMARY KEY CHECK(id=1), mood INTEGER NOT NULL DEFAULT 60, "
                       "energy INTEGER NOT NULL DEFAULT 70, health INTEGER NOT NULL DEFAULT 80, "
                       "closeness INTEGER NOT NULL DEFAULT 20, boredom INTEGER NOT NULL DEFAULT 10, "
                       "neglect INTEGER NOT NULL DEFAULT 0, curiosity INTEGER NOT NULL DEFAULT 25, irritation INTEGER NOT NULL DEFAULT 0, last_interaction TEXT, last_illness TEXT, "
                       "health_phase TEXT NOT NULL DEFAULT 'healthy', condition TEXT, recovery_progress INTEGER NOT NULL DEFAULT 0, "
                       "illness_started_at TEXT, phase_changed_at TEXT, last_illness_check_date TEXT, fullness INTEGER NOT NULL DEFAULT 45, last_digestion_at TEXT, updated_at TEXT NOT NULL)"),
        QStringLiteral("INSERT OR IGNORE INTO pet_state(id, updated_at) VALUES(1, datetime('now'))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS commitments("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, description TEXT NOT NULL, due_at TEXT, "
                       "status TEXT NOT NULL DEFAULT 'active', created_at TEXT NOT NULL, updated_at TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS diary_entries("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, entry_date TEXT NOT NULL UNIQUE, "
                       "content TEXT NOT NULL, created_at TEXT NOT NULL, uuid TEXT, updated_at TEXT, "
                       "deleted_at TEXT, sync_status TEXT NOT NULL DEFAULT 'pending', sync_revision INTEGER NOT NULL DEFAULT 0, memory_state TEXT NOT NULL DEFAULT 'active', locked INTEGER NOT NULL DEFAULT 0, expires_at TEXT, archived_at TEXT, retention TEXT NOT NULL DEFAULT 'long_term', governance_reason TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS diary_stickers(id INTEGER PRIMARY KEY AUTOINCREMENT,entry_date TEXT NOT NULL,emoji TEXT NOT NULL,label TEXT,x_percent INTEGER NOT NULL,y_percent INTEGER NOT NULL,rotation INTEGER NOT NULL DEFAULT 0,created_at TEXT NOT NULL,UNIQUE(entry_date,emoji,label),FOREIGN KEY(entry_date) REFERENCES diary_entries(entry_date) ON DELETE CASCADE)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS reminders("
                       "id INTEGER PRIMARY KEY AUTOINCREMENT, reminder_type TEXT NOT NULL, scheduled_at TEXT NOT NULL, "
                       "status TEXT NOT NULL DEFAULT 'pending', payload TEXT, created_at TEXT, updated_at TEXT, delivered_at TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS cognitive_records(id INTEGER PRIMARY KEY AUTOINCREMENT,uuid TEXT NOT NULL UNIQUE,record_type TEXT NOT NULL,subject TEXT NOT NULL,source_text TEXT,scheduled_at TEXT,event_end_at TEXT,follow_up_at TEXT,expires_at TEXT,status TEXT NOT NULL DEFAULT 'planned',explicit_request INTEGER NOT NULL DEFAULT 0,delivery_priority INTEGER NOT NULL DEFAULT 50,memory_importance INTEGER NOT NULL DEFAULT 30,follow_up_count INTEGER NOT NULL DEFAULT 0,max_follow_ups INTEGER NOT NULL DEFAULT 0,follow_up_policy TEXT NOT NULL DEFAULT 'none',reminder_id INTEGER,created_at TEXT NOT NULL,updated_at TEXT NOT NULL,sync_status TEXT NOT NULL DEFAULT 'pending',sync_revision INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS snack_inventory(id INTEGER PRIMARY KEY AUTOINCREMENT,uuid TEXT NOT NULL UNIQUE,snack_type TEXT NOT NULL UNIQUE,snack_name TEXT NOT NULL,emoji TEXT NOT NULL,quantity INTEGER NOT NULL DEFAULT 0,nutrition INTEGER NOT NULL DEFAULT 2,created_at TEXT NOT NULL,updated_at TEXT NOT NULL,sync_status TEXT NOT NULL DEFAULT 'pending',sync_revision INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS snack_catalog(snack_type TEXT PRIMARY KEY,snack_name TEXT NOT NULL,emoji TEXT NOT NULL,eaten_count INTEGER NOT NULL DEFAULT 0,preference INTEGER NOT NULL DEFAULT 50,consecutive_count INTEGER NOT NULL DEFAULT 0,first_unlocked_at TEXT NOT NULL,last_eaten_at TEXT,nickname TEXT,updated_at TEXT NOT NULL,sync_status TEXT NOT NULL DEFAULT 'pending',sync_revision INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS snack_history(id INTEGER PRIMARY KEY AUTOINCREMENT,uuid TEXT NOT NULL UNIQUE,event_type TEXT NOT NULL,snack_type TEXT NOT NULL,snack_name TEXT NOT NULL,source_name TEXT,source_size INTEGER NOT NULL DEFAULT 0,safety_level TEXT NOT NULL DEFAULT 'normal',created_at TEXT NOT NULL,sync_status TEXT NOT NULL DEFAULT 'pending',sync_revision INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS ai_summaries(id INTEGER PRIMARY KEY AUTOINCREMENT,uuid TEXT NOT NULL UNIQUE,title TEXT NOT NULL,source_name TEXT,summary_mode TEXT NOT NULL,content TEXT NOT NULL,created_at TEXT NOT NULL,deleted_at TEXT,sync_status TEXT NOT NULL DEFAULT 'pending',sync_revision INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS dreams(id INTEGER PRIMARY KEY AUTOINCREMENT,uuid TEXT NOT NULL UNIQUE,dream_date TEXT NOT NULL UNIQUE,title TEXT NOT NULL,content TEXT NOT NULL,mood TEXT NOT NULL DEFAULT 'warm',dream_type TEXT NOT NULL DEFAULT 'random_fantasy',symbols TEXT,color TEXT NOT NULL DEFAULT '#E7C7D5',reality_hint TEXT,continuation_key TEXT,memory_ids TEXT,created_at TEXT NOT NULL,opened_at TEXT,favorite INTEGER NOT NULL DEFAULT 0,reality_echo TEXT,echo_created_at TEXT,disclosure_level INTEGER NOT NULL DEFAULT 0,disclosed_at TEXT,deleted_at TEXT,sync_status TEXT NOT NULL DEFAULT 'pending',sync_revision INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS morning_lollipops(id INTEGER PRIMARY KEY AUTOINCREMENT,uuid TEXT NOT NULL UNIQUE,gift_date TEXT NOT NULL UNIQUE,flavor_id TEXT NOT NULL,flavor_name TEXT NOT NULL,category TEXT NOT NULL,emoji TEXT NOT NULL,color TEXT NOT NULL,rarity TEXT NOT NULL DEFAULT 'common',planned_at TEXT NOT NULL,actual_at TEXT,status TEXT NOT NULL DEFAULT 'planned',greeting TEXT,greeting_fingerprint TEXT,generation_source TEXT,delay_reason TEXT,viewed INTEGER NOT NULL DEFAULT 0,favorite INTEGER NOT NULL DEFAULT 0,created_at TEXT NOT NULL,updated_at TEXT NOT NULL,sync_status TEXT NOT NULL DEFAULT 'pending',sync_revision INTEGER NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS lollipop_metadata(gift_date TEXT PRIMARY KEY,acquisition_type TEXT NOT NULL DEFAULT 'regular',theme_tags TEXT,weather_snapshot TEXT,story TEXT,memorial_key TEXT UNIQUE,shape TEXT NOT NULL DEFAULT 'round',pattern TEXT NOT NULL DEFAULT 'swirl',FOREIGN KEY(gift_date) REFERENCES morning_lollipops(gift_date) ON DELETE CASCADE)"),
    };

    QSqlQuery query(database);
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            m_lastError = query.lastError().text();
            database.rollback();
            return false;
        }
    }

    if (!columnExists(QStringLiteral("pet_state"), QStringLiteral("last_interaction"))
        && !query.exec(QStringLiteral("ALTER TABLE pet_state ADD COLUMN last_interaction TEXT"))) {
        m_lastError = query.lastError().text();
        database.rollback();
        return false;
    }
    if (!columnExists(QStringLiteral("pet_state"), QStringLiteral("last_illness"))
        && !query.exec(QStringLiteral("ALTER TABLE pet_state ADD COLUMN last_illness TEXT"))) {
        m_lastError = query.lastError().text();
        database.rollback();
        return false;
    }
    if (!columnExists(QStringLiteral("pet_state"), QStringLiteral("curiosity"))
        && !query.exec(QStringLiteral("ALTER TABLE pet_state ADD COLUMN curiosity INTEGER NOT NULL DEFAULT 25"))) {
        m_lastError = query.lastError().text(); database.rollback(); return false;
    }
    if (!columnExists(QStringLiteral("pet_state"), QStringLiteral("irritation"))
        && !query.exec(QStringLiteral("ALTER TABLE pet_state ADD COLUMN irritation INTEGER NOT NULL DEFAULT 0"))) {
        m_lastError = query.lastError().text(); database.rollback(); return false;
    }
    const QStringList healthColumns{
        QStringLiteral("health_phase TEXT NOT NULL DEFAULT 'healthy'"), QStringLiteral("condition TEXT"),
        QStringLiteral("recovery_progress INTEGER NOT NULL DEFAULT 0"), QStringLiteral("illness_started_at TEXT"),
        QStringLiteral("phase_changed_at TEXT"), QStringLiteral("last_illness_check_date TEXT")};
    for(const QString &definition:healthColumns){const QString name=definition.section(QLatin1Char(' '),0,0);
        if(!columnExists(QStringLiteral("pet_state"),name)&&!query.exec(QStringLiteral("ALTER TABLE pet_state ADD COLUMN ")+definition)){
            m_lastError=query.lastError().text();database.rollback();return false;}}
    const QStringList hungerColumns{QStringLiteral("fullness INTEGER NOT NULL DEFAULT 45"),QStringLiteral("last_digestion_at TEXT")};
    for(const QString &definition:hungerColumns){const QString name=definition.section(QLatin1Char(' '),0,0);if(!columnExists(QStringLiteral("pet_state"),name)&&!query.exec(QStringLiteral("ALTER TABLE pet_state ADD COLUMN ")+definition)){m_lastError=query.lastError().text();database.rollback();return false;}}
    const QStringList diaryMigrations{
        QStringLiteral("ALTER TABLE diary_entries ADD COLUMN uuid TEXT"),
        QStringLiteral("ALTER TABLE diary_entries ADD COLUMN updated_at TEXT"),
        QStringLiteral("ALTER TABLE diary_entries ADD COLUMN deleted_at TEXT"),
        QStringLiteral("ALTER TABLE diary_entries ADD COLUMN sync_status TEXT NOT NULL DEFAULT 'pending'"),
        QStringLiteral("ALTER TABLE diary_entries ADD COLUMN sync_revision INTEGER NOT NULL DEFAULT 0")};
    const QStringList diaryColumns{QStringLiteral("uuid"), QStringLiteral("updated_at"),
        QStringLiteral("deleted_at"), QStringLiteral("sync_status"), QStringLiteral("sync_revision")};
    for (int i = 0; i < diaryColumns.size(); ++i) {
        if (!columnExists(QStringLiteral("diary_entries"), diaryColumns.at(i))
            && !query.exec(diaryMigrations.at(i))) {
            m_lastError = query.lastError().text(); database.rollback(); return false;
        }
    }
    const QStringList memoryColumns{QStringLiteral("subject TEXT NOT NULL DEFAULT ''"),QStringLiteral("confidence REAL NOT NULL DEFAULT 0.8"),
        QStringLiteral("next_question TEXT"),QStringLiteral("updated_at TEXT"),QStringLiteral("use_count INTEGER NOT NULL DEFAULT 0"),
        QStringLiteral("uuid TEXT"),QStringLiteral("deleted_at TEXT"),QStringLiteral("sync_status TEXT NOT NULL DEFAULT 'pending'"),
        QStringLiteral("sync_revision INTEGER NOT NULL DEFAULT 0"),QStringLiteral("memory_state TEXT NOT NULL DEFAULT 'active'"),QStringLiteral("locked INTEGER NOT NULL DEFAULT 0"),QStringLiteral("expires_at TEXT"),QStringLiteral("archived_at TEXT"),QStringLiteral("retention TEXT NOT NULL DEFAULT 'long_term'"),QStringLiteral("governance_reason TEXT")};
    for (const QString &definition : memoryColumns) {
        const QString name=definition.section(QLatin1Char(' '),0,0);
        if (!columnExists(QStringLiteral("memories"),name) && !query.exec(QStringLiteral("ALTER TABLE memories ADD COLUMN ")+definition)) {
            m_lastError=query.lastError().text(); database.rollback(); return false;
        }
    }
    const QStringList reminderColumns{QStringLiteral("created_at TEXT"),QStringLiteral("updated_at TEXT"),QStringLiteral("delivered_at TEXT")};
    for(const QString &definition:reminderColumns){const QString name=definition.section(QLatin1Char(' '),0,0);
        if(!columnExists(QStringLiteral("reminders"),name)&&!query.exec(QStringLiteral("ALTER TABLE reminders ADD COLUMN ")+definition)){
            m_lastError=query.lastError().text();database.rollback();return false;}}
    if(!columnExists(QStringLiteral("commitments"),QStringLiteral("updated_at"))&&!query.exec(QStringLiteral("ALTER TABLE commitments ADD COLUMN updated_at TEXT"))){m_lastError=query.lastError().text();database.rollback();return false;}
    const QStringList dreamColumns{QStringLiteral("disclosure_level INTEGER NOT NULL DEFAULT 0"),QStringLiteral("disclosed_at TEXT")};
    for(const QString &definition:dreamColumns){const QString name=definition.section(QLatin1Char(' '),0,0);if(!columnExists(QStringLiteral("dreams"),name)&&!query.exec(QStringLiteral("ALTER TABLE dreams ADD COLUMN ")+definition)){m_lastError=query.lastError().text();database.rollback();return false;}}
    if (!query.exec(QStringLiteral("UPDATE diary_entries SET uuid=lower(hex(randomblob(16))) WHERE uuid IS NULL"))
        || !query.exec(QStringLiteral("UPDATE diary_entries SET updated_at=created_at WHERE updated_at IS NULL"))
        || !query.exec(QStringLiteral("UPDATE diary_entries SET content=replace(replace(replace(replace(replace(content,'我的小主人','你'),'小主人','你'),'我的主人','你'),'主人','你'),'主仆','伙伴') WHERE content LIKE '%主人%' OR content LIKE '%主仆%'"))
        || !query.exec(QStringLiteral("UPDATE diary_entries SET content=trim(replace(replace(replace(replace(content,'【反向日记模块】',''),'【反向日记模块指令】',''),'[反向日记模块]',''),'[反向日记模块指令]','')) WHERE content LIKE '%反向日记模块%'"))
        || !query.exec(QStringLiteral("UPDATE diary_entries SET content=ltrim(substr(content,11)),updated_at=datetime('now'),sync_status='pending',sync_revision=sync_revision+1 WHERE content GLOB '????-??-??*'"))
        || !query.exec(QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_diary_uuid ON diary_entries(uuid)"))
        || !query.exec(QStringLiteral("UPDATE memories SET uuid=lower(hex(randomblob(16))) WHERE uuid IS NULL"))
        || !query.exec(QStringLiteral("UPDATE memories SET updated_at=created_at WHERE updated_at IS NULL"))
        || !query.exec(QStringLiteral("UPDATE memories SET confidence=confidence/100.0 WHERE confidence>1"))
        || !query.exec(QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_memory_identity ON memories(category,subject)"))
        || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_reminders_due ON reminders(status,scheduled_at)"))
        || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_snack_history_time ON snack_history(created_at DESC)"))
        || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_memory_lifecycle ON memories(memory_state,locked,expires_at)"))
        || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_summary_time ON ai_summaries(created_at DESC)"))
        || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_dreams_date ON dreams(dream_date DESC)"))
        || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_diary_stickers_date ON diary_stickers(entry_date)"))
        || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_cognitive_due ON cognitive_records(status,scheduled_at,follow_up_at)"))
        || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_lollipop_date ON morning_lollipops(gift_date DESC)"))
        || !query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_lollipop_memorial ON lollipop_metadata(memorial_key)"))
        || !query.exec(QStringLiteral("UPDATE schema_info SET version=18 WHERE version<18"))) {
        m_lastError = query.lastError().text();
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        m_lastError = database.lastError().text();
        return false;
    }
    return true;
}

bool StorageService::columnExists(const QString &table, const QString &column) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) {
        return false;
    }
    while (query.next()) {
        if (query.value(1).toString() == column) {
            return true;
        }
    }
    return false;
}
