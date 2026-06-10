# Native MCP Watcher

Purpose: compare Epic's native `ModelContextProtocol` source changes against ECABridge so we can clean-room port useful native MCP features to launcher builds and UE 5.7.

This is a porting workflow, not just a collision checker. The script inventories official native MCP changes, then produces clean-room artifacts that guide an ECABridge implementation without copying Epic source.

## Requirements
- A local Epic `UnrealEngine` git clone that your GitHub account can access.
- The clone must have the refs you compare, usually `origin/release` and `origin/ue5-main`.

## Run
```powershell
python skills\native-mcp-watcher\scripts\setup_native_mcp_checkout.py --target F:\CodeProjects\UnrealEngine
python tools\native_mcp_watcher.py --ue-repo F:\CodeProjects\UnrealEngine --base origin/release --target origin/ue5-main --fetch
```

Output is written to `artifacts/native-mcp-watcher/<run-id>/`:
- `report.json`: machine-readable paths, commits, candidate tool names, and ECABridge name overlaps.
- `report.md`: short triage report for port / mirror-api / ignore decisions.
- `clean-room-spec-template.md`: fill this in after inspecting Epic source; keep it behavior-only, no source snippets.

## Porting Rules
- You may inspect Epic source in the local UnrealEngine clone to understand behavior.
- Do not paste Epic source into ECABridge docs, code, issues, reports, or specs.
- Write behavior notes in your own words before implementing.
- Implement with public UE APIs and existing ECABridge command/schema patterns.
- Preserve UE 5.7 compatibility with guards or ECABridge-specific equivalents.
- Avoid native MCP tool-name overlap unless compatibility requires it; if overlap is intentional, document why.

## Useful Options
```powershell
# Use another watched path
python tools\native_mcp_watcher.py --ue-repo F:\UnrealEngine --path Engine/Plugins/Experimental/ModelContextProtocol

# Print JSON without artifacts
python tools\native_mcp_watcher.py --ue-repo F:\UnrealEngine --json-only
```