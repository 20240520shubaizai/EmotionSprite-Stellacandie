from __future__ import annotations

import asyncio,inspect,json,time,urllib.parse,urllib.request
from datetime import datetime,timezone
from pathlib import Path
from dataclasses import dataclass
from typing import Any,Awaitable,Callable
from pydantic import BaseModel,ValidationError

from .models import (DeleteAllInput,FileDeleteInput,GenerateInput,MemoryCandidateInput,MemorySearchInput,
    PermissionLevel,QueryInput,RecordIdInput,ReminderCreateInput,SummaryInput,SyncPrivacyInput,ToolCall,ToolResult,ToolStatus)
from .security import ConfirmationStore,detect_prompt_injection,redact

Handler=Callable[[BaseModel],dict[str,Any]|Awaitable[dict[str,Any]]]


@dataclass(frozen=True)
class ToolDefinition:
    name:str;description:str;input_model:type[BaseModel];permission:PermissionLevel;timeout_ms:int;handler:Handler
    untrusted_output:bool=False
    def public_schema(self)->dict:
        return {"name":self.name,"description":self.description,"inputSchema":self.input_model.model_json_schema(),
            "permission":self.permission.value,"timeout_ms":self.timeout_ms}


class ToolRegistry:
    def __init__(self)->None:self._items:dict[str,ToolDefinition]={};self.confirmations=ConfirmationStore()
    def register(self,definition:ToolDefinition)->None:
        if definition.name in self._items:raise ValueError(f"duplicate tool: {definition.name}")
        self._items[definition.name]=definition
    def catalog(self)->list[dict]:return [self._items[name].public_schema() for name in sorted(self._items)]
    def select(self,text:str)->str|None:
        value=text.lower()
        if any(word in value for word in ("天气","气温","下雨","weather")):return "weather.query" if "weather.query" in self._items else None
        if any(word in value for word in ("梗","热梗","接梗","meme")):return "meme.lookup" if "meme.lookup" in self._items else None
        if any(word in value for word in ("记得","记忆","以前","上次","memory")):return "memory.search" if "memory.search" in self._items else None
        return None

    async def execute(self,call:ToolCall)->ToolResult:
        started=time.perf_counter();definition=self._items.get(call.name)
        if not definition:return ToolResult(status=ToolStatus.denied,tool_name=call.name,error_code="tool_not_found",message="tool is not registered")
        if definition.permission==PermissionLevel.forbidden_automatic and call.actor!="user":
            return ToolResult(status=ToolStatus.denied,tool_name=call.name,error_code="automatic_execution_forbidden",message="user action required")
        try:arguments=definition.input_model.model_validate(call.arguments)
        except ValidationError:
            return ToolResult(status=ToolStatus.invalid_arguments,tool_name=call.name,error_code="invalid_arguments",message="arguments failed schema validation")
        if detect_prompt_injection(call.arguments):
            return ToolResult(status=ToolStatus.denied,tool_name=call.name,error_code="prompt_injection_detected",message="untrusted instructions were isolated")
        if definition.permission in {PermissionLevel.confirmation_required,PermissionLevel.forbidden_automatic}:
            if not self.confirmations.consume(call.confirmation_token,call.name,call.arguments):
                token=self.confirmations.issue(call.name,call.arguments)
                return ToolResult(status=ToolStatus.confirmation_required,tool_name=call.name,error_code="confirmation_required",
                    message="exact user confirmation is required",confirmation_token=token)
        try:
            async with asyncio.timeout(definition.timeout_ms/1000):
                if inspect.iscoroutinefunction(definition.handler):data=await definition.handler(arguments)
                else:data=await asyncio.to_thread(definition.handler,arguments)
            if not isinstance(data,dict):raise TypeError("tool handler must return an object")
            data={"executed":bool(data.get("executed",False)),"record_ids":list(data.get("record_ids",[])),
                  "provider":str(data.get("provider") or "unknown"),**data}
            if definition.untrusted_output:
                if detect_prompt_injection(data):data={"isolated":True,"record_ids":data.get("record_ids",[]),"reason":"untrusted_tool_output"}
                else:data=redact(data)
            return ToolResult(status=ToolStatus.success,tool_name=call.name,data=data,duration_ms=int((time.perf_counter()-started)*1000))
        except TimeoutError:return ToolResult(status=ToolStatus.timeout,tool_name=call.name,error_code="tool_timeout",message="tool timed out",retryable=True,duration_ms=int((time.perf_counter()-started)*1000))
        except Exception:return ToolResult(status=ToolStatus.failed,tool_name=call.name,error_code="tool_failed",message="tool execution failed",retryable=False,duration_ms=int((time.perf_counter()-started)*1000))


