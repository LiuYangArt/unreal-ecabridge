---
name: official-mcp-tool-port
description: Use when porting official Epic native ModelContextProtocol tools, ToolsetRegistry toolsets, or AICallable UE tool APIs into ECABridge. Builds a clean-room tool-surface map, detects existing ECABridge equivalents, probes public UE 5.7/5.8 APIs, and validates early to reduce migration time.
---

# Official MCP Tool Port

## Goal
Port official Epic native MCP/toolset behavior into ECABridge without copying Epic source, while reducing manual inventory, API guessing, and late compile failures.

## Required Order
1. Use `$native-mcp-watcher` first and keep its artifact path.
2. Run the scripted shortcuts before manual inspection.
3. Build a surface map before editing: official callable, existing ECABridge equivalent, public UE API, version risk, action.
4. Reuse existing ECABridge commands first. Do not create a new command when an equivalent already exists.
5. Implement by vertical slices: read-only, mutation, runtime/world, then version-guarded features.
6. Compile after the first slice that touches unfamiliar UE APIs. Do not wait until all commands are written.

## Scripted Shortcuts
```powershell
python .agents\skills\official-mcp-tool-port\scripts\surface_map.py --native-dir <EpicToolsetSource> --project-root . --domain-prefix <optional-domain> --out artifacts\native-mcp-port\<area>-surface
python .agents\skills\official-mcp-tool-port\scripts\probe_public_api.py --symbols SymbolA,SymbolB --engine "C:\Program Files\Epic Games\UE_5.8" --engine "C:\Program Files\Epic Games\UE_5.7" --include-path <PublicHeaderRoot>
python .agents\skills\official-mcp-tool-port\scripts\filter_build_errors.py --engine 5.8 --file <NewFile.cpp>
```

- `surface_map.py`: extracts official `AICallable` functions and compares them with ECABridge command names.
- `probe_public_api.py`: checks public header availability across engine versions before editing.
- `filter_build_errors.py`: returns only build errors for the new port file.

## Clean-Room Rules
- Do not paste Epic implementation code.
- Read official code only to extract behavior, public API names, and edge cases.
- Write ECABridge implementation in local style using public UE APIs.
- Prefer snake_case ECABridge command names and avoid native tool-name overlap unless compatibility requires it.

## API Probe Before Porting
Probe launcher/public headers for both engines when possible. If an API differs, prefer the older-compatible public shape or guard with:

```cpp
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
```

## Editing On Windows
If `apply_patch` fails once, switch to `$windows-patch-fallback` and stop retrying `apply_patch`.
Avoid huge PowerShell here-strings. For generated `.cpp`, write in chunks with stable anchors or create a short temporary generator script and delete it.

## Validation Ladder
```powershell
python tools\verify.py
git diff --check
python tools\build_plugin.py --engine-root "C:\Program Files\Epic Games\UE_5.8" --timeout 1200
python tools\build_plugin.py --engine-root "C:\Program Files\Epic Games\UE_5.7" --timeout 1200
```

## Porting Patterns
Read [references/porting-patterns.md](references/porting-patterns.md) when deciding what to reuse, guard, or skip.