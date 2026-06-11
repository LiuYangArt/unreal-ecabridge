# Graph Auto Layout Extension Plan

## Current State

`auto_layout_blueprint_graph` is a K2 Blueprint adapter over the engine-agnostic core in:

- `Source/ECABridge/Private/Commands/BlueprintLayout/LayeredGraphLayout.h`
- `Source/ECABridge/Private/Commands/BlueprintLayout/LayeredGraphLayout.cpp`

The reusable core only knows:

- vertices with width, height, original position, solved rank/order/position
- directed edges with source and target pin Y offsets
- rank assignment
- crossing reduction
- pin-aware coordinate relaxation

The Blueprint-specific adapter lives in:

- `Source/ECABridge/Private/Commands/BlueprintAutoLayout.h`
- `Source/ECABridge/Private/Commands/BlueprintAutoLayout.cpp`
- `FECACommand_AutoLayoutBlueprintGraph::Execute` in `ECABlueprintNodeCommands.cpp`

It is intentionally K2-specific: it skips comment and knot nodes, traces through `UK2Node_Knot`, classifies K2 exec pins, writes `UEdGraphNode::NodePosX/Y`, calls `Graph->NotifyGraphChanged()`, and marks the Blueprint structurally modified.

## Goal

Add graph-specific auto-layout commands that reuse `FLayeredGraphLayout` but keep each Unreal graph type's adapter separate.

Recommended command names:

- `auto_layout_material_graph`
- `auto_layout_pcg_graph`
- `auto_layout_animation_graph`
- `auto_layout_control_rig_graph`

Do not force these through `auto_layout_blueprint_graph`. Each graph type has different node classes, pin semantics, editor refresh paths, and asset dirty/compile behavior.

## Adapter Pattern

Each adapter should do the same six steps:

1. Resolve the asset and graph object.
2. Collect real layout nodes, excluding comments, frames, reroutes, or graph-type metadata nodes.
3. Convert nodes to `FLayeredGraphLayout` vertices using measured size when available, deterministic fallback size otherwise.
4. Convert graph links to directed edges, tracing through reroutes when the graph type has them.
5. Solve with `FLayeredGraphLayout` and write positions through native C++ fields/APIs.
6. Notify the owning editor graph, mark the asset dirty, and return machine-readable diagnostics.

Suggested result shape:

```jsonc
{
  "success": true,
  "asset_path": "/Game/...",
  "graph_name": "...",
  "layout_engine": "layered",
  "nodes_positioned": 12,
  "edge_count": 15,
  "rank_count": 6,
  "comments_wrapped": 0,
  "auto_knots_removed": 0,
  "auto_knots_created": 0,
  "warnings": []
}
```

## Type Notes

### Material Graph

Likely next best target.

- Asset: `UMaterial`, `UMaterialFunction`, or material instance graph if supported.
- Graph/node classes are material editor specific; inspect existing `ECAMaterialNodeCommands.cpp` first.
- Main layout direction should be data-flow left-to-right toward material output nodes.
- Outputs/root sinks should usually be the final material output or function output nodes.
- Refresh path must use material editor utilities already used by material mutation commands.

Validation:

- Dump node positions before/after if a dump command exists, or add one.
- Verify no overlapping non-comment nodes.
- Verify material recompiles or asset saves without errors.

### PCG Graph

Good second target.

- Asset: `UPCGGraph` / PCG graph asset.
- Graph is data-flow oriented, so rank from upstream data producers to output nodes.
- PCG pins have type constraints; edge direction should follow actual data links, not visual position.
- Preserve graph-specific reroute/label/comment nodes unless explicitly tagged by ECABridge.

Validation:

- Use existing PCG dump/introspection commands as the before/after source.
- Verify repeated layout is idempotent.
- If runtime execution is available, run a small PCG validation after layout.

### Animation Blueprint

Split this into K2 and animation graph work.

- K2 EventGraph/function graphs can keep using `auto_layout_blueprint_graph` when exposed by the Blueprint graph lists.
- AnimGraph and state machines need a separate adapter.
- State machines are not simple left-to-right data flow; use state graph semantics, entry state, transitions, and possibly nested graphs.

Validation:

- Compile the animation Blueprint.
- Dump state machine/transition positions if existing commands support it.
- Avoid repositioning transition rule graphs until explicitly handled.

### Control Rig / RigVM

Higher risk.

- Use existing Control Rig/RigVM commands as reference before adding layout.
- RigVM has its own graph model and editor refresh/compile path.
- Treat reroutes and collapsed graphs carefully; avoid topology mutation in v1.

Validation:

- Compile/recompile Control Rig after layout.
- Verify before/after dump has the same node/link count.
- Test on one small rig graph before broad use.

## Implementation Order

1. Extract a tiny shared adapter helper only if two adapters duplicate the same code.
2. Implement `auto_layout_material_graph` first.
3. Implement `auto_layout_pcg_graph` second.
4. Add animation graph only after inspecting AnimGraph/state-machine storage.
5. Add Control Rig last.

Keep each new adapter narrow. Do not add toolbar UI or editor preferences until command behavior is validated through MCP.

## Pre-Layout Cleanup

Project-side `Tools/eca_cleanup_blueprint_graph.py` is a useful pattern for Blueprint finalization, but cleanup should not be embedded into layout commands by default.

Current cleanup behavior:

- Loads `Blueprint`, `Blueprint Node`, and `Asset` categories.
- Dumps one Blueprint graph with positions.
- Finds unconnected nodes whose class is in an allowlist.
- Defaults the allowlist to `K2Node_VariableGet`.
- Deletes those nodes, compiles the Blueprint, and saves unless `--dry-run` or `--no-save` is set.

Recommended integration:

- Keep cleanup in finalize/post-edit wrappers before layout.
- Make cleanup opt-in, or at least constrained by an explicit allowlist.
- Preserve `--dry-run` so agents can report candidates before deletion.
- Run order should be cleanup, layout, compile, save.
- Do not apply this Blueprint cleanup rule to Material, PCG, Animation Graph, or Control Rig until each graph type has its own safe orphan-node definition.
## Guardrails

- Do not silently swallow layout failures. Return explicit `warnings` or an error.
- Do not mutate graph topology unless an option explicitly requests it.
- If auto-created reroute nodes are added later, tag them and remove only tagged nodes on repeat runs.
- Preserve user-authored comments/reroutes/frames by default.
- Keep UE 5.7 and 5.8 compatibility. Gate version-specific APIs.
- Every command should write enough result data for agents to verify layout without screenshots.

## Minimal Validation For Each New Adapter

Run local checks:

```powershell
python tools\verify.py
git diff --check
```

With Unreal Editor and ECABridge server running:

1. Dump graph positions before layout.
2. Run the new layout command twice.
3. Dump graph positions after each run.
4. Verify non-comment nodes do not overlap.
5. Verify node/link counts are unchanged unless a reroute option was explicitly enabled.
6. Compile or validate the owning asset.
7. Save artifacts under `artifacts/<graph-type>-layout/<run-id>/`.