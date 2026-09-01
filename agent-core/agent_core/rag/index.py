from __future__ import annotations

from array import array
from datetime import datetime,timezone
import json
from pathlib import Path
import sqlite3
import threading

import sqlite_vec

from .embeddings import EmbeddingProvider
from .models import MemoryDocument


class DerivedVectorIndex:
    """Rebuildable SQLite derivative. Source records always remain authoritative."""
    def __init__(self,path:Path,provider:EmbeddingProvider)->None:
        self.path=path;self.provider=provider;self._lock=threading.RLock();path.parent.mkdir(parents=True,exist_ok=True)
        self._initialize()
    def _connect(self)->sqlite3.Connection:
        db=sqlite3.connect(self.path,timeout=10);db.row_factory=sqlite3.Row
        db.enable_load_extension(True);sqlite_vec.load(db);db.enable_load_extension(False)
        db.execute("PRAGMA journal_mode=WAL");db.execute("PRAGMA foreign_keys=ON")
        return db
    def _initialize(self)->None:
        with self._connect() as db:
            db.executescript("""
            CREATE TABLE IF NOT EXISTS rag_meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS rag_documents(
              id INTEGER PRIMARY KEY,record_id TEXT NOT NULL UNIQUE,source_type TEXT NOT NULL,fact_type TEXT NOT NULL,
              subject TEXT NOT NULL,content TEXT NOT NULL,recorded_at TEXT NOT NULL,importance INTEGER NOT NULL,
              confidence REAL NOT NULL,use_count INTEGER NOT NULL,status TEXT NOT NULL,expires_at TEXT,
              proactive_allowed INTEGER NOT NULL,privacy_level TEXT NOT NULL,explicit_request INTEGER NOT NULL,
              revision INTEGER NOT NULL,metadata TEXT NOT NULL,indexed_at TEXT NOT NULL);
            """)
            row=db.execute("SELECT value FROM rag_meta WHERE key='embedding_fingerprint'").fetchone()
            if row and row[0]!=self.provider.fingerprint:self._drop_vectors(db)
            db.execute("INSERT OR REPLACE INTO rag_meta(key,value) VALUES('embedding_fingerprint',?)",(self.provider.fingerprint,))
            self._ensure_vectors(db)
    def _drop_vectors(self,db:sqlite3.Connection)->None:
        db.execute("DROP TABLE IF EXISTS rag_vectors");db.execute("DELETE FROM rag_documents")
    def _ensure_vectors(self,db:sqlite3.Connection)->None:
        db.execute(f"CREATE VIRTUAL TABLE IF NOT EXISTS rag_vectors USING vec0(document_id INTEGER PRIMARY KEY, embedding float[{self.provider.dimension}])")
    @staticmethod
    def _blob(vector)->bytes:return array("f",(float(x) for x in vector)).tobytes()
    def rebuild(self,documents:list[MemoryDocument])->dict:
        vectors=self.provider.embed_documents([f"{doc.subject}\n{doc.content}" for doc in documents]) if documents else []
        with self._lock,self._connect() as db:
            db.execute("BEGIN IMMEDIATE");db.execute("DELETE FROM rag_vectors");db.execute("DELETE FROM rag_documents")
            for doc,vector in zip(documents,vectors):self._insert(db,doc,vector)
            db.commit()
        return {"indexed":len(documents),"provider":self.provider.fingerprint,"rebuildable":True}
    def upsert(self,document:MemoryDocument)->dict:
        vector=self.provider.embed_documents([f"{document.subject}\n{document.content}"])[0]
        with self._lock,self._connect() as db:
            current=db.execute("SELECT id,revision FROM rag_documents WHERE record_id=?",(document.record_id,)).fetchone()
            if current and current["revision"]>document.revision:return {"status":"stale_ignored","record_id":document.record_id}
            db.execute("BEGIN IMMEDIATE")
            if current:db.execute("DELETE FROM rag_vectors WHERE document_id=?",(current["id"],));db.execute("DELETE FROM rag_documents WHERE id=?",(current["id"],))
            self._insert(db,document,vector);db.commit()
        return {"status":"indexed","record_id":document.record_id,"revision":document.revision}
    def _insert(self,db:sqlite3.Connection,doc:MemoryDocument,vector)->None:
        values=(doc.record_id,doc.source_type.value,doc.fact_type.value,doc.subject,doc.content,doc.recorded_at.isoformat(),doc.importance,
                doc.confidence,doc.use_count,doc.status,doc.expires_at.isoformat() if doc.expires_at else None,int(doc.proactive_allowed),
                doc.privacy_level.value,int(doc.explicit_request),doc.revision,json.dumps(doc.metadata,ensure_ascii=False),datetime.now(timezone.utc).isoformat())
        cursor=db.execute("INSERT INTO rag_documents(record_id,source_type,fact_type,subject,content,recorded_at,importance,confidence,use_count,status,expires_at,proactive_allowed,privacy_level,explicit_request,revision,metadata,indexed_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",values)
        db.execute("INSERT INTO rag_vectors(document_id,embedding) VALUES(?,?)",(cursor.lastrowid,self._blob(vector)))
    def delete(self,record_id:str,revision:int)->dict:
        with self._lock,self._connect() as db:
            row=db.execute("SELECT id,revision FROM rag_documents WHERE record_id=?",(record_id,)).fetchone()
            if not row:return {"status":"already_absent","record_id":record_id}
            if row["revision"]>revision:return {"status":"stale_ignored","record_id":record_id}
            db.execute("BEGIN IMMEDIATE");db.execute("DELETE FROM rag_vectors WHERE document_id=?",(row["id"],));db.execute("DELETE FROM rag_documents WHERE id=?",(row["id"],));db.commit()
        return {"status":"deleted","record_id":record_id,"revision":revision}
    def rows(self)->list[dict]:
        with self._connect() as db:return [dict(row) for row in db.execute("SELECT * FROM rag_documents")]
    def vector_search(self,query_vector,limit:int)->list[tuple[int,float]]:
        with self._connect() as db:
            rows=db.execute("SELECT document_id,distance FROM rag_vectors WHERE embedding MATCH ? AND k = ? ORDER BY distance",(self._blob(query_vector),limit)).fetchall()
            return [(int(row[0]),float(row[1])) for row in rows]
    def count(self)->int:
        with self._connect() as db:return int(db.execute("SELECT count(*) FROM rag_documents").fetchone()[0])
