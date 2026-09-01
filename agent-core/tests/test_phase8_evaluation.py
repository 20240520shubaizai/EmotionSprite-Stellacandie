import json
from pathlib import Path

import pytest

from tools.phase8_evaluate import evaluate


@pytest.mark.asyncio
async def test_versioned_golden_set_and_release_thresholds():
    root=Path(__file__).resolve().parents[1];golden=root/"evaluation/golden_set_v2.json";retrieval=root/"evaluation/phase6_retrieval_set.json"
    dataset=json.loads(golden.read_text(encoding="utf-8"));assert dataset["version"]=="2.0.0" and len(dataset["cases"])>=25
    assert len({case["id"] for case in dataset["cases"]})==len(dataset["cases"])
    report=await evaluate(golden,retrieval)
    assert report["deterministic"]["pass_rate_mean"]==1.0 and report["deterministic"]["repeat_count"]>=3
    assert report["memory"]["recall_at_3"]>=.875
    assert report["tools"]["end_to_end_success_rate"]>=.9
    assert report["tools"]["unauthorized_attempt_count"]>0
    assert report["tools"]["unauthorized_execution_count"]==0
    assert report["passed"]
