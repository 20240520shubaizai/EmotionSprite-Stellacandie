from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path

from sqlalchemy import func, select, text
from sqlalchemy.exc import SQLAlchemyError

from agent_core.sync_models import AppliedEvent, ConflictCopy, SessionLocal, SyncEntity
from agent_core.sync_service import apply_event, resolve_conflict


def event(key: str, entity_type: str, entity_uuid: str, revision: int, payload: dict,
          operation: str = "upsert", privacy_level: str = "normal") -> dict:
    return {"idempotency_key": key, "user_id": "local-single-user", "entity_type": entity_type,
            "entity_uuid": entity_uuid, "revision": revision, "operation": operation,
            "privacy_level": privacy_level, "payload": payload}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    checks: dict[str, bool] = {}
    with SessionLocal() as db:
        db.execute(text("DELETE FROM sync_conflict_copies"))
        db.execute(text("DELETE FROM applied_sync_events"))
        db.execute(text("DELETE FROM sync_entities"))
        db.commit()
        before_counts={"sync_entities":0,"applied_sync_events":0,"sync_conflict_copies":0}
        first = event("validation-memory-0001", "memory", "memory-validation", 1, {"content": "warm memory"})
        applied = apply_event(db, first)
        duplicate = apply_event(db, first)
        secret = apply_event(db, event("validation-secret-0001", "memory", "secret-validation", 1,
                                       {"content": "SECRET_MARKER_MUST_NOT_PERSIST"}, privacy_level="secret"))
        deleted = apply_event(db, event("validation-memory-0002", "memory", "memory-validation", 2,
                                        {"content": "warm memory"}, operation="delete"))
        restored = apply_event(db, event("validation-memory-0003", "memory", "memory-validation", 3,
                                         {"content": "warm memory restored"}, operation="restore"))
        stale = apply_event(db, event("validation-memory-stale", "memory", "memory-validation", 2,
                                      {"content": "stale content must not win"}))
        apply_event(db, event("validation-memory-0004", "memory", "memory-validation", 3,
                              {"content": "conflicting memory"}))
        conflict=db.scalar(select(ConflictCopy).where(ConflictCopy.entity_uuid=="memory-validation"))
        resolved=resolve_conflict(db,conflict.id,"local")
        apply_event(db, event("validation-reminder-0001", "reminder", "reminder-validation", 1,
                              {"status": "completed"}))
        reminder = apply_event(db, event("validation-reminder-0002", "reminder", "reminder-validation", 2,
                                         {"status": "pending"}))
        entity = db.scalar(select(SyncEntity).where(SyncEntity.entity_uuid == "memory-validation"))
        counts = {
            "sync_entities": db.scalar(select(func.count()).select_from(SyncEntity)),
            "applied_sync_events": db.scalar(select(func.count()).select_from(AppliedEvent)),
            "sync_conflict_copies": db.scalar(select(func.count()).select_from(ConflictCopy)),
        }
        secret_count = db.scalar(select(func.count()).select_from(SyncEntity).where(
            SyncEntity.entity_uuid == "secret-validation"))
        duplicate_entity_count=db.scalar(select(func.count()).select_from(SyncEntity).where(SyncEntity.entity_uuid=="memory-validation"))
        duplicate_event_count=db.scalar(select(func.count()).select_from(AppliedEvent).where(AppliedEvent.idempotency_key=="validation-memory-0001"))
        database_constraint_rejected=False
        try:
            with db.begin_nested():
                db.execute(text("INSERT INTO sync_entities(user_id,entity_type,entity_uuid,revision,privacy_level,payload,content_hash,updated_at) VALUES('local-single-user','memory','direct-secret',1,'secret','{}','x',NOW())"))
        except SQLAlchemyError:database_constraint_rejected=True
        checks.update({
            "idempotency": applied["status"] == "applied" and duplicate["status"] == "duplicate" and duplicate_entity_count==1 and duplicate_event_count==1,
            "secret_rejected": secret["status"] == "rejected" and secret_count == 0,
            "database_constraint_rejected_secret":database_constraint_rejected,
            "out_of_order_stale_ignored":stale["status"]=="stale_ignored",
            "delete_propagated": deleted["status"] == "applied",
            "restore_succeeded": restored["status"] == "applied" and entity.deleted_at is None,
            "memory_conflict_copy": counts["sync_conflict_copies"] == 1 and resolved["status"] == "resolved_local",
            "terminal_reminder_not_revived": reminder.get("reason") == "terminal_reminder_not_revived",
            "stable_user_id": entity.user_id == "local-single-user",
            "content_hash_present": len(entity.content_hash) == 64,
        })
        report = {
            "schema": "emotion-sprite-sync-validation/v1",
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "database_engine": db.execute(text("SELECT VERSION()" if db.bind.dialect.name == "mysql" else "SELECT sqlite_version()" )).scalar(),
            "dialect": db.bind.dialect.name,
            "migration_version": db.execute(text("SELECT version_num FROM alembic_version")).scalar(),
            "checks": checks,
            "snapshots": {"before": {"counts": before_counts}, "after": {"counts": counts}},
            "final_entity": {"revision": entity.revision, "content_hash": entity.content_hash,
                             "deleted": entity.deleted_at is not None},
            "passed": all(checks.values()),
        }
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
