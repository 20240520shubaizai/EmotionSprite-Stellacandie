# Windows 构建指南

## 依赖

- Visual Studio 2022 C++ Desktop；
- Qt 6.8.3 MSVC 2022 x64；
- CMake 3.21+ 与 Ninja；
- Python 3.12；
- PyInstaller 6.15；
- Inno Setup 6.7+（仅正式安装器）。

## Agent

```powershell
python -m venv agent-core/.venv
agent-core/.venv/Scripts/python -m pip install -r agent-core/requirements.lock pyinstaller==6.15.0
Push-Location agent-core
./.venv/Scripts/python -m pytest -q
./.venv/Scripts/python -m PyInstaller --clean --noconfirm agent-core.spec
Pop-Location
```

## Qt/C++

```powershell
cmake --preset release
cmake --build --preset release
ctest --test-dir build/release --output-on-failure
```

若 Qt 不在系统搜索路径，需要设置 `CMAKE_PREFIX_PATH`。仓库不提交 `.venv`、`build`、`dist` 或生成的二进制文件。

## 发布候选包

```powershell
./tools/build_public_release.ps1 `
  -BuildDirectory ./build/release `
  -OutputDirectory ./release-output `
  -Version 0.9.0-rc.1 `
  -InnoCompiler D:/InnoSetup6/ISCC.exe
```

打包器拒绝数据库、日志、`.env`、转储、常见密钥格式和开发机路径。输出包括标准安装器、便携 ZIP 和 SHA-256 文件。
