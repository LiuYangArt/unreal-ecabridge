#!/usr/bin/env python3
"""Watch Epic native ModelContextProtocol changes for clean-room ECABridge triage.

The script compares two refs in a local UnrealEngine git clone and writes a
machine-readable report under artifacts/native-mcp-watcher/<run-id>/.
It intentionally records paths, commits, identifiers, and summaries only; it
never writes Epic source snippets into this repository.
"""
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


SCRIPT_ROOT = Path(__file__).resolve().parent.parent


def default_project_root() -> Path:
    cwd = Path.cwd().resolve()
    if (cwd / "Source" / "ECABridge").exists():
        return cwd
    return SCRIPT_ROOT


REPO_ROOT = default_project_root()
DEFAULT_ARTIFACT_ROOT = REPO_ROOT / "artifacts" / "native-mcp-watcher"
DEFAULT_NATIVE_PATHS = ["Engine/Plugins/Experimental/ModelContextProtocol"]
TOOL_NAME_RE = re.compile(r"[\"']([a-z][a-z0-9]*_[a-z0-9_]+)[\"']")
ECA_GETNAME_RE = re.compile(r"GetName\(\)\s*const(?:\s+override)?\s*\{\s*return\s+TEXT\(\"([a-z0-9_]+)\"\)")
REGISTER_RE = re.compile(r"REGISTER_ECA_COMMAND\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)")
CLASS_RE = re.compile(r"^class\s+([A-Za-z_][A-Za-z0-9_]*)\b")


@dataclass
class ChangedFile:
    status: str
    path: str
    old_path: str | None = None


@dataclass
class CommitInfo:
    sha: str
    date: str
    subject: str


def run(cmd: list[str], *, cwd: Path, check: bool = True) -> subprocess.CompletedProcess[str]:
    proc = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True, encoding="utf-8", errors="replace")
    if check and proc.returncode != 0:
        raise RuntimeError(f"Command failed ({proc.returncode}): {' '.join(cmd)}\n{proc.stderr.strip()}")
    return proc


def git(repo: Path, args: list[str], *, check: bool = True) -> subprocess.CompletedProcess[str]:
    return run(["git", *args], cwd=repo, check=check)


def ensure_git_repo(path: Path) -> None:
    if not path.exists():
        raise FileNotFoundError(f"UnrealEngine repo not found: {path}")
    proc = git(path, ["rev-parse", "--is-inside-work-tree"], check=False)
    if proc.returncode != 0 or proc.stdout.strip() != "true":
        raise RuntimeError(f"Not a git worktree: {path}")


def ensure_ref(repo: Path, ref: str) -> None:
    git(repo, ["rev-parse", "--verify", f"{ref}^{{commit}}"])


def diff_name_status(repo: Path, base: str, target: str, paths: list[str]) -> list[ChangedFile]:
    proc = git(repo, ["diff", "--name-status", "-z", f"{base}..{target}", "--", *paths])
    parts = [p for p in proc.stdout.split("\0") if p]
    result: list[ChangedFile] = []
    index = 0
    while index < len(parts):
        status = parts[index]
        index += 1
        if status.startswith(("R", "C")):
            old_path = parts[index]
            new_path = parts[index + 1]
            index += 2
            result.append(ChangedFile(status=status, path=new_path, old_path=old_path))
        else:
            path = parts[index]
            index += 1
            result.append(ChangedFile(status=status, path=path))
    return result


def commit_log(repo: Path, base: str, target: str, paths: list[str], limit: int) -> list[CommitInfo]:
    proc = git(repo, ["log", f"--max-count={limit}", "--format=%H%x09%cs%x09%s", f"{base}..{target}", "--", *paths])
    commits: list[CommitInfo] = []
    for line in proc.stdout.splitlines():
        sha, date, subject = (line.split("\t", 2) + ["", "", ""])[:3]
        if sha:
            commits.append(CommitInfo(sha=sha, date=date, subject=subject))
    return commits


