# Combat credit acceptance

## Requirement and current evidence

The roadmap lists assist tracking as slice polish. Kill, death, and assist totals must agree
across the server, owning clients, round summaries, and match results. Combat credit must use
authoritative damage, avoid duplicate awards, and clear stale contributions across pawn,
round, and match lifetimes. Exact assist eligibility and reward tuning still need reference
evidence; this audit does not invent a final-alpha window or gold amount.

The initial source audit found these implementation gaps:

- `ABwayPlayerState::AddAssist` increments an authoritative replicated total, but has no
  native caller. The current death handler explicitly leaves assist tracking as a TODO.
- `ABwayCharacterWithAbilities::OnDeathStarted` derives the killer from the weak actor
  `LastDamageInstigator`. Its setter has no native caller, and none of the four exported
  active character Blueprint ancestors reference it. The subsequent runtime baseline
  confirmed missing killer credit through the production damage library.
- The round component awards configured kill gold only when given a killer controller.
  No implemented assist reward path or configured assist amount was found.

`ULyraHealthSet` already broadcasts positive, post-clamp effective damage before its
`OnOutOfHealth` notification. The authority-only combat feedback router resolves source and
target PlayerStates and records damage totals. This provides a candidate observation point
for combat credit without trusting client claims or adding a dependency from Lyra to the
Breakaway plugin. The repair uses the health set's native health-change delegate instead,
keeping essential awards independent of the cosmetic feedback router.

## Evidence and next gate

Evidence directory: `Saved/Logs/combat-credit-20260926/`.

`inspection01-result.json` and `inspection02-result.json` both exited 0 and passed their
read-only export checks. They used the existing editor binary and saved no assets. The second
inspection followed the parent Blueprint discovered in the first export. `source-audit.json`
records hashes and reference counts for `B_BW_Character_Humanoid`, `B_BW_Character_Base`,
`B_Hero_Default`, and `BW_Character_Default`. These commandlets saved no gameplay assets.

`compile-editor01.json` passed for the development-only baseline fixture. `editor01.log`
records 208 samples across four PIE worlds with 100 ms packet lag. The first attacker dealt
25 actual health damage; the second dealt lethal damage. All peers recorded one victim
death and a subsequent respawn, zero killer kills, and zero contributor assists. All four
roles gained only six passive-income gold; the configured 50 kill gold was missing.
`baseline01-deltas.json` preserves the samples and deltas. The fixture restored eight living
pawns' movement/AI holds; the destroyed victim accounted for its ninth held pawn.

Two Pixel Streaming target-window startup errors remain a visual-channel limitation in
that run. They do not explain the observed authoritative stat and economy defects.

## Repair and provisional policy

The character binds the authoritative health set after ASC initialization and unbinds during
uninitialization and EndPlay. Positive effective health loss records weak PlayerState identities
and server times. The source ASC owner preserves attribution after the source pawn dies.
Fractional health damage remains eligible even when the HUD rounds its number to zero.

The round component consumes each dead pawn once before elimination checks or result capture.
It updates K/D/A and gold together. A caller-supplied killer controller or legacy last-damage
setter cannot create an award. The victim must belong to the active roster and active round.
An eligible killer must deliver the lethal enemy health change. Self, friendly, invalid,
disconnected, and changed-team sources receive no credit. Other eligible enemy contributors
receive one assist each; the killer cannot also receive an assist for that death.

The provisional defaults are `AssistCreditWindowSeconds=10` and `GoldAwardForAssist=25`.
Both are editable component defaults. Existing kill gold remains 50. Two targeted web
search batches found no usable final-alpha assist timing or reward evidence; these defaults
are implementation tuning, not a parity claim. Environmental/knockback attribution and
support-only assists still require reference decisions and matching coverage.

Pawn replacement clears contribution state. Round number and victim-team identity guard
against carrying contributions across those boundaries. Runtime proof of round/rematch reset,
disconnect, result snapshots, remote-owner abilities, and packaged transport remains open.

The expanded fixture runs six cases: fractional contribution, a second death after respawn,
expired contribution, self death, friendly death, and an assister's death/respawn before the
victim dies. Each case repeats the production death callback twice and records immediate
gold transactions plus settled values on all four peers. `verify-combat-credit.py` checks
those observations and corrupted-evidence controls without relaunching the game.

