#!/usr/bin/env python3
"""Inventory UE 5.8 native Toolset callables and compare them with ECABridge."""
from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import re
import subprocess
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


SCRIPT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ARTIFACT_ROOT = SCRIPT_ROOT / "artifacts" / "native-mcp-gap"
DEFAULT_NATIVE_PATHS = ["Engine/Plugins/Experimental/Toolsets"]

UFUNCTION_RE = re.compile(r"UFUNCTION\s*\((.*?)\)", re.S)
CATEGORY_RE = re.compile(r"Category\s*=\s*\"([^\"]+)\"")
CLASS_RE = re.compile(r"\bclass\s+(?:\w+_API\s+)?(U\w+)\s*:\s*public\s+(\w+)")
SIGNATURE_RE = re.compile(
    r"^(?P<return>.+?)\s+(?P<name>[A-Za-z_]\w*)\s*\((?P<params>.*)\)\s*(?:const\s*)?;?$",
    re.S,
)
GET_NAME_RE = re.compile(r"GetName\s*\(\s*\)\s*const(?:\s+override)?\s*\{\s*return\s+TEXT\(\"([a-z0-9_]+)\"\)", re.S)
REGISTER_RE = re.compile(r"REGISTER_ECA_COMMAND\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
ECA_CLASS_RE = re.compile(r"\bclass\s+(FECACommand_[A-Za-z0-9_]+)\b")
TEXT_NAME_RE = re.compile(r"return\s+TEXT\(\"([a-z0-9_]+)\"\)")
BUILD_DEP_RE = re.compile(r"\"([A-Za-z0-9_]+)\"")
CAMEL_RE = re.compile(r"(?<!^)(?=[A-Z])")
STOP_TOKENS = {"get", "set", "add", "remove", "delete", "create", "list", "find", "open", "close", "toolset", "editor"}


@dataclass
class OfficialTool:
    module: str
    plugin: str
    class_name: str
    category: str
    function: str
    suggested_ecabridge_name: str
    return_type: str
    params: list[dict[str, str]]
    raw_params: str
    source_path: str
    line: int
    dependency_modules: list[str]


@dataclass
class Match:
    official: dict
    status: str
    best_ecabridge_match: str | None
    score: float


def run(cmd: list[str], *, cwd: Path, check: bool = True) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True, encoding="utf-8", errors="replace")
    if check and proc.returncode != 0:
        raise RuntimeError(f"Command failed ({proc.returncode}): {' '.join(cmd)}\n{proc.stderr.strip()}")
    return proc


