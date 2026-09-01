from sqlalchemy import create_engine

from agent_core.sync_models import SessionLocal


def test_mysql_unavailable_does_not_take_down_agent_health(client):
    original=SessionLocal.kw["bind"]
    unavailable=create_engine("mysql+pymysql://invalid:invalid@127.0.0.1:1/emotion_sprite?connect_timeout=1",pool_pre_ping=True)
    SessionLocal.configure(bind=unavailable)
    original_raise=client._transport.raise_server_exceptions
    client._transport.raise_server_exceptions=False
    try:
        headers={"X-Session-Token":"test-session-token","X-Client-Version":"1.0.0"}
        assert client.get("/health").status_code==200
        response=client.post("/v1/sync/batch",headers=headers,json={"events":[{
            "idempotency_key":"mysql-down-event","user_id":"local-single-user","entity_type":"memory",
            "entity_uuid":"mysql-down-memory","revision":1,"operation":"upsert","privacy_level":"normal",
            "payload":{"content":"local data remains authoritative"}}]})
        assert response.status_code==503
        assert response.json()=={"detail":"sync_backend_unavailable"}
        assert client.get("/health").status_code==200
    finally:
        client._transport.raise_server_exceptions=original_raise
        SessionLocal.configure(bind=original);unavailable.dispose()
