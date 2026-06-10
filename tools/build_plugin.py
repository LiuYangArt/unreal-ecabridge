#!/usr/bin/env python3
"""Build ECABridge with UAT BuildPlugin while isolating unrelated broken engine plugins."""
from __future__ import annotations

import argparse
import datetime as dt
import json
import subprocess
from pathlib import Path

from engine_plugin_workarounds import disable_known_engine_plugin_blockers, restore_disabled_engine_plugins

REPO_ROOT = Path(__file__).resolve().parent.parent


def resolve_engine_root(engine_root_arg: str | None, plugin_file: Path) -> Path:
    if engine_root_arg:
        root = Path(engine_root_arg).resolve()
    else:
        data = json.loads(plugin_file.read_text(encoding="utf-8-sig"))
        version = str(data.get("EngineVersion", "")).strip()
        if not version:
            raise SystemExit("Pass --engine-root; ECABridge.uplugin has no EngineVersion")
        major_minor = ".".join(version.split(".")[:2])
        root = Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "Epic Games" / f"UE_{major_minor}"

    run_uat = root / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat"
    if not run_uat.exists():
        raise SystemExit(f"RunUAT.bat not found under engine root: {root}")
    return root




def default_package_dir(engine_root: Path) -> Path:
    stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%d-%H%M%S")
    version = engine_root.name.replace("UE_", "ue")
    return REPO_ROOT / "artifacts" / "build-plugin" / f"{version}-{stamp}"


def main() -> int:
    parser = argparse.ArgumentParser(description="Build ECABridge via RunUAT BuildPlugin")
    parser.add_argument("--plugin", default=str(REPO_ROOT / "ECABridge.uplugin"), help="Path to ECABridge.uplugin")
    parser.add_argument("--engine-root", help="UE root containing Engine/Build/BatchFiles/RunUAT.bat")
    parser.add_argument("--package", help="Output package directory; defaults to artifacts/build-plugin/<engine>-<timestamp>")
    parser.add_argument("--target-platforms", default="Win64")
    parser.add_argument("--timeout", type=int, default=900)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    plugin = Path(args.plugin).resolve()
    if not plugin.exists():
        raise SystemExit(f"Plugin file not found: {plugin}")
    engine_root = resolve_engine_root(args.engine_root, plugin)
    package_dir = Path(args.package).resolve() if args.package else default_package_dir(engine_root)
    run_uat = engine_root / "Engine" / "Build" / "BatchFiles" / "RunUAT.bat"
    command = [str(run_uat), "BuildPlugin", f"-Plugin={plugin}", f"-Package={package_dir}", f"-TargetPlatforms={args.target_platforms}"]

    print(f"engine_root={engine_root}")
    print(f"plugin={plugin}")
    print(f"package={package_dir}")
    print("command=" + subprocess.list2cmdline(command))
    if args.dry_run:
        print("dry_run=true")
        return 0

    disabled_plugins: list[tuple[Path, Path]] = []
    try:
        disabled_plugins = disable_known_engine_plugin_blockers(engine_root)
        proc = subprocess.run(command, cwd=REPO_ROOT, text=True, timeout=args.timeout)
        return proc.returncode
    finally:
        restore_disabled_engine_plugins(disabled_plugins)


if __name__ == "__main__":
    raise SystemExit(main())