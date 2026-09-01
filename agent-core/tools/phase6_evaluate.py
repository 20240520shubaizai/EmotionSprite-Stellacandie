from __future__ import annotations

import argparse
from datetime import datetime,timezone
import json
from pathlib import Path
import tempfile

from agent_core.rag.embeddings import FastEmbedProvider,SemanticHashEmbedding
from agent_core.rag.service import RagService


def metrics(service:RagService,dataset:dict,hybrid:bool)->dict:
    reciprocal=0.0;hits=0;details=[]
    for case in dataset["queries"]:
        values=service.retrieve({"query":case["query"],"limit":3},hybrid=hybrid)["results"]
        ids=[item["record_id"] for item in values];rank=ids.index(case["expected"])+1 if case["expected"] in ids else 0
        hits+=int(rank>0);reciprocal+=1/rank if rank else 0
        details.append({"query":case["query"],"expected":case["expected"],"top3":ids,"rank":rank})
    total=len(dataset["queries"])
    return {"recall_at_3":round(hits/total,4),"mrr_at_3":round(reciprocal/total,4),"details":details}


def main()->int:
    parser=argparse.ArgumentParser();parser.add_argument("--dataset",type=Path,required=True);parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--cache",type=Path,required=True);args=parser.parse_args();dataset=json.loads(args.dataset.read_text(encoding="utf-8"))
    candidates=[]
    for name,provider in (("semantic_hash_fallback",SemanticHashEmbedding()),("bge_small_zh_fastembed",FastEmbedProvider(args.cache))):
        service=RagService(Path(tempfile.mkdtemp())/f"{name}.sqlite3",provider);service.rebuild(dataset["documents"])
        baseline=metrics(service,dataset,False);hybrid=metrics(service,dataset,True)
        candidates.append({"name":name,"provider":provider.fingerprint,"keyword_baseline":baseline,"hybrid":hybrid,
                           "improved":hybrid["recall_at_3"]>baseline["recall_at_3"] or hybrid["mrr_at_3"]>baseline["mrr_at_3"]})
    selected=max(candidates,key=lambda item:(item["hybrid"]["recall_at_3"],item["hybrid"]["mrr_at_3"],item["name"]=="bge_small_zh_fastembed"))
    report={"generated_at":datetime.now(timezone.utc).isoformat(),"dataset":str(args.dataset),"sample_count":len(dataset["queries"]),
            "vector_store":"sqlite-vec 0.1.9","fusion":"RRF + policy score","candidates":candidates,
            "selected":selected["name"] if selected["improved"] else "keyword_baseline_rollback","passed":selected["improved"]}
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    print(json.dumps({"passed":report["passed"],"selected":report["selected"],"candidates":[{"name":x["name"],"baseline":x["keyword_baseline"]["recall_at_3"],"hybrid":x["hybrid"]["recall_at_3"],"mrr":x["hybrid"]["mrr_at_3"]} for x in candidates]},ensure_ascii=False))
    return 0 if report["passed"] else 2


if __name__=="__main__":raise SystemExit(main())
