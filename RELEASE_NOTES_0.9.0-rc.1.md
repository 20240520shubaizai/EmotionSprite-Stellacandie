# Stellacandie v0.9.0-rc.1

首个公开 Windows Release Candidate，适合项目展示和体验测试。

## 重点

- 本地优先 Qt/QML AI 陪伴桌宠；
- 随包 Python Agent Runtime，无需目标电脑安装 Python；
- 长期记忆、RAG、提醒、日记、梦境、图片理解和总结；
- 标准 per-user Windows 安装器与升级、卸载入口；
- API Key 使用 Windows Credential Manager；
- SQLite 本地事实源，同步默认关闭。

## 安装

下载 `Stellacandie-0.9.0-rc.1-Windows-x64-Setup.exe`，使用 `SHA256SUMS.txt` 校验后运行。升级前请从系统托盘退出旧版本。

## 已知限制

- 尚未进行 Authenticode 代码签名，可能出现 SmartScreen 提示；
- HTTPS/MySQL 同步需要用户自行部署服务；
- Qt UI Automation 可访问性仍需改进；
- 这是候选版，不建议用于保存唯一副本的重要资料。
