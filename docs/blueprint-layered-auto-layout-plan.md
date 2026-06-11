# Blueprint Layered Auto Layout Plan

## Decision

Implement this inside ECABridge MCP, not as a separate Unreal Editor plugin.

The desired user workflow is:

1. An agent edits a Blueprint through ECABridge MCP.
2. The agent calls a post-edit command or script.
3. ECABridge cleans MCP leftovers, lays out the graph, compiles, and saves.
4. The Blueprint graph is readable without manual editor cleanup.

Do not install or depend on `ibrews/blueprint-auto-layout` as an external plugin.
Use it as a reference or MIT-licensed source for the layout algorithm.

## Reference

Project: https://github.com/ibrews/blueprint-auto-layout

Useful parts:

- `Source/BlueprintAutoLayout/Private/BPALLayeredLayout.h`
- `Source/BlueprintAutoLayout/Private/BPALLayeredLayout.cpp`
- `Source/BlueprintAutoLayout/Public/BlueprintAutoLayout.h`
- `Source/BlueprintAutoLayout/Private/BlueprintAutoLayout.cpp`
- `tests/test_layered.cpp`

License: MIT. If code is copied or adapted, preserve the copyright and MIT license notice in the relevant files or a local third-party notice.

## Current ECABridge State

Existing command:

- `auto_layout_blueprint_graph`

Relevant files:

- `Source/ECABridge/Public/Commands/ECABlueprintNodeCommands.h`
- `Source/ECABridge/Private/Commands/ECABlueprintNodeCommands.cpp`
- `Source/ECABridge/Private/Commands/BlueprintAutoLayout.h`
- `Source/ECABridge/Private/Commands/BlueprintAutoLayout.cpp`
- `Source/ECABridge/Private/Commands/ECABlueprintLispCommands.cpp`

Current layout is tree-based. It helps simple graphs but is weak for:

- overlapping MCP-created nodes
- cross-row links
- pure/data node chains
- `Sequence` lanes
- branch/multi-output exec nodes
- long wires crossing node bodies
- comment box rewrapping
- reroute knot positioning

## Target Behavior

Upgrade `auto_layout_blueprint_graph` to support a layered graph layout.

Required behavior:

- Lay out Blueprint K2 graphs left-to-right by execution/data dependency.
- Avoid node overlap using measured or estimated node sizes.
- Keep exec spines as straight as practical by aligning connected pin Y offsets.
- Place pure provider nodes close to their consumers.
- Handle `Sequence` and branch-like multi-output nodes without stacking all children on one lane.
- Ignore comment boxes during solve, then rewrap existing comments around their original members.
- Treat reroute knots as pass-through while solving.
- Optionally regenerate auto-created reroute knots for long edges; avoid accumulating duplicate knots.
- Mark Blueprint structurally modified and notify the graph after layout.

Non-goals for first implementation:

- Do not add toolbar UI, keyboard shortcuts, or Editor preference panels.
- Do not support Material/Niagara/BehaviorTree graphs.
- Do not install the third-party plugin.

## Proposed Command Surface

Keep the existing command name:

```jsonc
{
  "blueprint_path": "/Game/Path/BP_Name",
  "graph_name": "EventGraph",
  "layout_engine": "layered",
  "start_x": 0,
  "start_y": 0,
  "padding_x": 120,
  "padding_y": 40,
  "materialize_long_edges": true,
  "align_comments": true,
  "node_ids": []
}
```

Compatibility:

- Existing callers with only `blueprint_path` and `graph_name` must continue to work.
- Existing deprecated `strategy` values may be accepted but should route to the layered engine by default.
- Add `layout_engine: "legacy_tree"` only if keeping the old algorithm is useful for fallback/debug.

Suggested result fields:

```jsonc
{
  "success": true,
  "graph_name": "EventGraph",
  "layout_engine": "layered",
  "nodes_positioned": 12,
  "comments_wrapped": 1,
  "auto_knots_removed": 3,
  "auto_knots_created": 4,
  "warnings": []
}
```

## Implementation Plan

### 1. Add Engine-Agnostic Layered Core

Add files under one of these locations:

- Preferred: `Source/ECABridge/Private/Commands/BlueprintLayout/LayeredGraphLayout.h/.cpp`
- Acceptable: `Source/ECABridge/Private/Commands/BPALLayeredLayout.h/.cpp`

Port the engine-agnostic part from `BPALLayeredLayout.*`:

- vertices with width/height
- directed edges with source/dest pin Y offsets
- rank assignment
- dummy nodes for long edges
- crossing reduction
- pin-aware coordinate assignment
- no Unreal types in this core

