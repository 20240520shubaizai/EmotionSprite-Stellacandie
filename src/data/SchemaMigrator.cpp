#include "SchemaMigrator.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
bool columnExists(QSqlDatabase db, const QString &table, const QString &column)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA table_info(%1)").arg(table))) return false;
    while (query.next()) if (query.value(1).toString() == column) return true;
    return false;
}

bool exec(QSqlDatabase db, const QString &sql, QString *error)
{
    QSqlQuery query(db);
    if (query.exec(sql)) return true;
    if (error) *error = query.lastError().text() + QStringLiteral(" | SQL: ") + sql;
    return false;
}

bool addColumn(QSqlDatabase db, const QString &table, const QString &definition, QString *error)
{
    const QString name = definition.section(QLatin1Char(' '), 0, 0);
    return columnExists(db, table, name)
        || exec(db, QStringLiteral("ALTER TABLE %1 ADD COLUMN %2").arg(table, definition), error);
}
}

int SchemaMigrator::currentVersion(QSqlDatabase db)
{
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT version FROM schema_info LIMIT 1")) || !query.next()) return 0;
    return query.value(0).toInt();
}

bool SchemaMigrator::migrate(QSqlDatabase db, QString *error)
{
    const int from = currentVersion(db);
    if (from >= LatestVersion) return true;
    if (!db.transaction()) { if (error) *error = db.lastError().text(); return false; }

    if (!exec(db, QStringLiteral("CREATE TABLE IF NOT EXISTS schema_migrations("
                                 "version INTEGER PRIMARY KEY,name TEXT NOT NULL,applied_at TEXT NOT NULL)"), error)) {
        db.rollback(); return false;
    }

    if (from < 19) {
        const QStringList tables{
            QStringLiteral("messages"), QStringLiteral("memories"), QStringLiteral("diary_entries"),
            QStringLiteral("diary_stickers"), QStringLiteral("reminders"), QStringLiteral("commitments"),
            QStringLiteral("cognitive_records"), QStringLiteral("pet_state"), QStringLiteral("snack_inventory"),
            QStringLiteral("snack_catalog"), QStringLiteral("snack_history"), QStringLiteral("ai_summaries"),
            QStringLiteral("dreams"), QStringLiteral("morning_lollipops")};
        for (const QString &table : tables) {
            const QString privacy = (table == QStringLiteral("messages") || table == QStringLiteral("memories"))
                ? QStringLiteral("private") : QStringLiteral("normal");
            const QStringList columns{
                QStringLiteral("uuid TEXT"),
                QStringLiteral("user_id TEXT NOT NULL DEFAULT 'local-user'"),
                QStringLiteral("sync_revision INTEGER NOT NULL DEFAULT 0"),
                QStringLiteral("sync_status TEXT NOT NULL DEFAULT 'pending'"),
                QStringLiteral("privacy_level TEXT NOT NULL DEFAULT '%1'").arg(privacy),
                QStringLiteral("deleted_at TEXT")};
            for (const QString &definition : columns) {
                if (!addColumn(db, table, definition, error)) { db.rollback(); return false; }
            }
            if (!exec(db, QStringLiteral("UPDATE %1 SET uuid=lower(hex(randomblob(16))) WHERE uuid IS NULL OR uuid='' ").arg(table), error)
                || !exec(db, QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS idx_%1_uuid ON %1(uuid)").arg(table), error)) {
                db.rollback(); return false;
            }
        }
        if (!exec(db, QStringLiteral("INSERT OR REPLACE INTO schema_migrations(version,name,applied_at) VALUES(19,'unified_sync_privacy_fields','%1')")
                      .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)), error)
            || !exec(db, QStringLiteral("UPDATE schema_info SET version=19"), error)) {
            db.rollback(); return false;
        }
    }
    if(from<20){
        const QStringList sql{
            QStringLiteral("CREATE TABLE IF NOT EXISTS sync_preferences(entity_type TEXT PRIMARY KEY,enabled INTEGER NOT NULL DEFAULT 0,updated_at TEXT NOT NULL)"),
            QStringLiteral("INSERT OR IGNORE INTO sync_preferences(entity_type,enabled,updated_at) VALUES('settings',0,datetime('now')),('pet_state',0,datetime('now')),('memory',0,datetime('now')),('reminder',0,datetime('now'))"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS sync_outbox(id INTEGER PRIMARY KEY AUTOINCREMENT,idempotency_key TEXT NOT NULL UNIQUE,user_id TEXT NOT NULL,entity_type TEXT NOT NULL,entity_uuid TEXT NOT NULL,operation TEXT NOT NULL,revision INTEGER NOT NULL,privacy_level TEXT NOT NULL,payload TEXT NOT NULL,created_at TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'pending',retry_count INTEGER NOT NULL DEFAULT 0,next_attempt_at TEXT,last_error_code TEXT)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_sync_outbox_pending ON sync_outbox(status,next_attempt_at,id)"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS sync_conflicts(id INTEGER PRIMARY KEY AUTOINCREMENT,entity_type TEXT NOT NULL,entity_uuid TEXT NOT NULL,local_payload TEXT NOT NULL,cloud_payload TEXT NOT NULL,status TEXT NOT NULL DEFAULT 'open',created_at TEXT NOT NULL,resolved_at TEXT)"),
            QStringLiteral("CREATE TRIGGER IF NOT EXISTS trg_memory_sync_insert AFTER INSERT ON memories WHEN NEW.privacy_level='normal' BEGIN INSERT OR IGNORE INTO sync_outbox(idempotency_key,user_id,entity_type,entity_uuid,operation,revision,privacy_level,payload,created_at) VALUES(NEW.uuid||':'||NEW.sync_revision,NEW.user_id,'memory',NEW.uuid,'upsert',NEW.sync_revision,NEW.privacy_level,json_object('category',NEW.category,'subject',NEW.subject,'content',NEW.content,'importance',NEW.importance),datetime('now')); END"),
            QStringLiteral("CREATE TRIGGER IF NOT EXISTS trg_memory_sync_update AFTER UPDATE ON memories WHEN NEW.privacy_level='normal' BEGIN INSERT OR IGNORE INTO sync_outbox(idempotency_key,user_id,entity_type,entity_uuid,operation,revision,privacy_level,payload,created_at) VALUES(NEW.uuid||':'||NEW.sync_revision,NEW.user_id,'memory',NEW.uuid,CASE WHEN NEW.deleted_at IS NOT NULL THEN 'delete' WHEN OLD.deleted_at IS NOT NULL THEN 'restore' ELSE 'upsert' END,NEW.sync_revision,NEW.privacy_level,json_object('category',NEW.category,'subject',NEW.subject,'content',NEW.content,'importance',NEW.importance),datetime('now')); END"),
            QStringLiteral("CREATE TRIGGER IF NOT EXISTS trg_reminder_sync_insert AFTER INSERT ON reminders BEGIN INSERT OR IGNORE INTO sync_outbox(idempotency_key,user_id,entity_type,entity_uuid,operation,revision,privacy_level,payload,created_at) VALUES(NEW.uuid||':'||NEW.sync_revision,NEW.user_id,'reminder',NEW.uuid,'upsert',NEW.sync_revision,NEW.privacy_level,json_object('type',NEW.reminder_type,'scheduled_at',NEW.scheduled_at,'status',NEW.status,'payload',NEW.payload),datetime('now')); END"),
            QStringLiteral("CREATE TRIGGER IF NOT EXISTS trg_reminder_sync_update AFTER UPDATE ON reminders BEGIN INSERT OR IGNORE INTO sync_outbox(idempotency_key,user_id,entity_type,entity_uuid,operation,revision,privacy_level,payload,created_at) VALUES(NEW.uuid||':'||NEW.sync_revision,NEW.user_id,'reminder',NEW.uuid,CASE WHEN NEW.deleted_at IS NULL THEN 'upsert' ELSE 'delete' END,NEW.sync_revision,NEW.privacy_level,json_object('type',NEW.reminder_type,'scheduled_at',NEW.scheduled_at,'status',NEW.status,'payload',NEW.payload),datetime('now')); END"),
            QStringLiteral("CREATE TRIGGER IF NOT EXISTS trg_pet_state_sync AFTER UPDATE ON pet_state BEGIN INSERT OR IGNORE INTO sync_outbox(idempotency_key,user_id,entity_type,entity_uuid,operation,revision,privacy_level,payload,created_at) VALUES(NEW.uuid||':'||NEW.sync_revision,NEW.user_id,'pet_state',NEW.uuid,'upsert',NEW.sync_revision,NEW.privacy_level,json_object('mood',NEW.mood,'energy',NEW.energy,'health',NEW.health,'closeness',NEW.closeness),datetime('now')); END")};
        for(const QString&s:sql)if(!exec(db,s,error)){db.rollback();return false;}
        if(!exec(db,QStringLiteral("INSERT OR REPLACE INTO schema_migrations(version,name,applied_at) VALUES(20,'local_outbox','%1')").arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)),error)||!exec(db,QStringLiteral("UPDATE schema_info SET version=20"),error)){db.rollback();return false;}
    }
    if(from<21){
        const QStringList sql{
            QStringLiteral("UPDATE messages SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE memories SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE diary_entries SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE diary_stickers SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE reminders SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE commitments SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE cognitive_records SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE pet_state SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE snack_inventory SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE snack_catalog SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE snack_history SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE ai_summaries SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE dreams SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("UPDATE morning_lollipops SET user_id='local-single-user' WHERE user_id='local-user'"),
            QStringLiteral("DROP TRIGGER IF EXISTS trg_memory_sync_update"),
            QStringLiteral("CREATE TRIGGER trg_memory_sync_update AFTER UPDATE ON memories WHEN NEW.privacy_level='normal' BEGIN INSERT OR IGNORE INTO sync_outbox(idempotency_key,user_id,entity_type,entity_uuid,operation,revision,privacy_level,payload,created_at) VALUES(NEW.uuid||':'||NEW.sync_revision,NEW.user_id,'memory',NEW.uuid,CASE WHEN NEW.deleted_at IS NOT NULL THEN 'delete' WHEN OLD.deleted_at IS NOT NULL THEN 'restore' ELSE 'upsert' END,NEW.sync_revision,NEW.privacy_level,json_object('category',NEW.category,'subject',NEW.subject,'content',NEW.content,'importance',NEW.importance),datetime('now')); END"),
            QStringLiteral("CREATE TABLE IF NOT EXISTS sync_settings(setting_key TEXT PRIMARY KEY,setting_value TEXT NOT NULL,uuid TEXT NOT NULL DEFAULT (lower(hex(randomblob(16)))),user_id TEXT NOT NULL DEFAULT 'local-single-user',sync_revision INTEGER NOT NULL DEFAULT 0,sync_status TEXT NOT NULL DEFAULT 'pending',privacy_level TEXT NOT NULL DEFAULT 'normal',updated_at TEXT NOT NULL)"),
            QStringLiteral("CREATE TRIGGER IF NOT EXISTS trg_settings_sync_insert AFTER INSERT ON sync_settings BEGIN INSERT OR IGNORE INTO sync_outbox(idempotency_key,user_id,entity_type,entity_uuid,operation,revision,privacy_level,payload,created_at) VALUES(NEW.uuid||':'||NEW.sync_revision,NEW.user_id,'settings',NEW.uuid,'upsert',NEW.sync_revision,NEW.privacy_level,json_object('key',NEW.setting_key,'value',NEW.setting_value),datetime('now')); END"),
            QStringLiteral("CREATE TRIGGER IF NOT EXISTS trg_settings_sync_update AFTER UPDATE ON sync_settings BEGIN INSERT OR IGNORE INTO sync_outbox(idempotency_key,user_id,entity_type,entity_uuid,operation,revision,privacy_level,payload,created_at) VALUES(NEW.uuid||':'||NEW.sync_revision,NEW.user_id,'settings',NEW.uuid,'upsert',NEW.sync_revision,NEW.privacy_level,json_object('key',NEW.setting_key,'value',NEW.setting_value),datetime('now')); END")};
        for(const QString&s:sql)if(!exec(db,s,error)){db.rollback();return false;}
        if(!exec(db,QStringLiteral("INSERT OR REPLACE INTO schema_migrations(version,name,applied_at) VALUES(21,'stable_user_and_sync_settings','%1')").arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)),error)||!exec(db,QStringLiteral("UPDATE schema_info SET version=21"),error)){db.rollback();return false;}
    }
    if(from<22){
        const QStringList sql{
            QStringLiteral("CREATE TABLE IF NOT EXISTS agent_mutation_commits(request_id TEXT PRIMARY KEY,trace_id TEXT NOT NULL,proposal_hash TEXT NOT NULL,result_json TEXT NOT NULL,created_at TEXT NOT NULL)"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_agent_mutation_trace ON agent_mutation_commits(trace_id)")};
        for(const QString&s:sql)if(!exec(db,s,error)){db.rollback();return false;}
        if(!exec(db,QStringLiteral("INSERT OR REPLACE INTO schema_migrations(version,name,applied_at) VALUES(22,'agent_mutation_commit_v1','%1')").arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)),error)
            ||!exec(db,QStringLiteral("UPDATE schema_info SET version=22"),error)){db.rollback();return false;}
    }
    if(from<23){
        const QString deviceId=QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QStringList sql{
            QStringLiteral("CREATE TABLE IF NOT EXISTS sync_runtime(id INTEGER PRIMARY KEY CHECK(id=1),master_enabled INTEGER NOT NULL DEFAULT 0,device_id TEXT NOT NULL,last_success_at TEXT,last_error_code TEXT,updated_at TEXT NOT NULL)"),
            QStringLiteral("INSERT OR IGNORE INTO sync_runtime(id,master_enabled,device_id,updated_at) VALUES(1,0,'%1',datetime('now'))").arg(deviceId),
            QStringLiteral("UPDATE sync_outbox SET status='blocked' WHERE privacy_level IN ('secret','local_only') AND status IN ('pending','retry')")};
        for(const QString&s:sql)if(!exec(db,s,error)){db.rollback();return false;}
        if(!exec(db,QStringLiteral("INSERT OR REPLACE INTO schema_migrations(version,name,applied_at) VALUES(23,'sync_product_controls','%1')").arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)),error)
            ||!exec(db,QStringLiteral("UPDATE schema_info SET version=23"),error)){db.rollback();return false;}
    }
    if(from<24){
        const QStringList sql{
            QStringLiteral("UPDATE memories SET memory_state='deleted' WHERE deleted_at IS NOT NULL AND memory_state<>'deleted'"),
            QStringLiteral("CREATE INDEX IF NOT EXISTS idx_memories_active_retrieval ON memories(memory_state,deleted_at,importance DESC,updated_at DESC)")};
        for(const QString&s:sql)if(!exec(db,s,error)){db.rollback();return false;}
        if(!exec(db,QStringLiteral("INSERT OR REPLACE INTO schema_migrations(version,name,applied_at) VALUES(24,'memory_deletion_consistency','%1')").arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)),error)
            ||!exec(db,QStringLiteral("UPDATE schema_info SET version=24"),error)){db.rollback();return false;}
    }
    if (!db.commit()) { if (error) *error = db.lastError().text(); db.rollback(); return false; }
    return true;
}
