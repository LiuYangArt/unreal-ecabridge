---
name: ecabridge-build
description: Build, compile, and verify ECABridge inside Unreal Engine project workspaces on Windows. Use when updating ECABridge in a UE project, rebuilding the plugin DLL, checking MCP port configuration, diagnosing UnrealBuildTool failures, or validating that a project copy matches the GitHub ECABridge source.
---

# ECABridge Build

## Quick Workflow

1. Confirm the source repo is clean or intentionally dirty:
   ```powershell
   git status -sb
   ```
2. For a copied project plugin, compare or sync from the Git source before building. For the HOMEPCWS project, the target is:
   ```text
   F:\P4V\liuyang_HOMEPCWS_Env\Plugins\ECABridge
   ```
3. Prefer the bundled build helper:
   ```powershell
   python skills\ecabridge-build\scripts\build_ecabridge_project.py --project F:\P4V\liuyang_HOMEPCWS_Env\Paralogue.uproject
   ```
4. If the helper reports UnrealEditor is running, close the editor and rerun. The ECABridge DLL can be locked while the editor is open.
5. After a successful build, verify the server URL and port:
   ```powershell
   rg -n "127\.0\.0\.1:3000|localhost:3000|:3000|ServerPort = 3000" F:\P4V\liuyang_HOMEPCWS_Env\Plugins\ECABridge F:\P4V\liuyang_HOMEPCWS_Env\.codex\config.toml
   ```
   This should return no port matches. Expected default MCP URL is `http://127.0.0.1:8831/mcp`.

## Build Helper

Use `scripts/build_ecabridge_project.py` for the fragile parts:

- Locate the `.uproject` and infer the target name, for example `ParalogueEditor`.
- Resolve `EngineAssociation` such as `5.7` to `C:\Program Files\Epic Games\UE_5.7` unless `--engine-root` is passed.
- Refuse to build while `UnrealEditor.exe` is running unless `--skip-editor-check` is passed.
- Check ECABridge default port and optional `.codex/config.toml` before compiling.
- Run UnrealBuildTool through `Engine\Build\BatchFiles\Build.bat`.
- Surface compiler error lines and verify `UnrealEditor-ECABridge.dll` exists. Use `--require-fresh-dll` when a rebuild must relink the plugin.

Useful options:

```powershell
python skills\ecabridge-build\scripts\build_ecabridge_project.py --project <Project.uproject> --dry-run
python skills\ecabridge-build\scripts\build_ecabridge_project.py --project <Project.uproject> --engine-root <UE_ROOT>
python skills\ecabridge-build\scripts\build_ecabridge_project.py --project <Project.uproject> --config-file <workspace>\.codex\config.toml
```

## Known Failure Pattern

If a static helper in a command `.cpp` calls `MakeResult()`, it can fail with:

```text
error C3861: 'MakeResult': identifier not found
```

`MakeResult()` is a protected `IECACommand` helper. In namespace/static helper functions, use `MakeShared<FJsonObject>()` instead, then rebuild.

## Validation

For source repo changes, run:

```powershell
python tools\verify.py
git diff --check
```

For project plugin copies, run the build helper and confirm:

- `Result: Succeeded`
- `Plugins\ECABridge\Binaries\Win64\UnrealEditor-ECABridge.dll` exists and has the expected timestamp
- `.codex\config.toml` points to `http://127.0.0.1:8831/mcp`
- no `:3000` ECABridge port references remain

## Perforce Workspace Notes

`F:\P4V\liuyang_HOMEPCWS_Env` is a Perforce workspace, not a Git repo. Do not run Git workflow there. Use Git only in `F:\CodeProjects\unreal-ecabridge`; copy or sync plugin files into the P4 workspace as a deployment step.