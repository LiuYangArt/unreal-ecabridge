---
name: native-mcp-watcher
description: Track Epic Unreal Engine native ModelContextProtocol / native MCP source changes for ECABridge clean-room porting. Use when comparing Epic ue5-main vs release, setting up a sparse Epic UnrealEngine checkout for ModelContextProtocol, porting new native MCP protocol features or toolsets to ECABridge/UE 5.7, checking tool-name overlap, or producing port/mirror-api/ignore reports without copying Epic source.
---

# Native MCP Watcher

## Setup Checkout
Use the bundled setup script when the local Epic UnrealEngine checkout is missing or stale:

```powershell
python skills\native-mcp-watcher\scripts\setup_native_mcp_checkout.py --target F:\CodeProjects\UnrealEngine
```

It creates or updates a partial+sparse checkout of `https://github.com/EpicGames/UnrealEngine.git`, checks out `ue5-main`, limits files to native MCP plus AI toolset plugins (`Engine/Plugins/Experimental/ModelContextProtocol`, `Engine/Plugins/Experimental/ToolsetRegistry`, and `Engine/Plugins/Experimental/Toolsets`), and fetches `origin/release`, `origin/5.8`, and `origin/5.7` for diffing.

## Workflow
1. Ensure a local Epic `UnrealEngine` checkout exists. The repo is private; the user must have GitHub access linked to Epic.
2. Prefer the project script when inside ECABridge:
   ```powershell
   python tools\native_mcp_watcher.py --ue-repo <UnrealEngineClone> --base origin/release --target origin/ue5-main --fetch
   ```
3. If the project script is missing, run this skill's bundled watcher from the ECABridge repo root:
   ```powershell
   python skills\native-mcp-watcher\scripts\native_mcp_watcher.py --project-root . --ue-repo <UnrealEngineClone>
   ```
4. Inspect the changed Epic files in the local UnrealEngine clone to understand behavior.
5. Fill `artifacts/native-mcp-watcher/<run-id>/clean-room-spec-template.md` in your own words. Do not copy source snippets.
6. Classify each change as `port`, `mirror-api`, or `ignore`.
7. Implement useful native MCP behavior in ECABridge using public UE APIs, existing command/schema patterns, and UE 5.7 guards.
8. Avoid native MCP tool-name overlap unless compatibility requires it. If overlap is intentional, document why and run `python scripts\smoke-test.py --include-native` when native MCP is available.

## Output
- `report.json`: changed paths, commit subjects, candidate native tool names, ECABridge registered-name overlaps.
- `report.md`: concise porting triage report. It intentionally excludes source snippets.
- `clean-room-spec-template.md`: behavior spec scaffold for the actual port.

## Notes
- Default watched paths: `Engine/Plugins/Experimental/ModelContextProtocol`, `Engine/Plugins/Experimental/ToolsetRegistry`, and `Engine/Plugins/Experimental/Toolsets`.
- Use repeated `--path` arguments if Epic moves native MCP files.
- The watcher needs refs already fetchable by git; use `--fetch` to update remotes.