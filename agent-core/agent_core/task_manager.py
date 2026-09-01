from __future__ import annotations

import asyncio
from collections import deque
from dataclasses import dataclass, field
import json
import logging
import time
from typing import AsyncIterator
from uuid import UUID
from pydantic import ValidationError

from .schemas import AgentError, AgentEvent, AgentRequest, ErrorCode, EventType
from .schemas import TaskClass
from .agent_graph import AgentGraphRuntime
from .rag import RagService
from .tools import ToolCall,build_default_registry
from .observability import TraceEntry,observability

logger = logging.getLogger("agent_core.tasks")


@dataclass
class TaskState:
    request: AgentRequest
    events: list[AgentEvent] = field(default_factory=list)
    subscribers: set[asyncio.Queue[AgentEvent]] = field(default_factory=set)
    done: asyncio.Event = field(default_factory=asyncio.Event)
    cancelled: asyncio.Event = field(default_factory=asyncio.Event)
    enqueued_at: float = field(default_factory=time.perf_counter)

    def emit(self, event: EventType, data: dict | None = None, error: AgentError | None = None) -> AgentEvent:
        item = AgentEvent(sequence=len(self.events)+1, request_id=self.request.request_id,
                          trace_id=self.request.trace_id, event=event, data=data or {}, error=error)
        self.events.append(item)
        for subscriber in tuple(self.subscribers): subscriber.put_nowait(item)
        if event in {EventType.result, EventType.failed, EventType.cancelled}: self.done.set()
        return item


