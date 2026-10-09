# Buildable targeting lifecycle regression

September 5, 2026. Source changes compiled successfully; multiplayer verification pending.

## Evidence

The saved multiplayer log reports `PlaceBuildable BeginSpawningActor failed`. UE 5.8.1 AbilityTask_WaitTargetData.cpp shows that BeginSpawningActor intentionally returns false on a remote player's server when the target class neither replicates nor produces target data on the server. It still registers target-data callbacks. The ability previously labeled that expected path as an error.

A separate source-confirmed defect exists in OnTargetDataValid: invalid data, failed CommitAbility, failed transform creation, or failed authoritative spawn left the ability active. UserConfirmed WaitTargetData ends its task after the callback regardless of whether the buildable spawned. The ability could therefore retain its placement state and input callbacks without an active targeting task. The runtime probes below verify that malformed submissions now terminate cleanly.

## Change and contract

- The owning player retains a locally predicted preview and submits target data through GAS on their PlayerState ASC. Only the server commits and spawns the buildable.
- Accept exactly one SingleTargetHit entry before casting. Reject empty, unexpected, or multi-entry payloads safely.
- End each submitted placement attempt, with cancellation on failure, so EndAbility releases placement state and debug input bindings. An invalid local preview still does not submit data; it remains available for adjustment as before.
- Treat absent server-side preview actors as expected when UE's spawn policy does not require them. A genuine required-target-actor spawn failure cancels the ability.
- Do not activate the task after a synchronously consumed remote submission has already ended the ability.
- Existing server spawn checks, buildable quota marking, replicated buildable presentation, and ability prediction policy are unchanged. This patch is lifecycle cleanup, not a complete audit of placement geometry validation.

## Multiplayer acceptance checks

After rebuilding LyraEditor and restarting, test the listen host, owning remote client, and a second observing client:

1. Open preview, confirm valid placement, and verify exactly one server-spawned buildable visible to all players. Confirm placement state ends and combat input works.
2. Cancel preview and reopen it. Confirm no buildable or once-per-round quota is consumed.
3. Confirm an invalid local preview, adjust to valid ground, then confirm. Preview must remain active until it submits valid data.
4. Force an authoritative spawn failure after client validation, for example by moving an obstruction into the intended location under latency. Verify cancellation, no quota consumption, restored combat input, and ability to start a new placement attempt.
5. In a targeted development harness, submit empty, wrong-type, and multi-entry target data. Each must cancel without crashing or spawning.
6. Repeat under latency, including a submission arriving before the server registers callbacks. Verify no task is reactivated after termination.
7. Confirm normal remote placement no longer emits the misleading BeginSpawningActor error. Exercise an actual preview-spawn failure separately to verify error reporting and cleanup.
8. Verify once-per-round placement and Round 1 to Round 2 persistence after successful placement on both DevMap and Dorado.

Source inspection, diff whitespace validation, and compilation completed. PIE, packet emulation, and packaged verification have not run.

## Prevention

Match ability termination to the task's lifetime on both success and rejection. Check target-data script types before downcasting client submissions. A false deferred-spawn result may be an intentional networking policy outcome; inspect the engine implementation before treating it as failure.

## Build verification update

September 5, 2026: LyraEditor Win64 Development build succeeded (exit 0), using UE 5.8.1. Evidence: Saved/Logs/codex-tier1-20260905-retry.log. The initial build compiled both gameplay fixes but failed in existing bridge JSON key conversions; those conversions were fixed and the subsequent build passed. Editor launched after build success. Multiplayer acceptance checks remain pending.

## Runtime verification update

September 14, 2026: The latest LyraEditor Win64 Development build passed after the development-only buildable fixture and phase-aware hero-selection command changes (`Saved/Logs/buildable-placement-20260914/compile-39w.json`). In four-world same-process PIE, the production authority spawn created one replicated buildable, enforced the one-per-round quota, and converged on all three clients by the two-second sample (`Saved/Logs/buildable-placement-20260914/editor-39v.log`). The same run also passed cancel cleanup (`active=0`, placement tag cleared, quota unchanged), persistent buildable count across the reset boundary, and quota reset (`countBefore=1`, `countAfter=1`, `hasPlacedAfter=0`). The initial half-second sample is intentionally retained to show replication settling rather than hiding the transient state.

The bridge now invokes `bway.Test.AutoLockAllHeroes` without requiring an editor-supplied world; the command resolves the authoritative PIE world itself. `bway.Test.BuildableConfirm` reached the real placement ability and correctly remained active for an invalid/no-hit confirm (`active=1`, placement tag present, quota unchanged). The fixture's cancel command can now reuse an already active placement ability, so a follow-up invalid-confirm cleanup probe does not require a second activation attempt.

September 15, 2026: A fresh four-world PIE run using the compiled development probe (`Saved/Logs/buildable-placement-20260914/compile-40b.json`, `Saved/Logs/buildable-placement-20260914/editor-40c.log`) exercised malformed target data during the active playing phase. Empty data logged `Valid=0`; wrong-type and two-entry handles were structurally valid but failed the production single-entry/single-hit validation. All three ended with `active=0`, cleared placement tags, and `hasPlaced=0`, with no spawn (`Saved/Logs/buildable-placement-20260914/buildable-fixture-40c-brief.json`).

September 15, 2026: The development fixture then exercised a client-to-server GAS target-data submission under `Net PktLag=100`. It waited for the Playing phase and a complete client/authority ownership chain before activating the real placement ability. The authority callback received one entry, reported `accepted=0`, created no second buildable (`countBefore=0`, `countAfter=0`), ended the ability, and left the quota marked until the fixture cleanup (`Saved/Logs/buildable-placement-20260914/compile-41m.json`, `Saved/Logs/buildable-placement-20260914/editor-41n.log`, `Saved/Logs/buildable-placement-20260914/buildable-fixture-41n-brief.json`).

This evidence is limited to native four-world PIE. A valid target selected through physical input, packaged buildable UI, and second-PC LAN remain open. Computer Use exposed no targetable native window during this run, so physical/UI assertions were not claimed.

September 15, 2026: `bway.Test.BuildableReplicatedValidConfirm` waited for the replicated Playing
phase and client/authority ownership chain, activated the owning client's real placement ability,
and submitted one blocking target through `CallServerSetReplicatedTargetData`. The authority
received one valid entry, accepted it, spawned one buildable, ended its ability, and the buildable
converged in all four same-process PIE worlds. The fixture then destroyed only the test owner's
buildable and reset its round quota. Evidence: `Saved/Logs/buildable-placement-20260914/editor-42b.log`
and `buildable-fixture-42b-brief.json`. This is synthetic target-data transport proof; physical
cursor selection and packaged/second-PC UI proof remain separate requirements.