`compile-editor02.json` failed with C4458 because the health callback parameter `Instigator`
shadowed an inherited actor member. Renaming it to `DamageInstigator` is the sole compile
retry reason. `compile-editor03.json` records the initial repair build; its subsequent runtime
finding is documented below.

Candidate12 is still the frozen prior UI baseline and does not contain this repair. Its
accepted evidence remains in [countdown acceptance](./RESPAWN_COUNTDOWN_ACCEPTANCE.md).

## Repeated lethal-damage finding

The first repaired run, `editor02.log`, captured 560 rows. Its normal case awarded one kill,
one assist, 50 kill gold, and 25 assist gold. Repeating the death callback did not duplicate
the awards. A second lethal hit about eight seconds later reduced the replacement pawn to
zero health without starting another death or respawn. The fixture stopped on the subsequent
round transition; the remaining cases are unproven. `editor02-verification.json` correctly
fails. Its negative-control results are not acceptance evidence because the underlying run
failed. Four Python diagnostic error lines came from a missing `get_player_id` binding;
the corrected function returned serialized health/pawn values and retained no object globals.

`inspection03-result.json` exited 0 after exporting the granted `GA_Hero_Death` Blueprint and
its ability set. The graph waits on its `Duration` before ending. The native death ability
disables normal cancellation, while pawn uninitialization only requests cancellation and
the PlayerState ASC survives respawn. The existing respawn fixture waited 12 seconds between
deaths, leaving this shorter interval untested.

The lifecycle repair captures the dying avatar when the ability activates. Start/finish death
operate on that captured avatar. On replacement, an active old death ability ends and cleans
up its tasks before the new pawn can need another death activation. This also prevents a
delayed finish callback from acting on a replacement pawn. Native active-spec diagnostics and
a zero-health-without-death early failure check accompany the next fixture run.

This death-ability change requires fresh affected respawn/countdown and multiplayer evidence.
Earlier frozen candidates retain their historical proofs but cannot prove this current source.

## Verified combat-credit repair

`compile-editor04.json` passed with an empty `failureSignals` array. Both the Lyra and
Breakaway runtime DLLs advanced before editor03 launched. `editor03-source-hashes.json`
records the seven affected source files and both DLL hashes; the launch and dispatch records
identify the editor process and four-world Dorado topology with 100 ms packet lag.

The native probe measured `GA_Hero_Death`'s duration at eight seconds. The prior failed second
hit occurred about 7.7 seconds after the first death. With the repair, the log records seven
active old death abilities ending on avatar replacement. Every probed death ability was
inactive and ready before the next damage operation.

`editor03-verification.json` passes 276 checks across 1,700 samples, with ten corrupted-evidence
controls. Each of the six cases invokes the production death callback twice after the actual
death; neither stats nor immediate rewards duplicate. All four worlds converge repeatedly.

| Case | Verified result |
|---|---|
| Fractional contribution | 0.25 actual health damage earns one assist; lethal attacker earns one kill. Immediate rewards match the loaded 25/50 gold defaults. |
| New victim pawn | The next kill earns one kill and 50 gold; prior-life contribution earns no assist. |
| Expired contribution | Damage older than the configured window earns no assist or assist gold. |
| Self death | The victim gets one death; previous enemies receive no kill, assist, or combat gold. |
| Friendly lethal damage | The victim gets one death; neither the friendly source nor previous enemy contributor receives credit or combat gold. |
| Assister respawn | The contributor dies and respawns before the victim's death, then still receives one valid assist. The intermediate enemy kill is accounted for separately. |

The fixture completed with `reason=observed`, restored eight living pawns' holds, and accounted
for seven destroyed pawns. The two startup Pixel Streaming errors remain outside the structured
combat scope; there were no other error lines at completion. The saved verifier retains complete
checks, negative controls, log hash, and verifier hash. A final closed-editor screen must include
any subsequent countdown regression messages before the overall lifecycle is closed.

Overhead so far: four editor builds (baseline, one C4458 failure, the credit repair, and the
death-lifecycle repair), three read-only commandlets, four completed runtime attempts, and
zero packages. The current-source packaged,
round/rematch combat-credit, results, disconnect, and final-alpha tuning scopes remain open.

## Affected countdown regression and cleanup

