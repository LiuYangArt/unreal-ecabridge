# ECABridge Build Notes

## HOMEPCWS Project

- Workspace root: `F:\P4V\liuyang_HOMEPCWS_Env`
- Project: `F:\P4V\liuyang_HOMEPCWS_Env\Paralogue.uproject`
- Engine association: `5.7`
- Typical engine root: `C:\Program Files\Epic Games\UE_5.7`
- Build target: `ParalogueEditor Win64 Development`
- Plugin binary: `F:\P4V\liuyang_HOMEPCWS_Env\Plugins\ECABridge\Binaries\Win64\UnrealEditor-ECABridge.dll`
- Codex MCP config: `F:\P4V\liuyang_HOMEPCWS_Env\.codex\config.toml`
- Expected ECABridge URL: `http://127.0.0.1:8831/mcp`

## Last Known Compile Fix

A copied `main` build failed in `ECAPcgAuthoringCommands.cpp` because a namespace-level static helper called `MakeResult()`. That helper is protected on `IECACommand`, so static helpers should allocate directly:

```cpp
TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
```

After this change, `Build.bat ParalogueEditor Win64 Development <uproject> -WaitMutex` succeeded and refreshed `UnrealEditor-ECABridge.dll`.

## Minimal Manual Build Command

```powershell
& "C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" ParalogueEditor Win64 Development "F:\P4V\liuyang_HOMEPCWS_Env\Paralogue.uproject" -WaitMutex
```