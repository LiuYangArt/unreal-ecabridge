# Agent Workflow

Canonical location: `docs/agent/agent-workflow.md`.

## Default Loop
1. Locate the relevant files with `rg --files` or targeted filename search.
2. Read the smallest useful slices before editing.
3. Reuse existing command, schema, and registration patterns.
4. Make the smallest sufficient change.
5. Run `python tools\verify.py` or a narrower command that proves the change.
6. Save debug evidence under `artifacts/<run-id>/` when the task produced logs, screenshots, dumps, or traces.

## Common Entry Points
- Command contract: `Source/ECABridge/Public/Commands/*.h`.
- Command implementation: `Source/ECABridge/Private/Commands/*.cpp`.
- Registration pattern: `REGISTER_ECA_COMMAND(...)`.
- Tool surface lint: `python scripts\lint-skill-tools.py --skill docs\skill\SKILL.md --warn-only`.
- Runtime smoke: `python scripts\smoke-test.py` with the editor running.

## Native MCP Triage
- First check whether Epic's native `ModelContextProtocol` already ships the requested capability.
- Use `python tools\native_mcp_watcher.py --ue-repo <UnrealEngineClone> --base origin/release --target origin/ue5-main` to inventory official source changes and create a clean-room port spec.
- If native has it, mirror the public behavior clean-room and avoid tool-name overlap unless compatibility requires it.
- If native does not have it, implement ECABridge-only tooling using existing category and schema patterns.
- For coexistence risk, run `python scripts\smoke-test.py --include-native` when the native server is available.

## Escalate Before
- Copying or closely paraphrasing Epic source.
- Adding mandatory dependencies on optional engine plugins.
- Large refactors across command categories.
- Changing generated MCP client config behavior.