The same editor ran `bway.Test.RespawnHUDAudit` in a fresh four-world PIE session at 100 ms.
The unchanged accepted verifier, `respawn-countdown-20260926/verify-hud03-final.py`, passed
73 checks, nine controls, and 980 samples. `editor03-countdown-verification.json` preserves
the full result: two deaths for every human, one replacement per ordinary death, no PostRound
respawn, no duplicate/stale countdown, correct server-time display, reconstruction, shop
suppression, and next-round cleanup. This revalidates the affected structured countdown scope.

The fixture completed with `missing=0`, `duplicates=0`, and `postRoundRespawns=0`. It restored
eight living pawn holds and accounted for 19 destroyed pawns. PIE stopped, the editor closed
normally, and PID 23660 exited. The closed-editor INI retains the original four-client,
same-process listen configuration; the packet-lag overrides belonged to the destroyed PIE
worlds. `editor03-cleanup.json` records the final state and verifier identities.

Both verifiers were rerun against the closed editor log. The combined two-session log contains
three Pixel Streaming target-window startup errors, zero other errors, and no critical lines.
These results prove structured gameplay/UMG state only; rendered presentation remains open.

Prevention: death ability tasks must retain their original avatar and end at avatar replacement.
Test a second real lethal hit as soon as the replacement is playable, rather than waiting for
the previous death animation timer to finish. Use PlayerState identities for valid damage
contributors and consume awards before elimination captures round results.

## Round, result, rematch, and departure fixture

`BwayCombatCreditLifecycleAudit.cpp` adds a development-only lifecycle command. It selects
a remote human as assister, the host as killer, and an opposing human as victim. Movement
and bot-brain holds are owned by its bounded ticker and restored on completion or failure.

The fixture earns credit through actual damage, seeds another contribution on a replacement
victim, and calls production `EndRound` for each of two rounds. Those round-end triggers are
synthetic; the damage and resulting credit are real. During this owned match, the assist
window is 120 seconds, so expiry cannot conceal stale contribution carryover at round reset.
The win threshold is temporarily two rounds. The fixture restores the original settings
if their component survives; production rematch destroys that component and loads defaults.

Each peer's replicated post-round summary and active results widget payload are sampled.
Then production `RestartMatchFromResults` must create a fresh staging world and clear all four
human wallets and K/D/A. After normal selection and gameplay travel, another real kill must
start fresh totals. A newly contributing remote client then calls its game instance's
`ReturnToMainMenu`. The server must remove that player before another kill, while the damage
is still inside the ordinary assist window. The remaining peers must agree on the new credit.

The fixture has one 240-second timeout and keeps no world or actor alive across travel.
The initial topology receives 100 ms packet lag. New travel-created net drivers have not yet
been inspected for retained lag, so this fixture does not claim continuous adverse-network
coverage across rematch. Physical navigation and packaged transport remain separate gates.

The first two fixture builds, `compile-editor05.json` and `compile-editor06.json`, failed.
The first exposed `TObjectPtr` loop deduction, an unavailable getter, and protected widget
summary access. The second followed an incomplete getter-only fix and repeated the remaining
errors. Explicit `APlayerState*` loop elements and read-only reflected `CachedSummary` access
resolved them. `compile-editor07.json` passed with no failure signals. Production source was
unchanged by these retries. Inspect the complete primary compiler error set before rebuilding.

`editor04-launch.json`, `editor04-dispatch.json`, and `editor04-source-hashes.json` identify the
owned process, initial topology, fixture source, and binaries. The first lifecycle attempt
timed out at stage 10 after 423 captures. Cleanup restored eight surviving pawns and accounted
for 50 destroyed pawns. It reached results and reset four human wallets and K/D/A, but did
not complete the rematch-kill or disconnect proof. Its full failed record is
`editor04-verification.json`; controls on that failing baseline do not establish verifier quality.

The readiness failure exposed a bot-selection defect. The editor preference requests Argus,
but duplicate-hero rejection could leave new bots without a selected, locked hero. Read-only
inspection found all four humans locked and three rematch bots unlocked; the log records
the corresponding duplicate Argus assignment warnings. Both bot creation and the selection
phase now fall back to an available hero when the preferred assignment fails. The duplicate
restriction and the fixture's requirement that all eight players be locked remain intact.

