# Agent Debug Rules

## Goal
Make debugging observable, repeatable, and easy to verify without relying on repeated human testing.

## Rules
- Reproduce first. Record the exact command, asset, map, tool call, input file, or editor action that triggers the issue.
- Collect machine-readable evidence before guessing: logs, stack traces, screenshots, JSON dumps, metrics, or test output.
- Prefer let-it-crash during development. Do not hide failures with broad `try/catch`, silent fallback, default data, or vague logs.
- If a catch is necessary, add concrete context and rethrow unless there is a deliberate recovery strategy.
- Write generated evidence under `artifacts/<run-id>/` and mention that path in the final result.
- After a fix, run the smallest command that proves the affected path works.
- If debugging took more than one iteration or exposed a non-obvious root cause, add a postmortem in `docs/postmortems/`.

## Good Pattern
```cpp
if (!Asset)
{
	return ErrorResponse(TEXT("Failed to load asset: ") + AssetPath);
}
```

## Bad Patterns
```cpp
try { RunStep(); } catch (...) {}
```

```cpp
if (!Asset)
{
	return SuccessResponse(DefaultObject);
}
```