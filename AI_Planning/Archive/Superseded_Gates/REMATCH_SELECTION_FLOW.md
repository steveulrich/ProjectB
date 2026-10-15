# Rematch selection flow

Gate: `Saved/Logs/rematch-selection-20260920/gate-contract.json`.

## Cause and repair

Candidate 03 proved four-process rematch economy reset, but its host log showed
`SkipHeroSelection=1` being restored from CommandLine after production rematch requested
`L_BW_HeroSelect_Staging`. Removing the flag from the relative travel base was necessary but did
not cover later option-source merging or direct gameplay option queries.

`BwayGameplayUrlLibrary` now treats a current-world/game-mode staging directive as a request for
normal selection. It suppresses the initial selection-skip shortcut in priority merging, fragment
merging, and presence/numeric/string queries. Editor and command-line defaults cannot establish
the current destination. Explicit initial direct-match launches retain their skip behavior.

`Breakaway.FrontEnd.RematchStartupOptions` exercises the actual game-mode augmentation and public
queries in a temporary initialized world while preserving/restoring the process command line.
It also checks persistent match rules, fragment merging, and a subsequent direct-match control.
The existing URL engine-compatibility and repeated-travel parsing tests remain in the same suite.

## Verification

- LyraEditor and LyraGame builds passed, with empty failure signals.
- `Breakaway.FrontEnd`: three tests passed, zero test errors/warnings, framework and process exit
  codes 0. Evidence: `automation-frontend.json` in the gate directory.
- Candidate `LAN-20260920-04` Cook/Stage/Pak passed, with 131 staged-file hashes.
- Four packaged processes passed with 200 ms simulated latency. All four humans bought rank 1;
  all three clients observed their purchased state, cleared match state in staging, and a fresh
  living pawn in Playing after the real selection timeout. Selection started at 01:49:10 UTC,
  timed out at 01:49:41, and all clients passed the next-match check by 01:49:50 on September 21.
- `rematch-flow-brief.json` passes with `-RequireNextMatch`; `flow-inventory.json` records zero
  errors or critical lines across all four logs. All three client exit codes are 0. The host was
  stopped by owned fixture cleanup after clients completed; this does not prove frontend exit.
- `resume-finalization.json` confirms zero ProjectB processes after the run. The three changed
  C++ files match their frozen candidate copies (`source-review-final.json`). Later evidence,
  helper, and documentation changes do not update the packaged artifact or broaden its proof.

Overhead: two builds, one automation run (three tests), one package, one four-process runtime
attempt. Candidate 03 remains frozen as passing reset evidence and the option-leak reproduction.

The packaged observer command `bway.Test.MatchUpgradeRematchFlowObserve` preserves the purchase
and settled reset checks, then waits for a different GameState in the original arena, Playing
phase, a living owned pawn, no owned upgrades, and no prior damage stat for two seconds. It uses
monotonic deadlines and weak world/state references. The host remains alive until all three
clients emit `ClientNextMatch PASS` and exit.

Use `Get-PackagedRematchBrief.ps1 -RequireNextMatch` to require ordered production rematch,
selection start, selection timeout, selection travel, and each client's reset followed by a
fresh playable match. The helper preserves the original reset-only mode; candidate 03 passes
that mode and correctly fails the expanded flow mode.

This gate uses four packaged NullRHI processes on one PC with 200 ms simulated latency and
the real selection timeout fallback. Physical hero selection, rendered/UI presentation,
frontend LAN discovery, second-PC, and clean-device acceptance remain separate requirements.
Invalidate affected evidence when option precedence, selection/travel, actor readiness, or the
tested artifact changes. Preserve full logs, staged-file hashes, and frozen source copies in
the gate directory and `Saved/Logs/codex-packaged-<candidate>*`.
