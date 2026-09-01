import asyncio
from uuid import uuid4
import pytest
from agent_core.agent_graph import AgentGraphRuntime
from agent_core.schemas import AgentRequest,TaskClass
from agent_core.task_manager import TaskManager

HEADERS={"X-Session-Token":"test-session-token","X-Client-Version":"1.0.0"}

@pytest.mark.asyncio
async def test_structured_graph_trace_and_validated_commit():
    runtime=AgentGraphRuntime();request_id=str(uuid4())
    result=await runtime.execute(request_id,str(uuid4()),{"text":"\u660e\u5929\u63d0\u9192\u6211\u6362\u6795\u5934","current_time":"2026-08-18T10:00:00+08:00"})
    assert result.intent=="reminder" and not result.committed and not result.degraded
    assert result.node_trace==["input_normalizer","conversation_orchestrator","memory_analyst","reminder_planner","state_analyst","tool_executor","response_composer","response_verifier","mutation_proposal_finalize"]
    assert result.branch_trace==["qt_sqlite_commit_required"] and result.commit_protocol=="mutation_commit_v1"
    assert {item.kind.value for item in result.mutations}=={"reminder","state_delta"}

@pytest.mark.asyncio
async def test_chinese_relative_minute_reminder_keeps_subject_and_due_time():
    runtime=AgentGraphRuntime();request_id=str(uuid4())
    text="\u8bf7\u5728\u4e24\u5206\u949f\u540e\u63d0\u9192\u6211\u68c0\u67e5R2\u7ed3\u679c"
    result=await runtime.execute(request_id,str(uuid4()),{"text":text,"current_time":"2026-08-31T16:31:00+08:00"})
    reminder=next(item for item in result.mutations if item.kind.value=="reminder")
    assert reminder.payload["scheduled_at"]=="2026-08-31T16:33:00+08:00"
    assert "\u68c0\u67e5R2\u7ed3\u679c" in reminder.payload["subject"] and "\u4e24\u5206\u949f\u540e" not in reminder.payload["subject"]

@pytest.mark.asyncio
@pytest.mark.parametrize("behavior,error",[("empty","empty_body"),("malformed","format_error"),("timeout","model_timeout"),("unavailable","model_unavailable")])
async def test_model_failures_repair_once_with_deterministic_result(behavior,error):
    runtime=AgentGraphRuntime();request_id=str(uuid4())
    result=await runtime.execute(request_id,str(uuid4()),{"text":"\u4eca\u5929\u53d1\u751f\u4e86\u4e00\u4ef6\u4e8b","model_behavior":behavior})
    assert result.body=="\u6211\u521a\u624d\u6ca1\u7ec4\u7ec7\u597d\u8bed\u8a00\uff0c\u4f46\u6211\u5728\u8ba4\u771f\u542c\u3002\u4f60\u613f\u610f\u518d\u8bf4\u4e00\u70b9\u5417\uff1f"
    assert result.error_code==error and result.repair_count==1 and not result.committed
    assert result.branch_trace==["repair_once","qt_sqlite_commit_required"]

@pytest.mark.asyncio
async def test_unrecoverable_output_falls_back_and_never_commits_invalid_facts():
    runtime=AgentGraphRuntime();request_id=str(uuid4())
    result=await runtime.execute(request_id,str(uuid4()),{"text":"\u4e00\u5b9a\u8981\u8bb0\u4f4f\u8fd9\u4ef6\u4e8b","model_behavior":"unrecoverable"})
    assert result.degraded and result.repair_count==1 and "\u4e0d\u4f1a\u4e71\u8bb0" in result.body
    assert result.mutations==[] and not result.committed
    assert result.node_trace.count("response_verifier")==2

@pytest.mark.asyncio
async def test_mutation_permission_failure_does_not_write_long_term_fact():
    runtime=AgentGraphRuntime();request_id=str(uuid4())
    result=await runtime.execute(request_id,str(uuid4()),{"text":"\u4ee5\u540e\u4e00\u5b9a\u8981\u8bb0\u4f4f\u6211\u559c\u6b22\u6a58\u732b","allowed_mutations":["state_delta"]})
    assert result.repair_count==1 and result.mutations==[] and not result.committed

@pytest.mark.asyncio
async def test_parallel_graph_calls_are_request_isolated():
    runtime=AgentGraphRuntime()
    async def run(i):
        rid=str(uuid4());result=await runtime.execute(rid,str(uuid4()),{"text":f"\u63d0\u9192\u6211\u4efb\u52a1{i}"});return rid,i,result
    values=await asyncio.gather(*(run(i) for i in range(40)))
    assert len({rid for rid,_,_ in values})==40
    for rid,i,result in values:
        assert f"\u4efb\u52a1{i}" in result.body
        assert all(f"\u4efb\u52a1{i}" in str(item.payload) or item.kind.value=="state_delta" for item in result.mutations)

@pytest.mark.asyncio
async def test_user_chat_queue_precedes_waiting_background_tasks():
    manager=TaskManager(concurrency=1,queue_size=10);await manager.start()
    try:
        requests=[AgentRequest(operation="echo",payload={"value":"blocker","delay_ms":120}),AgentRequest(operation="echo",payload={"value":"background"},task_class=TaskClass.background),AgentRequest(operation="echo",payload={"value":"chat"},task_class=TaskClass.user_chat)]
        states=[]
        for request in requests:states.append((await manager.submit(request))[0])
        await asyncio.gather(*(state.done.wait() for state in states))
        background_done=next(event.timestamp for event in states[1].events if event.event.value=="result")
        chat_done=next(event.timestamp for event in states[2].events if event.event.value=="result")
        assert chat_done<=background_done
    finally:await manager.stop()

def test_conversation_endpoint_exposes_node_trace_without_cross_context(client):
    accepted=client.post("/v1/tasks",headers=HEADERS,json={"operation":"conversation_v1","task_class":"user_chat","payload":{"text":"\u6211\u6536\u62fe\u4e00\u4e0b\u51c6\u5907\u53bb\u5f00\u4f1a","current_time":"2026-08-18T14:30:00+08:00"}}).json()
    text=client.get(accepted["stream_url"],headers=HEADERS).text
    assert "event: node" in text and "mutation_proposal_finalize" in text and '"intent":"chat"' in text
