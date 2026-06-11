#!/usr/bin/env python3
"""Filter Unreal build logs to errors relevant to a new port file."""
from __future__ import annotations

import argparse
import re
from pathlib import Path

ERROR_RE = re.compile(r"\berror\b", re.IGNORECASE)


def default_log(engine: str) -> Path:
    safe_engine = f"C+Program+Files+Epic+Games+UE_{engine}"
    return Path.home() / "AppData/Roaming/Unreal Engine/AutomationTool/Logs" / safe_engine / "UBA-UnrealEditor-Win64-Development.txt"


def matching_blocks(lines: list[str], file_name: str, context: int) -> list[str]:
    file_key = file_name.lower()
    blocks: list[str] = []
    seen: set[str] = set()
    for index, line in enumerate(lines):
        if file_key not in line.lower() or not ERROR_RE.search(line):
            continue
        start = max(0, index - context)
        end = min(len(lines), index + context + 1)
        block = "\n".join(lines[start:end])
        if block not in seen:
            seen.add(block)
            blocks.append(block)
    return blocks


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--log", help="Build log path. Defaults from --engine")
    parser.add_argument("--engine", default="5.8", help="Engine version for default log path")
    parser.add_argument("--file", required=True, help="File basename or substring")
    parser.add_argument("--context", type=int, default=0)
    args = parser.parse_args()

    log_path = Path(args.log) if args.log else default_log(args.engine)
    if not log_path.exists():
        raise SystemExit(f"log not found: {log_path}")

    lines = log_path.read_text(encoding="utf-8", errors="ignore").splitlines()
    blocks = matching_blocks(lines, args.file, args.context)
    if not blocks:
        print(f"no errors for {args.file} in {log_path}")
        return 0

    print("\n---\n".join(blocks))
    return 1


if __name__ == "__main__":
    raise SystemExit(main())