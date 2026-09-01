from __future__ import annotations

import os
import re
from pathlib import Path
from typing import Any

from .embeddings import EmbeddingProvider,create_embedding_provider
from .index import DerivedVectorIndex
from .models import MemoryDocument,RetrievalRequest
from .retriever import HybridRetriever


def default_index_path()->Path:
    root=Path(os.getenv("LOCALAPPDATA",Path.home()))/"EmotionSprite"/"Stellacandie"
    return Path(os.getenv("AGENT_RAG_DB",root/"agent_rag_derived.sqlite3"))


class RagService:
    def __init__(self,path:Path|None=None,provider:EmbeddingProvider|None=None)->None:
        cache=Path(os.getenv("FASTEMBED_CACHE_PATH",Path(__file__).resolve().parents[2]/"model-cache"))
        self.provider=provider or create_embedding_provider(cache);self.index=DerivedVectorIndex(path or default_index_path(),self.provider)
        self.retriever=HybridRetriever(self.index)
    def rebuild(self,raw:list[dict[str,Any]])->dict:return self.index.rebuild([MemoryDocument.model_validate(item) for item in raw])
    def upsert(self,raw:dict[str,Any])->dict:return self.index.upsert(MemoryDocument.model_validate(raw))
    def delete(self,record_id:str,revision:int)->dict:return self.index.delete(record_id,revision)
    def retrieve(self,raw:dict[str,Any],hybrid:bool=True)->dict:
        request=RetrievalRequest.model_validate(raw);results=self.retriever.retrieve(request,hybrid)
        return {"results":[item.model_dump(mode="json") for item in results],"trace":[item.trace_view() for item in results],
                "provider":self.provider.fingerprint,"strategy":"rrf_bm25_vector" if hybrid else "bm25_baseline"}
    def retrieve_v2(self,raw:dict[str,Any])->dict:
        request=RetrievalRequest.model_validate(raw);candidates=self.retriever.retrieve(request,True)
        unsupported_probe=bool(re.search(r"没说过|从没|银行卡|密码|住在|身份证|住址",request.query))
        # Conservative rejection prevents weak semantic neighbours becoming invented facts.
        accepted=[] if unsupported_probe else [item for item in candidates if item.score>=.015 or item.source_type.value=="personality_bible"]
        caps={"personality_bible":1,"user_memory":3,"reminder":2,"event":2,"shared_experience":1};used={};results=[]
        for item in accepted:
            key=item.source_type.value
            if used.get(key,0)>=caps.get(key,1):continue
            used[key]=used.get(key,0)+1;results.append(item)
            if len(results)>=request.limit:break
        return {"schema_version":"memory_retrieve_v2","results":[item.model_dump(mode="json") for item in results],
                "trace":[item.trace_view() for item in results],"rejected_count":len(candidates)-len(results),
                "provider":self.provider.fingerprint,"strategy":"rrf_bm25_vector_threshold_rerank"}
