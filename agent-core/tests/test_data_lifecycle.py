import json
from pathlib import Path
import sqlite3
import zipfile

import pytest

from agent_core.data_lifecycle import DELETE_CONFIRMATION,delete_data,export_data,restore_data,verify_export


def make_database(path:Path)->None:
    path.parent.mkdir(parents=True)
    db=sqlite3.connect(path)
    try:
        db.execute("CREATE TABLE memories(id INTEGER PRIMARY KEY, content TEXT NOT NULL)")
        db.execute("INSERT INTO memories(content) VALUES('橘猫记忆')")
        db.commit()
    finally:db.close()


def test_export_delete_restore_round_trip(tmp_path):
    source=tmp_path/"profile";database=source/"emotion_sprite.db";make_database(database)
    (source/"preferences.json").write_text('{"theme":"warm"}',encoding="utf-8")
    archive=tmp_path/"export.zip";result=export_data(source,archive)
    assert result["passed"] and result["credentials_exported"] is False
    manifest=verify_export(archive);assert len(manifest["files"])==2
    with pytest.raises(PermissionError):delete_data(source,"DELETE")
    assert delete_data(source,DELETE_CONFIRMATION)["passed"] and not source.exists()
    restored=restore_data(archive,source);assert restored["passed"]
    db=sqlite3.connect(database)
    try:assert db.execute("SELECT content FROM memories").fetchone()[0]=="橘猫记忆"
    finally:db.close()


def test_export_detects_tampering_and_restore_refuses_nonempty_target(tmp_path):
    source=tmp_path/"profile";make_database(source/"emotion_sprite.db");archive=tmp_path/"export.zip";export_data(source,archive)
    with zipfile.ZipFile(archive) as bundle:items={name:bundle.read(name) for name in bundle.namelist()}
    items["emotion_sprite.db"]=b"tampered"
    with zipfile.ZipFile(archive,"w") as bundle:
        for name,value in items.items():bundle.writestr(name,value)
    with pytest.raises(ValueError):verify_export(archive)
    clean=tmp_path/"clean";clean.mkdir();(clean/"keep.txt").write_text("keep")
    with pytest.raises((ValueError,FileExistsError)):restore_data(archive,clean)
