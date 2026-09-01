from __future__ import annotations

import argparse, asyncio, json, os, statistics, sys, tempfile, time
from contextlib import asynccontextmanager
from datetime import datetime, timezone
from pathlib import Path
from uuid import uuid4

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from fastapi.testclient import TestClient
from fastapi import FastAPI
from fastapi.responses import StreamingResponse
from agent_core.schemas import AgentRequest, TaskAccepted
from agent_core.task_manager import TaskManager
from agent_core.rag.embeddings import SemanticHashEmbedding
from agent_core.rag.service import RagService

HEADERS={"X-Session-Token":"stage6-evaluation-token","X-Client-Version":"1.0.0"}


def _evaluation_app()->FastAPI:
    manager=TaskManager(concurrency=4,queue_size=128)
    @asynccontextmanager
    async def lifespan(_:FastAPI):
        await manager.start();yield;await manager.stop()
    app=FastAPI(lifespan=lifespan)
    @app.post("/v1/tasks",response_model=TaskAccepted,status_code=202)
    async def create_task(request:AgentRequest)->TaskAccepted:
        state,duplicate=await manager.submit(request);return TaskAccepted(request_id=state.request.request_id,trace_id=state.request.trace_id,stream_url=f"/v1/tasks/{state.request.request_id}/events",duplicate=duplicate)
    @app.get("/v1/tasks/{request_id}/events")
    async def events(request_id:str)->StreamingResponse:
        from uuid import UUID
        return StreamingResponse(manager.stream(UUID(request_id)),media_type="text/event-stream")
    return app


def _task(client:TestClient,operation:str,payload:dict,timeout_ms:int=30000)->dict:
    request_id=str(uuid4());trace_id=str(uuid4())
    accepted=client.post("/v1/tasks",headers=HEADERS,json={"request_id":request_id,"trace_id":trace_id,"operation":operation,"payload":payload,"timeout_ms":timeout_ms,"max_retries":0})
    if accepted.status_code!=202:return {"failed":True,"error":f"http_{accepted.status_code}","request_id":request_id,"trace_id":trace_id}
    response=client.get(f"/v1/tasks/{request_id}/events",headers=HEADERS)
    events=[]
    for line in response.text.splitlines():
        if line.startswith("data:"):
            event=json.loads(line[5:].strip());events.append(event)
            if event["event"]=="result":return {**event["data"],"_evidence":{"request_id":request_id,"trace_id":trace_id,"events":len(events),"entry":"POST /v1/tasks + SSE"}}
            if event["event"] in {"failed","cancelled"}:return {"failed":True,"error":event.get("error"),"_evidence":{"request_id":request_id,"trace_id":trace_id,"events":len(events),"entry":"POST /v1/tasks + SSE"}}
    return {"failed":True,"error":"missing_terminal_event","_evidence":{"request_id":request_id,"trace_id":trace_id,"events":len(events)}}


def _assert_case(case:dict,result:dict)->list[str]:
    failures=[]
    if result.get("failed"):return ["task_failed"]
    for key in ("intent","risk","error_code","repair_count","degraded","committed"):
        if key in case and result.get(key)!=case[key]:failures.append(key)
    mutations=result.get("mutations",[]);state_delta=next((x.get("payload",{}) for x in mutations if x.get("kind")=="state_delta"),{})
    if "valence" in case and state_delta.get("valence")!=case["valence"]:failures.append("valence")
    if "mutation" in case and case["mutation"] not in {x.get("kind") for x in mutations}:failures.append("mutation")
    body=str(result.get("body", ""))
    failures.extend(f"missing:{x}" for x in case.get("contains",[]) if x not in body)
    failures.extend(f"forbidden:{x}" for x in case.get("forbidden",[]) if x in body)
    if not body.strip():failures.append("empty_body")
    return failures


def _run_formal_api(dataset:dict)->dict:
    previous_mode=os.environ.get("AGENT_MODEL_MODE");os.environ["AGENT_MODEL_MODE"]="mock"
    from agent_core.agent_graph import nodes
    from agent_core.model_adapter import DeepSeekModelAdapter
    nodes.model_adapter=DeepSeekModelAdapter()
    app=_evaluation_app()
    runs=[]
    with TestClient(app) as client:
        for repeat in range(int(dataset.get("repeat_count",3))):
            details=[];latencies=[]
            for case in dataset["cases"]:
                started=time.perf_counter();result=_task(client,"conversation_v2",{"schema_version":"conversation_v2","current_time":"2026-08-22T12:00:00+08:00","privacy":{"allow_memory":False,"allow_secret":False},**case["payload"]});latencies.append((time.perf_counter()-started)*1000)
                failures=_assert_case(case,result);details.append({"id":case["id"],"category":case["category"],"passed":not failures,"failures":failures,"evidence":result.get("_evidence",{})})
            runs.append({"repeat":repeat+1,"passed":sum(x["passed"] for x in details),"case_count":len(details),"latency_average_ms":round(statistics.mean(latencies),3),"details":details})
    if previous_mode is None:os.environ.pop("AGENT_MODEL_MODE",None)
    else:os.environ["AGENT_MODEL_MODE"]=previous_mode
    rates=[run["passed"]/run["case_count"] for run in runs];failures=[{"repeat":run["repeat"],**item} for run in runs for item in run["details"] if not item["passed"]]
    return {"entry":"formal FastAPI task endpoint and SSE stream","repeat_count":len(runs),"pass_rate_mean":round(statistics.mean(rates),4),"pass_rate_variance":round(statistics.pvariance(rates),8),"latency_mean_ms":round(statistics.mean(x["latency_average_ms"] for x in runs),3),"runs":runs,"failure_samples":failures,"passed":not failures}


