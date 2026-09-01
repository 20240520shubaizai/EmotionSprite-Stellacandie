from __future__ import annotations

from collections.abc import Iterable
import hashlib
import math
import os
from pathlib import Path
import re

import numpy as np


class EmbeddingProvider:
    dimension:int
    fingerprint:str
    def embed_documents(self,texts:list[str])->list[np.ndarray]:raise NotImplementedError
    def embed_query(self,text:str)->np.ndarray:raise NotImplementedError


class SemanticHashEmbedding(EmbeddingProvider):
    """Offline deterministic fallback. It is a derived feature vector, never a fact store."""
    dimension=512
    fingerprint="semantic-hash-v1-512"
    _concepts={
        "睡眠":"睡觉 休息 熬夜 困 枕头 床 失眠", "工作":"上班 公司 会议 开会 项目 同事",
        "宠物":"猫 猫咪 橘猫 小猫 狗 动物", "食物":"吃 美食 蛋糕 抹茶 火锅 零食",
        "运动":"跑步 锻炼 健身 散步", "朋友":"朋友 同学 同事 伙伴", "生日":"生日 纪念日 庆祝",
        "游戏":"游戏 王者荣耀 排位 开黑", "提醒":"提醒 通知 别忘 记得 待办"
    }
    def _tokens(self,text:str)->Iterable[str]:
        normalized=text.lower()
        for concept,words in self._concepts.items():
            if concept in normalized or any(word in normalized for word in words.split()):
                for _ in range(8):yield "concept:"+concept
        chunks=re.findall(r"[\u4e00-\u9fff]+|[a-z0-9]+",normalized)
        for chunk in chunks:
            if re.fullmatch(r"[\u4e00-\u9fff]+",chunk):
                yield from chunk
                yield from (chunk[i:i+2] for i in range(len(chunk)-1))
            else:yield chunk
    def _embed(self,text:str)->np.ndarray:
        vector=np.zeros(self.dimension,dtype=np.float32)
        for token in self._tokens(text):
            digest=hashlib.blake2b(token.encode("utf-8"),digest_size=8).digest()
            value=int.from_bytes(digest,"little");index=value%self.dimension
            vector[index]+=1.0 if value&(1<<63) else -1.0
        norm=float(np.linalg.norm(vector))
        return vector/norm if norm else vector
    def embed_documents(self,texts:list[str])->list[np.ndarray]:return [self._embed(text) for text in texts]
    def embed_query(self,text:str)->np.ndarray:return self._embed(text)


class FastEmbedProvider(EmbeddingProvider):
    dimension=512
    model_name="BAAI/bge-small-zh-v1.5"
    fingerprint="fastembed-bge-small-zh-v1.5-512"
    def __init__(self,cache_dir:Path|None=None)->None:
        from fastembed import TextEmbedding
        cache=cache_dir or Path(os.getenv("FASTEMBED_CACHE_PATH",Path.home()/".cache"/"emotion-sprite-models"))
        cache.mkdir(parents=True,exist_ok=True)
        self._model=TextEmbedding(self.model_name,cache_dir=str(cache),threads=max(1,min(4,os.cpu_count() or 1)))
    def embed_documents(self,texts:list[str])->list[np.ndarray]:
        return [np.asarray(item,dtype=np.float32) for item in self._model.passage_embed(texts)]
    def embed_query(self,text:str)->np.ndarray:
        return np.asarray(next(iter(self._model.query_embed([text]))),dtype=np.float32)


def create_embedding_provider(cache_dir:Path|None=None)->EmbeddingProvider:
    mode=os.getenv("RAG_EMBEDDING_MODE","auto").lower()
    if mode=="hash":return SemanticHashEmbedding()
    try:return FastEmbedProvider(cache_dir)
    except Exception:
        if mode=="fastembed":raise
        return SemanticHashEmbedding()
