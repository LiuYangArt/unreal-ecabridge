#!/usr/bin/env python3
"""Create or update a sparse UnrealEngine checkout for native MCP source."""
from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


DEFAULT_REPO_URL = "https://github.com/EpicGames/UnrealEngine.git"
DEFAULT_PATHS = [
    "Engine/Plugins/Experimental/ModelContextProtocol",
    "Engine/Plugins/Experimental/ToolsetRegistry",
    "Engine/Plugins/Experimental/Toolsets",
]
DEFAULT_REFS = ["release", "5.8", "5.7"]


def run(cmd: list[str], *, cwd: Path | None = None) -> None:
    subprocess.run(cmd, cwd=cwd, check=True, text=True)


def is_git_repo(path: Path) -> bool:
    if not path.exists():
        return False
    proc = subprocess.run(["git", "rev-parse", "--is-inside-work-tree"], cwd=path, text=True, capture_output=True)
    return proc.returncode == 0 and proc.stdout.strip() == "true"


def setup_checkout(target: Path, repo_url: str, branch: str, sparse_paths: list[str], refs: list[str], fetch: bool) -> None:
    target = target.resolve()
    if target.exists() and not is_git_repo(target):
        raise RuntimeError(f"Target exists but is not a git repo: {target}")

    if not target.exists():
        run([
            "git", "clone",
            "--filter=blob:none",
            "--sparse",
            "--branch", branch,
            "--single-branch",
            repo_url,
            str(target),
        ])
    else:
        run(["git", "remote", "set-url", "origin", repo_url], cwd=target)
        if fetch:
            run(["git", "fetch", "--filter=blob:none", "origin", branch], cwd=target)
        run(["git", "checkout", branch], cwd=target)

    run(["git", "sparse-checkout", "set", *sparse_paths], cwd=target)

    if fetch:
        for ref in refs:
            run(["git", "fetch", "--filter=blob:none", "--depth=1", "origin", f"{ref}:refs/remotes/origin/{ref}"], cwd=target)

    for sparse_path in sparse_paths:
        plugin_path = target / sparse_path
        if not plugin_path.exists():
            raise RuntimeError(f"Sparse path was not checked out: {plugin_path}")
        print(str(plugin_path))


def main() -> int:
    parser = argparse.ArgumentParser(description="Create/update sparse UnrealEngine checkout for native MCP")
    parser.add_argument("--target", default=r"F:\CodeProjects\UnrealEngine", help="Target UnrealEngine checkout directory")
    parser.add_argument("--repo-url", default=DEFAULT_REPO_URL, help="Epic UnrealEngine git URL")
    parser.add_argument("--branch", default="ue5-main", help="Branch to checkout")
    parser.add_argument("--sparse-path", action="append", default=None, help="Sparse path to checkout; repeatable")
    parser.add_argument("--ref", action="append", dest="refs", default=None, help="Extra ref to fetch; repeatable")
    parser.add_argument("--no-fetch", action="store_true", help="Skip fetch for existing clones and extra refs")
    args = parser.parse_args()

    setup_checkout(
        target=Path(args.target),
        repo_url=args.repo_url,
        branch=args.branch,
        sparse_paths=args.sparse_path or DEFAULT_PATHS,
        refs=args.refs or DEFAULT_REFS,
        fetch=not args.no_fetch,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())