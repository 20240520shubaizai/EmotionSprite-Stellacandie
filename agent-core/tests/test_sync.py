import base64
from pathlib import Path
from agent_core.sync_models import ConflictCopy,SessionLocal,SyncEntity
from agent_core.app import cloud_sync_enabled

HEADERS={"X-Session-Token":"test-session-token","X-Client-Version":"1.0.0"}
def event(key,kind="memory",uuid="m1",revision=1,payload=None,privacy="normal",operation="upsert"):
    return {"idempotency_key":key,"user_id":"local-single-user","entity_type":kind,"entity_uuid":uuid,"revision":revision,"operation":operation,"privacy_level":privacy,"payload":payload or {"content":"value"}}
def send(client,*events):return client.post("/v1/sync/batch",headers=HEADERS,json={"events":list(events)}).json()["results"]

def test_idempotency_privacy_conflict_and_delete_restore(client):
    first=event("event-key-0001");assert send(client,first)[0]["status"]=="applied";assert send(client,first)[0]["status"]=="duplicate"
    assert send(client,event("event-key-secret",privacy="secret"))[0]=={"status":"rejected","reason":"privacy_forbidden"}
    conflict=event("event-key-0002",payload={"content":"different"});assert send(client,conflict)[0]["status"]=="conflict_copy_created"
    deleted=event("event-key-0003",revision=2,operation="delete");assert send(client,deleted)[0]["status"]=="applied"
    restored=event("event-key-0004",revision=3,operation="restore",payload={"content":"restored"});assert send(client,restored)[0]["status"]=="applied"
    with SessionLocal() as db:
        entity=db.query(SyncEntity).filter_by(entity_uuid="m1").one();assert entity.deleted_at is None and entity.payload["content"]=="restored";assert db.query(ConflictCopy).count()==1
        conflict_id=db.query(ConflictCopy).first().id
    listed=client.get("/v1/sync/conflicts",headers=HEADERS).json()["conflicts"]
    assert listed and listed[0]["id"]==conflict_id
    resolved=client.post(f"/v1/sync/conflicts/{conflict_id}/resolve",headers=HEADERS,json={"choice":"local"})
    assert resolved.status_code==200 and resolved.json()["status"]=="resolved_local"

def test_terminal_reminder_never_revives(client):
    assert send(client,event("reminder-key-1","reminder","r1",1,{"status":"completed"}))[0]["status"]=="applied"
    result=send(client,event("reminder-key-2","reminder","r1",2,{"status":"pending"}))[0]
    assert result=={"status":"ignored","reason":"terminal_reminder_not_revived"}

def test_secret_content_never_reaches_response_log_or_database(client,caplog):
    marker="SECRET_MARKER_MUST_NOT_ESCAPE"
    response=client.post("/v1/sync/batch",headers=HEADERS,json={"events":[event(
        "event-key-secret-marker",uuid="secret-marker",privacy="secret",payload={"content":marker})]})
    assert response.status_code==200 and marker not in response.text
    assert marker not in caplog.text
    with SessionLocal() as db:
        assert db.query(SyncEntity).filter_by(entity_uuid="secret-marker").count()==0

def test_export_and_confirmed_cloud_delete_preserve_local_contract(client):
    assert send(client,event("export-delete-event",uuid="export-delete-memory"))[0]["status"]=="applied"
    exported=client.get("/v1/sync/export?user_id=local-single-user",headers=HEADERS)
    assert exported.status_code==200
    assert any(row["entity_uuid"]=="export-delete-memory" for row in exported.json()["entities"])
    denied=client.post("/v1/sync/delete-cloud-data",headers=HEADERS,json={"user_id":"local-single-user","confirmation":"no"})
    assert denied.status_code==400
    deleted=client.post("/v1/sync/delete-cloud-data",headers=HEADERS,json={"user_id":"local-single-user","confirmation":"DELETE CLOUD DATA"})
    assert deleted.status_code==200 and deleted.json()["local_data_untouched"] is True
    with SessionLocal() as db:assert db.query(SyncEntity).filter_by(user_id="local-single-user").count()==0

def test_local_development_never_advertises_cloud_sync(monkeypatch):
    monkeypatch.setenv("SYNC_DEPLOYMENT_MODE","local_development")
    assert cloud_sync_enabled() is False
    monkeypatch.setenv("SYNC_DEPLOYMENT_MODE","test")

def test_transient_image_never_writes_file(client,tmp_path):
    before={p for p in tmp_path.rglob("*")};raw=b"not-a-real-photo-but-transient"
    result=client.post("/v1/vision/transient",headers=HEADERS,json={"image_base64":base64.b64encode(raw).decode(),"mime_type":"image/png"}).json()
    assert result["persisted"] is False and result["size"]==len(raw);assert {p for p in tmp_path.rglob("*")}==before
