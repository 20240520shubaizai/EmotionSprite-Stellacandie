import concurrent.futures
import time

HEADERS={"X-Session-Token":"test-session-token","X-Client-Version":"1.0.0"}


def test_parallel_requests_do_not_cross_talk(client):
        def run(i):
            accepted=client.post("/v1/tasks",headers=HEADERS,json={"operation":"echo","payload":{"value":i,"delay_ms":i%4}}).json()
            text=client.get(accepted["stream_url"],headers=HEADERS).text
            return i,accepted["request_id"],text
        with concurrent.futures.ThreadPoolExecutor(max_workers=12) as pool: results=list(pool.map(run,range(40)))
        ids={request_id for _,request_id,_ in results};assert len(ids)==40
        for i,request_id,text in results:
            assert request_id in text and f'"echo":{i}' in text


def test_cancel_timeout_and_retry_are_explicit(client):
        accepted=client.post("/v1/tasks",headers=HEADERS,json={"operation":"echo","payload":{"delay_ms":5000},"timeout_ms":100}).json()
        assert "event: failed" in client.get(accepted["stream_url"],headers=HEADERS).text
        retried=client.post("/v1/tasks",headers=HEADERS,json={"operation":"fail_once","max_retries":1}).json()
        retry_text=client.get(retried["stream_url"],headers=HEADERS).text
        assert "event: retrying" in retry_text and "event: result" in retry_text
        cancel=client.post("/v1/tasks",headers=HEADERS,json={"operation":"echo","payload":{"delay_ms":1000}}).json()
        assert client.delete(f'/v1/tasks/{cancel["request_id"]}',headers=HEADERS).json()["cancelled"]
        assert "event: cancelled" in client.get(cancel["stream_url"],headers=HEADERS).text
