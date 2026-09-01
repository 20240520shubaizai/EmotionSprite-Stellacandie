from __future__ import annotations

from collections import Counter
from datetime import datetime,timezone
import math
import re

from .index import DerivedVectorIndex
from .models import FactType,PrivacyLevel,RetrievalRequest,RetrievalResult,SourceType


TERMINAL={"archived","completed","cancelled","expired","deleted","delivered"}
STOP_TOKENS={"什么","是否","是不是","我的","自己","说过","用户","之前","最近","怎么","时候","有什","什么"}


def tokens(text:str)->list[str]:
    chunks=re.findall(r"[\u4e00-\u9fff]+|[a-z0-9]+",text.lower());out=[]
    for chunk in chunks:
        if re.fullmatch(r"[\u4e00-\u9fff]+",chunk):out.extend(chunk);out.extend(chunk[i:i+2] for i in range(len(chunk)-1))
        else:out.append(chunk)
    return [item for item in out if item not in STOP_TOKENS and len(item)>1]


class HybridRetriever:
    def __init__(self,index:DerivedVectorIndex)->None:self.index=index
    def _eligible(self,row:dict,request:RetrievalRequest)->bool:
        now=request.now if request.now.tzinfo else request.now.replace(tzinfo=timezone.utc)
        if row["privacy_level"]==PrivacyLevel.secret.value and not request.authorize_secret:return False
        if row["fact_type"]==FactType.model_inference.value and not request.include_model_inference:return False
        if row["expires_at"] and datetime.fromisoformat(row["expires_at"])<=now:return False
        if row["source_type"]==SourceType.event.value and row["status"] in TERMINAL:return False
        if request.proactive and (not row["proactive_allowed"] or row["status"] in TERMINAL):return False
        return row["status"] not in {"deleted","cancelled","expired"}
    def retrieve(self,request:RetrievalRequest,hybrid:bool=True)->list[RetrievalResult]:
        rows=[row for row in self.index.rows() if self._eligible(row,request)]
        if not rows:return []
        by_id={row["id"]:row for row in rows};query_tokens=tokens(request.query);documents={row["id"]:tokens(row["subject"]+" "+row["content"]) for row in rows}
        avgdl=sum(map(len,documents.values()))/max(1,len(documents));df=Counter(token for token in set(query_tokens) for doc in documents.values() if token in doc)
        lexical=[]
        for doc_id,words in documents.items():
            counts=Counter(words);score=0.0
            for token in query_tokens:
                if not counts[token]:continue
                idf=math.log(1+(len(documents)-df[token]+.5)/(df[token]+.5));tf=counts[token]
                score+=idf*tf*2.2/(tf+1.2*(.25+.75*len(words)/max(avgdl,1)))
            if score>0:lexical.append((doc_id,score))
        lexical.sort(key=lambda item:item[1],reverse=True)
        semantic=[]
        if hybrid:
            query_vector=self.index.provider.embed_query(request.query)
            semantic=[(doc_id,1-distance) for doc_id,distance in self.index.vector_search(query_vector,max(request.limit*8,40)) if doc_id in by_id]
        ranks:dict[int,float]={};reasons:dict[int,list[str]]={}
        for name,ranking in (("BM25关键词匹配",lexical),("中文语义相似",semantic)):
            for rank,(doc_id,_) in enumerate(ranking,1):ranks[doc_id]=ranks.get(doc_id,0)+1/(60+rank);reasons.setdefault(doc_id,[]).append(name)
        if not hybrid:ranks={doc_id:score for doc_id,score in lexical}
        for doc_id in list(ranks):
            row=by_id[doc_id];age=max(0,(request.now.date()-datetime.fromisoformat(row["recorded_at"]).date()).days)
            policy=(row["importance"]/100)*.001+row["confidence"]*.002+math.log1p(row["use_count"])*.0005+.0005/(1+age/365)
            if row["source_type"]==SourceType.personality_bible.value:policy+=.003;reasons[doc_id].append("人格圣经最高约束")
            reminder_query=bool(re.search(r"提醒|待办|别忘|记得|到期",request.query))
            if row["source_type"]==SourceType.reminder.value and row["explicit_request"]:
                if reminder_query:policy+=.015
                reasons[doc_id].append("明确提醒独立优先级")
            if not row["proactive_allowed"]:reasons[doc_id].append("禁止主动提起")
            ranks[doc_id]+=policy
        ordered=sorted(ranks,key=ranks.get,reverse=True)[:request.limit];results=[]
        for doc_id in ordered:
            row=by_id[doc_id]
            results.append(RetrievalResult(record_id=row["record_id"],source_type=row["source_type"],fact_type=row["fact_type"],
                subject=row["subject"],content=row["content"],recorded_at=datetime.fromisoformat(row["recorded_at"]),confidence=row["confidence"],
                score=round(ranks[doc_id],6),reasons=reasons[doc_id],privacy_level=row["privacy_level"]))
        return results
