from __future__ import annotations

from datetime import datetime, timezone
from enum import StrEnum
from typing import Any
from uuid import UUID, uuid4

from pydantic import BaseModel, ConfigDict, Field


class EventType(StrEnum):
    accepted = "accepted"
    queued = "queued"
    running = "running"
    retrying = "retrying"
    result = "result"
    failed = "failed"
    cancelled = "cancelled"
    node = "node"


class TaskClass(StrEnum):
    user_chat="user_chat"
    background="background"


class ErrorCode(StrEnum):
    unauthorized = "unauthorized"
    incompatible_version = "incompatible_version"
    queue_full = "queue_full"
    timeout = "timeout"
    cancelled = "cancelled"
    internal_error = "internal_error"
    not_found = "not_found"
    model_unavailable = "model_unavailable"
    model_timeout = "model_timeout"
    empty_body = "empty_body"
    format_error = "format_error"
    validation_failed = "validation_failed"


class AgentRequest(BaseModel):
    model_config = ConfigDict(extra="forbid")
    request_id: UUID = Field(default_factory=uuid4)
    trace_id: UUID = Field(default_factory=uuid4)
    operation: str = Field(min_length=1, max_length=64)
    payload: dict[str, Any] = Field(default_factory=dict)
    timeout_ms: int = Field(default=30_000, ge=100, le=120_000)
    max_retries: int = Field(default=1, ge=0, le=2)
    task_class: TaskClass = TaskClass.user_chat


class AgentError(BaseModel):
    code: ErrorCode
    message: str
    retryable: bool = False


class AgentEvent(BaseModel):
    sequence: int = Field(ge=1)
    request_id: UUID
    trace_id: UUID
    event: EventType
    timestamp: datetime = Field(default_factory=lambda: datetime.now(timezone.utc))
    data: dict[str, Any] = Field(default_factory=dict)
    error: AgentError | None = None


class TaskAccepted(BaseModel):
    request_id: UUID
    trace_id: UUID
    stream_url: str
    duplicate: bool = False


class HealthResponse(BaseModel):
    status: str = "ok"
    service: str = "emotion-sprite-agent-core"
    version: str
    protocol_version: str


class CapabilityResponse(BaseModel):
    protocol_version: str
    min_client_version: str
    max_client_major: int
    capabilities: list[str]
    limits: dict[str, int]

class SyncEvent(BaseModel):
    model_config=ConfigDict(extra="forbid")
    idempotency_key:str=Field(min_length=8,max_length=128)
    user_id:str=Field(default="local-single-user",max_length=64)
    entity_type:str
    entity_uuid:str
    revision:int=Field(ge=0)
    operation:str=Field(pattern="^(upsert|delete|restore)$")
    privacy_level:str=Field(default="normal")
    payload:dict[str,Any]=Field(default_factory=dict)
class SyncBatch(BaseModel):events:list[SyncEvent]=Field(max_length=100)
class TransientImageRequest(BaseModel):image_base64:str=Field(max_length=15_000_000);mime_type:str=Field(pattern="^image/")
class ConflictResolution(BaseModel):
    choice:str=Field(pattern="^(local|cloud)$")
    user_id:str=Field(default="local-single-user",max_length=64)
class CloudDeleteRequest(BaseModel):
    user_id:str=Field(default="local-single-user",max_length=64)
    confirmation:str
