# Stellacandie 情绪精灵

Stellacandie 是一个使用 C++20、Qt 6 与 QML 开发的 Windows AI 陪伴型桌宠。项目尝试让桌宠拥有持续状态、长期记忆、好奇心和主动行为，而不是只在用户输入后提供机械安慰。

![Stellacandie](assets/states/stellacandie_01_happy_clean.png)

## 项目特色

- AI陪伴对话：接入DeepSeek兼容接口，支持结构化回复、状态影响分析、失败重试与离线降级。
- 长期人格与记忆：区分长期记忆、限时事件、提醒和承诺，支持重要度、生命周期、归档及清理。
- 多逻辑角色：将回复、状态分析、健康、认知路由、梦境等职责拆分，便于持续扩展。
- 反向日记：精灵以自己的视角记录共同经历，支持午夜自动生成、离线期间的下次启动补写和治愈系贴纸。
- 梦境星星纸：每天生成独立梦境并折成星星纸，用户可收藏、查看或通过照片形成现实回声。
- 晨间棒棒糖：在可配置时间段赠送糖果，支持天气联动、限定款、隐藏款和纪念口味。
- 文件零食工厂：将用户确认的不再需要的文件加工成零食，具有安全检查、批量融合和精灵互动。
- 总结魔法：总结文章与笔记，支持自定义要求、历史记录、重新生成和零食奖励。
- 视觉理解：通过视觉模型识别用户分享的图片，并与主聊天和梦境系统联动。
- 桌宠表现：透明无边框窗口、逐帧动画、桌面巡游、鼠标追逐、状态动作与系统托盘。

## 技术架构

```text
Qt Quick / QML 表现层
        │
AppController 应用协调层
        │
功能模块层 ── 逻辑角色层 ── AI / 视觉服务层
        │
DataRepository 数据访问抽象
        │
SQLite 本地持久化（预留远程同步字段）
```

主要技术：

- C++20、Qt 6、QML、CMake
- Qt Quick、Qt Network、Qt SQL、Qt Widgets
- SQLite WAL与Repository数据访问抽象
- REST API、JSON结构化输出、异步网络请求
- Windows Credential Manager安全保存API Key
- Windows系统托盘、透明窗口、空闲检测与全屏检测
- 模块化状态机、定时调度、事件驱动通信

## 目录结构

```text
assets/                  运行素材与系统提示词
qml/                     桌宠及功能窗口
src/
  data/                  Repository接口与数据结构
  logic/                 独立逻辑角色
  modules/               日记、记忆、梦境、糖果等功能模块
  AppController.*        应用协调与QML接口
tests/                   测试资料
tools/                   素材处理等辅助工具
CMakeLists.txt           CMake构建配置
```

## 本地构建

环境要求：

- Windows 10/11 64位
- Visual Studio 2022（使用C++的桌面开发）
- Qt 6.5或更高版本，MSVC 64位套件
- CMake 3.21或更高版本
- Ninja

使用Qt命令行环境执行：

```powershell
cmake --preset debug
cmake --build --preset debug
./build/debug/EmotionSprite.exe
```

也可根据本机Qt路径调整并运行`build_release.bat`。

## AI配置与隐私

启动程序后，可在聊天窗口或系统托盘中打开设置：

1. 填写OpenAI兼容的API地址、模型名与API Key。
2. 可选配置视觉模型服务。
3. 点击“保存并测试”验证连接。

API Key只保存在Windows凭据管理器中，不写入源码、日志或聊天数据库。本地数据默认位于：

```text
%APPDATA%\EmotionSprite\Stellacandie\emotion_sprite.db
```

仓库不会提交API Key、用户聊天数据库、构建产物或个人角色规划文档。

## 当前状态

项目处于持续迭代的桌面原型阶段，当前重点是完善长期人格、记忆治理、主动陪伴与2D表现。后续计划引入Godot作为独立动画表现层，由Qt/C++继续负责AI、数据和桌面业务。
