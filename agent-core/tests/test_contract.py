from uuid import uuid4

HEADERS={"X-Session-Token":"test-session-token","X-Client-Version":"1.0.0"}


def test_health_and_capability_contract(client):
    health=client.get("/health");assert health.status_code==200
    assert set(health.json())=={"status","service","version","protocol_version"}
    assert client.get("/v1/capabilities").status_code==401
    caps=client.get("/v1/capabilities",headers=HEADERS);assert caps.status_code==200
    assert {"sse","resume","cancel","deduplicate"} <= set(caps.json()["capabilities"])
    assert client.get("/v1/capabilities",headers={**HEADERS,"X-Client-Version":"2.0.0"}).status_code==426


def test_schema_rejects_unknown_fields_and_deduplicates(client):
        request_id=str(uuid4());trace_id=str(uuid4())
        body={"request_id":request_id,"trace_id":trace_id,"operation":"echo","payload":{"value":"hello"}}
        first=client.post("/v1/tasks",headers=HEADERS,json=body);second=client.post("/v1/tasks",headers=HEADERS,json=body)
        assert first.status_code==202 and not first.json()["duplicate"]
        assert second.status_code==202 and second.json()["duplicate"]
        assert client.post("/v1/tasks",headers=HEADERS,json={**body,"unknown":1}).status_code==422


def test_sse_sequence_resume_and_no_payload_in_metadata_events(client):
        created=client.post("/v1/tasks",headers=HEADERS,json={"operation":"echo","payload":{"value":"private-text"}}).json()
        stream=client.get(created["stream_url"],headers=HEADERS)
        assert stream.status_code==200 and "event: result" in stream.text
        ids=[int(line[4:]) for line in stream.text.splitlines() if line.startswith("id: ")]
        assert ids==sorted(set(ids)) and ids[0]==1
        resumed=client.get(created["stream_url"]+"?last_event_id=2",headers=HEADERS)
        resumed_ids=[int(line[4:]) for line in resumed.text.splitlines() if line.startswith("id: ")]
        assert all(i>2 for i in resumed_ids)


def test_logs_never_contain_message_body_or_token(client,caplog):
    marker="TOP_SECRET_MEMORY_9f3a"
    with caplog.at_level("INFO"):
        created=client.post("/v1/tasks",headers=HEADERS,json={"operation":"echo","payload":{"value":marker}}).json()
        client.get(created["stream_url"],headers=HEADERS)
    rendered="\n".join(record.getMessage() for record in caplog.records)
    assert marker not in rendered and HEADERS["X-Session-Token"] not in rendered
