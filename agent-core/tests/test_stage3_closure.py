import json
from pathlib import Path

import pytest

from agent_core.rag import RagService


def test_memory_retrieve_v2_contract_and_secret_authorization(tmp_path:Path):
    source=json.loads((Path(__file__).parents[1]/"evaluation/phase6_retrieval_set.json").read_text(encoding="utf-8"))
    source["documents"].append({"record_id":"secret-note","source_type":"user_memory","fact_type":"confirmed_fact","subject":"暗号","content":"秘密暗号是月桂糖","privacy_level":"secret","confidence":1.0})
    service=RagService(tmp_path/"rag.db");service.rebuild(source["documents"])
    denied=service.retrieve_v2({"query":"秘密暗号是什么","limit":3,"authorize_secret":False})
    assert denied["schema_version"]=="memory_retrieve_v2"
    assert all(row["record_id"]!="secret-note" for row in denied["results"])
    allowed=service.retrieve_v2({"query":"秘密暗号是什么","limit":3,"authorize_secret":True})
    assert any(row["record_id"]=="secret-note" for row in allowed["results"])
    for row in allowed["results"]:
        assert {"record_id","source_type","fact_type","recorded_at","confidence","reasons"}<=row.keys()
    for query in ("我从没说过自己养了一只狗吧","我的银行卡密码是什么","我是不是住在北京"):
        assert service.retrieve_v2({"query":query,"limit":3})["results"]==[]


@pytest.mark.asyncio
async def test_conversation_v2_automatically_retrieves_but_never_commits_in_agent(tmp_path:Path):
    from agent_core.schemas import AgentRequest,TaskClass
    from agent_core.task_manager import TaskManager
    service=RagService(tmp_path/"rag.db");service.rebuild([{"record_id":"cat","source_type":"user_memory","fact_type":"user_statement","subject":"橘猫","content":"用户喜欢胖胖的橘猫","confidence":.95}])
    manager=TaskManager(concurrency=1);manager.rag_service=service;await manager.start()
    try:
        request=AgentRequest(operation="conversation_v2",task_class=TaskClass.user_chat,payload={"text":"我喜欢哪种猫","current_time":"2026-08-21T12:00:00+08:00","privacy":{"allow_memory":True,"allow_secret":False}})
        state=(await manager.submit(request))[0];await state.done.wait();result=next(e.data for e in state.events if e.event.value=="result")
        assert any(e.data.get("node")=="memory_retrieve_v2" for e in state.events if e.event.value=="node")
        assert result["committed"] is False and result["commit_protocol"]=="mutation_commit_v1"
        assert result["memory_citations"][0]["record_id"]=="cat"
    finally:await manager.stop()