`compile-editor08.json` passed in 8.58 seconds with no failure signals. The lifecycle capture
now includes each player's hero and lock state, and its verifier requires distinct teammate
heroes on all settled peers. Negative controls run only after the original evidence passes.
Editor04 stopped PIE and closed normally before this build. Editor05 is the next owned
attempt; its launch and exact source/DLL hashes are recorded alongside the prior failures.

Editor05 completed all 14 stages, with 2,572 player observations. It passed 534 of 539
checks, including round deltas, rematch credit reset, distinct locked teammate heroes,
and removal of a contributor 0.879 seconds after damage (inside the ten-second assist window).
The five failures were missing final-results observations: `UBwayResultsScreenWidget` is a
collapsed orchestrator, so requiring that object to be activated skipped its payload.
This is an observer defect, not evidence that the visible breakdown was missing. The log
and failed verification remain intact; no negative-control pass is claimed on that baseline.

The observer now samples the activated `UBwayMatchBreakdownWidget` and waits for three
consecutive captures of all four eight-column breakdowns before requesting rematch.
It also compares every result identity and canonical team with the contemporaneous roster.
Compile09 passed in 10.25 seconds without failure signals. Editor05 closed normally before
that build. Editor06 is the verification attempt for this changed observation point.

### Verified lifecycle outcome

The closed `editor06.log` passes `editor06-verification.json`: **611 checks and 12 negative
controls**, with 2,651 player observations and 352 breakdown-column observations. All four
peers have eight locked, valid heroes and distinct heroes within each team. Round one records
one kill, one assist and one death; round two adds one kill and one death without carrying
the prior contribution forward. Each active breakdown has eight matching identities and
teams, killer K=2, assister A=1, victim D=2, and no unrelated awards.

Production rematch clears the four human wallets and K/D/A, destroys the prior victim pawn,
and reaches another playable match. A fresh kill earns no stale assist. A contributing
remote client then returns to the main menu; the server removes it, and another kill
0.905 seconds after its contribution (inside the normal ten-second window) gives no departed
assist. Three remaining game peers converge. These are real damage and disconnect paths;
round completion is a fixture-triggered call to production `EndRound`, not natural scoring.

The fixture restored seven surviving pawns and accounted for 37 destroyed pawns. Its original
round configuration owner was destroyed by rematch; the new round component loaded the normal
assist window. PIE stopped and editor PID 26496 exited normally. `editor06-cleanup.json`
records unchanged source/DLL hashes and the original four-player listen-server PIE settings.
The closed log contains two known Pixel Streaming startup errors, zero other errors, and no
critical failures. This structured result does not pass a visual or physical-input gate.

This continuation used two LyraEditor builds (08 and 09), two editor launches/runs (05 and 06),
and no packages. The inherited editor04 failure was diagnosed and preserved, not relaunched
unchanged. Cumulative gate counts are nine builds, seven runtime attempts and three read-only
inspection commandlets. Prevention: a rejected preferred bot hero must resolve to an available
selection; observe the active breakdown rather than its collapsed controller; require all-peer
results readiness before travel; run negative controls only after the baseline passes.

Next, verify kill/assist attribution from an actual remote owner's granted attack ability.
`BwayTeamCombatTestCommands.cpp` already activates remote-owned attack specs and restores its
fixture actors; inspect that supported path before adding another harness. Then compile
LyraGame and collect current-source four-process evidence. Candidate12 lacks these repairs.
Final-alpha credit policy/tuning, continuous adverse-network travel, physical presentation,
second-PC LAN and final integrated acceptance remain open.

### Remote-owner attack credit

`BwayRemoteCreditAudit.cpp` reuses `ABwayPlayerController::Client_RunTeamRemoteAction`:
the remote client requests its already-granted primary through GAS, and production damage
drives authoritative credit. One attack contributes nonlethal damage before a teammate's
finishing hit; after respawn, another remote attack kills a target prepared at low health.
The teammate's contribution, finishing hit and low-health preparation are explicit fixtures.
There are no direct stat or gold awards. One 90-second timeout owns transform, movement and
bot-brain cleanup. All eight players are sampled on each peer.