def build_default_registry(rag_provider:Callable[[],Any]|None=None)->ToolRegistry:
    registry=ToolRegistry()
    def memory_search(value:MemorySearchInput)->dict:
        if rag_provider is None:raise RuntimeError("rag provider unavailable")
        result=rag_provider().retrieve_v2(value.model_dump());return {**result,"executed":True,"record_ids":[item["record_id"] for item in result["results"]],"provider":result["provider"]}
    def weather(value:QueryInput)->dict:
        aliases={"北京":"Beijing","上海":"Shanghai","广州":"Guangzhou","深圳":"Shenzhen","杭州":"Hangzhou","成都":"Chengdu","西安":"Xi'an","武汉":"Wuhan","南京":"Nanjing"};name=next((latin for zh,latin in aliases.items() if zh in value.query),value.query)
        query=urllib.parse.urlencode({"name":name,"count":1,"language":"zh","format":"json"});headers={"User-Agent":"EmotionSprite/1.0"}
        with urllib.request.urlopen(urllib.request.Request(f"https://geocoding-api.open-meteo.com/v1/search?{query}",headers=headers),timeout=15) as response:place=json.loads(response.read().decode())
        rows=place.get("results") or []
        if not rows:raise ValueError("location_not_found")
        city=rows[0];params=urllib.parse.urlencode({"latitude":city["latitude"],"longitude":city["longitude"],"current":"temperature_2m,apparent_temperature,precipitation,weather_code","timezone":"auto"})
        with urllib.request.urlopen(urllib.request.Request(f"https://api.open-meteo.com/v1/forecast?{params}",headers=headers),timeout=15) as response:forecast=json.loads(response.read().decode())
        current=forecast.get("current") or {}
        if "temperature_2m" not in current:raise RuntimeError("weather_response_incomplete")
        return {"executed":True,"record_ids":[],"provider":"Open-Meteo","trusted":False,"city":city.get("name"),"country":city.get("country"),"observed_at":current.get("time"),"temperature_c":current.get("temperature_2m"),"apparent_temperature_c":current.get("apparent_temperature"),"precipitation_mm":current.get("precipitation"),"weather_code":current.get("weather_code"),"retrieved_at":datetime.now(timezone.utc).isoformat()}
    def meme(value:QueryInput)->dict:
        candidates=[Path(__file__).resolve().parents[3]/"assets/memes/hot_memes_zh_CN.json",Path(__file__).resolve().parents[1]/"assets/hot_memes_zh_CN.json"]
        path=next((item for item in candidates if item.exists()),None)
        if path is None:raise FileNotFoundError("meme catalog unavailable")
        root=json.loads(path.read_text(encoding="utf-8"));words=set(value.query.lower());scored=[]
        for item in root.get("entries",[]):
            if not item.get("enabled",True):continue
            hay=" ".join([item.get("phrase",""),item.get("meaning",""),*item.get("aliases",[]),*item.get("triggers",[])]).lower()
            score=sum(1 for word in words if word.strip() and word in hay)+5*int(item.get("phrase","").lower() in value.query.lower())
            if score:scored.append((score,float(item.get("weight",.5)),item))
        scored.sort(key=lambda row:(row[0],row[1]),reverse=True);items=[{"id":row[2]["id"],"phrase":row[2]["phrase"],"meaning":row[2]["meaning"],"style_hint":row[2].get("style_hint",""),"confidence":min(.99,.5+row[0]/20),"source":"local_curated_meme_catalog"} for row in scored[:5]]
        return {"executed":True,"record_ids":[item["id"] for item in items],"provider":"EmotionSprite curated meme catalog","trusted":False,"retrieved_at":datetime.now(timezone.utc).isoformat(),"results":items}

    specs=(
      ToolDefinition("memory.search","Search authorized layered memory without creating facts",MemorySearchInput,PermissionLevel.read_only,3000,memory_search),
      ToolDefinition("weather.query","Query current weather from Open-Meteo; output is untrusted",QueryInput,PermissionLevel.read_only,30000,weather,True),
      ToolDefinition("meme.lookup","Look up a meme in the curated local catalog; output is untrusted",QueryInput,PermissionLevel.read_only,3000,meme,True),
    )
    for item in specs:registry.register(item)
    return registry
