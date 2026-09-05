# Buildable targeting lifecycle regression

September 5, 2026. Source changes compiled successfully; multiplayer verification pending.

## Evidence

The saved multiplayer log reports `PlaceBuildable BeginSpawningActor failed`. UE 5.8.1 AbilityTask_WaitTargetData.cpp shows that BeginSpawningActor intentionally returns false on a remote player's server when the target class neither replicates nor produces target data on the server. It still registers target-data callbacks. The ability previously labeled that expected path as an error.

A separate source-confirmed defect exists in OnTargetDataValid: invalid data, failed CommitAbility, failed transform creation, or failed authoritative spawn left the ability active. UserConfirmed WaitTargetData ends its task after the callback regardless of whether the buildable spawned. The ability could therefore retain its placement state and input callbacks without an active targeting task. Runtime reproduction of that failure is pending.

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
