# MySQL 同步服务部署

正式云同步必须同时设置 `SYNC_DEPLOYMENT_MODE=cloud_mysql` 和 `mysql+pymysql://...` 数据库地址；若仍是 SQLite，Agent 会拒绝启动。打包桌面端的本地 SQLite Agent 只负责开发与桌面业务，所有 `/v1/sync/*` 接口返回 503，不能伪装成云端。

1. 使用 `mysql_least_privilege.sql` 创建迁移账号和运行账号，替换示例密码；正式环境必须启用 MySQL TLS，限制来源 IP，并把密码放入部署平台密钥库。
2. 复制 `sync-server.env.example` 为部署环境配置，先以迁移账号执行：`python -m alembic upgrade head`。
3. 切换为只含 `SELECT/INSERT/UPDATE/DELETE` 权限的运行账号启动：`python -m uvicorn agent_core.app:app --host 127.0.0.1 --port 8765`。公网入口应由启用 HTTPS 的反向代理提供。
4. SQLite 是本地事实库。关闭总开关或分类开关只停止新上传，不删除本地记录。云端删除需输入 `DELETE CLOUD DATA`，也不会删除本机 SQLite。

同步白名单只有设置、精灵状态、普通记忆和提醒。聊天、图片、日记、梦境、秘密/仅本地记忆、API 密钥与文件路径不上传。Agent 入口拒绝非法隐私级别，MySQL 触发器再次拒绝绕过 API 的写入。