def git(repo: Path, args: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return run(["git", *args], cwd=repo, check=check)


def ensure_repo(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(f"UnrealEngine repo not found: {path}")
    proc = git(path, ["rev-parse", "--is-inside-work-tree"], check=False)
    if proc.returncode != 0 or proc.stdout.strip() != "true":
        raise RuntimeError(f"Not a git worktree: {path}")


def list_ref_files(repo: Path, ref: str, paths: list[str], suffixes: tuple[str, ...]) -> list[str]:
    proc = git(repo, ["ls-tree", "-r", "--name-only", ref, "--", *paths])
    return [line for line in proc.stdout.splitlines() if line.endswith(suffixes)]


def show_file(repo: Path, ref: str, path: str) -> str:
    proc = git(repo, ["show", f"{ref}:{path}"], check=False)
    if proc.returncode != 0:
        return ""
    return proc.stdout


def split_top_level_commas(text: str) -> list[str]:
    parts: list[str] = []
    start = 0
    angle = paren = bracket = 0
    quote: str | None = None
    i = 0
    while i < len(text):
        ch = text[i]
        if quote:
            if ch == quote and (i == 0 or text[i - 1] != "\\"):
                quote = None
        elif ch in "'\"":
            quote = ch
        elif ch == "<":
            angle += 1
        elif ch == ">" and angle:
            angle -= 1
        elif ch == "(":
            paren += 1
        elif ch == ")" and paren:
            paren -= 1
        elif ch == "[":
            bracket += 1
        elif ch == "]" and bracket:
            bracket -= 1
        elif ch == "," and angle == 0 and paren == 0 and bracket == 0:
            parts.append(text[start:i].strip())
            start = i + 1
        i += 1
    tail = text[start:].strip()
    if tail:
        parts.append(tail)
    return parts


def parse_param(param: str) -> dict[str, str]:
    clean = re.sub(r"\s*=.*$", "", param.strip())
    clean = re.sub(r"\bUPARAM\s*\([^)]*\)\s*", "", clean)
    match = re.match(r"(.+?[\*&\s])([A-Za-z_]\w*)$", clean)
    if not match:
        return {"name": clean, "type": ""}
    return {"name": match.group(2).strip(), "type": match.group(1).strip()}


def parse_params(raw: str) -> list[dict[str, str]]:
    raw = raw.strip()
    if not raw or raw == "void":
        return []
    return [parse_param(item) for item in split_top_level_commas(raw)]


def to_snake(name: str) -> str:
    name = name.strip()
    if "_" in name:
        snake = re.sub(r"_+", "_", name.lower()).strip("_")
    else:
        snake = re.sub(r"(.)([A-Z][a-z]+)", r"\1_\2", name)
        snake = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", snake).lower()
    replacements = {
        "p_i_e": "pie",
        "c_vars": "cvars",
        "u_m_g": "umg",
        "p_c_g": "pcg",
        "g_a_s": "gas",
        "x_r": "xr",
        "i_d": "id",
    }
    for old, new in replacements.items():
        snake = snake.replace(old, new)
    return re.sub(r"_+", "_", snake).strip("_")


def path_parts(path: str) -> tuple[str, str]:
    parts = path.split("/")
    plugin = ""
    module = ""
    if "Toolsets" in parts:
        idx = parts.index("Toolsets")
        if len(parts) > idx + 1:
            plugin = parts[idx + 1]
    if "Source" in parts:
        idx = parts.index("Source")
        if len(parts) > idx + 1:
            module = parts[idx + 1]
    return plugin, module or plugin


def build_deps(repo: Path, ref: str, build_files: Iterable[str]) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    for path in build_files:
        plugin, module = path_parts(path)
        text = show_file(repo, ref, path)
        deps = sorted(set(BUILD_DEP_RE.findall(text)))
        deps = [dep for dep in deps if dep not in {module, plugin}]
        result[module] = deps
    return result


def find_ufunction_macros(text: str) -> list[tuple[int, int, str]]:
    macros: list[tuple[int, int, str]] = []
    search_from = 0
    while True:
        start = text.find("UFUNCTION", search_from)
        if start < 0:
            return macros
        open_paren = text.find("(", start)
        if open_paren < 0:
            return macros
        depth = 0
        quote: str | None = None
        i = open_paren
        while i < len(text):
            ch = text[i]
            if quote:
                if ch == quote and text[i - 1] != "\\":
                    quote = None
            elif ch in "'\"":
                quote = ch
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    macros.append((start, i + 1, text[open_paren + 1 : i]))
                    search_from = i + 1
                    break
            i += 1
        else:
            return macros


def clean_return_type(value: str) -> str:
    value = re.sub(r"\bstatic\b", "", value)
    value = re.sub(r"\b[A-Z0-9_]+_API\b", "", value)
    return re.sub(r"\s+", " ", value).strip()


def parse_tools_from_header(repo: Path, ref: str, path: str, deps_by_module: dict[str, list[str]]) -> list[OfficialTool]:
    text = show_file(repo, ref, path)
    if "AICallable" not in text:
        return []
    plugin, module = path_parts(path)
    class_matches = list(CLASS_RE.finditer(text))

    tools: list[OfficialTool] = []
    for macro_start, macro_end, macro_body in find_ufunction_macros(text):
        if "AICallable" not in macro_body:
            continue
        line_no = 1 + text[:macro_start].count("\n")
        category_match = CATEGORY_RE.search(macro_body)
        category = category_match.group(1) if category_match else ""
        class_name = ""
        for class_match in class_matches:
            if class_match.start() < macro_start:
                class_name = class_match.group(1)
            else:
                break

        after = text[macro_end:]
        sig_lines: list[str] = []
        for raw_line in after.splitlines():
            stripped = raw_line.strip()
            if not stripped or stripped.startswith("//"):
                continue
            sig_lines.append(stripped)
            if ";" in stripped:
                break
        signature = " ".join(sig_lines)
        signature = re.sub(r"\s+", " ", signature).strip()
        sig_match = SIGNATURE_RE.match(signature)
        if not sig_match:
            continue
        function = sig_match.group("name")
        raw_params = sig_match.group("params").strip()
        tools.append(
            OfficialTool(
                module=module,
                plugin=plugin,
                class_name=class_name,
                category=category,
                function=function,
                suggested_ecabridge_name=to_snake(function),
                return_type=clean_return_type(sig_match.group("return")),
                params=parse_params(raw_params),
                raw_params=raw_params,
                source_path=path,
                line=line_no,
                dependency_modules=deps_by_module.get(module, []),
            )
        )
    return tools


def ecabridge_names(project_root: Path) -> set[str]:
    source = project_root / "Source" / "ECABridge"
    if not source.exists():
        raise FileNotFoundError(f"ECABridge source not found: {source}")
    registered: set[str] = set()
    class_to_name: dict[str, str] = {}
    for path in source.rglob("*.cpp"):
        text = path.read_text(encoding="utf-8", errors="replace")
        registered.update(REGISTER_RE.findall(text))
    for path in [*source.rglob("*.h"), *source.rglob("*.cpp")]:
        text = path.read_text(encoding="utf-8", errors="replace")
        classes = list(ECA_CLASS_RE.finditer(text))
        for idx, class_match in enumerate(classes):
            end = classes[idx + 1].start() if idx + 1 < len(classes) else len(text)
            body = text[class_match.start() : end]
            name_match = GET_NAME_RE.search(body) or TEXT_NAME_RE.search(body)
            if name_match:
                class_to_name[class_match.group(1)] = name_match.group(1)
    return {class_to_name[item] for item in registered if item in class_to_name}


def tokens(name: str) -> set[str]:
    snake = to_snake(name)
    return {part for part in re.split(r"[^a-z0-9]+", snake) if part and part not in STOP_TOKENS}


def similarity(a: str, b: str) -> float:
    at = tokens(a)
    bt = tokens(b)
    if not at or not bt:
        return 0.0
    return len(at & bt) / len(at | bt)


def classify_tools(tools: list[OfficialTool], eca_names: set[str]) -> list[Match]:
    matches: list[Match] = []
    for tool in tools:
        candidate = tool.suggested_ecabridge_name
        if candidate in eca_names:
            matches.append(Match(asdict(tool), "exact", candidate, 1.0))
            continue
        best_name = None
        best_score = 0.0
        for eca in eca_names:
            score = max(similarity(candidate, eca), similarity(tool.function, eca))
            if score > best_score:
                best_name = eca
                best_score = score
        status = "likely_existing" if best_score >= 0.55 else "gap"
        matches.append(Match(asdict(tool), status, best_name, round(best_score, 3)))
    return matches


def run_id() -> str:
    return dt.datetime.now(dt.timezone.utc).strftime("%Y%m%d-%H%M%S")


def write_report(path: Path, payload: dict, report_limit: int) -> None:
    matches = payload["matches"]
    gaps = [m for m in matches if m["status"] == "gap"]
    likely = [m for m in matches if m["status"] == "likely_existing"]
    exact = [m for m in matches if m["status"] == "exact"]
    by_module: dict[str, dict[str, int]] = {}
    for match in matches:
        module = match["official"]["module"]
        bucket = by_module.setdefault(module, {"total": 0, "exact": 0, "likely_existing": 0, "gap": 0})
        bucket["total"] += 1
        bucket[match["status"]] += 1

    lines = [
        "# Native MCP Gap Report",
        "",
        f"- UE repo: `{payload['ue_repo']}`",
        f"- Target: `{payload['target']}`",
        f"- Official AICallable tools: {len(matches)}",
        f"- Exact ECABridge matches: {len(exact)}",
        f"- Likely existing ECABridge matches: {len(likely)}",
        f"- Gaps: {len(gaps)}",
        "",
        "## By Module",
        "",
        "| Module | Total | Exact | Likely | Gap |",
        "| --- | ---: | ---: | ---: | ---: |",
    ]
    for module, counts in sorted(by_module.items()):
        lines.append(f"| {module} | {counts['total']} | {counts['exact']} | {counts['likely_existing']} | {counts['gap']} |")

    lines += ["", "## Gaps", ""]
    for match in gaps[:report_limit]:
        official = match["official"]
        params = ", ".join(f"{p['type']} {p['name']}".strip() for p in official["params"])
        lines.append(
            f"- `{official['module']}.{official['function']}` -> suggested `{official['suggested_ecabridge_name']}` "
            f"({official['return_type']}({params})) [{official['source_path']}:{official['line']}]"
        )
    if len(gaps) > report_limit:
        lines.append(f"- ... {len(gaps) - report_limit} more gaps in `official-tools.json`")

    lines += ["", "## Likely Existing", ""]
    for match in likely[:report_limit]:
        official = match["official"]
        lines.append(
            f"- `{official['module']}.{official['function']}` -> `{match['best_ecabridge_match']}` "
            f"score={match['score']}"
        )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_payload(args: argparse.Namespace) -> dict:
    ue_repo = Path(args.ue_repo).resolve()
    project_root = Path(args.project_root).resolve()
    ensure_repo(ue_repo)
    git(ue_repo, ["rev-parse", "--verify", f"{args.target}^{{commit}}"])

    header_files = list_ref_files(ue_repo, args.target, args.path, (".h",))
    build_files = list_ref_files(ue_repo, args.target, args.path, (".Build.cs",))
    deps = build_deps(ue_repo, args.target, build_files)
    tools: list[OfficialTool] = []
    for path in header_files:
        tools.extend(parse_tools_from_header(ue_repo, args.target, path, deps))
    tools.sort(key=lambda item: (item.module, item.class_name, item.function))

    eca = sorted(ecabridge_names(project_root))
    matches = classify_tools(tools, set(eca))
    return {
        "ok": True,
        "ue_repo": str(ue_repo),
        "project_root": str(project_root),
        "target": args.target,
        "scanned_paths": args.path,
        "official_tool_count": len(tools),
        "ecabridge_tool_count": len(eca),
        "official_tools": [asdict(item) for item in tools],
        "ecabridge_tools": eca,
        "matches": [asdict(item) for item in matches],
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Build a UE native MCP Toolset gap report for ECABridge")
    parser.add_argument("--ue-repo", default=os.environ.get("UE_REPO"), help="Local Epic UnrealEngine git clone")
    parser.add_argument("--target", default="origin/5.8", help="UE git ref to inspect")
    parser.add_argument("--path", action="append", default=None, help="UE path to scan; repeatable")
    parser.add_argument("--project-root", default=str(SCRIPT_ROOT), help="ECABridge repo root")
    parser.add_argument("--artifact-root", default=str(DEFAULT_ARTIFACT_ROOT), help="Output artifact root")
    parser.add_argument("--report-limit", type=int, default=200, help="Max rows per markdown section")
    parser.add_argument("--json-only", action="store_true", help="Print JSON only and do not write artifacts")
    args = parser.parse_args(argv)
    if not args.ue_repo:
        parser.error("--ue-repo is required unless UE_REPO is set")
    args.path = args.path or DEFAULT_NATIVE_PATHS

    try:
        payload = build_payload(args)
        if not args.json_only:
            artifact_dir = Path(args.artifact_root) / run_id()
            artifact_dir.mkdir(parents=True, exist_ok=True)
            payload["artifact_dir"] = str(artifact_dir)
            (artifact_dir / "official-tools.json").write_text(json.dumps(payload["official_tools"], indent=2), encoding="utf-8")
            (artifact_dir / "ecabridge-map.json").write_text(
                json.dumps(
                    {
                        "ecabridge_tool_count": payload["ecabridge_tool_count"],
                        "ecabridge_tools": payload["ecabridge_tools"],
                        "matches": payload["matches"],
                    },
                    indent=2,
                ),
                encoding="utf-8",
            )
            write_report(artifact_dir / "gap-report.md", payload, args.report_limit)
        print(json.dumps({k: v for k, v in payload.items() if k not in {"official_tools", "ecabridge_tools", "matches"}}, indent=2))
        return 0
    except Exception as error:
        print(json.dumps({"ok": False, "error": str(error)}, indent=2), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())