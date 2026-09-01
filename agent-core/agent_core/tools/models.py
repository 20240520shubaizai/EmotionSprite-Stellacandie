from __future__ import annotations

from enum import StrEnum
from typing import Any
from pydantic import BaseModel,ConfigDict,Field


class PermissionLevel(StrEnum):
    read_only="read_only"
    low_risk_write="low_risk_write"
    confirmation_required="confirmation_required"
    forbidden_automatic="forbidden_automatic"


class ToolStatus(StrEnum):
    success="success"
    confirmation_required="confirmation_required"
    denied="denied"
    invalid_arguments="invalid_arguments"
    timeout="timeout"
    failed="failed"


class ToolCall(BaseModel):
    model_config=ConfigDict(extra="forbid")
    name:str=Field(min_length=1,max_length=80)
    arguments:dict[str,Any]=Field(default_factory=dict)
    confirmation_token:str|None=None
    actor:str=Field(default="agent",pattern="^(agent|user)$")


class ToolResult(BaseModel):
    status:ToolStatus
    tool_name:str
    data:dict[str,Any]=Field(default_factory=dict)
    error_code:str|None=None
    message:str=""
    retryable:bool=False
    duration_ms:int=0
    confirmation_token:str|None=None


class MemorySearchInput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    query:str=Field(min_length=1,max_length=500)
    limit:int=Field(default=6,ge=1,le=20)
    authorize_secret:bool=False


class MemoryCandidateInput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    subject:str=Field(min_length=1,max_length=120)
    content:str=Field(min_length=1,max_length=1000)
    source_record_ids:list[str]=Field(default_factory=list,max_length=20)
    confidence:float=Field(default=.8,ge=0,le=1)
    privacy_level:str=Field(default="normal",pattern="^(normal|sensitive|secret|local_only)$")


class ReminderCreateInput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    title:str=Field(min_length=1,max_length=200)
    scheduled_at:str=Field(min_length=10,max_length=64)


class RecordIdInput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    record_id:str=Field(min_length=1,max_length=128)


class QueryInput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    query:str=Field(min_length=1,max_length=1000)


class SummaryInput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    source_record_id:str=Field(min_length=1,max_length=128)
    requirement:str=Field(default="提炼重点",max_length=1000)


class GenerateInput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    date:str=Field(min_length=8,max_length=16)
    source_record_ids:list[str]=Field(default_factory=list,max_length=30)


class FileDeleteInput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    path:str=Field(min_length=1,max_length=1024)


class DeleteAllInput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    scope:str=Field(pattern="^(all_user_data|memories|conversations)$")


class SyncPrivacyInput(BaseModel):
    model_config=ConfigDict(extra="forbid")
    data_type:str=Field(min_length=1,max_length=64)
    enabled:bool