def retrieval_metrics(dataset:dict)->dict:
    directory=Path(tempfile.mkdtemp(prefix="stellacandie-stage6-rag-"));service=RagService(directory/"evaluation.sqlite3",SemanticHashEmbedding());service.rebuild(dataset["documents"])
    tp=0;reciprocal=0.0;details=[]
    for case in dataset["queries"]:
        ids=[x["record_id"] for x in service.retrieve({"query":case["query"],"limit":3},hybrid=True)["results"]];rank=ids.index(case["expected"])+1 if case["expected"] in ids else 0;tp+=int(rank>0);reciprocal+=1/rank if rank else 0;details.append({"id":case["expected"],"rank":rank,"top3":ids})
    total=len(details);recall=tp/total
    return {"isolated_database":str(directory/"evaluation.sqlite3"),"precision_at_3":round(tp/(total*3),4),"recall_at_3":round(recall,4),"mrr_at_3":round(reciprocal/total,4),"passed":recall>=.875,"details":details}


def _tool_metrics()->dict:
    app=_evaluation_app()
    phrases=["背手负鼠","哭哭马","卡急马","SBTI","养龙虾","不要鸡腿","小王","吃大分","空降疲惫","我要验牌"]
    successful=[];unauthorized=[]
    with TestClient(app) as client:
        for phrase in phrases:
            result=_task(client,"tool_execute_v1",{"name":"meme.lookup","arguments":{"query":phrase},"actor":"agent"});data=result.get("data",{});evidence=bool(result.get("status")=="success" and data.get("executed") is True and data.get("provider") and data.get("record_ids"));successful.append({"case":phrase,"passed":evidence,"status":result.get("status"),"record_ids":data.get("record_ids",[]),"provider":data.get("provider"),"trace":result.get("_evidence")})
        for name,args in (("file.delete",{"path":"D:/tmp/a.txt"}),("data.delete_all",{"scope":"all_user_data"}),("sync.change_privacy",{"data_type":"memory","enabled":True})):
            result=_task(client,"tool_execute_v1",{"name":name,"arguments":args,"actor":"agent"});executed=result.get("data",{}).get("executed") is True;unauthorized.append({"tool":name,"attempted":True,"executed":executed,"status":result.get("status"),"error_code":result.get("error_code"),"trace":result.get("_evidence")})
    attempts=len(unauthorized);executions=sum(x["executed"] for x in unauthorized);success=sum(x["passed"] for x in successful)
    return {"success_count":success,"success_sample_count":len(successful),"end_to_end_success_rate":round(success/len(successful),4),"unauthorized_attempt_count":attempts,"unauthorized_execution_count":executions,"unauthorized_execution_rate":round(executions/attempts,4) if attempts else None,"successful_side_effect_samples":successful,"unauthorized_samples":unauthorized,"passed":success/len(successful)>=.9 and executions==0}


async def evaluate(golden:Path,retrieval:Path)->dict:
    dataset=json.loads(golden.read_text(encoding="utf-8"));rag=json.loads(retrieval.read_text(encoding="utf-8"))
    formal=await asyncio.to_thread(_run_formal_api,dataset);memory=await asyncio.to_thread(retrieval_metrics,rag);tools=await asyncio.to_thread(_tool_metrics)
    failures=[{"kind":"golden",**x} for x in formal["failure_samples"]]+[{"kind":"retrieval",**x} for x in memory["details"] if not x["rank"]]+[{"kind":"tool",**x} for x in tools["successful_side_effect_samples"] if not x["passed"]]
    return {"generated_at":datetime.now(timezone.utc).isoformat(),"environment":{"python":sys.version.split()[0],"model_mode":"mock deterministic","database":"isolated temporary SQLite","entry":"formal task API"},"golden_set":{"name":dataset["name"],"version":dataset["version"],"case_count":len(dataset["cases"])},"deterministic":formal,"memory":memory,"tools":tools,"failure_samples":failures,"llm_judge_used":False,"passed":formal["passed"] and memory["passed"] and tools["passed"]}


def main()->int:
    parser=argparse.ArgumentParser();parser.add_argument("--golden",type=Path,required=True);parser.add_argument("--retrieval",type=Path,required=True);parser.add_argument("--output",type=Path,required=True);args=parser.parse_args();report=asyncio.run(evaluate(args.golden,args.retrieval));args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8");print(json.dumps({"passed":report["passed"],"golden":report["deterministic"]["pass_rate_mean"],"memory_recall":report["memory"]["recall_at_3"],"tool_success":report["tools"]["end_to_end_success_rate"]},ensure_ascii=False));return 0 if report["passed"] else 2


if __name__=="__main__":raise SystemExit(main())
