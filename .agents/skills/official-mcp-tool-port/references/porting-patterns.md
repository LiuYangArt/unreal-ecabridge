# Official MCP Tool Porting Patterns

## Surface Classification
Classify each official callable before editing:
- `reuse`: ECABridge already has equivalent behavior. Prefer documenting the mapping over duplicating commands.
- `port`: no equivalent exists and behavior can be implemented through public UE APIs.
- `guard`: the API exists only in newer engines or optional plugins. Add build/runtime guards.
- `skip-interactive`: the official tool depends on viewport/user interaction and is not agent-native as-is.

## Common Reuse Signals
- Official names like `Create*`, `Add*`, `Connect*`, `Set*Params`, `Remove*Params` often map to existing ECABridge authoring commands.
- Toolset methods returning editor-only structs can usually be represented as ECABridge JSON result objects.
- Official async wrappers are usually unnecessary unless the user specifically needs progress/cancellation parity.

## Public API Compatibility
Before coding unfamiliar APIs, probe both UE 5.7 and 5.8 public headers. Prefer a compatible public shape over version-specific code.

Observed examples from prior ports:
- A field may be `FText`, not `FString`; serialize with `.ToString()` and assign with `FText::FromString()`.
- Methods may use out parameters in launcher engines even if official tool code suggests a newer helper shape.
- Newer convenience APIs can be missing in UE 5.7; walk older public object relationships when available.
- Include the exact public property header when reflection property subclasses are forward-declared in older engines.

## Preferred ECABridge Shape
- Put parity commands in a focused pair when the area is large:
  - `Source/ECABridge/Public/Commands/ECA<Area>NativeCompatCommands.h`
  - `Source/ECABridge/Private/Commands/ECA<Area>NativeCompatCommands.cpp`
- Register each command with `REGISTER_ECA_COMMAND`.
- Keep outputs machine-readable: ids, paths, classes, pins/links, params, counts, and `_meta` when partial.

## Time-Saving Pattern
1. Implement read-only commands and build once.
2. Add mutation commands and build once.
3. Add runtime/world commands and build once.
4. Add guarded/new-engine-only commands last.

This catches API mismatches near their source instead of producing a long final compile failure list.