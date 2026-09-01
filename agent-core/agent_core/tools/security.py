from __future__ import annotations

import hashlib,hmac,json,re,secrets,time
from dataclasses import dataclass
from typing import Any


INJECTION_PATTERNS=(
    r"ignore (all |any )?(previous|prior|system) instructions",
    r"reveal (the )?(system prompt|secret|api key|token)",
    r"bypass (confirmation|permission|security)",
    r"developer mode|jailbreak|prompt injection",
    r"忽略.{0,8}(之前|以上|系统).{0,8}(指令|提示)",
    r"泄露.{0,8}(密钥|令牌|系统提示词)",
    r"绕过.{0,8}(确认|权限|安全)",
)
SECRET_PATTERNS=(
    re.compile(r"(?i)(api[_ -]?key|token|password|secret)\s*[:=]\s*[^\s,;]+"),
    re.compile(r"(?i)sk-[a-z0-9_-]{12,}"),
)


def detect_prompt_injection(value:Any)->bool:
    text=json.dumps(value,ensure_ascii=False) if not isinstance(value,str) else value
    lowered=text.lower()
    return any(re.search(pattern,lowered,re.I|re.S) for pattern in INJECTION_PATTERNS)


def redact(value:Any)->Any:
    if isinstance(value,dict):
        return {key:("[REDACTED]" if any(word in key.lower() for word in ("content","body","key","token","password","secret","image")) else redact(item)) for key,item in value.items()}
    if isinstance(value,list):return [redact(item) for item in value]
    if isinstance(value,str):
        result=value
        for pattern in SECRET_PATTERNS:result=pattern.sub("[REDACTED]",result)
        return result[:240]
    return value


@dataclass
class Confirmation:
    digest:str
    expires_at:float
    used:bool=False


class ConfirmationStore:
    def __init__(self,ttl_seconds:int=300)->None:
        self.ttl_seconds=ttl_seconds;self._secret=secrets.token_bytes(32);self._items:dict[str,Confirmation]={}

    @staticmethod
    def _digest(name:str,args:dict)->str:
        raw=json.dumps({"name":name,"arguments":args},ensure_ascii=False,sort_keys=True,separators=(",",":")).encode()
        return hashlib.sha256(raw).hexdigest()

    def issue(self,name:str,args:dict)->str:
        nonce=secrets.token_urlsafe(18);digest=self._digest(name,args)
        signature=hmac.new(self._secret,f"{nonce}:{digest}".encode(),hashlib.sha256).hexdigest()
        token=f"{nonce}.{signature}";self._items[token]=Confirmation(digest,time.monotonic()+self.ttl_seconds);return token

    def consume(self,token:str|None,name:str,args:dict)->bool:
        if not token:return False
        item=self._items.get(token)
        if not item or item.used or item.expires_at<time.monotonic() or not hmac.compare_digest(item.digest,self._digest(name,args)):return False
        item.used=True;return True
