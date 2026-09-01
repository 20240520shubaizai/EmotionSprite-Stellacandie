from __future__ import annotations

from collections import Counter, deque
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import json
import os
from pathlib import Path
from threading import Lock
from time import time
from typing import Any

from .tools.security import redact


@dataclass
class TraceEntry:
    timestamp: float; trace_id: str; request_id: str; node: str; status: str
    duration_ms: int = 0; queue_ms: int | None = None; first_token_ms: int | None = None
    model_ms: int | None = None; retrieval_ms: int | None = None; tool_ms: int | None = None
    model: str = ""; input_tokens: int | None = None; output_tokens: int | None = None
    cache_hit_tokens: int | None = None; cache_miss_tokens: int | None = None
    token_source: str = "unavailable"; tool_name: str = ""; error_code: str = ""
    summary: dict[str, Any] | None = None


class ObservabilityStore:
    """Bounded, redacted trace storage with optional local JSONL persistence."""
    def __init__(self, limit: int | None = None, path: Path | None = None) -> None:
        self.limit = limit or int(os.getenv("AGENT_TRACE_MAX_COUNT", "300"))
        self.retention_days = int(os.getenv("AGENT_TRACE_RETENTION_DAYS", "14"))
        configured = os.getenv("AGENT_TRACE_PATH", "").strip()
        self.path = path or (Path(configured) if configured else None)
        self._items: deque[dict[str, Any]] = deque(maxlen=self.limit); self._counts: Counter[str] = Counter(); self._lock = Lock(); self._load()

    def _load(self) -> None:
        if not self.path or not self.path.exists(): return
        cutoff = time() - self.retention_days * 86400
        try:
            for line in self.path.read_text(encoding="utf-8").splitlines()[-self.limit * 2:]:
                item = json.loads(line)
                if float(item.get("timestamp", 0)) >= cutoff: self._items.append(item)
        except (OSError, ValueError, TypeError, json.JSONDecodeError): self._items.clear()

    def _persist(self) -> None:
        if not self.path: return
        self.path.parent.mkdir(parents=True, exist_ok=True); temporary = self.path.with_suffix(self.path.suffix + ".tmp")
        temporary.write_text("\n".join(json.dumps(x, ensure_ascii=False, separators=(",", ":")) for x in self._items) + "\n", encoding="utf-8"); temporary.replace(self.path)

    def record(self, entry: TraceEntry) -> dict[str, Any]:
        safe = asdict(entry); safe["summary"] = redact(safe.get("summary") or {})
        with self._lock:
            self._items.append(safe); self._counts["requests"] += int(entry.node == "request" and entry.status == "running")
            self._counts["retries"] += int(entry.status == "retrying"); self._counts["degraded"] += int(entry.status == "degraded"); self._counts["failures"] += int(entry.status == "failed"); self._persist()
        return safe

    @staticmethod
    def _average(items: list[int | float]) -> float | None: return round(sum(items) / len(items), 2) if items else None

    @staticmethod
    def _pricing(model: str) -> dict[str, Any] | None:
        path = Path(os.getenv("AGENT_PRICING_CONFIG", str(Path(__file__).resolve().parents[1] / "config" / "model_pricing_v1.json")))
        try:
            root = json.loads(path.read_text(encoding="utf-8")); row = root["models"].get(model)
            return {**row, "version": root["version"], "effective_at": root["effective_at"], "currency": root["currency"]} if row else None
        except (OSError, KeyError, TypeError, json.JSONDecodeError): return None

    def snapshot(self) -> dict[str, Any]:
        with self._lock: items = list(self._items); counts = dict(self._counts)
        completed = [x for x in items if x["node"] == "request" and x["status"] in {"success", "failed"}]
        metric = lambda name: self._average([x[name] for x in completed if isinstance(x.get(name), (int, float))])
        token_rows = [x for x in completed if x.get("token_source") == "provider" and isinstance(x.get("input_tokens"), int) and isinstance(x.get("output_tokens"), int)]
        input_tokens = sum(x["input_tokens"] for x in token_rows) if token_rows else None; output_tokens = sum(x["output_tokens"] for x in token_rows) if token_rows else None
        costs: list[float] = []; price_versions: set[str] = set(); currency = None
        for row in token_rows:
            price = self._pricing(row.get("model", ""))
            if price:
                hit=row.get("cache_hit_tokens");miss=row.get("cache_miss_tokens")
                if isinstance(hit,int) and isinstance(miss,int):costs.append(hit/1_000_000*price["input_cache_hit_per_million"]+miss/1_000_000*price["input_cache_miss_per_million"]+row["output_tokens"]/1_000_000*price["output_per_million"])
                price_versions.add(f'{price["version"]}@{price["effective_at"]}'); currency = price["currency"]
        return {"traces": items[-100:], "retention": {"days": self.retention_days, "max_count": self.limit, "persistent": self.path is not None}, "metrics": {
            "requests": counts.get("requests", 0), "failures": counts.get("failures", 0), "retries": counts.get("retries", 0), "degraded": counts.get("degraded", 0),
            "latency_ms": {"queue": metric("queue_ms"), "first_token": metric("first_token_ms"), "model": metric("model_ms"), "retrieval": metric("retrieval_ms"), "tool": metric("tool_ms"), "total": metric("duration_ms")},
            "tokens": {"input": input_tokens, "output": output_tokens, "source": "provider" if token_rows else "unavailable", "measured_requests": len(token_rows)},
            "cost": {"measured": round(sum(costs), 8) if costs else None, "currency": currency, "pricing_versions": sorted(price_versions), "status": "measured" if costs else "unavailable"},
            "generated_at": datetime.now(timezone.utc).isoformat()}}


observability = ObservabilityStore()
