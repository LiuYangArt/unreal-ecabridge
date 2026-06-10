#!/usr/bin/env python3
"""Workarounds for unrelated broken Unreal Engine plugin descriptors."""
from __future__ import annotations

import json
import os
from pathlib import Path

KNOWN_ENGINE_PLUGIN_BLOCKERS = ("CascLiveLink",)


def iter_engine_plugin_descriptors(engine_root: Path, plugin_name: str) -> list[Path]:
    plugins_root = engine_root / "Engine" / "Plugins"
    if not plugins_root.exists():
        return []

    direct = plugins_root / plugin_name / f"{plugin_name}.uplugin"
    if direct.exists():
        return [direct]

    matches: list[Path] = []
    for root, dirs, files in os.walk(plugins_root):
        dirs[:] = [name for name in dirs if not (Path(root) / name).is_symlink()]
        if f"{plugin_name}.uplugin" in files:
            matches.append(Path(root) / f"{plugin_name}.uplugin")
    return matches


def descriptor_has_missing_modules(plugin_file: Path) -> bool:
    try:
        data = json.loads(plugin_file.read_text(encoding="utf-8-sig"))
    except Exception as error:
        raise SystemExit(f"Failed to parse engine plugin descriptor {plugin_file}: {error}") from error

    modules = data.get("Modules", [])
    if not isinstance(modules, list) or not modules:
        return False

    plugin_root = plugin_file.parent
    for module in modules:
        if not isinstance(module, dict):
            continue
        module_name = str(module.get("Name", "")).strip()
        if not module_name:
            continue
        source_build = plugin_root / "Source" / module_name / f"{module_name}.Build.cs"
        has_binaries = (plugin_root / "Binaries").exists() and any(
            (plugin_root / "Binaries").glob(f"**/*{module_name}*")
        )
        if not source_build.exists() and not has_binaries:
            return True
    return False


def disable_known_engine_plugin_blockers(engine_root: Path) -> list[tuple[Path, Path]]:
    disabled: list[tuple[Path, Path]] = []
    for plugin_name in KNOWN_ENGINE_PLUGIN_BLOCKERS:
        for plugin_file in iter_engine_plugin_descriptors(engine_root, plugin_name):
            if not descriptor_has_missing_modules(plugin_file):
                continue
            disabled_file = plugin_file.with_name(plugin_file.name + ".codex-disabled")
            if disabled_file.exists():
                raise SystemExit(f"Refusing to disable {plugin_file}; restore or remove stale {disabled_file} first")
            try:
                plugin_file.rename(disabled_file)
            except PermissionError as error:
                raise SystemExit(f"Cannot temporarily disable unrelated broken engine plugin {plugin_file}: {error}") from error
            disabled.append((plugin_file, disabled_file))
            print(f"[workaround] temporarily disabled unrelated broken engine plugin descriptor: {plugin_file}")
    return disabled


def restore_disabled_engine_plugins(disabled: list[tuple[Path, Path]]) -> None:
    for plugin_file, disabled_file in reversed(disabled):
        if disabled_file.exists() and not plugin_file.exists():
            disabled_file.rename(plugin_file)
            print(f"[workaround] restored engine plugin descriptor: {plugin_file}")
        elif disabled_file.exists():
            raise SystemExit(f"Cannot restore {plugin_file}; both original and disabled files exist")