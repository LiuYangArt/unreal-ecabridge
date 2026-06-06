# ECABridge Stress Test — Stage Performance scenario

A repo-bound scaffold for the ECABridge full-surface stress test. Built by an autonomous agent during a `/goal` session, results captured to a structured findings file.

## What this tests

Composes ~80% of ECABridge categories into one "Stage Performance" scene:
character on a virtual production stage with HUD, AI controller, GAS abilities,
DMX-driven lights, sequencer cinematic, and a Path Tracer screenshot.

See `~/knowledge/automation/ecabridge-batches/stress-test-design-2026-05-20.md`
in the Agile Lens internal KB for the full design + per-category coverage matrix.

## What ships in this repo

```
examples/StressTest/
├── README.md                    (this file)
├── StressTest.uproject          (5.7 / 5.8 compatible; ECABridge + the required engine plugins)
├── Content/
│   └── StressTest.umap          (empty start level; agent populates dynamically)
├── scripts/
│   ├── build-stress-scene.py    (procedural reference script the agent follows)
│   ├── stress-test-runner.ps1   (orchestrator: fires the /goal session and collects findings)
│   └── findings-schema.json     (JSON schema for per-command findings entries)
└── findings/
    └── .gitkeep                  (findings/<engine>-<date>.json written at runtime, gitignored)
```

`Content/` and `findings/` outside of those starter files are gitignored — agent-generated
Blueprints / Niagara systems / etc. do not pollute the repo.

## How to run

Pre-reqs:
- ECABridge built and loaded; HTTP MCP responding on `:3000`
- All optional plugins enabled in `StressTest.uproject` (see Engine Compatibility wiki)
- Agent identity has access to the KB design doc (lives outside this repo)

Fire the orchestrator:

```powershell
.\scripts\stress-test-runner.ps1 -Engine 5.7
# → captures findings/5.7-YYYYMMDD.json

.\scripts\stress-test-runner.ps1 -Engine 5.8
# → captures findings/5.8-YYYYMMDD.json
```

Each run is a single `/goal` session, ~3-4 hr wall clock. The agent does NOT
fix bugs it encounters — it documents them.

## What a finding looks like

```json
{
  "id": 47,
  "category": "PhysicsAsset",
  "command": "set_body_sphere",
  "task": "character-physics-asset",
  "params": { ... },
  "expected": "Sphere shape added to body for bone with given radius",
  "outcome": "warn",
  "response": { ... },
  "verify_call": {
    "command": "get_body_shapes",
    "response": { ... }
  },
  "notes": "Worked but response omits bone_name — verify call required to confirm correct body",
  "improvement": "set_body_sphere response should echo bone_name",
  "duration_ms": 47
}
```

Outcomes: `ok` / `warn` / `fail`. Every authoring call has a `verify_call` (round-trip read confirming the write landed) per the verify-chain pattern from Batches W/V.

## How to triage findings

After both runs (5.7 + 5.8), diff the two findings files. Three deliverables:

1. **Critical bugs** — `outcome: fail` entries or `warn` entries that block downstream work. → Next gotcha-cleanup batch.
2. **Wart catalog** — `warn` entries that are paper cuts. → Track for future polish batches.
3. **Cross-version drift** — entries that pass on 5.7 but fail on 5.8 (or vice versa). → Engine-compat sweep.

## Why this exists

ECABridge has ~600 MCP commands. Each batch's verify chain proves the commands
register and respond. **None of them prove commands compose correctly into a
real workflow.** The stress test is the integration check we've been deferring.

History: 2026-05-20's Batch GTC fixed 14 gotchas surfaced by one accidental
8-hour real-use session. The stress test is a proactive version of that
discovery process — same scope of findings, planned rather than incidental.
