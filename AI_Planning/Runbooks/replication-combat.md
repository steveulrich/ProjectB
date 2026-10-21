# Verify replication and combat fixtures

Use this runbook for hero-dependent combat and remote-owner replication fixtures. Command paths are relative to the project root. The [editor and PIE runbook](editor-pie.md) owns process lifecycle, authority dispatch, Python safety, readiness baselines, timeouts, and cleanup. Follow those prerequisites before issuing a time-sensitive command.

## Prepare combat fixtures

Select the loaded primary or melee ability spec by runtime class family. Defer sequential activations until the real fire interval completes. During relic pass setup, isolate unrelated pawns and restore every moved transform before marking cleanup complete.

The remote fixture polls for production damage before pass setup. The score variant samples replicated health and team before the round reset. Preserve these bounded waits and the exact score-delta markers; they establish the fixture's causal ordering.

Use `Scripts/Test/Get-TeamCombatFixtureBrief.ps1` for one bounded marker read. It recognizes authority, remote-owner, and remote-owner-score marker sets. Do not diagnose remote runs with ad hoc searches for old authority-only markers.

## Prove settled peer convergence

Issue adverse latency as `Net PktLag=100`; the space form is invalid. After readiness and fixture collection, run `Scripts/Test/Get-ReplicationGateBrief.ps1` once. It joins readiness, authority-fixture results, and multi-sample peer evidence into one bounded record, while persisting detailed child briefs. Use `-IncludeSamples` only when a changed diagnosis needs them.

When health can regenerate, omit a fixed expected health value. Compare each child sample with its paired authority sample within a bounded tolerance; use 20 health points for the remote-owner lag fixture. Require exact team equality.

Capture child logs before teardown. Require the remote fixture's `COMPLETE` or `RemoteScore COMPLETE` marker before promoting the result. Target hold, damage polling, pre-score sampling, and the extended verifier are development-only aids owned by fixture cleanup; they do not change shipping gameplay.

If Unreal rotates a reused child log, archive all three child files and create a new baseline. Do not retry against stale offsets. Preserve transient samples as well as settled results, and require the settled all-peer result before marking convergence.

## Preserve scope and error classification

Same-process or separate-process PIE proves only the recorded topology and assertions. Keep packaged LAN, physical input, and rendered presentation proof separate when required by the gate. Pixel Streaming target-window errors may be classified by an authority-only combat fixture within that scope; they remain actionable for rendered/UI gates. Preserve total errors, unknown errors, and critical lines under the [evidence classification rules](gate-resume-evidence.md#poll-and-classify-failures).
