"""initial sync schema"""
from alembic import op
import sqlalchemy as sa
revision="0001";down_revision=None
def upgrade():
    op.create_table("sync_entities",sa.Column("id",sa.BigInteger(),primary_key=True,autoincrement=True),sa.Column("user_id",sa.String(64),nullable=False),sa.Column("entity_type",sa.String(32),nullable=False),sa.Column("entity_uuid",sa.String(64),nullable=False),sa.Column("revision",sa.Integer(),nullable=False),sa.Column("privacy_level",sa.String(16),nullable=False),sa.Column("payload",sa.JSON(),nullable=False),sa.Column("content_hash",sa.String(64),nullable=False),sa.Column("deleted_at",sa.DateTime(timezone=True)),sa.Column("updated_at",sa.DateTime(timezone=True),nullable=False),sa.UniqueConstraint("user_id","entity_type","entity_uuid",name="uq_sync_entity_identity"))
    op.create_table("applied_sync_events",sa.Column("idempotency_key",sa.String(128),primary_key=True),sa.Column("user_id",sa.String(64),nullable=False),sa.Column("applied_at",sa.DateTime(timezone=True),nullable=False))
    op.create_table("sync_conflict_copies",sa.Column("id",sa.BigInteger(),primary_key=True,autoincrement=True),sa.Column("user_id",sa.String(64),nullable=False),sa.Column("entity_type",sa.String(32),nullable=False),sa.Column("entity_uuid",sa.String(64),nullable=False),sa.Column("local_payload",sa.JSON(),nullable=False),sa.Column("cloud_payload",sa.JSON(),nullable=False),sa.Column("status",sa.String(16),nullable=False),sa.Column("created_at",sa.DateTime(timezone=True),nullable=False))
def downgrade():
    op.drop_table("sync_conflict_copies");op.drop_table("applied_sync_events");op.drop_table("sync_entities")
