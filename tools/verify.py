#!/usr/bin/env python3
"""Local verification entrypoint for ECABridge agent work.

Runs checks that do not require Unreal Editor by default and writes a
machine-readable summary under artifacts/verify/<run-id>/.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
import py_compile
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
ARTIFACT_ROOT = REPO_ROOT / "artifacts" / "verify"


def run_id() -> str:
    return dt.datetime.now(dt.timezone.utc).strftime("%Y%m%d-%H%M%S")


def project_files(patterns: list[str]) -> list[Path]:
    cmd = ["git", "ls-files", "--cached", "--others", "--exclude-standard", "--", *patterns]
    proc = subprocess.run(cmd, cwd=REPO_ROOT, text=True, capture_output=True)
    if proc.returncode != 0:
        raise RuntimeError(f"git ls-files failed: {proc.stderr.strip()}")
    return [REPO_ROOT / line for line in proc.stdout.splitlines() if line.strip()]


def check_python_compile() -> dict:
    files = project_files(["*.py"])
    failures: list[dict[str, str]] = []
    for path in files:
        try:
            py_compile.compile(str(path), doraise=True)
        except py_compile.PyCompileError as error:
            failures.append({"path": str(path.relative_to(REPO_ROOT)), "error": str(error)})
    return {"name": "python_compile", "ok": not failures, "checked": len(files), "failures": failures}


def check_json_parse() -> dict:
    files = project_files(["*.json", "*.uplugin"])
    failures: list[dict[str, str]] = []
    for path in files:
        try:
            json.loads(path.read_text(encoding="utf-8"))
        except Exception as error:
            failures.append({"path": str(path.relative_to(REPO_ROOT)), "error": repr(error)})
    return {"name": "json_parse", "ok": not failures, "checked": len(files), "failures": failures}


def run_subprocess(name: str, command: list[str], artifact_dir: Path, *, allow_failure: bool = False) -> dict:
    proc = subprocess.run(command, cwd=REPO_ROOT, text=True, capture_output=True)
    (artifact_dir / f"{name}.stdout.txt").write_text(proc.stdout, encoding="utf-8")
    (artifact_dir / f"{name}.stderr.txt").write_text(proc.stderr, encoding="utf-8")
    ok = proc.returncode == 0 or allow_failure
    return {"name": name, "ok": ok, "returncode": proc.returncode, "command": command}


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify ECABridge local checks")
    parser.add_argument("--smoke", action="store_true", help="Also run scripts/smoke-test.py; requires a running editor server")
    parser.add_argument("--url", default="http://127.0.0.1:3000/mcp", help="MCP URL for --smoke")
    args = parser.parse_args()

    artifact_dir = ARTIFACT_ROOT / run_id()
    artifact_dir.mkdir(parents=True, exist_ok=True)

    checks: list[dict] = []
    checks.append(check_python_compile())
    checks.append(check_json_parse())
    checks.append(run_subprocess(
        "unittest",
        [sys.executable, "-m", "unittest", "discover", "-s", "tests"],
        artifact_dir,
    ))
    checks.append(run_subprocess(
        "lint_skill_tools",
        [sys.executable, "scripts/lint-skill-tools.py", "--skill", "docs/skill/SKILL.md", "--warn-only"],
        artifact_dir,
    ))
    if args.smoke:
        checks.append(run_subprocess(
            "smoke_test",
            [sys.executable, "scripts/smoke-test.py", "--url", args.url],
            artifact_dir,
        ))

    summary = {
        "ok": all(check.get("ok") for check in checks),
        "artifact_dir": str(artifact_dir),
        "checks": checks,
    }
    (artifact_dir / "summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print(json.dumps(summary, indent=2))
    return 0 if summary["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())