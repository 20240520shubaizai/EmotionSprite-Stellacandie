import os
from pathlib import Path
import pytest
from fastapi.testclient import TestClient
os.environ["AGENT_SESSION_TOKEN"]="test-session-token"
os.environ["AGENT_MAX_CONCURRENCY"]="4"
os.environ["AGENT_QUEUE_SIZE"]="64"
os.environ["AGENT_MODEL_MODE"]="mock"
os.environ["SYNC_DEPLOYMENT_MODE"]="test"
sync_db=Path(__file__).parent/"test_sync.db"
sync_db.unlink(missing_ok=True)
os.environ["SYNC_DATABASE_URL"]=f"sqlite+pysqlite:///{sync_db.as_posix()}"

@pytest.fixture(scope="session")
def client():
    from agent_core.app import app
    from agent_core.sync_models import Base,SessionLocal
    Base.metadata.create_all(SessionLocal.kw["bind"])
    with TestClient(app) as value:
        yield value