class TaskManager:
    def __init__(self, concurrency: int = 4, queue_size: int = 64) -> None:
        self.concurrency = concurrency
        self.queue_size = queue_size
        self.tasks: dict[UUID, TaskState] = {}
        self.user_pending: deque[TaskState] = deque()
        self.background_pending: deque[TaskState] = deque()
        self._queue_condition=asyncio.Condition()
        self.graph_runtime=AgentGraphRuntime()
        self.rag_service:RagService|None=None
        self.tool_registry=build_default_registry(self._rag)
        self.workers: list[asyncio.Task] = []

    async def start(self) -> None:
        if not self.workers:
            self.workers = [asyncio.create_task(self._worker(i)) for i in range(self.concurrency)]

    async def stop(self) -> None:
        for worker in self.workers: worker.cancel()
        await asyncio.gather(*self.workers, return_exceptions=True)
        self.workers.clear()

    def _rag(self)->RagService:
        if self.rag_service is None:self.rag_service=RagService()
        return self.rag_service

    async def submit(self, request: AgentRequest) -> tuple[TaskState, bool]:
        existing = self.tasks.get(request.request_id)
        if existing: return existing, True
        if len(self.user_pending)+len(self.background_pending)>=self.queue_size: raise asyncio.QueueFull
        state=TaskState(request);self.tasks[request.request_id]=state
        state.emit(EventType.accepted);state.emit(EventType.queued, {"position":len(self.user_pending)+len(self.background_pending)+1,"queue":request.task_class.value})
        async with self._queue_condition:
            target=self.user_pending if request.task_class==TaskClass.user_chat else self.background_pending
            target.append(state);self._queue_condition.notify()
        logger.info("accepted request_id=%s trace_id=%s operation=%s",request.request_id,request.trace_id,request.operation)
        return state,False

    async def cancel(self, request_id: UUID) -> bool:
        state=self.tasks.get(request_id)
        if not state or state.done.is_set(): return False
        state.cancelled.set();return True

    async def stream(self, request_id: UUID, after: int=0) -> AsyncIterator[str]:
        state=self.tasks[request_id]; queue: asyncio.Queue[AgentEvent]=asyncio.Queue();state.subscribers.add(queue)
        try:
            for event in state.events:
                if event.sequence>after:
                    yield self._sse(event)
                    after=event.sequence
            while not state.done.is_set():
                try: event=await asyncio.wait_for(queue.get(),timeout=10)
                except asyncio.TimeoutError:
                    yield ": keepalive\n\n";continue
                if event.sequence>after: yield self._sse(event);after=event.sequence
            for event in state.events:
                if event.sequence>after: yield self._sse(event);after=event.sequence
        finally: state.subscribers.discard(queue)

    @staticmethod
    def _sse(event: AgentEvent) -> str:
        return f"id: {event.sequence}\nevent: {event.event.value}\ndata: {event.model_dump_json()}\n\n"

    async def _worker(self, index: int) -> None:
        while True:
            async with self._queue_condition:
                await self._queue_condition.wait_for(lambda:bool(self.user_pending or self.background_pending))
                state=self.user_pending.popleft() if self.user_pending else self.background_pending.popleft()
            await self._execute(state)

    async def _execute(self, state: TaskState) -> None:
        req=state.request;started=time.perf_counter();queue_ms=int((started-state.enqueued_at)*1000);input_tokens=output_tokens=cache_hit_tokens=cache_miss_tokens=None;model_name="";model_ms=first_token_ms=retrieval_ms=tool_ms=None;token_source="unavailable";state.emit(EventType.running)
        observability.record(TraceEntry(time.time(),str(req.trace_id),str(req.request_id),"request","running",summary={"operation":req.operation}))
        for attempt in range(req.max_retries+1):
            try:
                if state.cancelled.is_set(): raise asyncio.CancelledError
                async with asyncio.timeout(req.timeout_ms/1000):
                    if req.operation == "echo":
                        await asyncio.sleep(float(req.payload.get("delay_ms",0))/1000)
                        result={"echo": req.payload.get("value"), "worker_safe": True}
                    elif req.operation == "ping": result={"pong": True}
                    elif req.operation in {"conversation_v1","conversation_v2"}:
                        graph_payload=dict(req.payload)
                        use_v1_rag=bool(graph_payload.pop("use_rag",False))
                        if (req.operation=="conversation_v2" and bool(graph_payload.get("privacy",{}).get("allow_memory",True))) or use_v1_rag:
                            retrieval_started=time.perf_counter();retrieval=await asyncio.to_thread(self._rag().retrieve_v2,{"query":graph_payload.get("text","").strip(),
                                "limit":6,"authorize_secret":bool(graph_payload.get("privacy",{}).get("allow_secret",False)),"proactive":False})
                            retrieval_ms=int((time.perf_counter()-retrieval_started)*1000)
                            graph_payload["memory_context"]=retrieval["results"]
                            state.emit(EventType.node,{"node":"hybrid_memory_retrieval" if req.operation=="conversation_v1" else "memory_retrieve_v2","trace":retrieval["trace"]})
                        if req.operation=="conversation_v2":
                            selected=self.tool_registry.select(str(graph_payload.get("text","")))
                            if selected in {"weather.query","meme.lookup"}:
                                query=str(graph_payload.get("text","")).strip();tool_result=await self.tool_registry.execute(ToolCall(name=selected,arguments={"query":query}));tool_ms=tool_result.duration_ms
                                state.emit(EventType.node,{"node":"mcp_tool_adapter","tool":selected,"status":tool_result.status.value,"record_ids":tool_result.data.get("record_ids",[])})
                                if tool_result.status.value=="success":graph_payload["tool_context"]=[{"tool":selected,**tool_result.data}]
                        graph_result=await self.graph_runtime.execute(str(req.request_id),str(req.trace_id),graph_payload)
                        usage=self.graph_runtime.model_usage();model_name=usage.model
                        input_tokens=usage.input_tokens;output_tokens=usage.output_tokens;cache_hit_tokens=usage.cache_hit_tokens;cache_miss_tokens=usage.cache_miss_tokens;model_ms=usage.model_ms or None;first_token_ms=usage.first_token_ms;token_source=usage.token_source
                        for node in graph_result.node_trace:state.emit(EventType.node,{"node":node})
                        result=graph_result.model_dump(mode="json")
                        valence=1 if graph_result.emotion.value=="warm" else -1 if graph_result.emotion.value=="concerned" else 0
                        result["state_effect"]={"mood":valence,"energy":-1,
                            "curiosity":1 if graph_result.emotion.value=="curious" else 0,"confidence":85}
                    elif req.operation == "rag_rebuild_v1":
                        result=await asyncio.to_thread(self._rag().rebuild,req.payload.get("documents",[]))
                    elif req.operation == "rag_upsert_v1":
                        result=await asyncio.to_thread(self._rag().upsert,req.payload.get("document",{}))
                    elif req.operation == "rag_delete_v1":
                        result=await asyncio.to_thread(self._rag().delete,str(req.payload.get("record_id","")),int(req.payload.get("revision",0)))
                    elif req.operation == "memory_retrieve_v1":
                        retrieval_payload={key:value for key,value in req.payload.items() if key!="hybrid"}
                        result=await asyncio.to_thread(self._rag().retrieve,retrieval_payload,bool(req.payload.get("hybrid",True)))
                    elif req.operation == "memory_retrieve_v2":
                        result=await asyncio.to_thread(self._rag().retrieve_v2,req.payload)
                    elif req.operation == "tool_catalog_v1":result={"tools":self.tool_registry.catalog()}
                    elif req.operation == "tool_select_v1":result={"selected_tool":self.tool_registry.select(str(req.payload.get("text",""))),"catalog_version":"stage4-real-v1"}
                    elif req.operation == "tool_execute_v1":
                        call=ToolCall.model_validate(req.payload);tool_result=await self.tool_registry.execute(call)
                        trace=observability.record(TraceEntry(time.time(),str(req.trace_id),str(req.request_id),"tool",tool_result.status.value,
                            duration_ms=tool_result.duration_ms,tool_name=call.name,error_code=tool_result.error_code or "",
                            summary={"record_ids":tool_result.data.get("record_ids",[]),"permission_result":tool_result.status.value}))
                        state.emit(EventType.node,{"node":"tool_execution","trace":trace})
                        result=tool_result.model_dump(mode="json")
                    elif req.operation == "diagnostics_snapshot_v1":result=observability.snapshot()
                    elif req.operation == "fail_once" and attempt == 0: raise RuntimeError("transient")
                    else: result={"accepted_operation":req.operation}
                    if state.cancelled.is_set(): raise asyncio.CancelledError
                    duration=int((time.perf_counter()-started)*1000)
                    observability.record(TraceEntry(time.time(),str(req.trace_id),str(req.request_id),"request","success",duration_ms=duration,
                        queue_ms=queue_ms,first_token_ms=first_token_ms,model_ms=model_ms,retrieval_ms=retrieval_ms,tool_ms=tool_ms,
                        model=model_name,input_tokens=input_tokens,output_tokens=output_tokens,cache_hit_tokens=cache_hit_tokens,cache_miss_tokens=cache_miss_tokens,token_source=token_source,summary={"operation":req.operation}))
                    state.emit(EventType.result,result);return
            except asyncio.CancelledError:
                observability.record(TraceEntry(time.time(),str(req.trace_id),str(req.request_id),"request","failed",int((time.perf_counter()-started)*1000),error_code="cancelled"))
                state.emit(EventType.cancelled,error=AgentError(code=ErrorCode.cancelled,message="request cancelled"))
                # Manager shutdown cancels the worker task itself.  Do not
                # consume that cancellation and leave the worker waiting on an
                # empty queue forever; explicit request cancellation still
                # completes normally without killing its worker.
                if asyncio.current_task() and asyncio.current_task().cancelling():raise
                return
            except TimeoutError:
                observability.record(TraceEntry(time.time(),str(req.trace_id),str(req.request_id),"request","failed",int((time.perf_counter()-started)*1000),error_code="timeout"))
                state.emit(EventType.failed,error=AgentError(code=ErrorCode.timeout,message="request timed out",retryable=False));return
            except ValidationError:
                state.emit(EventType.failed,error=AgentError(code=ErrorCode.validation_failed,message="structured input validation failed",retryable=False));return
            except Exception:
                if attempt<req.max_retries:
                    observability.record(TraceEntry(time.time(),str(req.trace_id),str(req.request_id),"request","retrying",error_code="operation_failed"))
                    state.emit(EventType.retrying,{"attempt":attempt+1});await asyncio.sleep(.05);continue
                observability.record(TraceEntry(time.time(),str(req.trace_id),str(req.request_id),"request","failed",int((time.perf_counter()-started)*1000),error_code="internal_error"))
                state.emit(EventType.failed,error=AgentError(code=ErrorCode.internal_error,message="operation failed",retryable=False));return
