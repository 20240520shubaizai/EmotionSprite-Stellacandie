import json
from pathlib import Path

root = Path(__file__).resolve().parents[1]
data = json.loads((root / "tests" / "personality_training_cases.json").read_text(encoding="utf-8"))
cases = data["cases"]
ids = [case["id"] for case in cases]
assert len(ids) == len(set(ids)), "测试场景ID重复"
assert data["scoring"]["pass_score"] >= 70
for case in cases:
    assert case.get("user"), f"{case['id']} 缺少用户输入"
    assert case.get("expected"), f"{case['id']} 缺少预期行为"
    assert case.get("forbidden"), f"{case['id']} 缺少禁止行为"
print(f"人格训练集校验通过：{len(cases)}个场景，及格线{data['scoring']['pass_score']}分")
