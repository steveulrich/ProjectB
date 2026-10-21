# Verify buildables and rematch lifecycle

Use this runbook for buildable placement, authoritative target-data rejection or acceptance, round reset, and completed-match rematch probes. Command paths are relative to the project root. Follow [editor and PIE ownership](editor-pie.md) for dispatch, timing, observation, and cleanup, and [gate evidence rules](gate-resume-evidence.md) for verdicts and retry limits.

## Verify round reset and placement cleanup

When phase timing is unstable, the development round-reset fixture must call production `ResetRoundState` and each `ABwayPlayerState::BeginRoundStatTracking`. Evaluate persistent actor count and per-round quota independently. Do not claim a `StartRound` transition when the orchestrator is not in `Playing`.

A cancel fixture may reuse an already active placement spec before issuing `LocalInputCancel`. A no-hit confirm is an invalid-target probe: the ability should remain active with quota unused until a valid target is submitted. Physical valid-confirm proof remains a separate gate.

## Prove authoritative rejection

Issue `bway.Test.BuildableReplicatedQuotaReject` after `bway.Test.AutoLockAllHeroes`. The fixture waits for `Playing` and the client/authority ownership chain, then sends target data through GAS under `Net PktLag=100`.

Require an authority callback with no spawn, a clean ability end, and restored quota. This synthetic rejection does not replace physical valid-target proof.

## Prove synthetic valid confirmation

When physical input is unavailable, `bway.Test.BuildableReplicatedValidConfirm` can establish the synthetic client-to-server target-data path. Require all of these results:

- Authoritative acceptance.
- Exactly one spawned buildable.
- Four-world convergence.
- A clean ability end.
- Cleanup of fixture-owned actors.

Keep physical cursor selection, physical valid confirmation, and packaged UI/LAN as separate proof gaps.

## Extract buildable evidence

After a buildable fixture, run `Scripts/Test/Get-BuildableFixtureBrief.ps1` once with the saved editor log. Use its compact pass fields, peer counts, malformed-submission results, and first fixture error for routine decisions. Expand `-IncludeSamples` or narrow raw ranges only when the brief identifies a changed replication diagnosis.

For the output path, pass a bare filename beside the log or an explicit path containing a directory. The helper resolves either form; do not create a nested-path retry by guessing another invocation.

## Verify completed-match rematch

After the fixture, run `Scripts/Test/Get-MatchUpgradeLifecycleBrief.ps1` once with the saved editor log. Use its setup, request, verify, and `PASS` markers, setup-blocked state, and error counts. Read narrow raw ranges only when the compact lifecycle result changes the diagnosis.

This fixture proves same-process production rematch reset only. Packaged LAN and physical shop evidence remain separate acceptance requirements.
