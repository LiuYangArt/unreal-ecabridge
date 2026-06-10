# AGENTS.md

## Project Shape
- ECABridge is a UE editor plugin that exposes MCP tools. Keep one branch compatible with UE 5.7 and 5.8.
- Source lives in `Source/ECABridge/`; command implementations are usually paired between `Private/Commands/*.cpp` and `Public/Commands/*.h`.
- Public docs live in `README.md`, `CONTRIBUTING.md`, `docs/`, and `docs/skill/SKILL.md`.

## Search And Edits
- Windows repo: use PowerShell commands, `rg` for content search, and targeted reads before opening large files.
- Reuse existing command patterns before adding new structure. Avoid broad refactors unless the user asks or the current fix needs it.
- Do not overwrite unrelated dirty changes. Current user edits are authoritative.

## Validation
- Minimum local validation: `python tools\verify.py`.
- With a running UE editor and ECABridge server: `python scripts\smoke-test.py --url http://127.0.0.1:3000/mcp`.
- If locally available, plugin build/deploy is `powershell -File scripts\build-deploy.ps1 -Engine 5.8`; repeat with `-Engine 5.7` for cross-version risk.
- Verification writes machine-readable output under `artifacts/verify/<run-id>/`.

## Coding Rules
- Follow `CONTRIBUTING.md`: Epic C++ style, tabs, Allman braces, no unrelated formatting sweeps.
- No Epic source code may be copied into this repo. Implement from public APIs or plain-English behavior specs only.
- Optional engine plugin features must stay optional and registration must match build guards.
- Gate 5.8-only APIs so UE 5.7 keeps building.
- For read-side partial results, include `_meta` confidence using existing `MakeECADumpMeta()` patterns.
## Native MCP Compatibility
- Before adding a new MCP feature, check whether Epic's native `ModelContextProtocol` already has the capability or a public API shape to mirror.
- Use the project-local `skills/native-mcp-watcher` workflow. Run `python skills\native-mcp-watcher\scripts\setup_native_mcp_checkout.py --target F:\CodeProjects\UnrealEngine` if the sparse native MCP checkout is missing, then `python tools\native_mcp_watcher.py --ue-repo <UnrealEngineClone> --base origin/release --target origin/ue5-main` to inventory official source changes and create a clean-room port spec.
- Preferred order: reuse public UE APIs, mirror native behavior from a plain-English spec, then add ECABridge-only tooling for gaps.
- Never copy Epic source. Keep clean-room notes in docs or issue context when porting native behavior.
- New tool names must be snake_case, unique in ECABridge, and should avoid native MCP tool-name overlap when practical.
- If overlap is intentional, document why and verify coexistence with `python scripts\smoke-test.py --include-native` when the native server is available.

## Agent-Native Debugging
- Read `docs/agent/debug-rules.md` before non-trivial debugging.
- Prefer let it crash in development. Do not hide failures with broad catch blocks, silent fallback, or vague logs.
- Treat 验证, 日志, 截图, and 崩溃 evidence as first-class agent inputs.
- Keep logs, screenshots, dumps, traces, and validation summaries under `artifacts/<run-id>/`.
- If debugging takes more than one iteration or reveals a non-obvious root cause, write `docs/postmortems/<date>-<slug>.md` using `docs/agent/postmortem-template.md`.
- Use `docs/agent/subagent-rules.md` only when the user explicitly asks for sub-agent work, subagents, or delegation.