#!/usr/bin/env python3
"""Build a clean-room port surface map for Epic native MCP/toolset headers."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

CALLABLE_RE = re.compile(r"UFUNCTION\([^)]*AICallable[^)]*\).*?static\s+[^;=]+?\b([A-Z]\w*)\s*\(", re.S)
COMMAND_RE = re.compile(r"GetName\(\)\s+const\s+override\s*\{\s*return\s+TEXT\(\s*\"([^\"]+)\"\s*\)")
IGNORED_TOKENS = {"pin", "pins", "param", "params", "instance", "instances"}


def to_snake(name: str) -> str:
    name = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
    name = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name)
    return re.sub(r"_+", "_", name.replace("PCG", "pcg")).strip("_").lower()


def find_official_callables(native_dir: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for path in sorted(native_dir.rglob("*.h")):
        text = path.read_text(encoding="utf-8", errors="ignore")
        for match in CALLABLE_RE.finditer(text):
            official = match.group(1)
            rows.append({"official": official, "suggested_eca": to_snake(official), "source": str(path)})
    return rows


def find_eca_commands(project_root: Path) -> dict[str, str]:
    commands: dict[str, str] = {}
    source_roots = [project_root / "Source" / "ECABridge" / name for name in ("Public", "Private")]
    for source_root in source_roots:
        if not source_root.exists():
            continue
        for path in sorted(source_root.rglob("*.h")) + sorted(source_root.rglob("*.cpp")):
            for match in COMMAND_RE.finditer(path.read_text(encoding="utf-8", errors="ignore")):
                commands[match.group(1)] = str(path)
    return commands


def domain_variants(name: str, domain_prefix: str) -> list[str]:
    if not domain_prefix:
        return [name]
    parts = name.split("_")
    variants = [name, f"{domain_prefix}_{name}"]
    if len(parts) > 1:
        variants.append("_".join([parts[0], domain_prefix, *parts[1:]]))
    return list(dict.fromkeys(variants))


def stem_tokens(name: str) -> set[str]:
    tokens: set[str] = set()
    for token in name.split("_"):
        token = token.strip().lower()
        if not token or token in IGNORED_TOKENS:
            continue
        tokens.add(token[:-1] if token.endswith("s") and len(token) > 3 else token)
    return tokens


def find_existing(name: str, commands: dict[str, str], domain_prefix: str) -> str:
    for variant in domain_variants(name, domain_prefix):
        if variant in commands:
            return variant

    wanted = stem_tokens(name)
    if not wanted:
        return ""

    candidates = []
    for command in commands:
        if domain_prefix and domain_prefix not in command.split("_"):
            continue
        have = stem_tokens(command)
        if wanted.issubset(have) or len(wanted & have) >= max(2, len(wanted) - 1):
            candidates.append(command)
    return ";".join(sorted(candidates)[:3])


def classify(row: dict[str, str], commands: dict[str, str], domain_prefix: str) -> dict[str, str]:
    existing = find_existing(row["suggested_eca"], commands, domain_prefix)
    official = row["official"]
    action = "reuse" if existing else "port"
    if official.lower() == "drawspline":
        action = "skip-interactive"
    elif "CommentBox" in official:
        action = "guard"
    return {**row, "existing_eca": existing, "action": action}


def write_outputs(rows: list[dict[str, str]], out_dir: Path | None) -> None:
    data = {"count": len(rows), "rows": rows}
    print(json.dumps(data, indent=2, ensure_ascii=False))
    if not out_dir:
        return

    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "surface-map.json").write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")

    lines = ["| official | suggested_eca | existing_eca | action | source |", "| --- | --- | --- | --- | --- |"]
    for row in rows:
        source_name = Path(row["source"]).name
        lines.append(f"| {row['official']} | {row['suggested_eca']} | {row['existing_eca']} | {row['action']} | {source_name} |")
    (out_dir / "surface-map.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-dir", required=True, help="Epic native toolset source dir")
    parser.add_argument("--project-root", default=".")
    parser.add_argument("--out", help="Optional output directory for surface-map.json/md")
    parser.add_argument("--domain-prefix", default="", help="Optional domain token to try inside ECABridge names, e.g. pcg")
    args = parser.parse_args()

    native_dir = Path(args.native_dir).resolve()
    if not native_dir.exists():
        raise SystemExit(f"native dir not found: {native_dir}")

    project_root = Path(args.project_root).resolve()
    commands = find_eca_commands(project_root)
    rows = [classify(row, commands, args.domain_prefix) for row in find_official_callables(native_dir)]
    rows.sort(key=lambda row: (row["action"], row["official"]))
    write_outputs(rows, Path(args.out).resolve() if args.out else None)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())