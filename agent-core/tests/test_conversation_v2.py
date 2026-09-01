import asyncio
from uuid import uuid4
import pytest
from agent_core.schemas import AgentRequest,TaskClass
from agent_core.task_manager import TaskManager

def payload(i:int)->dict:
    return {"schema_version":"conversation_v2","text":f"第{i}条独立消息","current_time":"2026-08-21T16:00:00+08:00",
            "conversation_context":[{"role":"user","content":f"上下文{i}"}],"persona_context":"平等陪伴，不称主人",
            "pet_state":{"mood":60,"energy":70},"privacy":{"allow_memory":True,"allow_secret":False,"allow_cloud_sync":False},
            "allowed_mutations":["state_delta"],"attachment":None}

@pytest.mark.asyncio
async def test_conversation_v2_one_hundred_rounds_have_unique_identity_and_result():
    manager=TaskManager(concurrency=4,queue_size=128);await manager.start()
    try:
        seen=set()
        for i in range(100):
            request=AgentRequest(request_id=uuid4(),trace_id=uuid4(),operation="conversation_v2",payload=payload(i))
            state=(await manager.submit(request))[0];await state.done.wait()
            result=next(event.data for event in state.events if event.event.value=="result")
            assert str(request.request_id)==result["request_id"] and f"第{i}条独立消息" in result["body"]
            assert result["request_id"] not in seen;seen.add(result["request_id"])
    finally:await manager.stop()

@pytest.mark.asyncio
async def test_conversation_v2_forty_parallel_requests_never_cross():
    manager=TaskManager(concurrency=8,queue_size=64);await manager.start()
    async def run(i:int):
        request=AgentRequest(operation="conversation_v2",payload=payload(i));state=(await manager.submit(request))[0]
        await state.done.wait();result=next(event.data for event in state.events if event.event.value=="result")
        return i,request,result
    try:
        values=await asyncio.gather(*(run(i) for i in range(40)))
        assert len({result["request_id"] for _,_,result in values})==40
        assert len({result["trace_id"] for _,_,result in values})==40
        for i,request,result in values:
            assert result["request_id"]==str(request.request_id) and f"第{i}条独立消息" in result["body"]
            assert all(f"第{other}条独立消息" not in result["body"] for other in range(40) if other!=i)
    finally:await manager.stop()

@pytest.mark.asyncio
async def test_user_chat_is_selected_before_queued_background_work():
    manager=TaskManager(concurrency=1,queue_size=16);await manager.start()
    try:
        first=(await manager.submit(AgentRequest(operation="echo",payload={"value":"bg-running","delay_ms":80},task_class=TaskClass.background)))[0]
        await asyncio.sleep(.01)
        queued=[(await manager.submit(AgentRequest(operation="echo",payload={"value":f"bg-{i}","delay_ms":10},task_class=TaskClass.background)))[0] for i in range(4)]
        user=(await manager.submit(AgentRequest(operation="conversation_v2",payload=payload(999),task_class=TaskClass.user_chat)))[0]
        await asyncio.wait_for(user.done.wait(), timeout=2)
        assert first.done.is_set()
        assert all(not item.done.is_set() for item in queued)
    finally:await manager.stop()

@pytest.mark.asyncio
@pytest.mark.parametrize("behavior,error",[("empty","empty_body"),("malformed","format_error"),("timeout","model_timeout"),("unavailable","model_unavailable")])
async def test_conversation_v2_failure_classes_are_explicit(behavior,error):
    manager=TaskManager(concurrency=1);await manager.start()
    try:
        value=payload(1);value["model_behavior"]=behavior
        state=(await manager.submit(AgentRequest(operation="conversation_v2",payload=value)))[0];await state.done.wait()
        result=next(event.data for event in state.events if event.event.value=="result")
        assert result["error_code"]==error and result["repair_count"]==1
    finally:await manager.stop()
