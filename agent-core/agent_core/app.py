from __future__ import annotations

import asyncio
from contextlib import asynccontextmanager
import os
from uuid import UUID
from datetime import datetime,timezone

from fastapi import Depends, FastAPI, Header, HTTPException, Query, status
from fastapi.responses import StreamingResponse
from sqlalchemy.exc import SQLAlchemyError

from . import __version__
from .logging_config import configure_logging
from .schemas import AgentRequest, CapabilityResponse, HealthResponse, TaskAccepted,SyncBatch,TransientImageRequest,ConflictResolution,CloudDeleteRequest
from .task_manager import TaskManager
from .sync_models import Base,SessionLocal,ConflictCopy,SyncEntity,AppliedEvent
from .sync_service import apply_event,resolve_conflict
import base64,hashlib

PROTOCOL_VERSION="1.0"
CLIENT_MAJOR=1
manager=TaskManager(concurrency=int(os.getenv("AGENT_MAX_CONCURRENCY","4")),queue_size=int(os.getenv("AGENT_QUEUE_SIZE","64")))


def authorize(x_session_token: str = Header(default="")) -> None:
    expected=os.getenv("AGENT_SESSION_TOKEN","")
    if not expected or x_session_token != expected: raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED,detail="unauthorized")

def cloud_sync_enabled() -> bool:
    mode=os.getenv("SYNC_DEPLOYMENT_MODE","local_development")
    dialect=SessionLocal.kw["bind"].dialect.name
    return (mode=="cloud_mysql" and dialect=="mysql") or mode=="test"

def require_cloud_sync() -> None:
    if not cloud_sync_enabled():raise HTTPException(status_code=503,detail="cloud_sync_not_configured")


@asynccontextmanager
async def lifespan(_: FastAPI):
    configure_logging()
    engine=SessionLocal.kw["bind"]
    mode=os.getenv("SYNC_DEPLOYMENT_MODE","local_development")
    if mode=="cloud_mysql" and engine.dialect.name!="mysql":raise RuntimeError("cloud_mysql requires a MySQL SYNC_DATABASE_URL")
    if mode=="cloud_mysql" and "ssl_ca=" not in os.getenv("SYNC_DATABASE_URL","") and os.getenv("SYNC_ALLOW_INSECURE_MYSQL")!="1":raise RuntimeError("cloud_mysql requires TLS (ssl_ca)")
    if engine.dialect.name=="sqlite" and mode in {"local_development","test"}: Base.metadata.create_all(engine)
    await manager.start();yield;await manager.stop()


app=FastAPI(title="Emotion Sprite Agent Core",version=__version__,lifespan=lifespan)


@app.get("/health",response_model=HealthResponse)
async def health() -> HealthResponse:
    return HealthResponse(version=__version__,protocol_version=PROTOCOL_VERSION)


@app.get("/v1/capabilities",response_model=CapabilityResponse,dependencies=[Depends(authorize)])
async def capabilities(x_client_version: str = Header(default="1.0.0")) -> CapabilityResponse:
    try: major=int(x_client_version.split(".")[0])
    except ValueError: major=-1
    if major != CLIENT_MAJOR: raise HTTPException(status_code=426,detail="incompatible client version")
    return CapabilityResponse(protocol_version=PROTOCOL_VERSION,min_client_version="1.0.0",max_client_major=1,
                              capabilities=["sse","resume","cancel","deduplicate","echo","ping","transient_vision","conversation_v2","memory_retrieve_v2","mutation_commit_v1","mcp_tools_v1","safe_trace_v1"]+(["privacy_sync"] if cloud_sync_enabled() else []),
                              limits={"concurrency":manager.concurrency,"queue_size":manager.queue_size,"max_retries":2})


@app.post("/v1/tasks",response_model=TaskAccepted,status_code=202,dependencies=[Depends(authorize)])
async def create_task(request: AgentRequest) -> TaskAccepted:
    try: state,duplicate=await manager.submit(request)
    except asyncio.QueueFull: raise HTTPException(status_code=429,detail="task queue full")
    return TaskAccepted(request_id=state.request.request_id,trace_id=state.request.trace_id,
                        stream_url=f"/v1/tasks/{state.request.request_id}/events",duplicate=duplicate)


