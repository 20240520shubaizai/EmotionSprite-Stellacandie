from datetime import datetime,timezone
import hashlib,json
from sqlalchemy import select
from .sync_models import AppliedEvent,ConflictCopy,SyncEntity
ALLOWED={"settings","pet_state","memory","reminder"};DENIED={"secret","local_only"};TERMINAL={"delivered","completed","cancelled","expired"}
def content_hash(payload):return hashlib.sha256(json.dumps(payload,sort_keys=True,separators=(",",":"),ensure_ascii=False).encode()).hexdigest()
def apply_event(db,event):
    key=event["idempotency_key"]
    if db.get(AppliedEvent,key):return {"status":"duplicate","idempotency_key":key}
    if event["entity_type"] not in ALLOWED:return {"status":"rejected","reason":"entity_type_not_authorized"}
    if event.get("privacy_level","normal") in DENIED:return {"status":"rejected","reason":"privacy_forbidden"}
    identity={"user_id":event["user_id"],"entity_type":event["entity_type"],"entity_uuid":event["entity_uuid"]};current=db.scalar(select(SyncEntity).filter_by(**identity));rev=int(event["revision"]);payload=event.get("payload",{})
    if current and event["entity_type"]=="reminder" and current.payload.get("status") in TERMINAL and payload.get("status")=="pending":db.add(AppliedEvent(idempotency_key=key,user_id=event["user_id"]));db.commit();return {"status":"ignored","reason":"terminal_reminder_not_revived"}
    if current and event["entity_type"]=="memory" and rev==current.revision and content_hash(payload)!=current.content_hash:db.add(ConflictCopy(**identity,local_payload=payload,cloud_payload=current.payload));db.add(AppliedEvent(idempotency_key=key,user_id=event["user_id"]));db.commit();return {"status":"conflict_copy_created"}
    if current and rev<current.revision:db.add(AppliedEvent(idempotency_key=key,user_id=event["user_id"]));db.commit();return {"status":"stale_ignored"}
    if not current:current=SyncEntity(**identity);db.add(current)
    current.revision=rev;current.payload=payload;current.content_hash=content_hash(payload);current.privacy_level=event.get("privacy_level","normal");current.deleted_at=datetime.now(timezone.utc) if event.get("operation")=="delete" else None;current.updated_at=datetime.now(timezone.utc);db.add(AppliedEvent(idempotency_key=key,user_id=event["user_id"]));db.commit();return {"status":"applied","content_hash":current.content_hash,"revision":current.revision}

def resolve_conflict(db,conflict_id,choice):
    conflict=db.get(ConflictCopy,conflict_id)
    if not conflict or conflict.status!="open":return None
    current=db.scalar(select(SyncEntity).filter_by(user_id=conflict.user_id,entity_type=conflict.entity_type,entity_uuid=conflict.entity_uuid))
    if not current:return None
    payload=conflict.local_payload if choice=="local" else conflict.cloud_payload
    current.payload=payload;current.content_hash=content_hash(payload);current.revision+=1;current.updated_at=datetime.now(timezone.utc)
    conflict.status=f"resolved_{choice}";db.commit()
    return {"status":conflict.status,"revision":current.revision,"content_hash":current.content_hash}