LyraEditor compile10 passed in 17.9 seconds. `editor07-verification.json` passes **650 checks,
ten negative controls and 960 player observations** at 100 ms packet lag. The remote selected
Hexweaver and activated the existing Korryn primary class twice. Its first attack reduced
health from 335 to 301 and earned an assist; its second earned the kill. Remote and teammate
both finish K=1/A=1, the victim D=2, with configured 50 kill/25 assist gold and no unrelated
awards. All four peers agree and the victim respawns again. Cleanup restored eight surviving
pawns and accounted for ten expired pawns. Editor PID 5836 stopped PIE and exited normally.
The closed log has two known Pixel Streaming startup errors and no other error/critical lines.

The first verifier incorrectly treated the GoldPerSecond attribute (2) as the effective income
rate. Production `ApplyPassiveGoldIncomeToPlayerState` instead supplies the round component's
PassiveGoldPerSecond to its periodic effect. The original verifier/result are preserved as
`verify-remote-credit-initial.py` and `editor07-verification-initial.json`. The corrected check
uses five simultaneous noncombat players to isolate the configured combat rewards, allows at
most one gold of timer/replication skew, and independently compares every wallet with authority.
Immediate first-kill awards remain exact. No rerun or broader tolerance was used to hide a
missing award. This proves combat rewards and convergence, not final-alpha passive-income tuning.

After this PIE run, a read-only packaged peer journal was appended to the fixture. It waits
for an eight-player Playing world, captures for 45 seconds, and emits a normal client test-exit
marker; a separate 120-second startup bound prevents an indefinite observer. Source changes
after PIE are explicitly listed in `editor07-cleanup.json`; production combat/RPC code did not
change. LyraGame compile01 passed in 87.94 seconds with empty failure signals.

Candidate **LAN-20260927-01** cooked and staged in 103.82 seconds, exit 0. The frozen 193 changed
inputs had no build input newer than the compile; all 131 staged hashes and the executable
match. Executable SHA-256: `40176526F27751F028EC1D06B7A831EB24015D22380388BC10F357ADB2DF0FE5`.
`candidate27-01-preflight.json`, `candidate27-01-artifact-verification.json` and the immutable
candidate source snapshot record provenance. The four-process credit run is in progress;
successful packaging alone does not establish its runtime or final LAN acceptance.

Candidate01's run reached normal selection timeout, arena travel and Playing with all three
clients joined. Remote Argus primary damage reduced the victim from 400 to 370, and the first
assist/finishing-kill case ran. The fixture then timed out at stage 5 before its second attack:
its shared 90-second deadline started at frontend startup, so the normal 60-second selection
and travel consumed the combat window. `packaged-credit01-verification.json` correctly fails;
there is no complete packaged credit claim for this artifact.

All three read-only client journals captured their full 45 seconds and exited 0, with no
runtime error lines. The host was kept alive until they exited, then its verified owned PID
37448 was terminated because `CloseMainWindow` returned false. The runner records that host
exit separately as a failed run. `packaged-credit01-owned-abort.json` preserves the causal
fixture timeout and teardown; it is not a client disconnect or gameplay acceptance failure.

The next fixture has a 180-second startup limit and a separate 90-second active combat limit.
Peer startup is likewise bounded separately from its full 45-second journal. The runner now
recognizes a native terminal failure immediately instead of waiting on a completion that can
no longer occur. These are development-only observation changes; gameplay credit is unchanged.
LyraGame compile02 passed, and candidate02 is the sole replacement package. Prevention: budget
the normal frontend/selection/travel lifecycle separately from the selected combat observation,
and never treat a non-success COMPLETE marker as a live operation.

### Packaged remote credit verified

Candidate **LAN-20260927-02** passed Cook/Stage/Pak in 106.50 seconds and all 131 staged hashes
match. Executable SHA-256: `4EB9744626631567EB9F0AFD8BF19C3E62F516D3CD40E5DE3D1013FF2B8501F4`.
The only candidate01-to-02 source change is development fixture timing. Compile02 took 38.21
seconds and reported no failure signals.

`packaged-credit02-result.json` passes the transport scope, and
`packaged-credit02-verification.json` independently passes **715 checks, ten negative controls
and 2,200 player observations**. The listen host and three separate Development clients
request 200 ms packet lag. Normal selection timeout and staging travel reach Dorado. A real
remote Argus primary reduces the target from 335 to 305, earns an assist when the teammate
finishes, then earns the next-life kill. Remote and teammate converge on K=1/A=1, victim D=2;
all four wallets agree with configured combat awards above independently measured passive
income. The fixture restores eight surviving pawn transforms/movement states and accounts
for ten expired pawns.