@app.get("/v1/tasks/{request_id}/events",dependencies=[Depends(authorize)])
async def events(request_id: UUID,last_event_id: int=Query(default=0,ge=0)) -> StreamingResponse:
    if request_id not in manager.tasks: raise HTTPException(status_code=404,detail="request not found")
    return StreamingResponse(manager.stream(request_id,last_event_id),media_type="text/event-stream",
                             headers={"Cache-Control":"no-cache","X-Accel-Buffering":"no"})


@app.delete("/v1/tasks/{request_id}",status_code=202,dependencies=[Depends(authorize)])
async def cancel(request_id: UUID) -> dict[str,bool]:
    return {"cancelled":await manager.cancel(request_id)}

@app.post("/v1/sync/batch",dependencies=[Depends(authorize),Depends(require_cloud_sync)])
async def sync_batch(batch:SyncBatch)->dict:
    try:
        results=[]
        with SessionLocal() as db:
            for event in batch.events:results.append(apply_event(db,event.model_dump()))
        return {"results":results}
    except SQLAlchemyError:raise HTTPException(status_code=503,detail="sync_backend_unavailable")

@app.get("/v1/sync/conflicts",dependencies=[Depends(authorize),Depends(require_cloud_sync)])
async def sync_conflicts(user_id:str=Query(default="local-single-user",max_length=64))->dict:
    try:
        with SessionLocal() as db:
            rows=db.query(ConflictCopy).filter_by(user_id=user_id,status="open").order_by(ConflictCopy.id).limit(100).all()
            return {"conflicts":[{"id":row.id,"entity_type":row.entity_type,"entity_uuid":row.entity_uuid,"created_at":row.created_at.isoformat()} for row in rows]}
    except SQLAlchemyError:raise HTTPException(status_code=503,detail="sync_backend_unavailable")

@app.post("/v1/sync/conflicts/{conflict_id}/resolve",dependencies=[Depends(authorize),Depends(require_cloud_sync)])
async def sync_conflict_resolve(conflict_id:int,resolution:ConflictResolution)->dict:
    try:
        with SessionLocal() as db:
            conflict=db.get(ConflictCopy,conflict_id)
            if not conflict or conflict.user_id!=resolution.user_id:raise HTTPException(status_code=404,detail="open conflict not found")
            result=resolve_conflict(db,conflict_id,resolution.choice)
            if not result:raise HTTPException(status_code=404,detail="open conflict not found")
            return result
    except SQLAlchemyError:raise HTTPException(status_code=503,detail="sync_backend_unavailable")

@app.get("/v1/sync/export",dependencies=[Depends(authorize),Depends(require_cloud_sync)])
async def sync_export(user_id:str=Query(default="local-single-user",max_length=64))->dict:
    try:
        with SessionLocal() as db:
            rows=db.query(SyncEntity).filter_by(user_id=user_id).order_by(SyncEntity.entity_type,SyncEntity.entity_uuid).all()
            return {"user_id":user_id,"exported_at":datetime.now(timezone.utc).isoformat(),"entities":[{"entity_type":r.entity_type,"entity_uuid":r.entity_uuid,"revision":r.revision,"privacy_level":r.privacy_level,"payload":r.payload,"deleted_at":r.deleted_at.isoformat() if r.deleted_at else None} for r in rows]}
    except SQLAlchemyError:raise HTTPException(status_code=503,detail="sync_backend_unavailable")

@app.post("/v1/sync/delete-cloud-data",dependencies=[Depends(authorize),Depends(require_cloud_sync)])
async def sync_delete_cloud_data(request:CloudDeleteRequest)->dict:
    if request.confirmation!="DELETE CLOUD DATA":raise HTTPException(status_code=400,detail="confirmation_required")
    try:
        with SessionLocal() as db:
            entities=db.query(SyncEntity).filter_by(user_id=request.user_id).delete(synchronize_session=False)
            db.query(ConflictCopy).filter_by(user_id=request.user_id).delete(synchronize_session=False)
            db.query(AppliedEvent).filter_by(user_id=request.user_id).delete(synchronize_session=False)
            db.commit();return {"deleted_count":entities,"local_data_untouched":True}
    except SQLAlchemyError:raise HTTPException(status_code=503,detail="sync_backend_unavailable")

@app.post("/v1/vision/transient",dependencies=[Depends(authorize)])
async def transient_image(request:TransientImageRequest)->dict:
    buffer=bytearray(base64.b64decode(request.image_base64,validate=True));digest=hashlib.sha256(buffer).hexdigest();size=len(buffer)
    try:return {"accepted":True,"size":size,"sha256":digest,"persisted":False}
    finally:
        for i in range(len(buffer)):buffer[i]=0
