# Packaged upgrade lifetime investigation

## All-four-upgrade acceptance: September 22, 2026

Candidate `LAN-20260922-01` passed four rendered packaged processes on one PC at
200 ms packet latency. The gate is
`Saved/Logs/all-upgrade-lifetime-20260921/gate-contract.json`.

All four players bought AttackStrength, Armor, MoveSpeed, and CooldownReduction
rank 1 through authority-side production purchase validation. The independent
extractor verified 16 roster-correlated purchases and exact prices against the
frozen catalog, 80 checkpoint rows, owner-private ranks, and all four public
attributes. Every peer observed remote player 257 die, receive a different living
pawn with the same PlayerState/ASC, and retain the upgrades into a later production
round. Fractional bonuses use a 0.001 tolerance so a missing 5% modifier cannot pass
as rounding noise.

All four fixtures passed while all four processes remained alive. The three clients
then exited normally with code 0; the host was stopped by owned cleanup afterward.
All four logs contained zero runtime errors. This proves structured lifetime
behavior, not physical controls, visual polish, client purchase-RPC transport,
rematch reset, or second-PC LAN acceptance.

The gate directory contains `candidate22-01-lifetime-brief.json`,
`packaged-LAN-20260922-01-all-upgrade-lifetime-result.json`,
`candidate22-01-flow-inventory.json`, and `verifier-checks.json`. All 23 positive
and negative extractor controls passed. Mutations cover missing attribute fields,
lost modifiers/ranks, changed identities, wrong catalog values/debits, foreign
purchase owners, mismatched process roles/rounds, missing peers/latency, and runtime
errors. Mutated records are verifier tests, not gameplay evidence.

Cook/Stage/Pak exited 0; all 131 staged-file hashes matched and the staged game
executable matched the previously compiled binary. Attempt accounting: one game
compile, two package attempts, one four-process run, and no editor launches. The
first package (`LAN-20260921-01`) failed with disk error 112. Its evidence remains;
the explicitly approved intermediate cleanup restored capacity before retrying.
The second package reused the unchanged game build after source/binary review.

The catalog's upgrade magnitudes and prices remain provisional. Passing this gate
does not establish final-public-alpha tuning parity.

## Historical two-upgrade acceptance

Gate: `Saved/Logs/packaged-upgrade-lifetime-20260920/gate-contract.json`.
Candidate `LAN-20260920-07` passed the separate-process lifecycle gate, following
the [four-world PIE lifecycle check](./UPGRADE_LIFETIME_ACCEPTANCE.md).

### Accepted result

Four packaged NullRHI processes on one PC (one listen host and three clients)
each confirmed 200 ms packet latency. All four players bought AttackStrength and
Armor rank 1 with eight exact catalog-derived debits. Authority and owning clients
observed their ranks; other clients retained zero private ranks while observing
the correct public attack and armor attributes.

All peers observed player 257's original pawn die, a different living pawn
respawn with exactly one additional death in the same round, and a later
production round transition with both upgrades intact. The verifier accepted
80 checkpoint rows from 1,552 native samples; the minimum final recorded settling
interval was 2.004 seconds. All three clients exited 0 and all four logs had zero
runtime error/critical lines. The host was intentionally stopped after client
completion; this is not graceful frontend-exit evidence.

Evidence in the gate directory: `candidate07-lifetime-brief.json`,
`packaged-LAN-20260920-07-upgrade-lifetime-result.json`,
`candidate07-flow-inventory.json`, and `package07-result.json`. Frozen source and
the staged-file hash manifest preserve candidate identity. The independent
verifier rejected altered attributes and a removed native PASS marker in
`helper-negative-checks.json`; command-line text alone cannot satisfy acceptance.

Attempt accounting: seven compile attempts (one source compile failure), three
packages, three packaged runs, and one rejected uncooked source-game setup.
Prior attempts below remain diagnostic evidence, not accepted results.

### Historical native fixture and requirements

