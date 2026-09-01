# Stellacandie 情绪精灵

Stellacandie 是一款面向 Windows 的本地优先 AI 陪伴桌宠。它以 Qt/QML 呈现桌宠和交互界面，以 C++ 管理状态、数据与系统集成，并通过随包交付的 Python Agent Runtime 承担结构化编排、RAG、工具调用和模型适配。

> 当前版本：`v0.9.0-rc.1` Release Candidate。适合项目展示与测试，不代表已经完成代码签名和全部云同步验收。

![Stellacandie](assets/states/stellacandie_01_happy_clean.png)

## 下载

前往 [GitHub Releases](https://github.com/20240520shubaizai/EmotionSprite-Stellacandie/releases) 下载标准安装器、便携包和 `SHA256SUMS.txt`。系统要求为 Windows 10/11 x64，目标电脑无需安装 Qt、Python 或 Visual Studio。

详细使用方法见 [用户使用说明](docs/user-guide.md)，常见故障见 [故障排查](docs/troubleshooting.md)。

## 核心能力

- 多轮 AI 陪伴对话、真实时间注入、情绪和养成状态；
- 分层长期记忆、重要度评估、RAG 检索、纠错和删除失效；
- 提醒、事件归档和一次性后续关心；
- 图片理解、文章总结和自定义总结要求；
- 反向日记、梦境星星纸、文件零食、天气棒棒糖等陪伴模块；
- 数据导出、默认保留卸载、精确确认后的彻底删除；
- Windows 凭据管理器保存 API Key；
- SQLite 本地事实库和默认关闭、按类型授权的可选同步。

## 架构概览

```text
Qt 6 / QML UI
       │ signals / slots
C++20 Application Core ───── SQLite local source of truth
       │ HTTP + SSE                     │
Python Agent Runtime                    └─ optional outbox
       │ LangGraph / RAG / tools                     │ HTTPS
DeepSeek-compatible model / Qwen3-VL          optional MySQL service
```

Agent 只承担需要推理、检索或工具编排的职责；确定性的本地状态、数据删除和权限边界仍由 C++ 与 SQLite 控制。

- [总体架构](docs/architecture.md)
- [Agent Runtime](docs/agent-runtime.md)
- [记忆与 RAG](docs/memory-and-rag.md)
- [数据、同步与隐私](docs/privacy-and-security.md)
- [工程问题复盘](docs/engineering-retrospective.md)

## 技术栈

- C++20、Qt 6、QML、Qt Quick、Qt Network、Qt SQL、CMake、CTest；
- Python 3.12、FastAPI、LangGraph、Pydantic、SQLAlchemy、pytest；
- SQLite WAL、Repository、Schema Migrator、Outbox、可选 MySQL；
- BM25、向量检索、RRF 融合、版本化 Golden Set；
- HTTP/SSE、JSON Schema、结构化输出、确定性降级；
- Windows Credential Manager、系统托盘、透明窗口、单实例锁；
- PyInstaller、Inno Setup、GitHub Actions。

## 本地构建

开发环境：Visual Studio 2022、Qt 6.8.3 MSVC x64、CMake 3.21+、Ninja、Python 3.12。

```powershell
python -m venv agent-core/.venv
agent-core/.venv/Scripts/python -m pip install -r agent-core/requirements.lock pyinstaller==6.15.0
Push-Location agent-core
./.venv/Scripts/python -m PyInstaller --noconfirm agent-core.spec
Pop-Location
cmake --preset release
cmake --build --preset release
ctest --test-dir build/release --output-on-failure
```

完整说明见 [Windows 构建指南](docs/build-windows.md)。

## 测试状态

- Python Agent：66 项自动化测试；
- Qt/C++：5 组 CTest；
- R1～R3：独立 Windows 黑盒验收；
- R4：标准安装器全新安装、升级、Agent 启动、卸载与数据保留烟测。

证据等级和条件阻塞见 [测试说明](docs/testing.md)。

## AI 辅助开发说明

项目采用 AI 辅助开发完成部分需求分析、代码生成、重构建议、测试设计和文档整理。项目作者负责产品设计、功能取舍、隐私边界、测试验收和最终交付决策；AI 生成或修改的代码需经过本地构建、自动化测试和独立 Windows 验收。详见 [开发方式说明](docs/development-process.md)。

## 安全与许可

- 安全问题见 [SECURITY.md](SECURITY.md)，不要在公开 Issue 中提交 API Key 或用户数据库；
- 项目源码采用作品展示许可，见 [LICENSE](LICENSE)；
- 第三方组件见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)；
- 当前候选包没有 Authenticode 签名，Windows 可能显示“未知发布者”。

## 当前限制

- MySQL/HTTPS 同步仍是可选实验能力，需要自行部署服务；
- Qt 主界面的 Windows UI Automation 可访问性仍需加强；
- 当前候选包未进行 Authenticode 代码签名；
- 跨午夜自动日记和真实跨版本升级建议在更多独立环境补充证据。
