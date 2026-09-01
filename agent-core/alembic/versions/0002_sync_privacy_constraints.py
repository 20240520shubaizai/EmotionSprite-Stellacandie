"""server side sync allowlist and privacy constraints"""
from alembic import op

revision="0002"
down_revision="0001"

def upgrade():
    op.create_check_constraint("ck_sync_entity_type","sync_entities","entity_type IN ('settings','pet_state','memory','reminder')")
    op.create_check_constraint("ck_sync_privacy","sync_entities","privacy_level NOT IN ('secret','local_only')")
    op.create_index("idx_sync_entity_user_type","sync_entities",["user_id","entity_type"])
    op.execute("""CREATE TRIGGER trg_sync_entities_privacy_insert BEFORE INSERT ON sync_entities FOR EACH ROW BEGIN IF NEW.entity_type NOT IN ('settings','pet_state','memory','reminder') OR NEW.privacy_level IN ('secret','local_only') THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='forbidden sync entity'; END IF; END""")
    op.execute("""CREATE TRIGGER trg_sync_entities_privacy_update BEFORE UPDATE ON sync_entities FOR EACH ROW BEGIN IF NEW.entity_type NOT IN ('settings','pet_state','memory','reminder') OR NEW.privacy_level IN ('secret','local_only') THEN SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='forbidden sync entity'; END IF; END""")

def downgrade():
    op.execute("DROP TRIGGER IF EXISTS trg_sync_entities_privacy_update")
    op.execute("DROP TRIGGER IF EXISTS trg_sync_entities_privacy_insert")
    op.drop_index("idx_sync_entity_user_type",table_name="sync_entities")
    op.drop_constraint("ck_sync_privacy","sync_entities",type_="check")
    op.drop_constraint("ck_sync_entity_type","sync_entities",type_="check")
