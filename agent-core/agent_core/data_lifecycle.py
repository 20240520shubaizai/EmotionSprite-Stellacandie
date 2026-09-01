from __future__ import annotations

import ctypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import sqlite3
import tempfile
import zipfile

DELETE_CONFIRMATION = "DELETE STELLACANDIE DATA"
_CREDENTIAL_TARGETS = ("EmotionSprite/DeepSeekApiKey", "EmotionSprite/SiliconFlowVisionApiKey")


def default_data_directory() -> Path:
    root = Path(os.getenv("APPDATA", Path.home() / "AppData" / "Roaming"))
    return root / "EmotionSprite" / "Stellacandie"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _safe_target(path: Path) -> Path:
    value = path.expanduser().resolve()
    if value == value.parent or len(value.parts) < 3:
        raise ValueError("unsafe data directory")
    return value


def _sqlite_backup(source: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    src=sqlite3.connect(source);dst=sqlite3.connect(destination)
    try:
        src.backup(dst)
        result = dst.execute("PRAGMA integrity_check").fetchone()
        if not result or result[0] != "ok":
            raise RuntimeError("exported database failed integrity_check")
    finally:
        dst.close();src.close()


def export_data(data_directory: Path, archive_path: Path) -> dict:
    source = _safe_target(data_directory)
    archive = archive_path.expanduser().resolve()
    archive.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="stellacandie-export-") as temporary:
        stage = Path(temporary) / "payload"
        stage.mkdir()
        if source.exists():
            for item in source.rglob("*"):
                if not item.is_file() or item.suffix in {".db-wal", ".db-shm"}:
                    continue
                relative = item.relative_to(source)
                destination = stage / relative
                if item.name == "emotion_sprite.db":
                    _sqlite_backup(item, destination)
                else:
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copy2(item, destination)
        files = [{"path": item.relative_to(stage).as_posix(), "size": item.stat().st_size, "sha256": _sha256(item)}
                 for item in sorted(stage.rglob("*")) if item.is_file()]
        manifest = {"format": "stellacandie-data-export", "version": 1,
                    "created_at": datetime.now(timezone.utc).isoformat(), "files": files,
                    "credentials_exported": False}
        (stage / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
        temp_archive = archive.with_suffix(archive.suffix + ".tmp")
        temp_archive.unlink(missing_ok=True)
        with zipfile.ZipFile(temp_archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as bundle:
            for item in sorted(stage.rglob("*")):
                if item.is_file():
                    bundle.write(item, item.relative_to(stage).as_posix())
        temp_archive.replace(archive)
    verify_export(archive)
    return {"archive": str(archive), "file_count": len(files), "credentials_exported": False, "passed": True}


def verify_export(archive_path: Path) -> dict:
    archive = archive_path.expanduser().resolve()
    with zipfile.ZipFile(archive) as bundle:
        names = set(bundle.namelist())
        if "manifest.json" not in names:
            raise ValueError("manifest missing")
        manifest = json.loads(bundle.read("manifest.json"))
        if manifest.get("format") != "stellacandie-data-export" or manifest.get("version") != 1:
            raise ValueError("unsupported export format")
        for entry in manifest["files"]:
            name = entry["path"]
            if name not in names or hashlib.sha256(bundle.read(name)).hexdigest() != entry["sha256"]:
                raise ValueError(f"hash mismatch: {name}")
    return manifest


def _delete_windows_credentials() -> None:
    if os.name != "nt":
        return
    advapi32 = ctypes.WinDLL("Advapi32.dll")
    for target in _CREDENTIAL_TARGETS:
        advapi32.CredDeleteW(ctypes.c_wchar_p(target), 1, 0)


def delete_data(data_directory: Path, confirmation: str, *, delete_credentials: bool = False) -> dict:
    target = _safe_target(data_directory)
    if confirmation != DELETE_CONFIRMATION:
        raise PermissionError("exact deletion confirmation required")
    removed = target.exists()
    if removed:
        shutil.rmtree(target)
    if delete_credentials:
        _delete_windows_credentials()
    return {"data_directory": str(target), "removed": removed, "credentials_removed": delete_credentials,
            "remaining": target.exists(), "passed": not target.exists()}


def restore_data(archive_path: Path, data_directory: Path) -> dict:
    manifest = verify_export(archive_path)
    target = _safe_target(data_directory)
    if target.exists() and any(target.iterdir()):
        raise FileExistsError("restore target is not empty")
    target.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive_path) as bundle:
        for entry in manifest["files"]:
            relative = Path(entry["path"])
            if relative.is_absolute() or ".." in relative.parts:
                raise ValueError("unsafe archive path")
            destination = target / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(bundle.read(entry["path"]))
    restored = {item.relative_to(target).as_posix(): _sha256(item) for item in target.rglob("*") if item.is_file()}
    expected = {entry["path"]: entry["sha256"] for entry in manifest["files"]}
    if restored != expected:
        raise RuntimeError("restored data verification failed")
    database = target / "emotion_sprite.db"
    if database.exists():
        with sqlite3.connect(database) as connection:
            if connection.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
                raise RuntimeError("restored database failed integrity_check")
    return {"data_directory": str(target), "file_count": len(restored), "passed": True}
