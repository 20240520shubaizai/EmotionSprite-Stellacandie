import pytest

# Importing the graph package first completes the package-level runtime imports;
# the adapter can then be tested without changing the production import graph.
from agent_core.agent_graph import AgentGraphRuntime  # noqa: F401
from agent_core.model_adapter import DeepSeekModelAdapter, ModelAdapterError, ModelUsage


@pytest.mark.asyncio
@pytest.mark.parametrize("code", ["network_unavailable", "rate_limited", "model_timeout", "model_unavailable"])
async def test_retryable_provider_failures_retry_once_and_remain_classified(monkeypatch, code):
    monkeypatch.setenv("AGENT_MODEL_MODE", "real")
    monkeypatch.setenv("DEEPSEEK_API_KEY", "test-only-placeholder")
    adapter = DeepSeekModelAdapter(); calls = 0

    def fail(*_args):
        nonlocal calls; calls += 1
        raise ModelAdapterError(code, "redacted provider failure", code != "model_unavailable")

    monkeypatch.setattr(adapter, "_request", fail)
    with pytest.raises(ModelAdapterError) as caught:
        await adapter.compose(text="测试", conversation=[], memories=[], persona_context="平等陪伴",
                              current_time="2026-08-21T17:00:00+08:00", risk="low", valence=0)
    assert caught.value.code == code
    assert calls == (1 if code == "model_unavailable" else 2)


@pytest.mark.asyncio
async def test_invalid_structured_reply_is_repaired_only_once(monkeypatch):
    monkeypatch.setenv("AGENT_MODEL_MODE", "real")
    monkeypatch.setenv("DEEPSEEK_API_KEY", "test-only-placeholder")
    adapter = DeepSeekModelAdapter(); calls = 0

    async def invalid(_messages, _correction):
        nonlocal calls; calls += 1
        return "not-json", ModelUsage("test", 1, 1)

    monkeypatch.setattr(adapter, "_request_with_retry", invalid)
    with pytest.raises(ModelAdapterError) as caught:
        await adapter.compose(text="测试", conversation=[], memories=[], persona_context="平等陪伴",
                              current_time="2026-08-21T17:00:00+08:00", risk="low", valence=0)
    assert caught.value.code == "format_error"
    assert calls == 2
