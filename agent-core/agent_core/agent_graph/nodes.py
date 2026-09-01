from __future__ import annotations

import asyncio
import re
from datetime import datetime,timezone,timedelta
from pydantic import ValidationError
from .errors import AgentExecutionError
from .models import (AgentInput,AgentState,ComposerInput,DraftOutput,Emotion,Intent,MemoryAnalystInput,
                     MutationKind,MutationProposal,OrchestratorInput,PlannerInput,Risk,StateAnalystInput,
                     VerificationOutput,VerifierInput)
from ..model_adapter import DeepSeekModelAdapter,ModelAdapterError

model_adapter=DeepSeekModelAdapter()

def normalize_input(state:AgentState)->dict:
    raw=state["raw_payload"]
    value=AgentInput.model_validate({"text":str(raw.get("text","")).strip(),"current_time":str(raw.get("current_time") or datetime.now(timezone.utc).isoformat()).strip(),"conversation_context":raw.get("conversation_context",[]),"memory_context":raw.get("memory_context",[]),"tool_context":raw.get("tool_context",[]),"persona_context":raw.get("persona_context",""),"pet_state":raw.get("pet_state",{}),"privacy":raw.get("privacy",{}),"attachment":raw.get("attachment"),"allowed_mutations":raw.get("allowed_mutations",list(MutationKind)),"model_behavior":raw.get("model_behavior","normal")})
    return {"input":value,"repair_count":0,"degraded":False,"committed":False,"node_trace":["input_normalizer"]}

def conversation_orchestrator(state:AgentState)->dict:
    text=OrchestratorInput(text=state["input"].text).text
    if re.search("\u63d0\u9192|\u8bb0\u5f97|\u522b\u5fd8",text) or (re.search("\u660e\u5929|\u540e\u5929|\\d{1,2}\\s*\u70b9",text) and re.search("\u5f00\u4f1a|\u4f1a\u8bae|\u6362|\u529e|\u4ea4|\u53bb",text)):intent=Intent.reminder
    elif re.search("\u4e00\u5b9a\u8981|\u957f\u671f|\u4ee5\u540e.*\u8bb0",text):intent=Intent.memory
    elif re.search("\u5f00\u5fc3|\u96be\u8fc7|\u751f\u6c14|\u7d2f",text):intent=Intent.state
    else:intent=Intent.chat
    risk=Risk.high if re.search("\u81ea\u6740|\u4f24\u5bb3\u81ea\u5df1|\u4e0d\u60f3\u6d3b",text) else Risk.medium if re.search("\u751f\u75c5|\u75bc|\u836f",text) else Risk.low
    return {"intent":intent,"risk":risk,"node_trace":["conversation_orchestrator"]}

def memory_analyst(state:AgentState)->dict:
    value=MemoryAnalystInput(conversation_context=state["input"].conversation_context,memory_context=state["input"].memory_context)
    return {"conversation":value.conversation_context[-8:],"memories":value.memory_context[-8:],"node_trace":["memory_analyst"]}

def reminder_planner(state:AgentState)->dict:
    value=PlannerInput(intent=state["intent"],text=state["input"].text);tools=[]
    if value.intent==Intent.reminder:tools.append("propose_reminder")
    if value.intent==Intent.memory:tools.append("propose_memory")
    return {"tool_plan":tools,"node_trace":["reminder_planner"]}

def state_analyst(state:AgentState)->dict:
    value=StateAnalystInput(text=state["input"].text,risk=state["risk"]);text=value.text
    valence=1 if re.search("\u5f00\u5fc3|\u987a\u5229|\u559c\u6b22|\u597d\u68d2",text) else -1 if re.search("\u96be\u8fc7|\u751f\u6c14|\u70e6|\u7d2f",text) else 0
    return {"analysis":{"valence":valence,"energy_cost":1,"risk":value.risk.value},"node_trace":["state_analyst"]}

