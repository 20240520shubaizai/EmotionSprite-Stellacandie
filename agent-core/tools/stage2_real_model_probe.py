from __future__ import annotations

import asyncio
import hashlib
import json
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from agent_core.agent_graph import AgentGraphRuntime
from agent_core.model_adapter import ModelAdapterError


async def main() -> int:
    output = Path(os.environ.get("STAGE2_REAL_REPORT", "stage2_real_model_report.json"))
    started = time.perf_counter()
    report: dict[str, object] = {
        "mode": "real",
        "provider": "DeepSeek OpenAI-compatible API",
        "model": os.environ.get("DEEPSEEK_MODEL", "deepseek-chat"),
        "credential_present": bool(os.environ.get("DEEPSEEK_API_KEY", "").strip()),
        "prompt_or_reply_persisted": False,
        "attempts": [],
    }
    try:
        runtime = AgentGraphRuntime()
        for index in range(2):
            before = time.perf_counter()
            result = await runtime.execute(
                f"real-probe-request-{index + 1}", f"real-probe-trace-{index + 1}",
                {"schema_version": "conversation_v2",
                 "text": "今天完成了一项拖了很久的小任务，心里轻松了一点。",
                 "current_time": "2026-08-21T17:00:00+08:00",
                 "conversation_context": [],
                 "persona_context": "平等陪伴，不称用户为主人；自然回应并可追问一个细节。",
                 "privacy": {"allow_memory": False, "allow_secret": False, "allow_cloud_sync": False},
                 "allowed_mutations": ["state_delta"]},
            )
            if result.error_code or result.degraded:
                raise ModelAdapterError(result.error_code or "validation_failed", "graph rejected model reply")
            usage = runtime.model_usage()
            report["attempts"].append({
                "index": index + 1,
                "success": True,
                "latency_ms": int((time.perf_counter() - before) * 1000),
                "request_id": result.request_id,
                "trace_id": result.trace_id,
                "reply_sha256": hashlib.sha256(result.body.encode("utf-8")).hexdigest(),
                "body_length": len(result.body),
                "emotion": result.emotion.value,
                "node_trace": result.node_trace,
                "input_tokens": usage.input_tokens,
                "output_tokens": usage.output_tokens,
            })
        hashes = [item["reply_sha256"] for item in report["attempts"]]
        report["different_expression_observed"] = len(set(hashes)) > 1
        report["passed"] = True
    except ModelAdapterError as error:
        report.update({"passed": False, "error_code": error.code,
                       "retryable": error.retryable, "error_type": type(error).__name__})
    except Exception as error:
        report.update({"passed": False, "error_code": "probe_internal_error",
                       "error_type": type(error).__name__})
    report["total_ms"] = int((time.perf_counter() - started) * 1000)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return 0 if report.get("passed") else 2


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
