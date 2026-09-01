# Third-Party Notices

Stellacandie dynamically bundles or interacts with third-party software. Each component remains governed by its own license.

| Component | Purpose | License / source |
|---|---|---|
| Qt 6 | Desktop UI, networking and SQLite driver | LGPL-3.0/GPL/commercial options; https://www.qt.io/licensing |
| Python | Bundled Agent runtime | PSF License; https://docs.python.org/3/license.html |
| FastAPI / Starlette / Uvicorn | Local Agent HTTP service | MIT/BSD licenses in installed metadata |
| LangGraph / LangChain Core | Agent graph orchestration | MIT license |
| Pydantic | Structured validation | MIT license |
| SQLAlchemy / Alembic | Data and migration layer | MIT license |
| PyMySQL | Optional MySQL driver | MIT license |
| FastEmbed / ONNX Runtime / NumPy | Local embedding inference | Apache-2.0/MIT/BSD family |
| sqlite-vec | Derived vector index | Project license in distribution metadata |
| Model Context Protocol SDK | Tool protocol integration | MIT license |
| PyInstaller | Python runtime packaging | GPL with bootloader exception |
| Inno Setup | Windows installer generation | Inno Setup license; https://jrsoftware.org/isinfo.php |

Qt libraries are dynamically linked and shipped as replaceable DLLs. The LGPL-3.0 text is included under `licenses/LGPL-3.0.txt`. Python distributions retain `.dist-info` license and metadata files where provided.

DeepSeek, SiliconFlow and model providers are network services rather than redistributed code. Users must comply with each provider's terms and supply their own credentials.
