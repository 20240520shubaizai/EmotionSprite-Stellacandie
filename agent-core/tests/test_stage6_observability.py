import json, os
from pathlib import Path

import pytest

import agent_core.agent_graph  # initialize graph package before importing its model adapter
from agent_core.model_adapter import DeepSeekModelAdapter, ModelAdapterError
from agent_core.observability import ObservabilityStore, TraceEntry


def test_trace_persistence_retention_redaction_and_real_metric_semantics(tmp_path:Path,monkeypatch):
    pricing=tmp_path/"pricing.json";pricing.write_text(json.dumps({"version":"test-v1","effective_at":"2026-08-22T00:00:00Z","currency":"CNY","models":{"deepseek-chat":{"input_cache_hit_per_million":.02,"input_cache_miss_per_million":1,"output_per_million":2}}}),encoding="utf-8")
    monkeypatch.setenv("AGENT_PRICING_CONFIG",str(pricing));path=tmp_path/"trace.jsonl";store=ObservabilityStore(limit=2,path=path)
    store.record(TraceEntry(1,"old","r0","request","success",summary={"content":"SECRET"}))
    store.record(TraceEntry(2,"t1","r1","request","success",duration_ms=100,queue_ms=2,model_ms=80,model="deepseek-chat",input_tokens=100,output_tokens=20,cache_hit_tokens=40,cache_miss_tokens=60,token_source="provider"))
    store.record(TraceEntry(3,"t2","r2","request","success",duration_ms=50,model="mock",token_source="unavailable"))
    snapshot=store.snapshot();assert len(snapshot["traces"])==2 and snapshot["retention"]["persistent"]
    assert snapshot["metrics"]["tokens"]=={"input":100,"output":20,"source":"provider","measured_requests":1}
    assert snapshot["metrics"]["latency_ms"]["first_token"] is None
    assert snapshot["metrics"]["cost"]["measured"]==0.0001008 and "SECRET" not in path.read_text(encoding="utf-8")


@pytest.mark.asyncio
@pytest.mark.parametrize("fault,code",[("timeout","model_timeout"),("unavailable","model_unavailable"),("empty","format_error"),("format","format_error")])
async def test_transport_level_model_fault_injection(monkeypatch,fault,code):
    monkeypatch.setenv("AGENT_MODEL_MODE","real");monkeypatch.setenv("DEEPSEEK_API_KEY","evaluation-only-placeholder");monkeypatch.setenv("AGENT_MODEL_FAULT_INJECTION",fault)
    adapter=DeepSeekModelAdapter()
    with pytest.raises(ModelAdapterError) as caught:
        await adapter.compose(text="测试",conversation=[],memories=[],persona_context="",current_time="2026-08-22T12:00:00+08:00",risk="low",valence=0)
    assert caught.value.code==code


def test_mock_usage_is_unavailable_not_fabricated_zero(monkeypatch):
    monkeypatch.setenv("AGENT_MODEL_MODE","mock");adapter=DeepSeekModelAdapter()
    assert adapter.last_usage.input_tokens is None and adapter.last_usage.output_tokens is None and adapter.last_usage.token_source=="unavailable"
