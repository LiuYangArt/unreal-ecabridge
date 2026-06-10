#!/usr/bin/env python3
"""Build ECABridge inside an Unreal Engine project workspace."""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import os
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(REPO_ROOT / "tools"))

from engine_plugin_workarounds import disable_known_engine_plugin_blockers, restore_disabled_engine_plugins

DEFAULT_PORT = 8831
ERROR_PATTERNS = (" error ", ": error", "fatal error", "Result: Failed")




def find_project(path_arg: str | None) -> Path:
    if path_arg:
        path = Path(path_arg).resolve()
        if path.is_dir():
            matches = sorted(path.glob("*.uproject"))
            if len(matches) != 1:
                raise SystemExit(f"Expected exactly one .uproject in {path}, found {len(matches)}")
            return matches[0]
        return path

    matches = sorted(Path.cwd().glob("*.uproject"))
    if len(matches) != 1:
        raise SystemExit("Pass --project; current directory does not contain exactly one .uproject")
    return matches[0].resolve()


def read_project(project: Path) -> dict:
    if not project.exists():
        raise SystemExit(f"Project file not found: {project}")
    return json.loads(project.read_text(encoding="utf-8-sig"))


def resolve_engine_root(project_data: dict, engine_root_arg: str | None) -> Path:
    if engine_root_arg:
        root = Path(engine_root_arg).resolve()
    else:
        association = str(project_data.get("EngineAssociation", "")).strip()
        if not association:
            raise SystemExit("Project has no EngineAssociation; pass --engine-root")
        candidate = Path(association)
        if candidate.exists():
            root = candidate.resolve()
        else:
            root = Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Epic Games" / f"UE_{association}"

    build_bat = root / "Engine" / "Build" / "BatchFiles" / "Build.bat"
    if not build_bat.exists():
        raise SystemExit(f"Build.bat not found under engine root: {root}")
    return root


def unreal_editor_processes() -> list[dict[str, str]]:
    proc = subprocess.run(
        ["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe", "/FO", "CSV", "/NH"],
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        return []
    rows: list[dict[str, str]] = []
    for row in csv.reader(proc.stdout.splitlines()):
        if len(row) >= 2 and row[0].lower() == "unrealeditor.exe":
            rows.append({"image": row[0], "pid": row[1]})
    return rows


def check_port(project_root: Path, expected_port: int, config_file: Path | None) -> None:
    defaults = project_root / "Plugins" / "ECABridge" / "Source" / "ECABridge" / "Public" / "ECABridgeDefaults.h"
    if not defaults.exists():
        print(f"[warn] ECABridgeDefaults.h not found: {defaults}")
    else:
        text = defaults.read_text(encoding="utf-8", errors="replace")
        if f"ServerPort = {expected_port}" not in text:
            raise SystemExit(f"Expected ServerPort = {expected_port} in {defaults}")

    if config_file and config_file.exists():
        text = config_file.read_text(encoding="utf-8", errors="replace")
        expected = f"http://127.0.0.1:{expected_port}/mcp"
        if expected not in text:
            raise SystemExit(f"Expected {expected} in {config_file}")


def summarize_build_output(text: str) -> None:
    lines = text.splitlines()
    errors = [line for line in lines if any(pattern in line.lower() for pattern in ERROR_PATTERNS)]
    if errors:
        print("\n[errors]")
        for line in errors[-40:]:
            print(line)
    print("\n[tail]")
    for line in lines[-80:]:
        print(line)


def main() -> int:
    parser = argparse.ArgumentParser(description="Build ECABridge in a UE project workspace")
    parser.add_argument("--project", help="Path to .uproject or a directory containing one")
    parser.add_argument("--engine-root", help="UE root containing Engine/Build/BatchFiles/Build.bat")
    parser.add_argument("--target", help="Build target; defaults to <ProjectName>Editor")
    parser.add_argument("--platform", default="Win64")
    parser.add_argument("--configuration", default="Development")
    parser.add_argument("--expected-port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--config-file", help="Optional MCP client config to validate before build")
    parser.add_argument("--skip-editor-check", action="store_true", help="Build even if UnrealEditor.exe is running")
    parser.add_argument("--dry-run", action="store_true", help="Print checks and command without building")
    parser.add_argument("--require-fresh-dll", action="store_true", help="Fail if the ECABridge DLL timestamp is older than this build run")
    parser.add_argument("--timeout", type=int, default=900, help="Build timeout in seconds")
    args = parser.parse_args()

    project = find_project(args.project)
    project_root = project.parent
    project_data = read_project(project)
    engine_root = resolve_engine_root(project_data, args.engine_root)
    target = args.target or f"{project.stem}Editor"
    config_file = Path(args.config_file).resolve() if args.config_file else project_root / ".codex" / "config.toml"

    print(f"project={project}")
    print(f"engine_root={engine_root}")
    print(f"target={target}")
    print(f"config_file={config_file}")

    if not args.skip_editor_check:
        processes = unreal_editor_processes()
        if processes:
            pids = ", ".join(process["pid"] for process in processes)
            raise SystemExit(f"UnrealEditor.exe is running (pid {pids}); close it before rebuilding ECABridge")

    check_port(project_root, args.expected_port, config_file)

    build_bat = engine_root / "Engine" / "Build" / "BatchFiles" / "Build.bat"
    command = [str(build_bat), target, args.platform, args.configuration, str(project), "-WaitMutex"]
    print("command=" + subprocess.list2cmdline(command))

    dll = project_root / "Plugins" / "ECABridge" / "Binaries" / "Win64" / "UnrealEditor-ECABridge.dll"
    start = dt.datetime.now(dt.timezone.utc)
    if args.dry_run:
        print("dry_run=true")
        return 0

    disabled_plugins: list[tuple[Path, Path]] = []
    try:
        disabled_plugins = disable_known_engine_plugin_blockers(engine_root)
        proc = subprocess.run(command, cwd=project_root, text=True, capture_output=True, timeout=args.timeout)
        combined = (proc.stdout or "") + ("\n" + proc.stderr if proc.stderr else "")
        summarize_build_output(combined)
        if proc.returncode != 0:
            return proc.returncode
    finally:
        restore_disabled_engine_plugins(disabled_plugins)

    if not dll.exists():
        raise SystemExit(f"Build succeeded but ECABridge DLL not found: {dll}")
    dll_mtime = dt.datetime.fromtimestamp(dll.stat().st_mtime, dt.timezone.utc)
    if args.require_fresh_dll and dll_mtime + dt.timedelta(seconds=5) < start:
        raise SystemExit(f"Build succeeded but ECABridge DLL timestamp was not refreshed: {dll}")

    print(f"\n[ok] ECABridge DLL exists: {dll}")
    print(f"[ok] ECABridge DLL timestamp: {dll_mtime.astimezone().isoformat(timespec='seconds')}")
    print(f"[ok] MCP URL: http://127.0.0.1:{args.expected_port}/mcp")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())