def tool_executor(state:AgentState)->dict:
    value=state["input"];results=[]
    if "propose_reminder" in state["tool_plan"]:
        now=datetime.fromisoformat(value.current_time.replace("Z","+00:00"));text=value.text
        chinese={"\u534a":30,"\u4e00":1,"\u4e24":2,"\u4e8c":2,"\u4e09":3,"\u56db":4,"\u4e94":5,"\u516d":6,"\u4e03":7,"\u516b":8,"\u4e5d":9,"\u5341":10}
        minute_match=re.search("(\\d{1,4}|\u534a|[\u4e00\u4e8c\u4e24\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d\u5341])\\s*\u5206\u949f\u540e",text)
        hour_after_match=re.search("(\\d{1,3}|\u534a|[\u4e00\u4e8c\u4e24\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d\u5341])\\s*\u5c0f\u65f6\u540e",text)
        if minute_match:
            token=minute_match.group(1);due=now+timedelta(minutes=int(token) if token.isdigit() else chinese[token])
        elif hour_after_match:
            token=hour_after_match.group(1);amount=.5 if token=="\u534a" else int(token) if token.isdigit() else chinese[token];due=now+timedelta(hours=amount)
        else:due=now+timedelta(days=2 if "\u540e\u5929" in text else 1 if "\u660e\u5929" in text else 0)
        hour_match=re.search("(?:\u4e0a\u5348|\u65e9\u4e0a|\u665a\u4e0a|\u4e0b\u5348)?\\s*(\\d{1,2})\\s*\u70b9",text)
        if hour_match and not (minute_match or hour_after_match):
            hour=int(hour_match.group(1));hour+=12 if ("\u4e0b\u5348" in text or "\u665a\u4e0a" in text) and hour<12 else 0;due=due.replace(hour=hour,minute=0,second=0,microsecond=0)
        elif not (minute_match or hour_after_match):due=due.replace(hour=9,minute=0,second=0,microsecond=0)
        cleanup="(?:\u660e\u5929|\u540e\u5929|\u5230\u65f6\u5019|\u4e00\u5b9a\u8981|\u8bb0\u5f97|\u8bf7|\u63d0\u9192\u6211|\u522b\u5fd8\u4e86?|\\d{1,4}\\s*\u5206\u949f\u540e|[\u4e00\u4e8c\u4e24\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d\u5341\u534a]\\s*\u5206\u949f\u540e|\\d{1,3}\\s*\u5c0f\u65f6\u540e|[\u4e00\u4e8c\u4e24\u4e09\u56db\u4e94\u516d\u4e03\u516b\u4e5d\u5341\u534a]\\s*\u5c0f\u65f6\u540e|(?:\u4e0a\u5348|\u65e9\u4e0a|\u4e0b\u5348|\u665a\u4e0a)?\\s*\\d{1,2}\\s*\u70b9)"
        subject=re.sub(cleanup,"",text).strip(" \uff0c\u3002\uff01!") or text;subject=re.sub("^\u5728","",subject)
        results.append(MutationProposal(kind=MutationKind.reminder,payload={"type":"agent.reminder","subject":subject,"source_text":text,"scheduled_at":due.isoformat(),"status":"pending"},confidence=.95))
    if "propose_memory" in state["tool_plan"]:results.append(MutationProposal(kind=MutationKind.memory_candidate,payload={"content":value.text,"status":"candidate"}))
    results.append(MutationProposal(kind=MutationKind.state_delta,payload={"energy":-1,"valence":state["analysis"]["valence"]}))
    return {"tool_results":results,"node_trace":["tool_executor"]}

async def response_composer(state:AgentState)->dict:
    source=state["input"];value=ComposerInput(text=source.text,risk=state["risk"],valence=state["analysis"]["valence"],tool_results=state["tool_results"],model_behavior=source.model_behavior,memories=state.get("memories",[]),external_tools=source.tool_context);behavior=value.model_behavior;error=None;draft=None
    try:
        if behavior=="timeout":await asyncio.sleep(0);raise TimeoutError
        if behavior=="unavailable":raise ConnectionError
        if behavior=="empty":raw={"body":"","emotion":"neutral","mutations":[]}
        elif behavior=="malformed":raw={"body":42,"emotion":"unknown","mutations":"bad"}
        elif behavior=="unrecoverable":raw={"body":"\u4e3b\u4eba","emotion":"neutral","mutations":[]}
        else:
            reply=await model_adapter.compose(text=value.text,conversation=state.get("conversation",[]),memories=state.get("memories",[]),persona_context=source.persona_context,current_time=source.current_time,risk=value.risk.value,valence=value.valence,tool_context=value.external_tools)
            raw={"body":reply.body,"emotion":reply.emotion.value,"mutations":[item.model_dump(mode="json") for item in state["tool_results"]]}
        draft=DraftOutput.model_validate(raw)
    except TimeoutError:error=AgentExecutionError.model_timeout.value
    except ConnectionError:error=AgentExecutionError.model_unavailable.value
    except ModelAdapterError as exc:error=exc.code
    except ValidationError as exc:error=AgentExecutionError.empty_body.value if any(item["type"]=="string_too_short" for item in exc.errors()) else AgentExecutionError.format_error.value
    return {"draft":draft,"error_code":error,"node_trace":["response_composer"]}

