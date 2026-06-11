#!/usr/bin/env python3
"""Finalize a Blueprint graph after ECABridge MCP edits."""
from __future__ import annotations

import argparse
import json
from typing import Any

from eca_client import ECAClient
from eca_defaults import DEFAULT_MCP_URL


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Layout, compile, and optionally save a Blueprint graph")
    parser.add_argument("--url", default=DEFAULT_MCP_URL, help="ECABridge MCP URL")
    parser.add_argument("--blueprint", required=True, help="Blueprint content path, e.g. /Game/Blueprints/BP_Foo")
    parser.add_argument("--graph", default="EventGraph", help="Graph name")
    parser.add_argument("--start-x", type=int, default=0)
    parser.add_argument("--start-y", type=int, default=0)
    parser.add_argument("--padding-x", type=int, default=120)
    parser.add_argument("--padding-y", type=int, default=40)
    parser.add_argument("--materialize-long-edges", action="store_true")
    parser.add_argument("--no-comments", action="store_true", help="Do not rewrap existing comment boxes")
    parser.add_argument("--cleanup-orphans", action="store_true", help="Run cleanup_orphan_nodes before layout")
    parser.add_argument("--delete-orphans", action="store_true", help="Delete orphans when --cleanup-orphans is set")
    parser.add_argument("--no-save", action="store_true", help="Compile but do not save the asset")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    client = ECAClient(args.url, timeout=120.0)

    loaded_categories: list[dict[str, Any]] = []
    for category in ("Blueprint", "Blueprint Node", "Asset", "Editor"):
        loaded_categories.append(dict(client.call("load_category", category=category)))

    cleanup_result: dict[str, Any] | None = None
    if args.cleanup_orphans:
        cleanup_result = dict(client.call(
            "cleanup_orphan_nodes",
            blueprint_path=args.blueprint,
            graph_name=args.graph,
            delete=args.delete_orphans,
        ))

    layout_result = dict(client.call(
        "auto_layout_blueprint_graph",
        blueprint_path=args.blueprint,
        graph_name=args.graph,
        layout_engine="layered",
        start_x=args.start_x,
        start_y=args.start_y,
        padding_x=args.padding_x,
        padding_y=args.padding_y,
        materialize_long_edges=args.materialize_long_edges,
        align_comments=not args.no_comments,
    ))
    compile_result = dict(client.call("compile_blueprint", blueprint_path=args.blueprint))
    save_result = None if args.no_save else dict(client.call("save_asset", asset_path=args.blueprint))

    print(json.dumps({
        "success": True,
        "blueprint_path": args.blueprint,
        "graph_name": args.graph,
        "loaded_categories": loaded_categories,
        "cleanup": cleanup_result,
        "layout": layout_result,
        "compile": compile_result,
        "save": save_result,
    }, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
