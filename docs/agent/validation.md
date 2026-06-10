# Validation

## Minimum Check
```powershell
python tools\verify.py
```

This runs local checks that do not require Unreal Editor:
- Python compile check for tracked project scripts.
- JSON and `.uplugin` parse check.
- Tool registry vs. skill-doc lint in warn-only mode.
- Artifact summary at `artifacts/verify/<run-id>/summary.json`.

## Runtime Smoke
With Unreal Editor open and ECABridge listening on `127.0.0.1:3000`:

```powershell
python scripts\smoke-test.py --url http://127.0.0.1:3000/mcp
```

To also probe Epic's native MCP server on `127.0.0.1:8000`:

```powershell
python scripts\smoke-test.py --include-native
```

## Build Risk
For changes touching module dependencies, optional plugins, or UE version guards, run the local build/deploy script if present:

```powershell
powershell -File scripts\build-deploy.ps1 -Engine 5.8
powershell -File scripts\build-deploy.ps1 -Engine 5.7
```