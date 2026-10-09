# Death and respawn parity audit

Started September 26, 2026 after candidate26-07 passed the relic ownership gate. Lifecycle
repairs now pass scoped packaged tests; countdown, original timing progression and final
integrated acceptance remain open.

## Reference and uncertainty

The [September 5 Game Informer first-hand account](https://gameinformer.com/games/breakaway/b/pc/archive/2017/09/05/landing-big-dunks-in-breakaway)
describes respawn delays increasing with deaths. It does not provide the initial delay, increment,
cap, or whether the count resets each round. Exact final-public-alpha tuning remains unresolved.
The supplied `Supplemental_Examples/basecharacter_statedeath.lua:40` calls
`GetPlayerRespawnTime(owner)`, but the supplied files do not define that function and have no
verified build date. Do not infer a formula from them.

## Baseline source findings

- `ABwayCharacterWithAbilities::OnDeathStarted` records a death and calls the round component's
  `OnPlayerDied` on authority (`Private/BwayCharacterWithAbilities.cpp:135`).
- `UBwayRoundManagementComponent::OnPlayerDied` schedules every victim using the same
  `RespawnDelay` (`Private/GameState/BwayRoundManagementComponent.cpp:1571`), whose native default
  is three seconds. The scheduling path does not read a death count.
- `RespawnPlayer` requires an `APlayerController`; bots cannot respawn through that body.
  ShooterCore also contains `GA_AutoRespawn`. Its active grants, graph and timer policy must be
  inspected before changing native scheduling or claiming which path actually spawns a pawn.
- Elimination is checked before the native respawn timer is scheduled. Verify terminal-round
  timing and cleanup; the code ordering alone does not prove an observed unwanted respawn.

Paths above are under `Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime`.

## Baseline asset and compile evidence

Read-only exports in `Saved/Logs/respawn-parity-20260926/` show that ShooterCore's
`GA_AutoRespawn` has a five-second delay but is referenced by its ControlPoint and Elimination
ability sets, not the inspected Breakaway experience or Humanoid/BallMode sets. Runtime grants
still need confirmation. The production native timer remains three seconds.

Four inspection commandlets were attempted: the first failed on script-path quoting, the third
exported seven assets before an uninitialized registry prevented class lookup, and the second
and fourth completed successfully. Forward-slash script paths and an explicit registry scan
resolved those distinct setup failures. No assets were saved.

`compile-editor01.json` records the observer's `TObjectPtr` deduction errors and three existing
UE 5.8 JSON-key conversion errors in bridge handlers. Explicit pointer types and explicit
`FString` construction fixed them; `compile-editor02.json` passed with no failure signatures,
and `editor02-dll.json` confirms the runtime DLL advanced. These are diagnostic/compatibility
changes; production respawn scheduling has not changed.

## Runtime baseline and repair

`editor01.log` and `baseline-result.json` establish four PIE worlds with eight players each,
four humans/four bots, and no AutoRespawn specs on any authority player. A remote human
respawned once after each death (3.366 and 3.030 seconds); the selected bot never respawned.
Elimination reached PostRound, then two humans respawned during that pause. The observer
completed and restored eight surviving pawns; twelve prior pawn references had expired.
The two missing-bot observations refer to the same bot, which was unavailable for the second
kill. This is a failed gameplay baseline, not two distinct successful bot death cycles.

The scheduler repair supports both controller types, checks restart eligibility and a living
replacement before crediting alive counts, consumes each pending timer once, and rejects
late callbacks outside the active round. Timers clear on round/phase exit and component end.
The timer captures a weak controller and its originating round. Elimination is rechecked before
scheduling so its final death cannot recreate a cancelled timer. The three-second value is
unchanged; this repairs lifecycle defects without claiming original progression parity.

`compile-editor03.json` failed because ClearTimer mutates its handle and the cleanup loop used
a const reference. The corrected mutable reference passed `compile-editor04.json`. The expanded
observer adds settled human/bot recovery, death totals, team identity and alive counts.

Baseline setup noise: two Pixel Streaming target-window errors and one Python syntax error
(three log lines) are preserved. Correcting the function-local inventory probe confirmed all
four worlds; no Unreal object wrappers were retained globally. Visual/physical acceptance is
not claimed. PIE and editor shut down normally before the repair build.

## Verified repair: four-world PIE

`editor02.log` showed both repeated death cycles recovering and no PostRound respawn. Its 16
peer samples used the identical short world name, so `repaired-result.json` correctly rejected
unique-peer proof. Only the observer's identity field changed to `GetPathName()`;
`compile-editor05.json` passed before the final run.

`editor03.log` and `repaired03-result.json` pass all 11 checks:

- Four humans/four bots; all eight authority players have zero AutoRespawn specs.
- Two real health deaths each for one remote human and one bot; exactly one living replacement
  per death. Observed delays were 3.389/3.058 seconds for the human and 3.314/3.056 for the bot.
- Sixteen settled samples identify four distinct PIE worlds and agree on living pawns,
  death totals, team identity and four living teammates after each recovery.
- Team elimination enters PostRound with no pawn replacements during that pause. The later
  round resets all eight pawns. No missing or duplicate respawn is recorded.
- Observer cleanup restores eight surviving pawns and releases twelve expired pawn references;
  `cleanup03.json` confirms normal editor exit, unchanged input hashes and original PIE settings.

Six corrupted-evidence controls are rejected: missing peer, dead peer, duplicate world identity,
duplicate restart, PostRound restart and unfinished observer. The log retains two Pixel Streaming
startup target-window errors, classified as nonfatal only for this structured scope, with zero
unknown error lines. This is not visual acceptance.

Exact runtime inputs are in `editor03-input-hashes.json`. Changed scheduler/header inputs invalidate
this result; diagnostic-only follow-ups must preserve and identify this frozen scope. Total gate
cost so far: five editor builds (two failures, three successes), three editor/PIE lifecycles, four
read-only inspection commandlets and zero packages. Each retry above has a changed causal input.

## Packaged transport and round-reset repair

Candidate `LAN-20260926-08` preserved the first packaged run. Its server observer completed and
three client test exits returned zero, but the independent verifier failed: all eight pawns were
alive in round two while the eliminated team's replicated alive count remained zero on every
client. The 345 client snapshots and `packaged-respawn01-verification.json` retain that defect.
`ResetRoundState` recreated human and bot pawns without refreshing the team counts.

The production repair recounts living, successfully spawned team members after both restart paths,
then requests replication. It does not assume that every roster member spawned successfully.
The expanded fixture kills one human in round two and requires three surviving teammates with
the round still Playing, followed by one replacement and a count of four. This catches a false
elimination caused by stale counts as well as checking the replicated value itself.

Candidate `LAN-20260926-09` passes `packaged-respawn02-verification.json`: 77 checks and 11
corrupted-evidence controls. One rendered listen host and three rendered clients use 200 ms
packet lag, four humans/four bots, normal frontend selection timeout and the 240-second rules.
This controlled fixture does not finish a full match.

- All three clients independently recorded the pre-death roster, both human/bot death cycles,
  one new pawn per death, matching death totals, team identity and living-player counts.
- Elimination suppressed respawns throughout PostRound. All eight pawns reset for round two,
  and both team counts returned to four.
- The additional round-two death kept Playing active with three living teammates; the victim
  respawned once and the count returned to four, without replacing the other seven pawns.
- Clients recorded 167, 162 and 151 snapshots: 480 snapshots / 3,840 player rows. All client
  test exits returned zero. The host remained until clients finished and was then terminated
  by its cleanup owner. `packaged02-cleanup.json` confirms zero remaining owned processes.
- All four logs contain zero errors/critical lines. All 131 staged file hashes matched before
  and after the run; no current build input changed during testing.

The initial candidate09 verifier required death state and PlayerState statistics to arrive
together after 0.75 seconds. Client2 had two transient rows at about 0.82 seconds: death state
had arrived, but health/death-total replication settled by about 1.04 seconds. The original
verifier and failed output remain as `verify-packaged-respawn09-initial.py` and
`packaged-respawn02-initial-verification.json`. The corrected contract still requires death
state by 0.75 seconds, coherent health/statistics by 1.5 seconds, at least two settled samples
through 2.5 seconds and no rollback. Transients remain in the final report, and a control whose
death statistics never converge is rejected. This evidence correction required no relaunch or
rebuild; the prior round-two alive-count failure was fixed in production, not reclassified.

Final executable SHA-256:
`6A72B7B119D6BF9882928188D2572311BA8B9193F5301776E1033A8B00DA7245`.
Build/source records: `compile-game03.json`, `candidate26-09-preflight.json`,
`candidate26-09-changed-files-sha256.json`, `source-snapshot-LAN-20260926-09.json`, and
`candidate26-09-artifact-verification.json`, all under this gate's evidence directory.
UAT records and the 131-file hash manifest use `Saved/Logs/codex-packaged-LAN-20260926-09-*`.

Gate totals: eight builds (five editor, three game), three PIE lifecycles, four earlier read-only
commandlets, two successful packages and two four-process runs. Game compile02 passed; compile03
followed a client-observer hardening change so the round-start pawn gap cannot impersonate the
new round-two death. The latter requires an actual death-count increase after a living baseline.
No editor was launched for the packaged phase. Candidate08 remains the failed baseline; do not
reuse its gameplay evidence for this repair. Changes to the scheduler, round reset, replicated
roster/health/death state, match config or relevant assets invalidate the affected current proof.

## Next gate

The [countdown acceptance report](./RESPAWN_COUNTDOWN_ACCEPTANCE.md) records the active-asset
inspection, server-owned deadline and local HUD implementation, plus the repaired stale
hero-selection screen. Four-world PIE passed 73 checks and nine negative controls. Candidate10
also passes four-process countdown testing at 200 ms (76 checks, 13 controls, 674 HUD samples)
and browser/results/return/rehost/rediscovery into a second match. Candidate12 adds direct
unlocked late-join selection (483 samples/eight controls) and widget travel cleanup
(2,653 samples/nine controls) at 200 ms. The existing full-match and
ownership results remain frozen to their prior candidates.
Current integrated acceptance still needs a final run after the remaining related changes.

Then define a data-driven progression only with evidence for its reset boundary; keep unverified
numeric tuning labelled provisional. Verify the countdown shown to the owning client agrees
with the authoritative deadline and that match reset clears the appropriate state. The new
deadline/UI has editor and packaged evidence, while original progression tuning remains unresolved.

A September 26 search for the exact supplemental Lua symbol `GetPlayerRespawnTime` found no
usable public definition. The first-hand September 2017 account still establishes increasing
delays without a numeric formula, cap or reset boundary. Do not repeat the same lookup as new
evidence or silently claim fixed three-second timing is final-alpha parity.

An earlier [PC Gamer preview before the first open alpha](https://www.pcgamer.com/hands-on-with-amazons-bizarre-no-lane-moba/)
describes respawns as roughly ten seconds. This is an approximate observation from an earlier
build, not evidence for the final-public-alpha initial value, progression, cap, or reset rule.
It does not justify replacing the current provisional tuning with a guessed final formula.

Preserve candidate26-07's ownership proof. This diagnosis requires fresh evidence for death and
respawn behavior; it does not justify rerunning unchanged ownership or full-match fixtures.

## September 27 video observations

Reference: [GameSpot's September 7, 2017 match recording](https://www.gamespot.com/videos/breakaway-16-minutes-of-gameplay-from-amazons-espo/2300-6440780/).
The visible article date is September 7, 2017, 12:53 PM PDT. Browser playback reports
947.9333 seconds. The recording was inspected through the browser's video controls and
screenshots; media was not downloaded. Times below are the player's media time, independently
read from the video element after seeking. Portrait positions count from the left within
each team's group. These are manual observations, not an automated parity verdict.

| Media time | Visible observation | Interpretation limit |
| --- | --- | --- |
| 00:04.974 | Game clock 02:42; blue second portrait has a countdown of 3 | Opening excerpt starts mid-round; not an initial delay |
| 00:21.028 | Game clock 02:26; red fourth portrait shows 15 | Earlier deaths are outside the inspected segment |
| 00:34.974 | Arena introduction, without the combat scoreboard | Opening excerpt is separated from the subsequent match; do not infer hero changes across a round |
| 00:54.974 | Pre-round countdown 02 | Fresh round context |
| 00:59.974 | Game clock 03:57; all eight portraits active | Baseline for the complete match segment |
| 01:19.974 | Game clock 03:37; all eight portraits active | Sampled baseline, not continuous no-death proof |
| 01:34.974 | Game clock 03:22; red third portrait shows 8; kill feed names Dirk as victim | Earlier observed death for this player |
| 01:39.974 | Game clock 03:17; blue second portrait shows 10 and red third shows 3; kill feed names violetzombie as victim | Direct 10-second display near a new death |
| 02:09.974 | Game clock 02:47; blue portraits active; red fourth shows 7 | Intermediate sample; no claimed death ordinal |
| 02:39.974 | Game clock 02:17; blue third and red third portraits show 15; kill feed names dylanmeeble and Dirk as victims | Later death of Dirk; consistent with increasing delay, but intermediate deaths were not exhaustively counted |
| 04:59.974 | RED ROUND result; team K/D/A blue 10/8/13 and red 8/10/8 | Confirms an actual round boundary in the complete segment |
| 06:09.974 | Game clock 03:09; same eight portraits active | A subsequent round |
| 06:34.974 | Game clock 02:44; blue fourth portrait shows 5 | Remaining time only |
| 06:59.974 | Pre-round countdown 10; scoreboard shows dylanmeeble K/D/A 1/4/1 and Dirk 1/4/1 | Cumulative four deaths before the next round; no zero-death assumption |
| 07:39.974 | Game clock 03:30; blue third portrait shows 10; kill feed names dylanmeeble as victim | A 10-second death display after four prior match deaths supports reset between rounds |
| 07:49.974 | Game clock 03:20; blue third portrait is active again; red second shows 2 | Bounds the observed blue player's return to about ten seconds |

The 10- and 15-second displays suggested a 10 + 5-per-player-death hypothesis, but subsequent
sampling does not establish that rule. At 02:09.974 red fourth (Tyler) has 7 remaining, returns
by 02:19.974, and at 02:54.974 shows 17 remaining on the next observed death. The portraits
were sampled every five seconds from 01:00 through 03:15; brief events between samples are
not excluded. At 02:36.008 the blue and red third portraits were still active, before both
showed 15 at 02:39.974. These observations require more than an assumed personal death count.
Cap, reset conditions, modifiers, and the exact final-build identity remain unresolved.
Do not derive an initial delay from the remaining value on an arbitrary frame. Do not treat
the supplemental Lua's fallback `7` as the enabled-respawn policy: that branch runs only when
`AllowPlayerRespawn()` is false.

The [September 7 Destructoid hands-on](https://www.destructoid.com/amazons-breakaway-might-be-the-next-great-esport/)
describes a team respawn counter and purchasable respawn reduction. Equipment therefore
confounds inference from displayed countdowns. The missing `GetPlayerRespawnTime` definition
was requested from the owner. Preserve that reference question while implementing the supported
[respawn upgrade requirement](./RESPAWN_UPGRADE_ACCEPTANCE.md); do not claim a verified progression.

Browser controls verified for this source: the video application's Space key toggles pause;
ArrowLeft/ArrowRight seek by five seconds. Wait for the sought frame to decode before reading
its screenshot; an immediate screenshot can retain the preceding frame. Home did not seek.
Controls can disappear from accessibility while playback continues; read the bounded video
time/paused state before another action. Fullscreen was restored to the normal page before
handoff. No game editor, compile, package, or runtime launch was needed for this research.
