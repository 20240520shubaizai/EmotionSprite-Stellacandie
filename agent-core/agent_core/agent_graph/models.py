from __future__ import annotations

from enum import StrEnum
from typing import Annotated, Any, Literal, TypedDict
import operator

from pydantic import BaseModel, ConfigDict, Field


class Intent(StrEnum):
    chat="chat"; reminder="reminder"; memory="memory"; state="state"


class Risk(StrEnum):
    low="low"; medium="medium"; high="high"


class Emotion(StrEnum):
    neutral="neutral"; warm="warm"; curious="curious"; concerned="concerned"


class MutationKind(StrEnum):
    memory_candidate="memory_candidate"; reminder="reminder"; state_delta="state_delta"


class AgentInput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    text:str=Field(min_length=1,max_length=4000)
    current_time:str
    conversation_context:list[Any]=Field(default_factory=list,max_length=20)
    memory_context:list[dict[str,Any]]=Field(default_factory=list,max_length=20)
    tool_context:list[dict[str,Any]]=Field(default_factory=list,max_length=5)
    persona_context:str=Field(default="",max_length=16000)
    pet_state:dict[str,int]=Field(default_factory=dict)
    privacy:dict[str,bool]=Field(default_factory=dict)
    attachment:dict[str,Any]|None=None
    allowed_mutations:set[MutationKind]=Field(default_factory=lambda:set(MutationKind))
    model_behavior:Literal["normal","empty","malformed","timeout","unavailable","unrecoverable"]="normal"


class MutationProposal(BaseModel):
    model_config=ConfigDict(extra="forbid")
    kind:MutationKind
    payload:dict[str,Any]
    source_record_ids:list[str]=Field(default_factory=list,max_length=16)
    permission:Literal["local_write","denied"]="local_write"
    confidence:float=Field(default=.8,ge=0,le=1)


class DraftOutput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    body:str=Field(min_length=1,max_length=500)
    emotion:Emotion=Emotion.neutral
    mutations:list[MutationProposal]=Field(default_factory=list,max_length=8)


class VerificationOutput(BaseModel):
    verified:bool
    reasons:list[str]=Field(default_factory=list)


class OrchestratorInput(BaseModel):text:str
class MemoryAnalystInput(BaseModel):conversation_context:list[Any];memory_context:list[dict[str,Any]]
class PlannerInput(BaseModel):intent:Intent;text:str
class StateAnalystInput(BaseModel):text:str;risk:Risk
class ComposerInput(BaseModel):text:str;risk:Risk;valence:int;tool_results:list[MutationProposal];model_behavior:str;memories:list[dict[str,Any]]=Field(default_factory=list);external_tools:list[dict[str,Any]]=Field(default_factory=list)
class VerifierInput(BaseModel):draft:DraftOutput|None;risk:Risk;allowed_mutations:set[MutationKind];error_code:str|None=None


class GraphResult(BaseModel):
    request_id:str
    trace_id:str
    body:str
    emotion:Emotion
    intent:Intent
    risk:Risk
    mutations:list[MutationProposal]
    committed:bool
    commit_protocol:str="mutation_commit_v1"
    memory_citations:list[dict[str,Any]]=Field(default_factory=list)
    degraded:bool=False
    error_code:str|None=None
    repair_count:int=0
    node_trace:list[str]
    branch_trace:list[str]


class AgentState(TypedDict, total=False):
    request_id:str;trace_id:str;raw_payload:dict[str,Any];input:AgentInput
    intent:Intent;risk:Risk;conversation:list[Any];memories:list[dict[str,Any]]
    tool_plan:list[str];tool_results:list[MutationProposal];analysis:dict[str,Any];draft:DraftOutput|None
    verified:bool;verification_reasons:list[str];repair_count:int
    error_code:str|None;degraded:bool;committed:bool
    node_trace:Annotated[list[str],operator.add]
    branch_trace:Annotated[list[str],operator.add]