def response_verifier(state:AgentState)->dict:
    value=VerifierInput(draft=state.get("draft"),risk=state["risk"],allowed_mutations=state["input"].allowed_mutations,error_code=state.get("error_code"));reasons=[];draft=value.draft
    if not draft:reasons.append(state.get("error_code") or AgentExecutionError.validation_failed.value)
    else:
        if "\u4e3b\u4eba" in draft.body:reasons.append("companion_boundary_violation")
        if value.risk==Risk.high and not re.search("\u73b0\u5b9e|\u8054\u7cfb|\u5e2e\u52a9|\u966a",draft.body):reasons.append("high_risk_support_missing")
        if any(item.kind not in value.allowed_mutations for item in draft.mutations):reasons.append("mutation_not_authorized")
    result=VerificationOutput(verified=not reasons,reasons=reasons)
    return {"verified":result.verified,"verification_reasons":result.reasons,"node_trace":["response_verifier"]}

def repair_response(state:AgentState)->dict:
    count=state.get("repair_count",0)+1
    draft=DraftOutput(body="\u4e3b\u4eba" if state["input"].model_behavior=="unrecoverable" else "\u6211\u521a\u624d\u6ca1\u7ec4\u7ec7\u597d\u8bed\u8a00\uff0c\u4f46\u6211\u5728\u8ba4\u771f\u542c\u3002\u4f60\u613f\u610f\u518d\u8bf4\u4e00\u70b9\u5417\uff1f",emotion=Emotion.neutral if state["input"].model_behavior=="unrecoverable" else Emotion.warm,mutations=[])
    return {"draft":draft,"repair_count":count,"error_code":state.get("error_code") or AgentExecutionError.validation_failed.value,"node_trace":["response_repair"],"branch_trace":["repair_once"]}

def safe_fallback(state:AgentState)->dict:
    body="\u8fd9\u542c\u8d77\u6765\u53ef\u80fd\u5f88\u5371\u9669\u3002\u8bf7\u5148\u8054\u7cfb\u73b0\u5b9e\u4e2d\u53ef\u4fe1\u4efb\u7684\u4eba\u6216\u5f53\u5730\u7d27\u6025\u5e2e\u52a9\uff0c\u6211\u4e5f\u4f1a\u966a\u4f60\u628a\u4e0b\u4e00\u6b65\u8bf4\u6e05\u695a\u3002" if state["risk"]==Risk.high else "\u6211\u8fd9\u6b21\u6709\u70b9\u6ca1\u542c\u660e\u767d\uff0c\u4f46\u6211\u4e0d\u4f1a\u4e71\u8bb0\u3002\u4f60\u53ef\u4ee5\u6362\u4e00\u79cd\u8bf4\u6cd5\u518d\u544a\u8bc9\u6211\u3002"
    return {"draft":DraftOutput(body=body,emotion=Emotion.warm,mutations=[]),"verified":True,"degraded":True,"verification_reasons":[],"error_code":state.get("error_code") or AgentExecutionError.validation_failed.value,"node_trace":["safe_fallback"],"branch_trace":["deterministic_fallback"]}

def route_after_verify(state:AgentState)->str:
    if state.get("verified"):return "commit"
    return "repair" if state.get("repair_count",0)==0 else "fallback"

def mutation_proposal_finalize(state:AgentState)->dict:
    return {"committed":False,"node_trace":["mutation_proposal_finalize"],"branch_trace":["qt_sqlite_commit_required"]}
