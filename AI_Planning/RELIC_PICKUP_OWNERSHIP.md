# Relic pickup ownership

The active gate is `Saved/Logs/relic-pickup-ownership-20260926/gate-contract.json`.
Candidate `LAN-20260926-05` retains its accepted [controlled full-match scope](./DEMO_MATCH_RULES.md).
Candidate07 passes the four-peer ownership gate. Candidate06's earlier run remains preserved:
it passed the initial ownership checks but stopped on an incorrect throw-ability lookup in the
development fixture. Candidate07 corrects that lookup without changing the production repair.

## Confirmed defect

The candidate05 host logs two grants of `AS_BW_BallAbilities` to the same PlayerState for
one isolated pickup. Evidence: `Saved/Logs/demo-match-rules-20260926/packaged-outcomes02-host.log`,
lines 2503/2507 and 3284/3287. `ARelicActor::OnPickedUp` calls `OnRep_CurrentCarrier`, which
calls `AttachToCarrier`, and then calls `AttachToCarrier` directly. Both authority calls grant
the authored ability set. The native pass handle has a guard, but the authored set does not.

Ownership is also committed before the carried-state change. Attachment can synchronously
produce overlap callbacks while the relic is still pickup-eligible. The earlier candidate04
fixture observed multiple pickup recipients; ordinary contested-play behavior still needs proof.

## Expected result and verification

One accepted carrier receives one authored ability set plus the existing native pass ability.
Repeated pickup calls add nothing. A competing pickup cannot replace the current carrier or
give the competitor abilities, possession flags, or carrier tags. Release removes the prior
carrier's state and permits a subsequent valid pickup.

`Breakaway.Relic.OwnershipLifecycle` uses a transient authority world and the actual authored
ability set. It derives the expected number of grants independently on a reference ASC, then
checks pickup, repeated pickup, competing pickup, drop, and reacquisition. Run it against the
baseline before changing production code. This does not prove replication or physical input.

After the source repair, verify throw/pass/reset and four-peer ownership convergence under
200 ms latency in a fresh packaged candidate. Physical input, visual quality, second-PC LAN,
and final acceptance remain separate. Invalidate affected pickup/release and packaged evidence
when the relevant source, ability set, or replicated possession behavior changes.

## Repair and authority regression

The authority now commits the carrier and carried state before notification and attachment.
It rejects repeated or competing pickup calls and pickups while scoring, resetting, or already
scored. `OnRep_CurrentCarrier` handles client presentation only; authority transitions perform
their attachment or detachment once. The overlap eligibility check uses the same availability
conditions. Thrown, passed, dropped, neutral, and pending-request states remain pickup candidates.

The regression derives three authored abilities plus one existing native pass ability from the
actual set. Evidence in `Saved/Logs/relic-pickup-ownership-20260926/`:

- `baseline01.json`: eight failed assertions before the production fix. Initial pickup granted
  seven abilities instead of four; repeated pickup raised this to thirteen. Competing pickup
  replaced the carrier, and dropping left the former owner's thirteen abilities and possession.
- `fixed01.json`: all original assertions passed. Twelve added throw/pass assertions failed
  because the transient world's RPC entry points were not dispatched. Engine `AActor::ProcessEvent`
  requires initialized actors or an explicit script-execution scope (`Actor.cpp:1513`).
- `fixed02.json`: the expanded regression passed, with zero errors/warnings, framework exit 0,
  and process exit 0. It covers pickup, repeated/competing calls, authority RepNotify idempotence,
  flags/tags, drop, authority throw/pass bodies, reset, reception, scored-state rejection, and
  subsequent pickup after the scoring flag clears. Throw/pass calls explicitly use their authority
  implementations; this test makes no RPC-transport claim.
- `compile-editor03.json`: the final LyraEditor compile passed with no failure signals, and the
  runtime DLL timestamp advanced before the successful test process launched.

Three LyraEditor builds and three automation runs were used. The failed baseline and fixture
dispatch attempt remain intact. Packaged work below adds LyraGame builds; no GUI editor was launched.

## Packaged verification

The bounded native probe gives each human a replicated, owner-only observer. It checks authority
grants for all players and each owner's locally visible specs, plus carrier, possession flags, and
carrier-tag counts on all four peers. It requests production GAS activation for request/pass/throw.
Pickup invokes the production overlap handler directly; released physics is held while peers settle.
These controlled operations do not prove physical input, ballistic flight, or catch timing.

Candidate06 compiled successfully (`compile-game02.json`) after a fixture variable-name collision
in `compile-game01.json`. Cook/Stage/Pak succeeded, and all 131 staged hashes matched the frozen
compile and changed inputs. `packaged-ownership01-result.json` records four joined humans, four
bots, and 200 ms lag. Stages 2, 3, 5, 7, and 8 passed on all four peers: host pickup, duplicate
rejection, host pass cleanup, remote pickup, and remote duplicate rejection. Each carrier had four
abilities, and possession flags and exact carrier-tag counts agreed. The remote throw action at
stage 9 failed to select a spec: the fixture searched for `ThrowRelic`, while the authored class is
`GA_BW_Relic_Throw_C`. This is a fixture failure, not a demonstrated production throw failure.
Cleanup restored all eight pawns; the runner closed its four processes. The failed run and verifier
remain preserved. Candidate07 changes only the fixture's throw class lookup.

Candidate07 completed all 23 stages in `packaged-ownership02-result.json`. Its independent
`packaged-ownership02-verification.json` passes 37 checks and rejects all 48 negative controls.
The actual `GA_BW_Relic_Throw_C` spec was selected in both remote throw actions.

- Four distinct human carriers; each receives exactly four relic abilities. All four peers agree
  on possession and exact carrier-tag counts throughout 56 settled observations (14 per peer).
- Five request activations, two passes, and two throws use the owners' actual GAS specs. Both
  remote pass and remote throw paths complete on authority and converge across peers.
- Four repeated/competing pickup attempts retain the original carrier and grant count. Release
  leaves no carrier, possession flag, tag, or relic ability grants on the previous owner.
- A fifth pickup followed by an actual enemy-goal overlap increments the player and team scores
  once and clears ownership on all peers. Production round completion reports goal condition 1.
- Cleanup restores all eight fixture pawns. Three clients exit 0 after their success RPCs; the
  host stays alive until they finish and is then terminated by its owned runner. No game/editor
  processes remain. All four complete logs have zero error and critical lines.

`candidate26-07-artifact-verification.json` rechecks all 131 staged files before and after the run,
with no changed build inputs. Executable SHA-256:
`FAF9C52136C9496808C34997628021A875ACE6B1E99F2DEFEFED22DFCD284584`.
`compile-game03.json` is its successful game build; its source snapshot and full changed-file
hashes remain beside the gate contract.

Total gate overhead: three LyraEditor builds, three LyraGame builds, three automation runs, two
packages, and two four-process packaged attempts. The failed compile name collision and failed
throw fixture lookup remain recorded; neither failure was hidden or retried unchanged.

Physical input, presentation quality, ballistic flight/prepared catch timing, second-PC LAN, and
final integrated acceptance remain open. Do not promote this controlled ownership scope into those
claims. Changes to the ownership source, carrier ability set, or possession replication invalidate
the affected assertions. Reuse this evidence while those inputs remain unchanged.
Do not repeat source-editor game startup attempts that stalled after `SequenceRecorder` loading.
Candidate05's recorded full-match pass remains evidence for its frozen earlier scope.
