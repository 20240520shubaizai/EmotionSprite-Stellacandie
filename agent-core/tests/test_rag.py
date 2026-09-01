from __future__ import annotations

from datetime import datetime,timedelta,timezone
from pathlib import Path

import pytest

from agent_core.rag.embeddings import SemanticHashEmbedding
from agent_core.rag.service import RagService
from agent_core.schemas import AgentRequest,TaskClass
from agent_core.task_manager import TaskManager


NOW=datetime(2026,8,18,12,0,tzinfo=timezone.utc)


def document(record_id,subject,content,**overrides):
    value={"record_id":record_id,"source_type":"user_memory","fact_type":"confirmed_fact","subject":subject,
           "content":content,"recorded_at":NOW.isoformat(),"importance":50,"confidence":.9,"status":"active","revision":1}
    value.update(overrides);return value


@pytest.fixture
def rag(tmp_path:Path)->RagService:
    return RagService(tmp_path/"derived.sqlite3",SemanticHashEmbedding())


def test_hybrid_recall_explains_source_and_beats_keyword_baseline(rag:RagService):
    documents=[
        document("sleep","换枕头","用户准备换一个更合适的枕头，免得早上脖子酸。"),
        document("cat","胖橘猫","用户很喜欢公园里胖胖的橘猫。"),
        document("food","抹茶蛋糕","用户想尝尝那家店的抹茶蛋糕。"),
        document("sport","晨跑","用户计划每周跑步三次。"),
        document("work","周会","用户周二上午要开会讨论项目。"),
        document("game","王者荣耀","用户最近在王者荣耀里练辅助。"),
    ]
    rag.rebuild(documents)
    cases=[("最近有什么睡眠用品计划","sleep"),("更偏爱哪种宠物","cat"),("想吃什么甜点","food"),
           ("有什么锻炼安排","sport"),("近期工作安排呢","work"),("最近在玩什么游戏","game")]
    keyword_hits=hybrid_hits=0
    for query,expected in cases:
        baseline=rag.retrieve({"query":query,"now":NOW.isoformat(),"limit":3},hybrid=False)["results"]
        hybrid=rag.retrieve({"query":query,"now":NOW.isoformat(),"limit":3},hybrid=True)["results"]
        keyword_hits+=int(any(item["record_id"]==expected for item in baseline))
        hybrid_hits+=int(any(item["record_id"]==expected for item in hybrid))
        selected=next(item for item in hybrid if item["record_id"]==expected)
        assert selected["source_type"]=="user_memory" and selected["fact_type"]=="confirmed_fact"
        assert selected["recorded_at"] and selected["confidence"]==.9 and selected["reasons"]
    assert hybrid_hits==len(cases) and hybrid_hits>keyword_hits


def test_secret_requires_request_authorization_and_trace_never_contains_body(rag:RagService):
    marker="SECRET_BODY_MUST_NOT_ENTER_TRACE"
    rag.rebuild([document("secret","私密约定",marker,privacy_level="secret")])
    denied=rag.retrieve({"query":"私密约定","now":NOW.isoformat()})
    allowed=rag.retrieve({"query":"私密约定","now":NOW.isoformat(),"authorize_secret":True})
    assert denied["results"]==[] and allowed["results"][0]["content"]==marker
    assert marker not in str(allowed["trace"]) and allowed["trace"][0]["content_redacted"] is True


def test_archived_event_expiry_inference_and_proactive_permission_filters(rag:RagService):
    rag.rebuild([
        document("archived","旧会议","用户上周参加过会议。",source_type="event",fact_type="temporary_event",status="archived"),
        document("expired","旧计划","用户以前计划买书。",expires_at=(NOW-timedelta(days=1)).isoformat()),
        document("inference","推测","模型推测用户讨厌下雨。",fact_type="model_inference"),
        document("no-proactive","安静的话题","用户不希望主动提这件事。",proactive_allowed=False),
        document("active","普通事实","用户最近喜欢听轻音乐。"),
    ])
    result=rag.retrieve({"query":"用户以前最近的事情","now":NOW.isoformat(),"proactive":True,"limit":10})["results"]
    assert [item["record_id"] for item in result]==["active"]


def test_explicit_reminder_is_independent_of_memory_importance(rag:RagService):
    rag.rebuild([
        document("reminder","换枕头","两天后提醒用户换枕头。",source_type="reminder",fact_type="temporary_event",
                 importance=1,explicit_request=True,status="pending"),
        document("memory","高重要事实","用户非常喜欢猫。",importance=100),
    ])
    result=rag.retrieve({"query":"到期提醒","now":NOW.isoformat(),"limit":2})["results"]
    reminder=next(item for item in result if item["record_id"]=="reminder")
    assert "明确提醒独立优先级" in reminder["reasons"]


def test_incremental_upsert_delete_and_revision_guard(rag:RagService):
    rag.rebuild([document("m1","旧称呼","用户的朋友叫小林。",revision=1)])
    assert rag.upsert(document("m1","新称呼","用户的朋友叫小岚。",revision=2))["status"]=="indexed"
    assert rag.upsert(document("m1","过期修改","错误旧内容。",revision=1))["status"]=="stale_ignored"
    result=rag.retrieve({"query":"朋友小岚","now":NOW.isoformat()})["results"]
    assert result[0]["subject"]=="新称呼" and "错误旧内容" not in result[0]["content"]
    assert rag.delete("m1",1)["status"]=="stale_ignored"
    assert rag.delete("m1",3)["status"]=="deleted" and rag.index.count()==0


@pytest.mark.asyncio
async def test_task_operations_and_graph_use_retrieved_memory(rag:RagService):
    manager=TaskManager(concurrency=1);manager.rag_service=rag;await manager.start()
    try:
        rag.rebuild([document("cat","橘猫","用户上次在公园看到了一只很胖的橘猫。")])
        request=AgentRequest(operation="conversation_v1",task_class=TaskClass.user_chat,
            payload={"text":"还记得上次那只宠物吗？","current_time":NOW.isoformat(),"use_rag":True})
        state=(await manager.submit(request))[0];await state.done.wait()
        result=next(event.data for event in state.events if event.event.value=="result")
        assert "胖的橘猫" in result["body"]
        retrieval_event=next(event for event in state.events if event.event.value=="node" and event.data.get("node")=="hybrid_memory_retrieval")
        assert "用户上次" not in str(retrieval_event.data["trace"])
    finally:await manager.stop()


def test_retrieval_does_not_create_new_fact(rag:RagService):
    rag.rebuild([document("known","已知事实","用户喜欢橘猫。")]);before=rag.index.count()
    result=rag.retrieve({"query":"用户是不是去火星捡过石头","now":NOW.isoformat()})
    assert rag.index.count()==before
    assert all("火星" not in item["content"] for item in result["results"])