`BwayUpgradeLifetimeTestCommands.cpp` adds development-only commands:

- `bway.Test.UpgradeLifetimeAuthority` waits for four human players and one relic
  on Dorado, purchases both catalog upgrades for all players through production
  validation, applies native lethal damage to one remote player, and advances
  production rounds by temporarily shortening/restoring the timer.
- `bway.Test.UpgradeLifetimeObserve` checks owner-only ranks and public attributes
  on clients, observes the same death and replacement pawn, and requires stable
  post-purchase, respawn, and next-round checkpoints.

The fixture uses monotonic deadlines and weak world/player/component references.
Pawn, phase, round, and death-count changes reset settlement. Native client pass
markers trigger client exits; the host remains alive until owned runner cleanup.
The processes are disposable: test purchases/wallet values end with the processes.

`Get-PackagedUpgradeLifetimeBrief.ps1` independently verifies eight distinct exact
debits, four rosters and local owners, public attribute agreement, private rank
visibility, native damage outside the script guard, the same remote victim's
death/replacement, stable checkpoints, later round progression, 200 ms packet-lag
markers, clean client exits, and runtime error counts. Full logs remain on disk.

## Candidate 05: host pass, client timeouts

Both targets compiled after correcting an unbraced logging-macro branch. The
game/editor success records have empty failure signatures. Cook/Stage/Pak passed.

The host bought eight upgrades and passed through round 3 at 02:39:17 UTC on
September 21. Native damage targeted player 257 at 02:38:59 (`guard=0`, health 0).
Client 1 timed out in Purchased. Clients 2 and 3 first recorded Purchased at
02:39:04, after the victim had already respawned, and timed out waiting to observe
death. All three 120-second timeouts are preserved. No client acceptance is claimed.

The owned processes were stopped after their terminal fixture failures. Evidence:
`candidate05-failed-brief.json`, `runtime-client-progress.json`, and
`packaged-LAN-20260920-05-upgrade-lifetime-result.json` in the gate directory.
Candidate 05 and its native source copy remain frozen.

## Diagnostic route and current work

Added bounded `Observe`/`State` markers for health-component health, persistent ASC
health, pawn/avatar identity, dying state, and the failed readiness predicates.
They are diagnostic records, not additional accepted checkpoints.

An attempted four-process uncooked `UnrealEditor -game` route could not resolve
the CaptureTheRelic primary experience and fell back to the default. Toolset Python
startup errors also appeared. It never reached fixture readiness and was stopped.
Do not repeat this route unchanged or treat its errors as gameplay evidence.
`source-diag-progress.json` and the owned cleanup/result files preserve the attempt.

Both diagnostic targets compiled and candidate 06 reproduced the failure. Its
client diagnostics showed `bonuses=1`, `alive=1`, matching pawn/avatar identities,
and matching health-component/ASC values, but a zero settling clock. The fixture's
`Stable` function cleared the clock on an unqualified sample but failed to rearm it
when readiness recovered with the same pawn/phase key. A later pawn change could
incidentally start it, explaining the late purchase checkpoint in candidate 05.

The fixture now rearms a cleared clock on the first qualifying sample, preserving
the full two-second interval. Death observation also retains a weak reference to
the known pre-death pawn while possession changes; it requires that same pawn's
dying state before accepting a different living replacement. Candidate 07 passed
with this corrected observer. No production replication fix was justified or
made by this diagnosis. Candidate 06 was stopped once the diagnostic was captured.

## Scope and workflow safeguards

All packaged runs here use four processes on one PC and NullRHI. They do not prove
physical shop input, rendered presentation, frontend LAN discovery, second-PC
networking, equipment completeness, or the upgrade's damage-calculation impact.

The generic log-delta helper no longer classifies Pixel Streaming target-window
errors as known nonfatal. Re-extracting the prior arena log preserved both as
unresolved errors (`ui-error-classification.json`); its gameplay-only acceptance
already recorded that UI limitation.
