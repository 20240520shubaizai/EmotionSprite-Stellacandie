# 发布流程

1. 冻结版本和变更范围；
2. 运行 Python 与 Qt/C++ 自动化测试；
3. 构建 PyInstaller Agent Runtime 和 Qt Release；
4. 运行发布白名单与敏感内容扫描；
5. 生成便携包、Inno Setup 安装器和 SHA-256；
6. 执行全新安装、升级、运行、卸载和数据保留烟测；
7. 更新 Changelog、已知限制和第三方许可；
8. 创建版本提交和 Git 标签；
9. 创建 GitHub Draft Release 并上传附件；
10. 从远端重新下载并校验后再发布。

安装包不得提交进 Git 历史，只能作为 GitHub Release Asset。正式 1.0 之前还应完成 Authenticode 签名和更多独立安全扫描。
