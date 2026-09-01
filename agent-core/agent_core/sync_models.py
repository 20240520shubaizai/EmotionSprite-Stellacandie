from datetime import datetime, timezone
import os
from sqlalchemy import BigInteger, CheckConstraint, DateTime, Integer, JSON, String, UniqueConstraint, create_engine
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, sessionmaker

class Base(DeclarativeBase): pass
class SyncEntity(Base):
    __tablename__="sync_entities"
    id:Mapped[int]=mapped_column(BigInteger().with_variant(Integer,"sqlite"),primary_key=True,autoincrement=True)
    user_id:Mapped[str]=mapped_column(String(64),index=True);entity_type:Mapped[str]=mapped_column(String(32),index=True);entity_uuid:Mapped[str]=mapped_column(String(64));revision:Mapped[int]=mapped_column(Integer,default=0);privacy_level:Mapped[str]=mapped_column(String(16),default="normal");payload:Mapped[dict]=mapped_column(JSON);content_hash:Mapped[str]=mapped_column(String(64));deleted_at:Mapped[datetime|None]=mapped_column(DateTime(timezone=True),nullable=True);updated_at:Mapped[datetime]=mapped_column(DateTime(timezone=True),default=lambda:datetime.now(timezone.utc))
    __table_args__=(UniqueConstraint("user_id","entity_type","entity_uuid",name="uq_sync_entity_identity"),CheckConstraint("entity_type IN ('settings','pet_state','memory','reminder')",name="ck_sync_entity_type"),CheckConstraint("privacy_level NOT IN ('secret','local_only')",name="ck_sync_privacy"))
class AppliedEvent(Base):
    __tablename__="applied_sync_events"
    idempotency_key:Mapped[str]=mapped_column(String(128),primary_key=True);user_id:Mapped[str]=mapped_column(String(64),index=True);applied_at:Mapped[datetime]=mapped_column(DateTime(timezone=True),default=lambda:datetime.now(timezone.utc))
class ConflictCopy(Base):
    __tablename__="sync_conflict_copies"
    id:Mapped[int]=mapped_column(BigInteger().with_variant(Integer,"sqlite"),primary_key=True,autoincrement=True);user_id:Mapped[str]=mapped_column(String(64),index=True);entity_type:Mapped[str]=mapped_column(String(32));entity_uuid:Mapped[str]=mapped_column(String(64));local_payload:Mapped[dict]=mapped_column(JSON);cloud_payload:Mapped[dict]=mapped_column(JSON);status:Mapped[str]=mapped_column(String(16),default="open");created_at:Mapped[datetime]=mapped_column(DateTime(timezone=True),default=lambda:datetime.now(timezone.utc))
def database_url():
    url=os.getenv("SYNC_DATABASE_URL")
    if not url:raise RuntimeError("SYNC_DATABASE_URL must be provided by the desktop application or sync-server deployment")
    return url
def make_engine(url=None):return create_engine(url or database_url(),pool_pre_ping=True)
SessionLocal=sessionmaker(bind=make_engine(),expire_on_commit=False)