Each client writes its own journal. The verifier joins those observations to the authority's
phase timestamps within 0.5 seconds, allowing one second for the first post-travel baseline.
Every settled checkpoint requires three distinct client samples. Raw rows, source sample IDs,
timestamps, alignment deltas and log hashes remain in the verification JSON. This projection
does not treat server observations as client evidence. The negative controls remove actual
client activations or a client log and corrupt K/D/A, team and gold values.

All three client observers complete and exit 0. The host stays alive until then and is cleaned
up separately by the owned runner. `packaged-credit02-flow.json` records zero error, unknown
error and critical lines in all four logs. This proves the declared combat/replication scope
on one PC, not physical attack input, every hero kit, second-PC LAN or the full match/rematch
flow. Current final-alpha economy/assist policy remains provisional.

The same frozen candidate is now running `packaged-countdown-regression01`, using the prior
accepted countdown fixture and verifier. Its expected result is two deaths per human,
correct reconstructed local countdown widgets and server deadlines, shop suppression/resume,
round-end cleanup and round-two recovery. No further build or cook is needed for that scope.

### Countdown regression: timing proof still open

`packaged-countdown-regression01-result.json` passes transport: all three clients exit 0,
the host is terminated only during owned cleanup, and all four logs have zero errors or
critical lines. The native fixture completes round-two recovery with `missing=0`,
`duplicates=0`, `postRoundRespawns=0`, eight restored pawns and 19 expired pawns.

The unchanged accepted `verify-packaged-hud10-final.py` passes **74 of 76 checks over 693 HUD
observations**, but fails `258.1.deadlineMatchesActualTimer` and
`259.1.deadlineMatchesActualTimer`. This regression is **not accepted**. Its negative controls
are not promoted as proof because the baseline fails. The shared candidate's verified remote
combat-credit scope remains valid; this is a separate affected timing proof.

`packaged-countdown-regression01-clock-diagnosis.json` records the measurements. The failed
formula maps a world-time deadline onto wall-clock `kill + 3 seconds`; maximum errors are
approximately 0.792 and 0.788 seconds versus its 0.65-second limit. All first-death deadlines
are 77.750 in server game time, and the authority clock at the kill is 74.750. Host observations
also have a 1.127-second gap during HUD reconstruction. Client-to-interpolated-authority clock
differences peak at 0.505/0.489 seconds, but interpolation across that gap is not sufficient
independent timing evidence. The two failures must not be erased by widening the threshold.

Next, capture the authoritative clock and respawn scheduling/callback times independently
of HUD lifetime. Keep the clock-agreement requirement, actual timer/respawn assertions and
all existing widget checks. Use that evidence to distinguish verifier time-domain assumptions
from a real countdown-clock defect before changing either. All owned test processes are now
closed, all 131 artifact hashes still match, and no build input drifted.

This continuation used one LyraEditor build, two LyraGame builds, two packages, one PIE run
and three packaged runs. Cumulative gate counts: 12 builds, two packages, 11 runtime attempts,
three read-only commandlets. Candidate01 was replaced only after the observed startup-budget
failure; candidate02 served both remote credit and the affected HUD regression. No further
build or repeat run was spent on the unresolved clock hypothesis.


### Independent native countdown timing — candidate27-03

The new candidate resolves the timing proof with **102 checks and 17 negative controls**
over 660 local HUD observations, 400 independent authority-clock samples and 25 native timer
events in four packaged processes. Three clients exit 0; the owned host exits -1 during
cleanup only after their completion. All four logs have zero error and critical lines.

Development-only `bway.Test.RespawnTimerTrace` records the actual native timer remaining,
rate, elapsed time, replicated deadline and server clock when scheduling and firing. The
owned HUD fixture enables and restores it and logs the authority clock independently of
widget lookup. Shipping gameplay and the respawn delay are unchanged.

`verify-packaged-countdown-clock.py` retains the previous widget, actual respawn, shop,
reconstruction, cleanup and transport checks. It replaces the mixed-clock oracle with
native schedule/deadline equality and callback timing, plus a conservative comparison with
**both** surrounding authority-clock observations at the unchanged **0.65-second** limit.
It does not interpolate across a hitch. The largest conservative difference is 0.551670s;
first/second-cycle timer callback errors are 0.100229s/0.082919s in game time.

