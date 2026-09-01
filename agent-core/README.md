# Emotion Sprite Agent Core

本地 Agent 通信边界由FastAPI/SSE承载，SQLAlchemy、Alembic和MySQL用于按授权同步。正式Qt聊天现已通过`conversation_v2`优先进入Agent和真实DeepSeek结构化链路；旧`AiService`仅作为用户可见的回退模式。

## 协议

- `GET /health`：无需令牌的存活探测。
- `GET /v1/capabilities`：鉴权、版本协商和能力发现。
- `POST /v1/tasks`：创建或按`request_id`去重任务。
- `GET /v1/tasks/{id}/events?last_event_id=N`：SSE事件流及断线续传。
- `DELETE /v1/tasks/{id}`：取消任务。
- `POST /v1/sync/batch`：接收经过本地隐私授权的幂等 Outbox 事件。
- `POST /v1/vision/transient`：仅在内存中处理图片字节，请求结束后销毁缓冲区。
- `POST /v1/tasks` 的 `conversation_v2`：执行 LangGraph 结构化会话链路；输入包含时间、最近对话、隐私许可、状态和附件元数据，结果包含请求/追踪标识、节点轨迹、结构化回复及状态影响。

所有受保护接口必须携带随机的`X-Session-Token`；客户端版本通过`X-Client-Version`协商。日志只记录请求ID、追踪ID和操作名，不记录正文、密钥或秘密记忆。

## 开发验证

```powershell
.\.venv\Scripts\python.exe -m pytest -q
```

依赖版本见`requirements.lock`，构建依赖见`requirements-build.lock`。发布包使用PyInstaller onedir形式，用户不需要安装或手动启动Python。
