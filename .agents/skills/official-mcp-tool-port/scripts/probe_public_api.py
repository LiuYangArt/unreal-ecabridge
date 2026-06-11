#!/usr/bin/env python3
"""Probe public UE headers for symbol availability across engine versions."""
from __future__ import annotations

import argparse
import json
from collections.abc import Iterator
from pathlib import Path
from typing import Any

HEADER_GLOBS = ("*.h", "*.hpp", "*.inl")


def iter_headers(root: Path) -> Iterator[Path]:
    for pattern in HEADER_GLOBS:
        yield from root.rglob(pattern)


def scan_symbol(root: Path, symbol: str) -> list[dict[str, Any]]:
    hits: list[dict[str, Any]] = []
    for path in iter_headers(root):
        text = path.read_text(encoding="utf-8", errors="ignore")
        for line_number, line in enumerate(text.splitlines(), 1):
            if symbol in line:
                hits.append({"file": str(path), "line": line_number, "text": line.strip()[:240]})
    return hits[:50]


def probe(engine_root: Path, include_path: str, symbols: list[str]) -> dict[str, Any]:
    root = engine_root / include_path
    symbol_hits = {symbol: scan_symbol(root, symbol) if root.exists() else [] for symbol in symbols}
    return {"engine_root": str(engine_root), "include_path": str(root), "exists": root.exists(), "symbols": symbol_hits}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--symbols", required=True, help="Comma-separated symbols")
    parser.add_argument("--engine", action="append", required=True, help="Engine root; repeat for each version")
    parser.add_argument("--include-path", default="Engine/Plugins/PCG/Source/PCG/Public")
    parser.add_argument("--out", help="Optional JSON output path")
    args = parser.parse_args()

    symbols = [item.strip() for item in args.symbols.split(",") if item.strip()]
    data = {"symbols": symbols, "results": [probe(Path(engine), args.include_path, symbols) for engine in args.engine]}
    text = json.dumps(data, indent=2, ensure_ascii=False)
    print(text)

    if args.out:
        out = Path(args.out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())