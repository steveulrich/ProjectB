# ProjectB working guidance

## Scope and authority

- Preserve unrelated work. Complete authorized changes, relevant verification, and fixes before reporting completion. Existing approval remains valid; ask only for a missing consequential decision, expanded scope, unsaved-work conflict, or a permission the tools cannot satisfy.
- Use skills for the requested operation and behavior. Prefer canonical `projectb-*` utilities over legacy personal equivalents; incidental keywords do not trigger skills. Read each applicable skill once per available context and only references needed for the current decision.
- Documentation, planning, and instruction maintenance do not resume a paused gameplay goal. Follow the pause/resume state in [SLICE_STATUS.md](AI_Planning/SLICE_STATUS.md).
- Acceptance requirements live in the selected gate contract and linked acceptance documents. The runbooks below own operational procedures. Resolve scope explicitly; a targeted fix does not require unrelated subsystem redesign or tests, and efficiency changes never waive required proof.

## Work and evidence

- During active vertical-slice work, read the status ledger and run `Scripts/Test/Get-NextIterationBrief.ps1` once with the selected contract and phase. Follow [gate resume and evidence](AI_Planning/Runbooks/gate-resume-evidence.md) for freshness, snapshots, and follow-ups. Historical process state is not current state.
- Select one pending gate and define its expected observable result before editing. After a pass or recorded blocker, continue the next authorized gate while the goal is active. Preserve expected result, evidence, retry condition, and next action before a handoff.
- Reuse candidates and passed evidence only when recorded source, patch, assets, and configuration cover the change. Stop artifact reuse on snapshot/workflow freshness warnings or candidate mismatch until reconciled; retest invalidated results and complete final end-to-end checks.
- Run the cheapest applicable verification first. Build only `LyraEditor` for editor/PIE or `LyraGame` for packaged work through `Scripts/Test/Tier1-CompileGate.ps1`; required dependencies are allowed. Never use solution-wide MSBuild/VS builds, all-target builds, or standalone engine test/utility targets. Apply the [build and package](AI_Planning/Runbooks/build-package.md) preflight and circuit breakers before launching.
- Reuse owned build/test/editor handles; never launch duplicates. Treat hidden editors as a stop signal, identify their owners, and close normally. After two attempts with the same failure and no new evidence, record the cause, remedies, and retry condition; pivot to an independent authorized gate if possible.
- Prefer structured inspection. Require fresh UI observations for visual polish, physical input, frontend flow, and security dialogs. An unavailable UI channel limits proof; it never converts structured checks into physical/UI acceptance.
- Preserve the target: final-public-alpha Breakaway gameplay parity, integrated placeholder art, and a working four-player listen-server LAN flow. Headless, startup, same-process PIE, synthetic input, and same-PC transport results prove only their recorded scope.

## Load procedures on demand

Read only the runbook sections needed for the selected operation, before performing it:

| Operation | Required runbook |
| --- | --- |
| Resume, preflight, provenance, logs, phase records, handoff | [Gate resume and evidence](AI_Planning/Runbooks/gate-resume-evidence.md) |
| Compile, automation, cook, stage, package | [Build and package](AI_Planning/Runbooks/build-package.md) |
| Editor lifecycle, Python bridge, PIE dispatch, readiness, travel | [Editor and PIE](AI_Planning/Runbooks/editor-pie.md) |
| Combat fixtures, remote ownership, lag, replicated samples | [Replication and combat](AI_Planning/Runbooks/replication-combat.md) |
| Packaged transport topology, teardown, shop probes | [Packaged transport and shop](AI_Planning/Runbooks/packaged-transport-shop.md) |
| Buildable placement/quota/cleanup and rematch reset | [Buildables and rematch](AI_Planning/Runbooks/buildables-rematch.md) |

Consult [Scripts/Test/README.md](Scripts/Test/README.md) for the invoked helper's parameters and gate record. Consult [Agent_Testing_Ladder.md](Plugins/GameFeatures/BreakawayCore/Docs/Agent_Testing_Ladder.md) before builds, editor restarts, or multiplayer evidence collection. Use [WORKFLOW_EFFICIENCY.md](AI_Planning/WORKFLOW_EFFICIENCY.md) when measuring or changing workflow efficiency.

## Context budget

- Use `rg` and narrow reads. Default routine tool output to about 1,500 tokens; filter on disk first. Start with compact evidence helpers. Read narrow raw ranges, full records, or historical material only when necessary to resolve the current decision; never ingest them routinely.
- Keep full evidence under `Saved/Logs/`; report outcome, first causal error, scope, and evidence path. Update the ledger after verified results and keep counts/retry reasons with the gate evidence. Replace stale state instead of appending history.
- Keep this file within 6,000 characters and the status ledger within 4,000 characters and about 100 lines. Move detailed procedures and history into linked documents; budgets must not delete acceptance requirements or evidence. Detailed acceptance documents have no blanket 100-line cap.
- Save repeated useful diagnostics as parameterized scripts. Avoid one-off frameworks, unchanged log polling, repeated unsupported probes, and rerunning passed checks without relevant invalidation.