def native_tool_candidates(repo: Path, ref: str, paths: list[str]) -> set[str]:
    proc = git(repo, ["grep", "-h", "-I", "-E", r'"[a-z][a-z0-9]*_[a-z0-9_]+"', ref, "--", *paths], check=False)
    if proc.returncode not in (0, 1):
        raise RuntimeError(f"git grep failed: {proc.stderr.strip()}")
    return {match.group(1) for match in TOOL_NAME_RE.finditer(proc.stdout)}


def eca_registered_names(root: Path) -> set[str]:
    source = root / "Source" / "ECABridge"
    registered: set[str] = set()
    class_to_name: dict[str, str] = {}
    for path in source.rglob("*.cpp"):
        text = path.read_text(encoding="utf-8", errors="replace")
        registered.update(match.group(1) for match in REGISTER_RE.finditer(text))
    for path in [*source.rglob("*.h"), *source.rglob("*.cpp")]:
        current_class: str | None = None
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            class_match = CLASS_RE.match(line.strip())
            if class_match:
                current_class = class_match.group(1)
            name_match = ECA_GETNAME_RE.search(line)
            if current_class and name_match:
                class_to_name[current_class] = name_match.group(1)
    return {class_to_name[name] for name in registered if name in class_to_name}


def run_id() -> str:
    return dt.datetime.now(dt.timezone.utc).strftime("%Y%m%d-%H%M%S")