The historical wall formula still fails one check on this new run. Independent records show
a 0.922331s wall sampling interval with only 0.225943s of game-clock advance; second-cycle
callbacks take about 3.49–3.51 wall seconds but 3.082919 game seconds. Engine source confirms
`AGameStateBase::GetServerWorldTimeSeconds` uses world time plus the synchronized delta,
while `FTimerManager::Tick` advances using the world's delta. A fixed wall deadline therefore
does not measure the actual native timer. The earlier candidate02 failure remains preserved
and is not retroactively promoted; candidate03 supplies the missing direct evidence.

The verifier rejects old logs missing native observations. Additional negative controls
remove the clock journal, shift client time, corrupt native remaining time and delay the
native callback. Controls count only when the baseline passes.

Evidence: `Saved/Logs/combat-credit-20260926/packaged-countdown-clock01-` files
`result.json`, `verification.json`, `clock-summary.json` and `flow.json`; candidate03
preflight/artifact verification and `countdown-clock-cleanup.json`. Build input drift is
checked separately. Compile-game03 passes in 34.41s with empty failure signals; package
BuildCookRun passes in 103.69s. This continuation uses one game build, one package and one
runtime attempt; cumulative counts are 13 builds, three packages, 12 runtime attempts and
three read-only commandlets.

Scope remains one-PC, four-process Development with requested 200ms packet lag. Physical
input, rendered appearance, second-PC LAN, final-alpha tuning and the final integrated match
remain open. Next is current-source packaged credit/results/rematch and screen lifetime.


### Four-process credit, results and rematch — candidate27-04

`packaged-lifecycle01-verification-v3.json` passes **575 checks and 14 negative controls**
from 7,448 independent player observations and 672 local result-column observations. The
fixture applies real damage, ends two rounds through production `EndRound`, observes actual
activated breakdown widgets, requests production rematch and performs fresh combat. It
does not prove natural scoring, physical input, rendered appearance or another PC.

All four processes agree on round-one K1/A1/D1, round-two cumulative K2/A1/D2, exact round
summary deltas, eight result identities/teams and final KDA. All four human wallets and KDA
are zero in the selection map. All peers resume with locked valid, distinct teammate heroes,
one gameplay respawn HUD, no active results breakdown and a fresh K1/D1 without an assist.
Clients confirm the old match world expired. The three client processes exit 0; host exit -1
is owned cleanup after completion. Eight held pawns are restored and 35 have expired, exactly
covering all 43 held pawns. Four logs contain zero errors and critical lines.

The client observer is read-only and independent of the authority fixture. Source sample IDs,
timestamps, raw journals, projected records and log/verifier hashes remain in the result.
State comparisons use three distinct settled snapshots at common timestamps, selected only
by observation availability, with preceding client observations bounded to 0.5s. The maximum
observed difference is 0.323s. UI activation is asynchronous: each client's own pre-rematch
result samples are checked against its validated final roster, with at least three samples
and a separate 0.65s readiness bound. Observed delays are 0.146/0.372/0.211 seconds.

The first two extraction attempts are preserved. Version 1 sampled the first host result
frame before some client widgets activated and incorrectly expected the staging world's
round component to report Prematch. The actual selection map leaves that component at None.
Version 2 tried treating the delayed-travel interval as the same live-roster state. Production
`RestartMatchFromResults` clears stats and removes bots after accepting travel, while the
immutable result payload remains visible. Version 3 keeps pre-request live-state checks,
uses independent pre-request result samples and validates the real staging phase. No runtime
was repeated, tolerance widened or failed payload discarded to obtain the corrected result.
All raw transients and unavailable sample timestamps remain saved. Negative controls cover
missing peers/results/summaries/reset/UI, corrupted rewards, uncleared client wallets,
unlocked/duplicate heroes and stale or missing widgets.

One fixture warning remains actionable: `bway.Test.AutoLockAllHeroes` only resolves a PIE
world. The packaged call failed once; the live match completed through the normal 60-second
selection timeout. Remove that unsupported call in the next coherent fixture build and
preserve the normal packaged flow. Do not repeat it unchanged.

