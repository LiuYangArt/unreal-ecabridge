# Sub-agent Rules

## When To Use
Use sub-agents only when the user explicitly asks for subagents, delegation, or parallel agent work.

Good uses:
- Codebase exploration and architecture mapping.
- Log, test, screenshot, or artifact analysis.
- Independent review of a proposed fix.
- Documentation or API research.
- Parallel implementation only when ownership boundaries are clear.

Avoid sub-agents when parallel edits would touch the same files or when the main agent must immediately inspect every intermediate detail.

## Main Agent Responsibilities
- Keep requirements, design decisions, and acceptance criteria in the main context.
- Split work into small tasks with explicit ownership.
- Integrate results and run final verification.
- Produce the final user-facing summary.

## Worker Prompt Checklist
- Goal.
- Owned files or modules.
- Forbidden areas.
- Expected output.
- Validation command.
- Requirement to report changed files, verification result, and risks.

## Result Format
Return distilled findings, changed paths, validation output, and risks. Do not dump raw logs unless requested.