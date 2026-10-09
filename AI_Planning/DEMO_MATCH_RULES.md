# Demo match rules

Normal Capture the Relic matches now resolve to four-minute rounds and three round wins.
The development experience retains its explicit 90-second configuration. The current gate
is `Saved/Logs/demo-match-rules-20260926/gate-contract.json`. Candidate `LAN-20260926-04`
built and cooked successfully; candidate `LAN-20260926-05` adds fixture participant isolation.

## Requirements and cause

The [slice definition](./VERTICAL_SLICE_DEFINITION.md) requires best-of-five matches and
three win conditions. The [September 2017 PAX hands-on](https://massivelyop.com/2017/09/05/pax-west-2017-hands-on-with-amazons-breakaway/)
describes four-minute rounds and goal, team elimination, and territory-at-timeout outcomes.
These references do not establish every detail of the final public build.

The cold audit inspected 11 playlist assets, both gameplay experiences, and the match config:

- The current LAN playlist targets Dorado and Capture the Relic, with no round/score override.
- Capture the Relic had no match-config action. The production resolver used the Dev fallback.
- The Dev experience explicitly references `DA_BW_MatchFlow_Dev`: three wins, 90 seconds.
- Native config and resolved-struct defaults were one win and 90 seconds; the round component
  default was three wins and 180 seconds.
- `RoundDuration` was replicated, but `PointsToWin` was not. Clients could retain a different
  win threshold when the authority applied a hosted override.

Evidence: `assets-before04.json`, `assets-before04-run.json`, and the production resolver,
round component, and user-facing experience source. All evidence names in this report are
relative to `Saved/Logs/demo-match-rules-20260926/` unless otherwise specified.

## Changes

- Added `DA_BW_MatchFlow_Demo`, copied through Unreal's asset API from the existing config,
  with three wins and 240 seconds. Phase classes, phase durations, and bot settings are preserved.
- Normal resolver fallback now uses Demo. The Dev experience's explicit action still selects Dev.
- Native config, resolved settings, and round-component timer defaults agree with the demo rules.
- Saved Dev's 90 seconds explicitly against the new native default, preventing an inherited
  default change from silently lengthening development matches.
- Replicated the authority-owned `PointsToWin` property on the existing round component.
- Added `Breakaway.FrontEnd.DemoMatchRules`, exercising actual experience/config resolution,
  no-experience fallback, forwarded staging URL overrides, and asset immutability.
- Added the development-only `bway.Test.MatchRules <points> <seconds>` observer. It waits for
  four human peers and Playing, checks four settled samples plus a decreasing countdown, and
  removes its ticker on completion or timeout. It does not change gameplay state.

No experience Blueprint was reparented, no playlist override was added, and no existing asset
identifier was renamed.

## Verified scope

- `compile-editor03.json`: demo defaults/resolver test compiled successfully.
- `configure-rules-run.json`: asset API creation/save succeeded, exit 0; two intended assets saved.
- `assets-after01.json`: a fresh process read Demo=3/240, Dev=3/90, native=3/240.
  That commandlet exited with a verifier error because its expected playlist count was incorrectly
  10 instead of 11. `cold-asset-validation.json` compares the exact before/after inventory and
  values successfully without relaunching unchanged assets. The original failed run is retained.
- `automation-frontend01.json`: all five frontend tests passed, zero test errors/warnings,
  framework exit 0 and process exit 0. This includes a fresh load through the production resolver.
- `compile-editor05.json`: win-threshold replication and the peer observer compiled successfully,
  with no `failureSignals`. Attempt04's probe include path was corrected to the verified
  `String/LexFromString.h` in this engine branch.

`rules-peer-verification.json` now passes two live runs: normal frontend LAN hosting at 3/240,
and a staged URL override at 2/60. Each run used four source-editor game processes, four humans
plus four bots, and 200 ms packet lag. All peers loaded Dorado before sampling. In total,
32 settled samples and 32 verifier negative controls passed; each peer's countdown decreased.
The nondefault win threshold verifies receipt of authority state rather than agreement by default.

These runs used NullRHI and explicit owned-process termination after every peer passed. They do
not prove packaged cooking, normal exit, visible UI, physical input, or a complete best-of-five.
Each process logged 58 Python error lines from engine editor-toolset startup scripts expecting
editor-only `ToolsetDefinition`, `AgentSkill`, or `PythonTestRunner` APIs under `-game`. The direct
override host additionally logged an initial experience-registry fallback error before successful
staging travel to Dorado. Preserve those errors; neither run establishes clean startup. Normal
frontend hosting had no non-Python error lines. A future source-only game probe can use the
engine-supported `-DisablePython` flag after preserving this diagnosis; editor/Python audits
must keep Python enabled. Packaged startup still requires independent screening.

## Packaged full-match result

Candidate05 passed `packaged-outcomes02`: one rendered listen host, three rendered clients,
four bots, and 200 ms packet lag on every peer. Production resolved the cooked
`DA_BW_MatchFlow_Demo` with three wins and 240 seconds. The five winners were 0, 1, 0, 1, 0,
through goal, elimination, territory timeout, goal, and elimination. Final score: 3–2.

`packaged-outcomes02-verification.json` passes 35 checks and 45 negative controls. Evidence includes:

- Five production round-end events, two goal overlaps, and eight lethal damage targets.
- A real territory timeout: 238 seconds remaining at setup and 238.72 seconds to the settled summary.
- Twenty peer round summaries and four results-widget snapshots with five rounds and eight players.
- Four aligned ground-stripe observations and five cleanup records accounting for all held pawns.
- Three client test exits with code 0. Host termination was owned cleanup after all clients passed.
- Zero error or critical lines in all four logs. `packaged02-flow.json` retains the whole-log counts.

The 131 staged hashes still match, and the staged executable matches the compiled game binary:
`8873D24487C57DADBE6E9CCBF320AC3A679FD1D0F7B7E7F9729A5361892F009B`.
The verifier also rejected the earlier failed source run. It verifies captured events and state;
it does not convert synthetic actions into physical-input evidence.

Physical controls, visual quality, natural contested play, second-PC LAN, and full final acceptance
remain open. Candidate03 retains its earlier listener/browser-return/rehost scope.

Invalidate the relevant evidence after changes to configs, experience actions, playlist travel,
rule resolution, replicated round state, timers, scoring, or outcome paths. Retest only the
affected scope, then complete the packaged end-to-end gate.

## Attempts and recovery

The audit retained three setup failures before passing: an unavailable Python convenience API,
an incomplete generated-class object path, and a native action property whose Python wrapper
required its original `MatchFlowConfig` spelling. The corrected inspector uses the full class
object path, native property name, and complete playlist inventory. These are inspection failures,
not evidence of gameplay defects.

The initial editor compile also repaired three pre-existing bridge JSON-key conversion errors;
those changes were required to load current editor code. Complete commands, outputs, and failed
attempts remain in the gate directory. Build, commandlet, automation, runtime, and package counts
are maintained in the contract.

## Arena geometry and full-match verification

Dorado's opposing goals lie along world Y, but the territory classifier previously used world X.
The production classifier now derives its midpoint and horizontal axis from the two team goals.
Missing, duplicate, or coincident goals leave the half undecided. The existing possession tie-break
still applies at midfield after a valid arena frame is established.

The divider uses that same goal-derived frame. Its mesh remains a flat ground stripe, with authored
height and dimensions preserved. The saved GameState's old 90-degree offset was changed to zero
through Unreal's asset API. `divider-before.json`, `configure-divider.json`, and `divider-after.json`
record the mutation and cold verification. `automation-midfield01.json` passes the geometry test
for rotated/translated arenas, lateral displacement, tolerance, possession, and invalid goal layouts.

The development-only `bway.Test.MatchOutcomes` fixture requires five rounds with winners
0, 1, 0, 1, 0. It uses goal overlaps, production lethal damage, and the real 240-second timer;
it never calls `EndRound` or the timeout handler. All peers must verify every round summary and
the actual results widget's five-round, 3–2, eight-player snapshot. Temporary movement holds and
relic placement have explicit cleanup. The controlled host keeps four bots but disables their
relic behavior tree for this session to prevent unrelated bot goals from preempting the fixture.
This is controlled gameplay evidence; physical play and bot behavior need their own acceptance.

Four source-editor attempts are retained:

1. `source-outcomes01-result.json`: the old authored divider offset failed alignment before actions.
   The asset migration and flat-stripe correction address that finding.
2. `source-outcomes02-result.json`: a natural bot goal occurred before fixture readiness. The
   controlled host command and bounded participant diagnostics address test interference.
3. `source-outcomes03-result.json`: one client stopped logging after loading `SequenceRecorder`;
   only three humans reached gameplay. The participant readiness check correctly failed.
4. `source-outcomes04-result.json`: sequential launch and per-peer join checks isolated the same
   source-editor startup limitation earlier, with only three processes launched. All owned processes
   exited. Do not repeat this source-editor path without a changed diagnosis.

The next verification uses the packaged game to remove the editor startup dependency and prove
cooked behavior. No full-match pass is claimed by these failed setup attempts. Source runs with
`-DisablePython` no longer contain the earlier Python startup errors; the earlier logs remain intact.

Candidate04's first packaged attempt reached four humans and four bots, with the correct rules
and aligned divider. It then failed `production-pickup-failed`: multiple nearby pawns intercepted
the synthetic pickup during synchronous attachment/overlap updates. All eight held transforms
were restored. The fixture now separates every held pawn and relocates the loose relic before
calling production pickup, following the existing combat fixture's isolation pattern. This does
not establish whether ordinary contested pickup has a reentrancy defect; retain that as a separate
relic acceptance check rather than generalizing from a synthetic setup.

Candidate05 includes that fixture change. Both candidates compiled and cooked successfully.
Each has 131 staged files whose hashes match its manifest, and its staged executable matches the
corresponding compiled game binary. The second packaged attempt, `packaged-outcomes02`, passed.

The passing run also confirms a separate relic-ownership defect: a single pickup grants
`AS_BW_BallAbilities` twice to the same authority PlayerState (host lines 2503/2507 and 3284/3287).
`OnPickedUp` calls `OnRep_CurrentCarrier`, which invokes `AttachToCarrier`, then calls
`AttachToCarrier` again. That routine grants abilities. Fix and verify the ownership/grant lifecycle
in a separate gate, including duplicate/contested pickup and release cleanup.

Current overhead: eight LyraEditor builds, two LyraGame builds, nine asset commandlets,
two automation runs, six source-game attempts (23 processes), two packaged attempts (eight
processes), no GUI editor launch, and two packages. The gate contract records current phases,
counts, and retry conditions.
