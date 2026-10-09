# Respawn upgrade acceptance

Status: source implementation, seven-test economy suite, catalog save/cold load, four-world
PIE, and candidate `LAN-20260927-06` four-process transport and production rematch pass their
recorded scopes. Physical input, visual appearance, second-PC LAN, final natural flow, and
original progression/tuning remain open. Candidate05 is an older artifact without this upgrade.

## Requirement and provisional policy

The [September 7, 2017 Destructoid hands-on](https://www.destructoid.com/amazons-breakaway-might-be-the-next-great-esport/)
describes purchasing respawn-time reduction. It also describes deaths increasing a team's
respawn counter. Neither statement supplies exact tuning or a complete progression formula.

Implement `RespawnTimeReduction` through the existing shop, persistent PlayerState ASC, and
reversible infinite effect. Preserve the four-item limit and existing catalog entries. Proposed
four-rank values are 10/20/30/40 percent at incremental costs 75/125/175/225 gold, matching the
current provisional cooldown price structure; these are project tuning, not verified alpha values.

The authority applies reduction to the delay captured at death. A committed purchase while dead
recalculates that death from its original start time. Failed transactions and transient overlap
between old and new rank effects must not affect its timer. Removal restores the baseline minus
elapsed time. An expired or fully reduced wait schedules a positive 0.05-second asynchronous timer;
zero would cancel Unreal's timer. Round cleanup still cancels both timer and replicated deadline.
The existing countdown consumes the revised server deadline. The baseline remains provisional
while team-versus-player progression, cap, and reset details are unresolved.

## Gate

Contract and evidence: `Saved/Logs/respawn-upgrade-20260927/gate-contract.json`.

- Compile `LyraEditor`; run the affected economy automation suite.
- Validate catalog preview, scoped save, and independent cold load.
- Prove actual deaths, purchases before and during death, rank replacement, rejection, and
  one authoritative restart. Require matching owner/proxy reduction and countdown deadlines.
- Prove persistence across pawn replacement and rounds, then removal on production rematch.
- Compile `LyraGame`, package, and verify the changed behavior through four-process transport.
- Keep physical shop input, visual countdown, second-PC LAN, and final natural match acceptance open.

Invalidate affected evidence after changes to attributes/effects, transactions, timer scheduling,
round cleanup, catalog, countdown presentation, fixture, or artifact identity. A unit fixture's
injected death age does not prove a real death/restart, replicated convergence, or original tuning.

## September 27 editor evidence

`compile-editor01.json` passed in 66.66 seconds with no failure signals. The linked runtime DLL
advanced before launch. `automation01.json` passed all seven `Breakaway.Economy` tests with zero
warnings/errors. The new test covers transactions, rollback, clamps, timer arithmetic, retained
elapsed time, avatar replacement, reset, positive asynchronous scheduling, and round cleanup.

`catalog-{preview,apply,cold}-r1.json` and matching process records show one saved entry, six
choices, four slots, unchanged existing definitions, and successful validation in a fresh process.

`editor03-verification.json` passes 78 checks and eight corruption controls over 3,584 peer samples.
One actual remote owner submits all four ranks during two real deaths through the production
purchase RPC. Two deliberately stale requests do not add timer reschedules. Native deadlines
equal the original death start plus the catalog-adjusted delay, within 0.01 seconds. Native fires
are required within the existing 0.65-second timing bound. The authority, owner, and two other
clients converge on the modifier and deadline; owned ranks remain private to authority/owner.
The fixture also verifies pawn replacement and rank-four persistence into the next round.

The native fixture restores its wallet and removes its upgrades. It accounts for all 21 held
pawns: eight restored and thirteen expired. PIE stopped with zero remaining worlds and no dirty
content; the editor closed normally. `editor03-source-hashes.json` supplements the snapshot with
the exact untracked fixture/test, timer implementation, catalog, and linked binary hashes.
The resume identity indexes untracked filenames, so it alone cannot detect edits to their contents.

Two Pixel Streaming startup errors remain in the raw log (`Cannot set target window - GEngine
is not valid`). No additional errors or critical lines occurred during the native fixture.
This gate makes no rendered/UI claim; those errors remain actionable for presentation acceptance.
Network lag was requested as `Net PktLag=100` on all four settled worlds. This is one process with
four PIE worlds, not four packaged processes or second-PC LAN, and it does not continuously measure latency.

## Editor attempts

Four editor builds: production/test compile passed; the first native-fixture compile failed on
`auto*` deduction from `TObjectPtr`; explicit `APlayerState*` fixed it; the fourth build added a
test-wallet refill. One automation run and three catalog commandlets passed. Three editor/PIE
lifecycles were used: the first discovered Python reflection limits, the second exposed the
fixture's wallet-cap error, and the third passed. These counts precede the packaged phase below.

`editor02-verification.json` remains a failed record. The fixture attempted to seed 1,000 gold,
but the production 500-gold cap left too little for the fourth rank. The corrected fixture uses
500 and refills to that same cap before its second death. It does not relax purchase validation.

## September 27 packaged evidence

The packaged fixture sends a development-only owning-client RPC through `ABwayPlayerController`.
That client calls the production purchase RPC; authority still validates eligibility, rank, cost,
and wallet. Separate client observers survive results, rematch staging, and the next gameplay world.
The fixture uses actual deaths and production round endings/rematch; scoring and input are controlled.

`compile-game01.json` failed on a shadowed PlayerController member and `TObjectPtr` type deduction.
Explicit types and an `OwnerState` local resolved both; `compile-game02.json` passed in 31.05 seconds
with empty failure signals. Cook/Stage/Pak completed in 99.96 seconds. Candidate06 has 131 verified
staged-file hashes and executable SHA-256
`45DE982D87EA6EF0C2A1457A0D61FC9DA5356A52E54DAB8AC4BC59C532DD8D40`.
`candidate27-06-changed-files-sha256.json` freezes 196 changed files, including untracked source
contents. The final artifact audit found no changed build inputs or staged files.

`packaged-upgrade01-final-verification.json` passes 133 checks and 17 corruption controls over
19,933 independent client rows. One listen host and three rendered packaged clients request
200 ms packet lag. All four ranks purchase during two real deaths; stale intents do not reschedule.
Native deadlines match the original death clock and loaded catalog within 0.01 seconds; timer
fires retain the 0.65-second bound. Every client observes the final modifier/deadline and replacement
pawn. Owner-only rank privacy, countdown widget state, catalog-derived debits, and rank-four
persistence through the next round pass. Latency is requested, not continuously measured.

Production rematch clears ownership, wallet, deadlines, and deaths in each client's actual staging
map. Its combat round component is still `None`; combat `Prematch` starts in Dorado. Authority
reset rows cover that fresh Prematch and the next Playing phase, where all four humans have zero
modifier/ranks/deadline/deaths and valid living avatars. The stage-6 marker named `SelectionReset`
records Dorado Prematch; it is not the evidence for the earlier staging map. Client world-qualified
rows provide that evidence. All three clients emit their completion-triggered exit and exit 0;
host log activity continues after all three triggers, followed by separately recorded owned teardown.

`packaged-upgrade01-flow.json` retains zero error/unknown-error/critical lines in all four logs,
two host selection/travel cycles, one production rematch, and results shown on every peer.
No physical input, screenshot appearance, second-PC discovery, natural scoring, or graceful menu
exit is claimed by these synthetic fixtures.

## Evidence correction and overhead

The original `packaged-upgrade01-result.json` remains failed: the live wrapper read a client log,
then observed process exit before consuming the final completion bytes. The finished raw logs
contain all three observed completions and test-exit markers; recorded OS exits are all zero.
The independent final audit checks those logs, hashes, exits, and host ordering. It does not rewrite
the original verdict. Its only allowed failed-wrapper condition is the identified exit-0 read race.

The first verifier also expected combat `Prematch` in the staging map. Its corrected assertion
requires the exact staging world, round zero, sustained owner samples, and zero reset state.
The initial failed verification and both prior scripts remain beside the final report. Corruption
controls must add a new failed assertion; a baseline failure cannot count as a rejected control.
`runner-final-drain-regression.json` reproduces the race with the old poller, passes after draining
once more after confirmed exit, and rejects a command-line-only completion string.

Totals: six builds (four editor, two game), one package, four runtime lifecycles (three PIE and
one packaged), one automation run, and three catalog commandlets. No rebuild, recook, or runtime
retry was needed for the evidence corrections. No owned editor/game process remains.

Continue with the packaged buildable placement gate while preserving this frozen scope.
Physical shop selection, visual countdown, second-PC LAN, final integrated flow, and original
respawn progression/tuning remain required.