Keep a standalone test equivalent to `tests/test_layered.cpp` if practical.

### 2. Upgrade Unreal Adapter

Update `FBlueprintAutoLayout` to build the layered graph from `UEdGraph`:

- Include all real Blueprint nodes except comments and knots.
- Classify comments and knots separately.
- Collect edges from output pins to real input pins.
- Trace through reroute knots when collecting edges.
- Estimate pin Y offsets from pin order if live Slate widget size is unavailable.
- Use existing `NodeWidth` / `NodeHeight`, falling back to estimates.
- After solve, write `NodePosX` and `NodePosY`.

Important: use direct C++ mutation with `Node->Modify()` and Blueprint editor utils. Do not reposition nodes through Python reflection.

### 3. Comments And Knots

Comments:

- Before layout, capture which nodes each comment geometrically contains.
- After layout, resize/reposition comments around those same members.
- Preserve comment text and color.

Knots:

- Existing user-created knots should be treated as pass-through where possible.
- Auto-created knots must be tagged, e.g. `NodeComment = "ECABridgeAutoRoute"`.
- Before rerouting, remove only tagged auto knots and reconnect source/destination pins.
- Recreate tagged knots only when `materialize_long_edges` or route mode is enabled.

### 4. MCP Post-Edit Script

After the C++ command works, add a Python wrapper script, for example:

- `scripts/eca-finalize-blueprint-graph.py`, or
- project-side `Tools/eca_finalize_blueprint_graph.py`

Behavior:

1. `load_category` for Blueprint / Blueprint Node / Asset / Editor.
2. Optional pre-layout cleanup of disconnected MCP leftovers, using the same rule as project-side `Tools/eca_cleanup_blueprint_graph.py`: delete unconnected nodes whose class is in an allowlist, defaulting to `K2Node_VariableGet`.
3. Call `auto_layout_blueprint_graph`.
4. Call `compile_blueprint`.
5. Call `save_asset` unless `--no-save`.
6. Print JSON summary.

Cleanup should stay in the wrapper, not inside `auto_layout_blueprint_graph`, because it mutates graph topology. Keep it explicit or opt-in, support `--dry-run`, and never delete arbitrary orphan nodes without an allowlist.
Example CLI:

```powershell
python Tools/eca_finalize_blueprint_graph.py --blueprint /Game/Path/BP_Name --graph EventGraph
```

This wrapper is what agents should call after MCP graph edits.

## Validation Plan

Minimum local validation:

```powershell
python tools\verify.py
```

With a running editor and ECABridge server:

```powershell
python scripts\smoke-test.py --url http://127.0.0.1:8831/mcp
```

Add or update smoke coverage:

1. Create or use a test Blueprint graph with deliberately overlapping nodes.
2. Call `auto_layout_blueprint_graph`.
3. Dump graph with `include_positions: true`.
4. Verify no positioned non-comment nodes overlap by bounding boxes.
5. Verify compile succeeds.
6. Verify repeated layout does not increase tagged auto-knot count.

Suggested artifact output:

- before dump JSON
- after dump JSON
- overlap report JSON
- command result JSON
- optional before/after screenshots if editor automation is available

## Risk Notes

- UE 5.7/5.8 compatibility matters. Guard any version-specific API.
- Live Slate node size lookup may not always be available; keep deterministic fallback estimates.
- Long-edge knot regeneration mutates graph topology; keep it optional and idempotent.
- Do not silently swallow layout failures. Return warnings or errors visible to MCP callers.
- Keep the old tree layout temporarily if needed for rollback, but default should become layered once validated.

## Agent Checklist

Before coding:

- Read this document.
- Read `docs/new-layout-commands-2026-05-20.md`.
- Read current `BlueprintAutoLayout.*` and `FECACommand_AutoLayoutBlueprintGraph::Execute`.
- Inspect the reference project's `BPALLayeredLayout.*` and Unreal adapter.

Implementation checklist:

- Add layered core.
- Wire layered core into `FBlueprintAutoLayout`.
- Preserve current MCP command compatibility.
- Add result fields for diagnostics.
- Add cleanup/finalize script only after the command works.
- Add smoke or verification coverage.
- Run minimum validation and record artifacts.

Definition of done:

- `auto_layout_blueprint_graph` lays out MCP-generated Blueprint graphs without overlapping nodes.
- Repeated runs are idempotent for auto-created knots.
- Blueprint compiles after layout.
- A Python finalizer command exists for agents to call after graph edits.
- Validation output is machine-readable.