The carry-across-match interval was 86.119 wall seconds. The original match used the fixture's
120-second assist window, but the fresh match restores the normal component defaults. This
run proves old-world expiry, reset match state and fresh combat; **it does not independently
prove an unexpired stale contributor is rejected under the fresh match's active window**.
The next focused probe must record and control that current window and exercise a real
packaged client departure before expiry. The existing PIE disconnect evidence keeps its scope.

Candidate04 has 131 matching staged hashes and no build-input drift. Game compile04 passes
in 35.91s with empty failure signals; BuildCookRun passes in 103.75s. Evidence is under
`Saved/Logs/combat-credit-20260926/packaged-lifecycle01-`: `result.json`, the three verifier
results, `flow.json` and `cleanup.json`. `verify-packaged-credit-lifecycle-v3.py` is the current
extractor; its earlier versions are diagnostic history. No editor or packaged process remains.

Across this continuation, two LyraGame builds, two packages and two runtime attempts completed
the countdown and lifecycle scopes. Cumulative counts: 14 builds, four packages, 13 runtime
attempts and three read-only commandlets. Final-alpha credit/respawn policy, all-kit physical
and visual checks, second-PC LAN and final integrated natural match/return/rehost remain open.

### Direct contribution cleanup and packaged departure — candidate27-05

`packaged-disconnect01-verification-v2.json` passes **659 checks and 22 negative controls**
over 7,826 player observations and 680 result columns. It retains the two-round credit,
four-local-results, production rematch, wallet reset, and UI-lifetime checks. A remote owner
then returns through `GameInstance::ReturnToMainMenu`; the authority observes its departure
before applying another real lethal hit. The remaining three gameplay peers agree on K2/D2/A0,
teams, and wallets within one bounded passive-income tick.

Development-only observations directly inspect the contribution map and consumed assister
count. Before fresh rematch combat, the map is empty and the original victim pawn has expired.
A positive control records a contribution and one consumed assister in the initial match.
Before disconnect, fresh damage creates one contribution. At the subsequent kill, the departed
player is absent, its old player state is invalid, and the contribution age is **1.125 game
seconds**. That is inside both the active 120-second fixture window and the normal 10-second
window. The death consumes zero assisters. Cleanup restores the active window to 10 seconds.

Client 2 reaches the standalone frontend world and its original match world expires. This
proves world arrival, not rendered frontend readiness or physical input. All three clients
exit 0. The host is terminated only during owned cleanup after client completion. Seven held
pawns are restored and 37 have expired, covering all 44 held pawns. The packaged fixture no
longer issues the unsupported PIE-only AutoLock command; normal selection timeout completes.
All four logs have zero error and critical lines; no owned game or editor process remains.

The initial extraction failed six baseline sampling assertions. Only one shared host snapshot
had a preceding client observation within 0.5 seconds on every peer, although each client had
multiple Playing observations before the first hit. The corrected extractor permits the
nearest observation on either side **only for the action-free rematch baseline**, bounded by
the explicit Stage 11 start and first hit. The limit stays at 0.5 seconds, sample IDs remain
distinct, and selection does not depend on player values. Other stages retain preceding-only
alignment. The maximum selected skew is 0.316 seconds. New corruption controls reject a
baseline after the hit or in Warmup. Raw observations, signed offsets, initial failures,
verifier hashes, and timing candidates remain saved. No runtime retry or tolerance increase
was used to correct extraction.

Evidence is under `Saved/Logs/combat-credit-20260926/`: `packaged-disconnect01-result.json`,
both verification records, `packaged-disconnect01-flow.json`, and
`packaged-disconnect01-cleanup.json`. The current extractor is
`verify-packaged-credit-disconnect-v2.py`. Candidate05's artifact verification matches all
131 staged files, executable SHA-256
`5D8E0DB6622639EC1FB3EDC2197858478FB8F1377201CBE5526534D73E434C36`,
and records no changed build inputs. Game compile05 passes in 71.96 seconds with empty failure
signals; BuildCookRun passes in 101.35 seconds. This scope used one build, one package, and
one runtime attempt. Cumulative counts are **15 builds, five packages, 14 runtime attempts,
and three read-only commandlets**.

Scope is four processes on one PC, Development, with requested 200 ms packet lag. Continuous
latency after travel is not independently measured. Real damage and production round-ending
calls provide controlled gameplay evidence; all-kit physical input, visual polish, natural
integrated match/return/rehost, second-PC LAN, and final-alpha policy/tuning remain open.
