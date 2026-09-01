from __future__ import annotations

from datetime import datetime, timezone
from enum import StrEnum
from typing import Any

from pydantic import BaseModel, ConfigDict, Field, field_validator


class SourceType(StrEnum):
    personality_bible="personality_bible"
    user_memory="user_memory"
    event="event"
    reminder="reminder"
    shared_experience="shared_experience"


class FactType(StrEnum):
    confirmed_fact="confirmed_fact"
    user_statement="user_statement"
    model_inference="model_inference"
    temporary_event="temporary_event"
    shared_experience="shared_experience"
    personality_rule="personality_rule"


class PrivacyLevel(StrEnum):
    normal="normal"
    sensitive="sensitive"
    secret="secret"


class MemoryDocument(BaseModel):
    model_config=ConfigDict(extra="forbid")
    record_id:str=Field(min_length=1,max_length=128)
    source_type:SourceType
    fact_type:FactType
    subject:str=Field(default="",max_length=200)
    content:str=Field(min_length=1,max_length=8000)
    recorded_at:datetime=Field(default_factory=lambda:datetime.now(timezone.utc))
    importance:int=Field(default=50,ge=0,le=100)
    confidence:float=Field(default=.8,ge=0,le=1)
    use_count:int=Field(default=0,ge=0)
    status:str=Field(default="active",max_length=32)
    expires_at:datetime|None=None
    proactive_allowed:bool=True
    privacy_level:PrivacyLevel=PrivacyLevel.normal
    explicit_request:bool=False
    revision:int=Field(default=0,ge=0)
    metadata:dict[str,Any]=Field(default_factory=dict)

    @field_validator("recorded_at","expires_at")
    @classmethod
    def timezone_aware(cls,value:datetime|None)->datetime|None:
        if value is not None and value.tzinfo is None:return value.replace(tzinfo=timezone.utc)
        return value


class RetrievalRequest(BaseModel):
    model_config=ConfigDict(extra="forbid")
    query:str=Field(min_length=1,max_length=2000)
    limit:int=Field(default=6,ge=1,le=20)
    now:datetime=Field(default_factory=lambda:datetime.now(timezone.utc))
    authorize_secret:bool=False
    proactive:bool=False
    include_model_inference:bool=False


class RetrievalResult(BaseModel):
    record_id:str
    source_type:SourceType
    fact_type:FactType
    subject:str
    content:str
    recorded_at:datetime
    confidence:float
    score:float
    reasons:list[str]
    privacy_level:PrivacyLevel

    def trace_view(self)->dict[str,Any]:
        return {"record_id":self.record_id,"source_type":self.source_type.value,
                "fact_type":self.fact_type.value,"recorded_at":self.recorded_at.isoformat(),
                "confidence":self.confidence,"score":self.score,"reasons":self.reasons,
                "privacy_level":self.privacy_level.value,"content_redacted":True}