def write_markdown(report: dict, path: Path) -> None:
    changed = report["changed_files"]
    commits = report["commits"]
    conflicts = report["tool_name_overlap_with_ecabridge"]
    lines = [
        "# Native MCP Watcher Report",
        "",
        f"- UE repo: `{report['ue_repo']}`",
        f"- Range: `{report['base']}`..`{report['target']}`",
        f"- Watched paths: {', '.join('`' + p + '`' for p in report['watched_paths'])}",
        f"- Changed files: {len(changed)}",
        f"- Native tool-name candidates: {len(report['native_tool_candidates'])}",
        f"- ECABridge overlap candidates: {len(conflicts)}",
        "",
        "## Purpose",
        "",
        "This report is for clean-room porting Epic native MCP behavior into ECABridge so launcher users and UE 5.7 projects can get the capability early. It is not just a name-conflict report.",
        "",
        "## Porting Workflow",
        "",
        "1. Inspect the changed Epic files in the local UnrealEngine clone.",
        "2. Write a behavior spec in your own words in `clean-room-spec-template.md`.",
        "3. Decide `port`, `mirror-api`, or `ignore` for each relevant change.",
        "4. Implement in ECABridge using public UE APIs and existing ECABridge command/schema patterns.",
        "5. Keep UE 5.7 compatibility with version guards where needed.",
        "6. Run ECABridge validation and native coexistence smoke checks when available.",
        "",
        "## Triage Labels",
        "",
        "- port: native has a capability ECABridge should clean-room reimplement.",
        "- mirror-api: native shape is useful, but ECABridge should keep existing behavior.",
        "- ignore: unrelated, internal-only, or not useful for launcher users.",
        "",
    ]
    if conflicts:
        lines += ["## Tool Name Overlap", ""]
        lines += [f"- `{name}`" for name in conflicts]
        lines += [""]
    if commits:
        lines += ["## Commits", ""]
        lines += [f"- `{c['sha'][:12]}` {c['date']} {c['subject']}" for c in commits]
        lines += [""]
    if changed:
        lines += ["## Changed Files", ""]
        for file in changed:
            old = f" (from `{file['old_path']}`)" if file.get("old_path") else ""
            lines.append(f"- `{file['status']}` `{file['path']}`{old}")
        lines += [""]
    lines += [
        "## Clean-room Notes",
        "",
        "Do not paste Epic source into ECABridge. Write behavior specs in your own words, then implement against public UE APIs or ECABridge patterns.",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")

def write_spec_template(report: dict, path: Path) -> None:
    changed = report["changed_files"]
    lines = [
        "# Clean-room Native MCP Port Spec",
        "",
        f"Range: `{report['base']}`..`{report['target']}`",
        "",
        "## Source Review Boundary",
        "",
        "- You may inspect Epic source in the local UnrealEngine clone to understand behavior.",
        "- Do not paste Epic source or line-by-line paraphrases here.",
        "- Describe behavior, inputs, outputs, error cases, and UE API dependencies in your own words.",
        "",
        "## Candidate Changes",
        "",
    ]
    if changed:
        for file in changed:
            lines.append(f"- [ ] `{file['status']}` `{file['path']}`: `<port | mirror-api | ignore>`")
    else:
        lines.append("- No changed files in watched paths.")
    lines += [
        "",
        "## Behavior Spec",
        "",
        "### Capability",
        "<What user-visible MCP capability changed or was added?>",
        "",
        "### Native Behavior Summary",
        "<Plain-English behavior only. No copied source.>",
        "",
        "### ECABridge Implementation Plan",
        "<Files, command names, schemas, optional plugin gates, UE 5.7 guards.>",
        "",
        "### Compatibility Notes",
        "<Tool-name overlap, native coexistence, launcher/UE 5.7 behavior.>",
        "",
        "### Validation",
        "```powershell",
        "python tools\\verify.py",
        "python scripts\\smoke-test.py --include-native",
        "```",
        "",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def build_report(args: argparse.Namespace) -> dict:
    ue_repo = Path(args.ue_repo).resolve()
    project_root = Path(args.project_root).resolve()
    ensure_git_repo(ue_repo)
    if args.fetch:
        git(ue_repo, ["fetch", "--all", "--prune"])
    ensure_ref(ue_repo, args.base)
    ensure_ref(ue_repo, args.target)

    changed = diff_name_status(ue_repo, args.base, args.target, args.path)
    commits = commit_log(ue_repo, args.base, args.target, args.path, args.commit_limit)
    native_names = native_tool_candidates(ue_repo, args.target, args.path)
    eca_names = eca_registered_names(project_root)
    overlap = sorted(native_names & eca_names)

    return {
        "ok": True,
        "ue_repo": str(ue_repo),
        "project_root": str(project_root),
        "base": args.base,
        "target": args.target,
        "watched_paths": args.path,
        "changed_files": [asdict(item) for item in changed],
        "commits": [asdict(item) for item in commits],
        "native_tool_candidates": sorted(native_names),
        "ecabridge_tool_count": len(eca_names),
        "tool_name_overlap_with_ecabridge": overlap,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Compare Epic native MCP changes for ECABridge triage")
    parser.add_argument("--ue-repo", default=os.environ.get("UE_REPO"), help="Local Epic UnrealEngine git clone. Can also use UE_REPO env var.")
    parser.add_argument("--base", default="origin/release", help="Base git ref")
    parser.add_argument("--target", default="origin/ue5-main", help="Target git ref")
    parser.add_argument("--path", action="append", default=None, help="Native path to watch; repeatable")
    parser.add_argument("--project-root", default=str(REPO_ROOT), help="ECABridge repo root")
    parser.add_argument("--artifact-root", default=str(DEFAULT_ARTIFACT_ROOT), help="Artifact output root")
    parser.add_argument("--commit-limit", type=int, default=50, help="Max commits to include")
    parser.add_argument("--fetch", action="store_true", help="Run git fetch --all --prune in the UE repo first")
    parser.add_argument("--json-only", action="store_true", help="Print JSON only and do not write artifacts")
    args = parser.parse_args(argv)

    if not args.ue_repo:
        parser.error("--ue-repo is required unless UE_REPO is set")
    args.path = args.path or DEFAULT_NATIVE_PATHS

    try:
        report = build_report(args)
        if not args.json_only:
            artifact_dir = Path(args.artifact_root) / run_id()
            artifact_dir.mkdir(parents=True, exist_ok=True)
            report["artifact_dir"] = str(artifact_dir)
            (artifact_dir / "report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
            write_markdown(report, artifact_dir / "report.md")
            write_spec_template(report, artifact_dir / "clean-room-spec-template.md")
        print(json.dumps(report, indent=2))
        return 0
    except Exception as error:
        payload = {"ok": False, "error": str(error)}
        print(json.dumps(payload, indent=2), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())