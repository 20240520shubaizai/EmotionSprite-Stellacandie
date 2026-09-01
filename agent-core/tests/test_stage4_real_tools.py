import asyncio
import json
from pathlib import Path

import pytest

from agent_core.rag import RagService
from agent_core.tools import ToolCall,build_default_registry


class Response:
    def __init__(self,payload:dict):self.payload=payload
    def __enter__(self):return self
    def __exit__(self,*_):return False
    def read(self):return json.dumps(self.payload,ensure_ascii=False).encode()


@pytest.mark.asyncio
async def test_every_registered_tool_has_real_success_and_failure(monkeypatch,tmp_path:Path):
    rag=RagService(tmp_path/"rag.db");rag.rebuild([{"record_id":"memory:m1","source_type":"user_memory","fact_type":"confirmed_fact","subject":"猫","content":"用户喜欢橘猫","confidence":1.0}])
    registry=build_default_registry(lambda:rag)
    memory=await registry.execute(ToolCall(name="memory.search",arguments={"query":"喜欢什么猫"}))
    assert memory.status.value=="success" and memory.data["executed"] and "memory:m1" in memory.data["record_ids"]
    meme=await registry.execute(ToolCall(name="meme.lookup",arguments={"query":"王者荣耀上分"}))
    assert meme.status.value=="success" and meme.data["executed"] and meme.data["record_ids"]
    original_exists=Path.exists;monkeypatch.setattr(Path,"exists",lambda _self:False)
    meme_failed=await registry.execute(ToolCall(name="meme.lookup",arguments={"query":"王者荣耀"}))
    assert meme_failed.status.value=="failed" and meme_failed.data=={}
    monkeypatch.setattr(Path,"exists",original_exists)
    calls=0
    def urlopen(*_args,**_kwargs):
        nonlocal calls;calls+=1
        return Response({"results":[{"name":"北京","country":"中国","latitude":39.9,"longitude":116.4}]} if calls==1 else {"current":{"time":"2026-08-22T08:00","temperature_2m":25.0,"apparent_temperature":26.0,"precipitation":0.0,"weather_code":1}})
    monkeypatch.setattr("urllib.request.urlopen",urlopen)
    weather=await registry.execute(ToolCall(name="weather.query",arguments={"query":"北京天气"}))
    assert weather.status.value=="success" and weather.data["executed"] and weather.data["provider"]=="Open-Meteo"
    invalid=await registry.execute(ToolCall(name="memory.search",arguments={"query":""}))
    assert invalid.status.value=="invalid_arguments"
    monkeypatch.setattr("urllib.request.urlopen",lambda *_a,**_k:(_ for _ in ()).throw(OSError("offline")))
    failed=await registry.execute(ToolCall(name="weather.query",arguments={"query":"北京"}))
    assert failed.status.value=="failed" and failed.data=={} and not failed.retryable


@pytest.mark.asyncio
async def test_read_tool_timeout_and_duplicate_calls_have_no_side_effect(monkeypatch):
    from agent_core.tools.models import PermissionLevel,QueryInput
    from agent_core.tools.registry import ToolDefinition,ToolRegistry
    def slow(_):import time;time.sleep(.05);return {"executed":True,"record_ids":[],"provider":"test"}
    registry=ToolRegistry();registry.register(ToolDefinition("slow.read","slow",QueryInput,PermissionLevel.read_only,10,slow))
    result=await registry.execute(ToolCall(name="slow.read",arguments={"query":"x"}))
    assert result.status.value=="timeout" and result.retryable
    real=build_default_registry();first=await real.execute(ToolCall(name="meme.lookup",arguments={"query":"王者荣耀"}));second=await real.execute(ToolCall(name="meme.lookup",arguments={"query":"王者荣耀"}))
    assert first.data["record_ids"]==second.data["record_ids"]


@pytest.mark.asyncio
async def test_conversation_v2_invokes_selected_real_adapter(monkeypatch,tmp_path:Path):
    from agent_core.schemas import AgentRequest,TaskClass
    from agent_core.task_manager import TaskManager
    calls=0
    def urlopen(*_args,**_kwargs):
        nonlocal calls;calls+=1
        return Response({"results":[{"name":"北京","country":"中国","latitude":39.9,"longitude":116.4}]} if calls==1 else {"current":{"time":"2026-08-22T08:00","temperature_2m":25.0,"apparent_temperature":26.0,"precipitation":0.0,"weather_code":1}})
    monkeypatch.setattr("urllib.request.urlopen",urlopen)
    manager=TaskManager(concurrency=1);manager.rag_service=RagService(tmp_path/"rag.db");await manager.start()
    try:
        request=AgentRequest(operation="conversation_v2",task_class=TaskClass.user_chat,payload={"text":"北京天气怎么样","current_time":"2026-08-22T08:00:00+08:00","privacy":{"allow_memory":False}})
        state=(await manager.submit(request))[0];await state.done.wait()
        nodes=[event.data for event in state.events if event.event.value=="node"]
        assert any(node.get("node")=="mcp_tool_adapter" and node.get("tool")=="weather.query" and node.get("status")=="success" for node in nodes)
    finally:await manager.stop()
