# Buildable authority and packaged placement acceptance

Status: September 27 shared authority/preview validation compiles and passes focused geometry
automation for all four current buildable definitions. Four-world Dorado PIE now passes the
owning-client GAS, cleanup, replicated bounds, and production round-persistence scope. Packaged
placement and presentation remain unverified. Candidate `LAN-20260927-06` contains the
old implementation; its passed respawn-upgrade scope remains frozen. Current gate:
`Saved/Logs/buildable-authority-20260927/gate-contract.json`.

## Observed source gap

Paths below are relative to `Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime`.

- `Private/Abilities/BwayGameplayAbility_PlaceBuildable.cpp:253` commits the ability, then passes
  the client's `SingleTargetHit` to `MakePlacementTransformFromHit` and the production spawn helper.
  It checks target-data shape and the blocking flag, but does not independently validate geometry.
- `Private/Buildable/BwayBuildablePlacementLibrary.cpp:195` constructs the transform from the
  submitted location and normal. `CanPlayerPlaceBuildable` checks data, quota, and active count.
  Spawn collision adjustment can reject an overlap, but cannot establish valid range or terrain.
- `Private/ActorPlacementTargeting/BwayTargetActor_ActorPlacementFace.cpp:317` validates slope,
  footprint support, pivot lift, and obstruction in the local targeting preview. The remote
  server intentionally waits for GAS target data without creating that preview actor.
- `Private/Development/BwayBuildableTestCommands.cpp:401` fabricates a blocking hit at pawn
  position plus 600 units forward and 20 units upward. It is evidence for GAS transport and
  quota/cleanup, not for real ground selection. The valid/reject commands also require both
  client and authority PIE worlds in one process.

These describe the baseline. The source audit JSON records exact input hashes and candidate
identity. The implementation and focused evidence below do not establish packaged gameplay yet.

## Expected behavior and ownership

The local owner selects a candidate and predicts its preview. The PlayerState ASC carries the
owning connection's GAS intent. Authority derives the permitted buildable and limits from the
selected hero/ability, rechecks geometry in its world, and alone spawns the actor and consumes quota.
The accepted actor and mesh/team state must converge for host, owner, and both observers.
Placement quota is intentionally owner-only replicated; authority and owner must agree on it.
The client cannot establish legality by supplying a blocking flag, surface normal, or transform.

Share geometry rules with the preview so legitimate targeting stays consistent. Reject nonfinite
values, out-of-range targets, missing ground, excessive slopes, unsupported footprints, and blocked
placements before committing cost/cooldown or consuming quota. Keep rejection/cancellation cleanup
complete. Use server-validated ground and orientation, account for the existing preview's pivot
lift without applying it twice, and preserve explicitly bounded network/quantization tolerance.
Inspect the engine's ground-trace range semantics before changing vertical-distance behavior.

## Required evidence

1. Meaningful geometry/transaction tests distinguish legal placement from forged range, normal,
   unsupported-ground, and obstruction cases. Invalid requests leave no actor, spent cost, quota,
   placement tag, target actor, or active ability. Record the unchanged legal preview behavior.
2. Compile only the target for the selected phase, using Tier1 and its persisted failure signals.
   Editor automation requires `LyraEditor`; the later package requires `LyraGame`.
3. In four packaged processes at adverse latency, a remote owner submits through actual GAS target
   data. One legal request creates exactly one correct buildable. Invalid and repeated quota
   requests produce no extra actor, and owner/server abilities end cleanly.
4. Independent peer samples prove actor/mesh/team/quota convergence. A real production round
   transition preserves the surviving actor and restores the per-round allowance. Require the
   next legal placement as well as the reset flag; do not infer capacity from metadata alone.
5. Restore fixture-owned movement/AI/transforms, remove only fixture actors, stop callbacks, and
   keep the host alive until every client completes. Persist OS exits and raw-log identities.

Physical cursor/gamepad placement, visual readability, second-PC LAN, original buildable tuning,
all-kit gameplay, and final integrated natural flow remain separate requirements. Passing the
synthetic transport gate cannot close those gaps.

## Next action

Compile `LyraGame` and freeze a new candidate for four packaged processes at 200 ms packet lag.
Reuse the grounded owning-client fixture and independent observers. Clients must wait for the
authority's cleanup-release RPC before normal test exit. Candidate06 does not contain this fix
or the new replicated bounds. Physical targeting, visual alignment, second-PC LAN, and the final
natural integrated flow remain separate acceptance requirements.

## Shared query and geometry evidence

`FBwayPlacementQuery` now owns finite/range checks, world ground re-tracing, surface normal,
slope, footprint support, pivot lift, visibility, and obstruction tests. The preview uses the
same geometry rules. Confirmation submits the raw ground point; authority calculates the final
transform before `CommitAbility`, with a fresh quota check. Invalid data still ends the ability
through its existing cleanup path; its runtime side effects remain a required next assertion.

The engine clips the aim ray, then performs a vertical ground trace. The new range check retains
that downhill behavior. Authority permits an explicit 100 cm horizontal movement tolerance;
ground agreement is bounded to 3 cm and submitted/traced normals must have a dot product of at
least 0.98. These are engineering tolerances, not original-alpha tuning or a measured latency claim.

The first implementation compiled in 39.74 seconds (`compile-editor01.json`), but `automation01`
failed five assertions: no current definition had usable blocking components. The read-only
`asset-geometry01.json` commandlet confirmed all four visual meshes have `NoCollision`, and their
gameplay effect spheres ignore Visibility. Their placeholder meshes include a cube, character,
and pistol; those visuals cannot define a stable gameplay footprint. No content package was saved.

Buildable data now declares placement center and extent independently of artwork. The native
default is a box with 50 cm half-extents centered at the actor origin; this is provisional project
tuning. Spawned actors replicate that occupancy. The shared query combines it with any blocking
class components, including Blueprint-authored components. Oriented-box overlap prevents two
nonblocking structures from occupying the same space without changing their effect collision
responses. Visual alignment for the differing placeholder meshes remains unverified and must be
checked before calling art integration complete.

`compile-editor02.json` passed in 61.00 seconds with no failure signals; `editor-dll02.json`
confirms the linked runtime DLL advanced. `automation02.json` passed
`Breakaway.Buildables.AuthorityGeometry` with zero test errors/warnings, framework marker 0,
and process exit 0 in 15.90 seconds. For each of the four actual data assets, the test requires:

- Legal flat-ground placement and identical preview/authority position and orientation.
- Rejection of forged range, floating/nonblocking/penetrating hits, false or zero normals,
  nonfinite or conflicting positions, unsupported footprint, missing ground, blocked visibility,
  volume obstruction, and a steep surface disguised with an upright normal.
- Rejection of overlapping nonblocking buildables, acceptance of separated rotated volumes
  whose world-aligned boxes overlap, preserved downhill targeting, and legal placement after cleanup.

This is a collision-world automation test, not a GAS transaction, network, physical-input, or
rendered proof. All original failed evidence is retained.

## Owning-client GAS and round persistence

`editor02-verification.json` passes 109 checks and 18 corruption controls over 970 state samples.
One listen authority and three independently journaled client worlds ran on Dorado with four
humans, four bots, and `Net PktLag=100` applied to each world. This is same-process PIE scope.

- The owner activates and cancels through GAS, then submits actual traced ground with forged
  range, height, or normal. Each submitted malformed target reaches production authority and
  rejects before placement. An already-active ability also rejects an injected quota race.
- Legal placement creates one buildable. Repeating activation with quota consumed fails. The
  owner and authority release the ability, placement tag, confirm/cancel callbacks, target
  actor, and reticle. Current wallets never fall below the action baseline; this lower-bound
  check does not establish generic exact cost accounting under passive income.
- All peers receive the same actor identity, class, mesh, team, and position within 1 cm.
  Deliberately changed occupancy center `(7, 3, 5)` and extent `(45, 55, 40)` replicate exactly.
- Production `EndRound` reaches the next Playing round. The first actor survives; authority
  and owner observe cleared quota. A second real placement reaches all peers. Cleanup removes
  both fixture actors and accounts for all 16 held pawn lifetimes: eight restored, eight replaced.

`compile-editor03.json` failed because the fixture used an incorrect round-end enum name.
Editor04 passed in 8.58 seconds. `pie01` then exposed a setup error: the selection staging world
had no suitable placement ground. The same editor's `pie02` loaded Dorado and reached all four
production rejections, but its next action RPC beat owner-only quota replication. Editor05
passed in 8.96 seconds after adding one bounded prerequisite wait before owner activation.
It does not retry ability activation. The final editor02 run completed all fixture phases.

The initial final-run verifier wrongly required quota on nonowning observers. Its failed report
is preserved as `editor02-verification-initial.json`. Source confirms `COND_OwnerOnly`; the
corrected verifier requires owner/authority quota and default quota on other observers. It also
requires actual production target-result markers. No gameplay rerun was needed for that correction.

The editor02 log contains six error lines before fixture setup: two Pixel Streaming target-window
errors and four lines from a preflight Python `json` import failure. The import was corrected and
exception references cleared before dispatch. No new error or critical line occurred during the
fixture. Pixel Streaming remains actionable for rendered acceptance; this gate claims structured
state only. Both editor lifecycles closed normally with zero PIE worlds and no dirty assets.
The original server-option INI value was restored and recorded in `editor-settings-restored.json`.

After this PIE pass, the development observer gained a `PeerReleased` marker on its existing
cleanup RPC. Packaged clients use it to exit after authority cleanup. This change still requires
the packaged run; it does not alter shipping gameplay. Current overhead before packaging:
five editor builds, two automation runs, one read-only commandlet, two editor lifecycles,
three PIE attempts, and zero new packages. Commands, original failures, hashes, and verifier
details remain under `Saved/Logs/buildable-authority-20260927/`.

## Packaged lifecycle distinction

Candidate `LAN-20260927-07` compiled in 85.90 seconds and packaged in 70.32 seconds. Its four
processes reached the grounded owner rejection and valid-placement phases without runtime
errors. The persistence assertion failed because the selected hero was Argus. Its Siege Engine
rolled from the placement point and expired before the next round. Both
`BwaySiegeEngineBuildable.h` and `Docs/Buildable_System.md` define this as a one-use barrel with a
ten-second roll. The earlier PIE player used Korryn's stationary ward. This was a fixture
assumption, not evidence that round reset destroyed a surviving structure.

The failed harness result, per-process exit records, original logs, and independent failed audit
remain under `packaged-buildable01*`. Do not promote this candidate or treat its zero error count
as successful placement acceptance. Before choosing a persistence fixture, inspect the actual
hero's buildable class and lifetime rather than assuming every buildable is stationary.

Candidate `LAN-20260927-08` changes only the development fixture after the previous production
build. It temporarily disables fixture-owned Siege Engine actor ticks during the round-retention
assertion. It then restores their original tick flags and requires both barrels to move and
expire through native behavior within the reflected configured duration plus a bounded margin.
Observers record both phases independently. Cleanup restores surviving tick flags on every exit
path and destroys only fixture-owned actors. This isolates persistence of a surviving actor
without changing the gameplay barrel's lifetime. Native damage behavior and physical placement
remain separate requirements. Game compile02 passed in 39.00 seconds; Cook/Stage/Pak passed in
68.01 seconds. Its four-process attempt also failed the fixture's round-retention assertion,
this time with Rawlins. The floor trap survived the transition, then a newly spawned enemy
triggered it. The actor journal records its replacement by `BP_BW_Gunslinger_JailCage`; native
`TriggerJailCapture` explicitly spawns cages and destroys the consumed trap. This is a different
hero lifecycle, not a second unexplained deletion. No `HoldRoll` marker was emitted in this
attempt. The barrel hold/resume experiment was later removed after inspecting the actual
one-active cap; no runtime proof is claimed for that abandoned branch.

Both packaged attempts have zero runtime error/critical lines but failed acceptance. The
failed harness results distinguish normal test-triggered exits from owned cleanup termination;
all four processes were closed after each attempt. `packaged-attempt-family.json` preserves
the shared error inventory without treating zero errors as success.

The development ground search now prefers farther valid targets and excludes locations within
10 m of current pawns or `APlayerStart` anchors. It still submits an actual hit through the
production geometry validator and GAS; these extra distances are fixture isolation, not gameplay
placement restrictions. This prevents an unrelated round-reset spawn from consuming a trap
during the persistence assertion. `compile-game03.json` passed in 37.84 seconds with no failure
signals. Candidate09 packaged this correction in 104.37 seconds, but was withheld before
runtime when a read-only asset inspection exposed a separate configuration defect.

## Authored lifetime and persistence correction

`asset-config01.json` records the actual loaded definitions: Siege Engine has a one-active cap
and a ten-second roll; Jail, Shrine, and Ward each have a cap of three and persist between rounds.
The Siege Engine Blueprint incorrectly overrode native round persistence to false, contrary to
`CoreLoop_Implementation_Plan.md`'s explicit `bPersistsBetweenRounds=true` requirement.

The single-property Blueprint correction was saved through Unreal's asset APIs. A fresh
commandlet verified persistence is now true and that every other inspected setting, including
the one-active cap and ten-second lifetime, is unchanged. No packages were left dirty. Evidence:
`asset-persistence-fix01.json`, `asset-config02.json`, and
`asset-persistence-cold-verification.json`. Actual Siege Engine round-boundary survival remains
unproven; the cold asset read is configuration evidence only.

The accumulation fixture now selects a remote hero whose actual definition persists and permits
at least two active structures. It records that class and cap, and the verifier rejects an
ineligible selection or a different observed class. This keeps the existing accumulation test
valid without changing the Siege Engine cap or pausing its lifetime. `compile-game04.json`
passed in 58.78 seconds with no failure signals. Candidate10 packaging and runtime are next.

Current overhead: five editor builds, four game builds, two geometry automation runs, three
read-only asset commandlets, one asset mutation commandlet, two editor lifecycles, three PIE
attempts, three packages, and two four-process attempts. Retain all original failed evidence.
After the scoped accumulation run, verify Siege Engine lifetime, cap, and actual round survival,
then third-round accumulation and configured caps. Per-hero effects, physical targeting,
placeholder visual alignment, second-PC LAN, and the final integrated flow remain open.

## Separate-process readiness and identity audit

Candidate10 Cook/Stage/Pak passed in 99.84 seconds. Rendered `packaged-buildable03` joined all
three clients, but host frames slowed to roughly 1–2 seconds and observed GPU utilization was
99%. The fixture reached `quota-race-no-active-ability`; the independent verifier rejected it.
The original process records and logs remain intact. This does not pass rendered performance.

The same frozen candidate ran again with only `NullRHI` added to its launch arguments.
`packaged-buildable04` completed the native harness, retained a Sun Shrine across the production
round transition, placed a second shrine, and exited all three clients with code 0 after cleanup.
The independent verifier nevertheless rejected the evidence: the first cancel activation arrived
before the owner's prerequisites were ready, and actor object names differed across processes.
Corresponding actor meshes, teams, bounds, and positions agreed, but matching those fields alone
would not prove network identity. No acceptance pass is claimed for this attempt.

The development fixture now waits up to ten seconds for the owner's real phase, avatar, spec,
and `CanActivateAbility` prerequisites before issuing a single positive activation. The cancel
gate must observe authority activation before accepting clean termination. The quota-race probe
also waits for actual authority activation and measures its cleanup window from quota injection.
Negative activation and already-active submission probes keep their existing semantics.

Actor journals now record the native network GUID alongside the process-local object name.
The verifier requires assigned GUIDs, exact identity across peers and rounds, and all existing
team, mesh, occupancy, and position assertions. Additional corruption controls reject missing
cancel-activation evidence, unassigned GUIDs, and a different peer GUID. Prior verifiers and all
failed reports are retained. This change needs game05 and a new frozen candidate; candidate10
cannot prove it. NullRHI covers structured transport only, leaving rendered/physical acceptance open.

The final-alpha reference still establishes once-per-round placement and accumulation of
surviving structures ([September 21 hands-on](https://www.pcgamer.com/amazons-fantasy-sports-game-breakaway-shoots-but-may-not-score/)).
The configured three-active cap is an inspected current setting, not independently established
final-alpha tuning. Resolve that parity question before treating cap behavior as final completion.

## Candidate11 packaged transport result

`compile-game05.json` passed in 49.07 seconds; candidate11 Cook/Stage/Pak passed in 101.35 seconds.
All 131 staged-file hashes and the compiled executable identity matched. The original four
process logs, runner/verifier hashes, source snapshot, and independent result are retained under
`Saved/Logs/buildable-authority-20260927/packaged-buildable05*` and `candidate27-11*`.

The independent audit passed **31 transport checks, 115 gameplay checks, 23 corruption controls,
and 1,036 samples**. This run used Rawlins's actual persistent Jail trap, four separate packaged
processes, 200 ms packet lag, and NullRHI. It establishes:

- Real remote-owner cancellation, three malformed geometry submissions, quota-race rejection,
  valid placement, and repeat-quota rejection through GAS. Authority cancellation activation is
  observed before clean termination; there is one positive activation per requested action.
- Matching native network GUIDs across all peers and the next production round, with exact
  class, team, mesh, occupancy fields, and positions within 1 cm. No unassigned GUID samples
  occurred in this run. The first actor survives, quota resets, and a second placement converges.
- Fixture actor removal, owner ability/tag/callback/reticle cleanup, and accounting for all
  16 held pawn lifetimes. All three clients observe completion before the host releases them
  and exit with code 0. The host is then terminated by its recorded fixture cleanup owner.
- Zero runtime error or critical lines in all four original logs.

This is a **headless structured multiplayer pass**, not rendered, physical-input, second-PC,
per-hero effect, or final integrated acceptance. Candidate10's rendered readiness failure remains
recorded. Current gate overhead is ten builds, five packages, five packaged attempts, three PIE
attempts, two editor lifecycles, two geometry automation runs, three read-only commandlets, and
one asset mutation commandlet. Candidate11 must remain frozen; the next native fixture needs a
fresh game build and candidate.

## Next selected scope: Siege Engine lifecycle

Expected observable result: a real owning-client Siege placement creates one normal rolling
actor; an immediate production round transition retains that same network identity while it
is still alive and clears the per-round quota. With quota demonstrably clear, the configured
one-active cap rejects a new placement. Natural expiry removes the actor on all peers and frees
the cap; another valid placement consumes the new round's quota once.

The fixture must select a remote Argus explicitly during normal selection setup, use actual
ground targeting, and leave the barrel's ticks, collision, configured one-second build time,
900 cm/s speed, and ten-second maximum roll duration unchanged. Record gameplay time and the
actual stop cause: collision can end a roll before its maximum duration. End the round promptly
after creation so the default five-second PostRound pause does not consume the entire lifetime.
Independent peer journals must prove motion, identity, round survival, owner quota, removal,
and cleanup. Restore all fixture-owned pawn state and preserve physical/visual proof gaps.

Source checks confirm `StartRoll` resets elapsed time once, authority `Tick` advances it and
expires on duration or blocking movement, and `ResetRoundState` removes only nonpersistent
buildables before respawning players. The corrected Blueprint is cold-verified but has no
runtime Siege lifecycle proof in candidate11. Candidate13 supplies the dedicated proof below;
the Jail pass remains separate evidence.

## Candidate13 Siege lifecycle result

The dedicated fixture compiled in game06 (90.03 seconds), and candidate12 packaged in 113.06
seconds. `packaged-siege01` failed before any placement with `ClientNoGround sequence=101`.
The shared development ground search had required ten metres of clearance from pawns and
spawn anchors to isolate Jail persistence. Game07 (62.75 seconds) removes that fixture-only
exclusion for the rolling Siege definition and adds bounded search counters. Production range,
ground/support, obstruction, and authority validation remain unchanged.

Candidate13 packaged in 115.18 seconds. Its actual owner search reports `maxRange=400.0` and
finds valid ground at 350 cm, explaining why the earlier ten-metre exclusion could never pass
for this hero. Both placements use the same production GAS path; no actor is spawned by a
fixture shortcut and no barrel tick, speed, collision, or duration is overridden.

`packaged-siege02-verification.json` passes **65 gameplay checks, 22 transport checks, 16
corruption controls, and 1,191 samples** across four separate NullRHI processes at 200 ms lag.
All 131 staged files match their frozen hashes. The evidence establishes:

- Remote Argus selected through the production selection request; two actual grounded owner
  submissions accepted by the server, with exactly one positive activation per placement.
- Native network identity `510` survives the production round transition into Playing round 2.
  Authority and owner quota are clear, yet the one-active cap rejects another activation.
- The original barrel continues its existing lifetime across that transition and expires
  natively at 10.001 seconds of rolling. Its removal frees the active-count cap on authority;
  a second owner placement creates identity `628`, observed by every peer.
- The second barrel expires natively at 10.000 seconds. The owner's round quota remains
  consumed, so repeat activation rejects even though no active barrel remains.
- Every peer observes motion, matching identities and settled class/team/mesh/occupancy fields,
  the first actor in the next Playing round, removal, and complete targeting cleanup. All
  16 held pawn lifetimes are accounted for. No fixture-owned barrel requires forced destruction.
- Three clients exit with code 0 after observed completion and host release; host teardown is
  explicitly owned. All four original logs contain zero runtime errors or critical lines.

Original journals, the failed first attempt, source snapshots, runner/verifier hashes, artifact
identity, and per-process cleanup remain under `Saved/Logs/buildable-authority-20260927/`.
This proves the corrected persistence flag's recorded runtime scope. Damage, wall collision,
rendered/physical input, second-PC LAN, other heroes' effects and accumulation parity, and the
final integrated natural flow remain open. Total gate overhead is twelve builds, seven packages,
seven packaged attempts, three PIE attempts, two editor lifecycles, two automation runs, three
read-only commandlets, and one asset mutation commandlet.

## Next selected scope: collision and damage with placeholder assets

`asset-geometry01.json` reports `NoCollision` on the Siege native root mesh. The base constructor
uses that skeletal mesh as the root, and the native visual-mesh assignment does not change its
collision profile. This is a reason to test effective runtime collision, not proof that all
Blueprint child components or runtime graph behavior lack collision.

Before changing production code, inspect every runtime primitive component and test the actual
weapon/ability trace channels against the authored buildables. Expected result: intended enemy
buildables are hittable and take production damage, friendly structures obey team protection,
and the Siege Engine's configured damage affects enemy buildables while its native movement
stops at an actual blocking world obstacle. Collision must work with the current placeholders
and remain independent of future artwork. Preserve legitimate trap behavior and placement
support/overlap rejection when fixing any confirmed defect. Keep DPS/collision proof distinct
from candidate13's unobstructed lifetime result.

## Placeholder collision and primary damage: isolated runtime verification

The September 27 baseline (`collision01.json`) inspected authored Blueprint actors after
construction and native BeginPlay in an isolated world. That initial fixture did not enable Blueprint callback dispatch; its observations alone do not establish all Blueprint BeginPlay behavior. All four observed visual skeletal roots had collision disabled. Shrine and Ward effect
spheres and the Jail trigger supported pawn overlaps, but they were not weapon hit geometry.
Siege had no usable movement sweep. The baseline also incorrectly expected the armed Jail
floor trap to be damageable; native behavior and the design distinguish the invulnerable trap
from its damageable cages. The corrected test preserves that distinction.

Production changes:

- Buildables now have a query box driven by their authored placement bounds. RepNotify updates
  its center and extent on clients. Visual meshes remain noncolliding, so changing artwork does
  not change attack geometry. The box overlaps pawns and dynamic projectiles, blocks weapon and
  visibility traces, and preserves existing player movement. The floor trap disables this box;
  Jail cages retain their own sphere, with melee and Lyra weapon query responses added.
- Placement validation excludes the duplicate native box template from its authored footprint
  and rejects using another buildable as a supporting surface.
- Argus primary and the Rawlins, Alona, and Korryn primary projectiles now accept enemy buildables
  through the existing GAS damage effect. Friendly and unknown teams are rejected. The shared
  damage boundary enforces buildable construction/trap immunity, which GAS does not enforce
  through `AActor::CanBeDamaged` automatically. Character knockback and armor-shred targeting
  retain their existing character scope.
- Siege sweeps its gameplay box against world obstacles independently of the visual root.
  Dynamic structures remain overlaps so the barrel reaches its damage radius. The native team
  check no longer falls through to the player-team fallback after finding an equal buildable
  team. Health depletion now expires Siege and releases its registry entry.

Verified evidence under `Saved/Logs/buildable-authority-20260927/`:

| Check | Evidence | Result and scope |
|---|---|---|
| Placement geometry | `collision09-report/index.json`, `AuthorityGeometry` | All four authored definitions; existing geometry controls plus rejection of a real buildable top in preview and authority |
| Runtime hit geometry | `collision09-report/index.json`, `CollisionContract` | Four actual buildable Blueprints plus the Jail cage; weapon/visibility and melee/projectile queries; trap exception; asymmetric bounds and nonzero centers; mesh removal preserves hits and pivot; actual Siege tick stops and expires at a wall at a 0.5-second step |
| Siege damage and death | `collision09-report/index.json`, `SiegeDamage` | Native ticks at 0.1, 0.02, and 1/60 seconds deliver 27.3333 total damage at 200 DPS, preserve friendly/unknown teams, retain dynamic overlap movement at 900 cm/s, and remove Siege plus registry occupancy after lethal GAS damage |
| Actual primary attacks | `collision09.json`, `collision09-report/index.json` | Eight real ability activations: four heroes against friendly/enemy actual Shrine Blueprint. Enemy health deltas 10/2/28/22 with fixture attack strength zero; friendly deltas zero. Real projectiles spawn, move, overlap and expire on enemy hits. All abilities end; report has zero errors/warnings; framework marker and process exit both zero |

These are isolated authority tests, not network, rendered, physical-input, or parity-damage-tuning
proof. The primary test uses native production ability classes and loaded kit configuration;
it does not prove every Blueprint ability override or every buildable/hero pairing. Candidate13
does not contain these changes. Candidate11 and candidate13 remain immutable historical evidence.
New collision can change the barrel's route and earlier expiry in the arena; reverify affected
placement, movement, damage, health replication, and owner activation in a fresh candidate.

### Retained attempts and fixture lessons

`compile-editor06` compiled the baseline. `compile-editor07` failed on a missing
`FGameplayEffectSpec` declaration and a `TObjectPtr` passed directly to a test assertion; both
were corrected. Editor builds08–15 passed. `collision02` passed geometry/collision before the
new damage fixture crashed because it had no game-instance message router. `collision03`
passed geometry, collision, and Siege damage but exposed missing source health attributes and
the projectile fixture's incomplete world lifecycle. `collision04`–`collision07` retain the
bounded movement, collision, and callback diagnostics; `collision08` passes primary damage.
After finding the callback requirement, collision and Siege fixtures were upgraded to initialized worlds as well. `collision09` is the authoritative combined result: 4/4 tests, zero errors/warnings, framework marker 0, process exit 0, 41.15 seconds. It supersedes the earlier isolated collision/lifecycle results. The family inventory is `collision-family.json`.

For future isolated damage fixtures, start from a real `UGameInstance`, initialize the world's
actors with `InitializeActorsForPlay`, and establish BeginPlay before expecting native dynamic
overlap callbacks. Initialize the pawn extension with PawnData, the source health attributes,
and the correct ASC owner/avatar and team. A begun actor and successful scene query alone do
not prove callback delivery: `AActor::ProcessEvent` also requires initialized world actors.
Keep a positive movement/hit control alongside friendly-damage rejection. Use bounded callback
diagnostics as soon as movement and scene queries pass, rather than inferring damage behavior
from the absence of health changes. All earlier failures remain recorded.

This scope added ten editor builds (one compile failure) and nine automation launches, with
no package or gameplay editor lifecycle. Cumulative gate counts are 22 builds, 11 automation
runs, seven packages, seven packaged attempts, three PIE attempts, two editor lifecycles,
three read-only commandlets, and one asset mutation commandlet. Next: a four-process owner
activation/health/collision convergence fixture on a new game build and candidate, then the
remaining buildable effects, accumulation/parity, visual/physical, second-PC, and integrated flow.

## Candidate14: four-player primary damage and collision convergence

`LAN-20260927-14` contains the placeholder collision and damage changes above. LyraGame
`compile-game08` failed on three `auto*` loops over `TObjectPtr` player arrays in the new
development fixture. Using `const auto&` resolved those compile errors; `compile-game09`
passed in 44.61 seconds with no failure signals. Cook/Stage/Pak completed in 112.80 seconds.
All 131 staged files match the manifest; the compiled and staged game SHA-256 is
`A3FEBB7A6343CD05C48679CFED1330D9D04249914EDFB3C73994DD3ABF06DA4E`.

The `packaged-combat01` run passed **202 checks, 16 corruption controls, and 877 samples**
in one packaged listen host and three packaged clients with `-PktLag=200` and NullRHI.
The four humans selected distinct production heroes. Each owning player activated the
actual granted Blueprint primary ability once against a friendly and once against an enemy
Shrine Blueprint. No abilities were granted or damage effects directly injected by the fixture.

| Hero | Friendly health | Enemy health | Actual owner ability |
|---|---|---|---|
| Argus | 750 → 750 | 750 → 720 | `GA_BW_Spartacus_PrimaryAttack_C` |
| Rawlins | 750 → 750 | 750 → 712 | `GA_BW_Rawlins_PrimaryAttack_C` |
| Alona | 750 → 750 | 750 → 712 | `GA_BW_Alona_PrimaryAttack_C` |
| Korryn | 750 → 750 | 750 → 716 | `GA_BW_Korryn_PrimaryAttack_C` |

Every peer's final three samples agree with authority on health, team, assigned NetGUID,
Blueprint class, mesh, and attack-box geometry. Friendly cases use authored 50 cm extents;
enemy cases use fixture-only asymmetric extents and a nonzero center to exercise the
replicated-bounds update. Actual weapon-channel queries hit the gameplay box on every peer.
All eight abilities finish. The authority destroys all eight fixture structures and restores
all eight held pawn transforms, movement settings, velocities, and previously running AI.
The three clients receive observer release and exit with status 0 before owned host teardown.
The final process inventory is empty.

Evidence is under `Saved/Logs/buildable-authority-20260927/`: `packaged-combat01-result.json`,
`packaged-combat01-verification.json`, four original peer logs, `packaged-combat01-cleanup.json`,
the candidate14 source snapshot/input hashes/artifact verification, and harness hashes.
The snapshot identity discrepancy was reviewed in `candidate27-14-provenance-review.json`:
status/untracked records gained a UTF-8 BOM across PowerShell hosts; their text and the patch
are identical. Staged files and changed build inputs also match independently.

All four logs have zero error/critical lines. Warning counts are retained (host 162; clients
18/20/19). Actionable presentation leads include duplicate `GameplayCue.Weapon.Pistol.Fire`
registrations and an unregistered `GameplayCue.Breakaway.Combat.Hit` cue tag. These remain
open; a clean error count does not establish cue or visual correctness.

This proves primary attacks against the Shrine and replicated gameplay hit boxes in a
synthetic, fixture-positioned arena. It does not prove every hero/buildable pairing, natural
aiming, physical input, appearance, performance, client destruction convergence, second-PC
LAN, or the complete integrated match. Siege wall/DPS/death multiplayer coverage and the
affected placement/persistence regression remain required. The latter now reuses candidate14.

This continuation has used two game builds (one fixture compile failure), one package, and
one completed combat run; the placement regression is in progress. Cumulative counts are
24 builds, 11 automation runs, eight packages, and nine launched packaged attempts.

### Candidate14 placement and Siege lifecycle regressions

Both affected regressions reused the exact candidate14 artifact, with no additional build or
package. All staged hashes and changed build inputs were rechecked before each launch.

- `packaged-buildable06-verification.json`: 31 transport and 115 gameplay checks, 23 corruption
  controls, and 1,034 samples pass. Remote-owner cancellation, malformed submissions, quota
  rejection, valid placement, production round persistence, quota reset, and second placement
  retain their prior behavior with the new collision code.
- `packaged-siege03-verification.json`: 22 transport and 65 gameplay checks, 16 corruption
  controls, and 1,174 samples pass. The remote player selects Argus through production
  selection, places through the owning GAS path, retains the same Siege NetGUID across the
  production round reset, receives active-cap rejection with clear quota, and places again
  after natural expiry. Both barrels stop for native duration at ten seconds; the new sweep
  does not prematurely collide on this captured route. Expiry leaves round quota consumed.

Each run uses one listen host and three separate packaged clients at 200 ms with NullRHI.
All six client exits are 0 with normal test-exit markers; hosts remain alive until client
completion and are then terminated by their owning runners. All eight original regression
logs have zero error/critical lines. Counts and source hashes remain in their verifier JSON;
the harness records are in `candidate27-14-regression-harness-sha256.json`. No owned editor
or game process remains after the three completed runs.

Candidate14 now has 435 checks, 55 corruption controls, and 3,085 samples across these three
distinct scopes. These totals are an evidence inventory, not a full vertical-slice verdict.
The Shrine attack fixture does not establish every buildable pairing. Natural Siege lifetime
does not establish wall collision, damage delivery, or damage-driven destruction on clients.
Those multiplayer checks are the next selected gate; the isolated wall/DPS/death tests remain
supporting evidence only. Other buildable effects, final-alpha accumulation rules, cue
registration/presentation, physical input, second-PC LAN, and final integrated flow stay open.

Final cumulative overhead for this gate: **24 builds, 11 automation runs, eight packages,
ten packaged attempts**, three PIE attempts, two editor lifecycles, three read-only commandlets,
and one asset mutation commandlet. This continuation added two builds, one package, and three
packaged runs; there were no runtime retries. The sole build retry corrected the demonstrated
`TObjectPtr` compile error. The next build should contain a coherent Siege multiplayer fixture
with distinct damage, obstacle, death, identity, and cleanup assertions before another package.

## Candidate15: Siege damage, obstacle, and destruction transport

`LAN-20260927-15` passes the three-case Siege combat gate in four separate packaged processes
at 200 ms simulated latency: **120 checks, 12 corruption controls, and 1,156 samples**.
LyraGame `compile-game10` passed in 74.90 seconds with no failure signals; Cook/Stage/Pak
completed in 113.24 seconds. The manifest verifies all 131 staged files and the unchanged
build inputs. Compiled/staged game SHA-256:
`FC277ABF4690BA5E71261101BAB29171357769A64E344D0D913337B74EA3C0F6`.

The fixture uses actual Siege and Shrine Blueprints and normal server ticks. It temporarily
holds eight pawns away from the test corridor and owns all spawned actors and observers.
It neither disables Siege collision nor changes speed, lifetime, damage rate, or asset defaults.

- **Damage and natural expiry:** the barrel passes between an enemy and friendly Shrine.
  Enemy health falls from 750 to 688.912; friendly health remains 750. The native per-tick
  journal reconciles the applied damage with health loss and verifies 200 damage per second.
  Total damage also matches the time spent inside the 150 cm radius within a bounded tick
  tolerance. The barrel expires at its native ten-second limit.
- **Obstacle:** the gameplay box hits the fixture's world-static wall at x=2410 cm, matching
  wall position minus wall half-width and Siege bounds. The native collision stop occurs
  after 2.678 seconds of rolling, well before the lifetime limit.
- **Health depletion:** an enemy source applies 300 damage through the production GAS damage
  helper to the 250-health Siege. Its native health callback destroys it after 2.008 seconds
  of rolling. This verifies the damage/death boundary; it does not claim a physical attack.

All four peers observe assigned identities, rolling movement, settled Shrine health,
Siege destruction, and registry removal. Movement is judged from actual samples; the
Blueprint reports actor movement replication disabled, but the replicated mesh-root path
still transports its position. Do not require a particular replication flag when the
observable contract is met, or add a second transform transport path without a defect.

All three clients finish their observers and exit 0 before the owned host teardown. The
authority accounts for three native barrel destructions, six fixture Shrine destructions,
one wall creation/destruction, eight restored pawns, and restored diagnostic cvar state.
No owned process remains. All four logs contain zero errors/critical lines; warning counts
163/16/17/16 remain recorded, including the previously identified cue-registration leads.

Evidence under `Saved/Logs/buildable-authority-20260927/`: `packaged-siege-combat01-result.json`,
`packaged-siege-combat01-verification.json`, the four SHA-256 identified original logs,
`candidate27-15-final-processes.json`, candidate15 source/input/artifact records, and harness
hashes. The final verifier hash is recorded separately because its configuration assertion
was corrected before the runtime observations: actual movement is required, not the actor's
specific replication flag. Corruption controls still reject stationary peers, incorrect
health/identity/registry state, missing native stop/damage/death/cleanup, and missing exits.

This is a synthetic corridor in the real networked match with NullRHI. Rendered motion,
physical placement/attacks, every buildable pairing, and second-PC LAN remain separate.
Candidate14's three prior scopes remain frozen evidence; candidate15 adds this specific
combat scope. Next: the remaining Shrine/Jail/Korryn buildable effects and accumulation rules,
followed by cue/presentation fixes and the final integrated acceptance work.

This continuation used one successful game build, one package, and one packaged run, with
no retry. Cumulative gate overhead: **25 builds, 11 automation runs, nine packages, and eleven
packaged attempts**; other counts are unchanged.

### Next selected gate: Shrine aura continuity

Source review identifies a specific unverified persistence risk in
`BwayAlonaSunShrineBuildable.cpp`: `IsAllyOfShrine` resolves the owner's current pawn or the
instigator and returns false when neither exists. `TickHeal` then removes affected allies
from its tracked set. `ShouldThisShrineHealAlly` still considers other active same-team
Shrines when choosing the nearest non-stacking source. Test this before changing behavior:
an owner's death/replacement must not strand surviving structures or suppress healing by
another valid Shrine. Korryn already has a stored-team fallback; that is a useful comparison,
not proof of its entire aura lifecycle.

Expected observable result: the actual Shrine Blueprint heals a living allied target at
35 HP/s inside 500 cm, excludes enemies and out-of-range targets, never doubles healing
when two friendly Shrines overlap, and continues correctly while its owner's pawn is absent
and after replacement. Destroying the nearest Shrine must let the other continue; destroying
the last must stop its healing and release its registry entry. Keep passive player regeneration
separate from Shrine healing evidence. Preserve all fixture health/effects, transforms,
ownership, timers, and cleanup. Use initialized worlds and production spawn ownership before
BeginPlay to avoid reproducing earlier fixture lifecycle errors. Package/multiplayer evidence
is required after the cheapest meaningful authority regression and any demonstrated repair.

### Shrine continuity repair and isolated regression

The actual Shrine Blueprint reproduced the risk in `shrine01`: normal healing and overlapping
source protection passed, but healing stopped after owner-pawn destruction and did not recover
after replacement, nearest-source destruction, or re-entry. Four assertions failed.

The repair uses the structure's stored team, reconciles eligible overlaps at its existing
one-second healing cadence, and rejects a destroyed or uninitialized instigator when constructing
the healing effect. A valid current owner pawn retains attribution; the persistent Shrine ASC
provides the fallback while no owner pawn is available. Healing amount, radius, and non-stacking
policy are unchanged. Reconciliation also covers zero-build-time Blueprints whose aura starts
before placement assigns their team.

`shrine05` passes all nine lifecycle phases using real initialized world, team, ASC, and Blueprint
lifecycles: baseline, one source, overlapping sources, absent owner, replacement owner, nearest
source destruction, range exit, re-entry, and last-source destruction. Each eligible tick heals
35 HP; enemies and the out-of-range control remain unchanged. Registry counts fall to one, then
zero, after production lethal damage. This is isolated authority evidence with accelerated timer
advancement; it does not prove network transport or natural player respawn.

`shrine06-all-buildables.json` then passes all five `Breakaway.Buildables` tests in 40.77 seconds,
with zero test errors or warnings, framework exit 0, and process exit 0. It includes geometry,
collision, primary damage, Shrine continuity, and Siege damage. `compile-editor22.json` records
the successful current editor build with empty failure signals. `shrine05-source-hashes.json`
identifies the repaired source and regression test.

The six automation runs and seven editor builds preserve each changed hypothesis: initial
reproduction, stored-team lookup, team/overlap diagnostics, initial-overlap reconciliation, and
stale-instigator fallback. Editor17 failed because the fixture called protected team initialization;
the corrected fixture uses production initialization and public roster registration. The original
source, all failed attempts, and the bounded attempt inventory remain in `shrine01-sources/` and
`shrine-family.json`. Cumulative overhead is **32 builds, 17 automation runs, nine packages, and
eleven packaged attempts**. No owned process remains at the next resume preflight.

Next: prove the repaired aura in four separate packaged processes at 200 ms, including real owner
death/respawn and settled peer health/identity/registry agreement. Candidate15 predates this repair
and retains only its frozen Siege scope. Jail capture/cage release, Ward application/removal,
accumulated survivors, physical presentation, second-PC LAN, and final integrated flow remain open.

### Candidate16: healing passes authority; replicated state fails

Candidate `LAN-20260927-16` completed Cook/Stage/Pak in 114.95 seconds after `compile-game12`.
Game11's diagnostic `TObjectPtr<APawn>` deduction error was corrected with an explicit pawn
pointer. The staged identity passed before launching one listen host and three clients at
200 ms. All nine authority phases healed exactly 105 HP over three native 35 HP ticks where
eligible. Natural owner death and replacement, source destruction, and owned cleanup completed.
All three clients exited normally; all four logs contain zero errors or critical lines.

The independent verifier correctly **fails** this candidate: 15 of 205 checks fail across
1,399 samples. Several clients retain missing registry entries until a later array mutation.
Client1 also retains 400 health during the 100-health baseline. The cause is consistent with
two production defects: `OnRep_RegisteredBuildables` prunes unresolved actor-reference slots,
and client health-component initialization resets already-replicated health to the maximum.
The proposed repairs preserve client reference slots for replication fix-up and restrict health
initialization to authority. Getters continue filtering unresolved entries; server pruning remains.

The natural three-second respawn retains the dying pawn until replacement on authority; it
does not provide a sustained null-pawn interval. Keep its death/respawn evidence, but add a
separate bounded unpossession/re-possession phase for the absent-pawn fallback. Do not relax
the healing or peer-state assertions. Candidate17 will carry these changes and ten phases.

Original candidate16 logs, runner result, verifier, source/artifact identities, and the
20 corruption controls are preserved under `Saved/Logs/buildable-authority-20260927/` with
the `packaged-shrine-aura01`, `candidate27-16`, and `verify-shrine-aura.py` names. A runner
completion marker alone is not acceptance. This changes the next action to fresh verification
of the registry and health repairs, followed by broader health/respawn regression coverage.

### Candidate17: replicated-state repairs pass; fixture recovery needs correction

Candidate17 packaged successfully in 109.29 seconds from `compile-game13`. Its first five
phases pass the formerly failing independent registry and baseline-health checks on all peers.
The explicit unpossession phase also heals four times through the Shrine's own ASC while the
owner controller has no pawn. The full verifier still fails: 15 of 226 checks across 1,554
samples, including host error screening, synthetic ownership observations, and late relevancy.

Source inspection explains two fixture issues. `ACharacter::Restart` restores movement during
re-possession; the fixture must reapply its elevated-pawn hold. Errors begin about 4.8 seconds
after re-possession, consistent with that pawn falling from its test position and repeatedly
requesting out-of-world damage. All 20,870 host error lines remain evidence, not suppressed noise.
Also, `PlayerState::GetPawn()` is a local cache: when a pawn's replicated PlayerState becomes
null, the old cached link can remain until destruction. Observe the bidirectional pawn/PlayerState
binding to identify actual unpossession; keep the independent health and registry checks unchanged.

Candidate18 adds the fixture hold and bound-pawn observation. The health review also corrects
the initial maximum-health delegate to broadcast `MaxHealth`, instead of the current health,
which can now legitimately be lower when a client binds to an injured pawn. `compile-game14`
passes for this source. Preserve `packaged-shrine-aura02` and its original verifier as a failed
full gate even though the specific registry and initial-health defects no longer reproduce.

### Candidate18: four-process Shrine aura gate passes

`LAN-20260927-18` passes the complete ten-phase aura gate: **227 checks, 20 corruption controls,
and 1,556 independent samples** in one listen host and three clients at 200 ms. The verifier
compares native healing events with authority health deltas and each peer's last three settled
samples. It requires matching pawn bindings, player/team identity, health, and registry GUIDs.
The controls reject missing or doubled healing, false authority values, divergent peer health,
wrong registry state, lost respawn/destruction records, missing cleanup, and abnormal exits.

The actual Shrine Blueprint heals 35 HP per one-second tick without stacking overlapping
sources. Its allied target heals while the owner dies, after natural respawn, and through an
explicit controller-unpossession interval. The latter uses the Shrine ASC fallback and restores
the same pawn afterward. Enemies and the out-of-range control remain at 100 health. Range exit
stops healing; re-entry resumes it. Production GAS damage destroys the nearest source, transfers
healing to the survivor, then destroys the last source and stops healing. Registry identity
survives owner death and converges to zero after both structures are destroyed.

Re-possession restores the engine's default movement mode, so the fixture explicitly reapplies
its owned hold. All eight surviving pawns regain their saved transforms and movement state;
the original dead owner pawn is accounted for separately. Both buildables are destroyed through
production damage, every observer is released, and the diagnostic cvar is restored. All three
clients exit normally; host teardown remains a separate harness action. All four logs have zero
errors or critical lines. Warning counts are **171/15/17/16** and remain in the evidence, including
the existing cue-registration leads.

Game14 compiled in 42.51 seconds with no failure signatures. Cook/Stage/Pak completed in
97.22 seconds; all 131 staged hashes match. The compiled and staged game SHA-256 is
`AD9B0F128299414D5FC6C5DE328F08D1C822546BDDE80414485C3E93032021FB`.
The frozen source identity is `DDE37B45F07CF2F15FA03C305DC616E0C3190564AF7586C844B109F2F59D33CE`.
Evidence: `packaged-shrine-aura03-{result,verification}.json`, four original logs and their hashes,
`candidate27-18-*`, `compile-game14.json`, and `verify-shrine-aura03.py` in the gate directory.

This is a synthetic, NullRHI multiplayer gameplay scope using actual native timers, effects,
death/respawn, and replication. It does not prove physical placement, rendered aura/UI appearance,
second-PC LAN, or the final integrated match. The initial maximum-health notification is corrected
in source; rendered HUD binding remains part of the presentation gate. The same artifact now needs
the existing repeated human/bot death, elimination, and next-round regression because health
initialization is shared across characters.

### Candidate18: shared health and respawn regression passes

The same artifact passes the existing repeated human/bot death and round-reset regression:
**77 checks, 11 corruption controls, and 606 client snapshots**. Each tested human and bot dies
and respawns twice. Elimination enters PostRound without an illicit respawn, all eight players
recover in the next round, and a subsequent human death respawns without falsely eliminating
its team. Replicated death totals and zero health converge within the existing bounded contract;
no assertion or timing window was relaxed for this run. All three clients exit 0, all four logs
contain zero runtime errors, and host teardown is recorded separately.

Evidence: `packaged-shrine-respawn01-{result,verification}.json` and original peer logs.
The reused verifier is `verify-shrine-respawn-regression.py`; its only scope-label change
identifies NullRHI. The combined current artifact has **304 checks, 31 corruption controls,
1,556 aura samples, and 606 respawn snapshots**. This continuation added four game builds
(one compiler failure), three packages, and four packaged runs. Cumulative gate overhead is
**36 builds, 17 automation runs, twelve packages, and fifteen packaged attempts**.

### Next selected gate: Jail capture and release

Verify the actual floor-trap and cage Blueprints before making further production changes.
Friendly and dead actors must not consume an armed trap, including during owner-pawn loss.
A qualifying enemy overlap must consume one trap and capture eligible nearby enemies in
separate cages. Verify the configured cage health, movement-only restriction with abilities
still usable, enemy damage, friendly protection, victim release after cage destruction, and
cleanup after victim death or round reset. Surviving floor traps and non-persistent cages
have different round policies; do not conflate them. Require settled peer attachment, health,
tags, and registry identity in the later multiplayer gate.

Source leads, not established defects: `IsEnemyOfTrap` first resolves the current owner pawn
or instigator, then negates `AreOnSameTeam`; its stored-team fallback runs only when neither
proxy exists. `ApplyJailedEffect` also accepts a non-null instigator without checking its ASC.
Exercise stale/unpossessed and destroyed owner-pawn lifetimes separately. Cage capture and
release use production GAS effect handles and victim death delegates. Start with the cheapest
actual-Blueprint authority regression, then package only after a demonstrated repair or a
coherent multiplayer fixture is ready. Ward effects, accumulated survivors, cue warnings,
rendered/physical behavior, second-PC LAN, and final integrated flow remain open.

### Jail authority repair and multiplayer preparation (September 27, 2026)

`jail01` reproduced friendly capture after the placer was unpossessed or destroyed. The trap
resolved a stale pawn as its team proxy and treated an unknown team as an enemy. The trap now
uses its stored team and the target's current team. The independent cage search in `jail02`
then found both real cages missing from the match registry. Cage spawn now calls
`InitializeBuildable` with a null placer, preserving zero placement-quota cost while registering
the cages for match cleanup and replicated enumeration.

`jail04.json` passes all three actual-Blueprint owner-lifetime cases in 15.21 seconds, with zero
test errors or warnings and normal framework/process exits. The checks cover friendly and dead
rejection, two nearby enemy cages, configured 450 health, movement-only restriction, actual
melee damage while jailed, friendly damage rejection, release on destruction/death, and the
production round-cleanup method. Its isolated world has no game mode, so that reset does not
prove a full round transition. `jail05-all-buildables.json` passes all eight buildable tests in
39.03 seconds, including geometry, collision, four primary damage paths, Shrine, and Siege.

Preserve the failed fixtures: editor23 had a shadowed-variable compiler error; jail01's cage
lookup assumed registry membership; jail02's friendly probe used the low-level damage entry
instead of the production team-aware entry; jail03's controller had no local role, so GAS
rejected its predicted primary. The corrected fixture initializes the local controller role,
disables remote activation fallback, and still requires real cage damage. No melee/collision
production change was made for that test-ownership failure. Editor24–27 passed. Game15 caught
two fixture pawn-type mismatches; game16 passed their correction. Game17 adds reviewed fixture
separation and post-respawn settling before the first package.

The new `BwayJailCaptureAudit.cpp` uses seven phases in four processes at 200 ms: baseline,
capture while the placer is unpossessed, a remote trapped Argus's production primary, lethal
cage damage, victim death/respawn, recapture, and production round reset with one surviving
floor trap. Each peer observes local attachments, cage health, tags, registry identities, and
quota. The runner preserves client exits separately from host teardown; an independent verifier
checks actual state and corrupted-evidence controls. Candidate19 is in preparation; no packaged
Jail pass is claimed yet. Physical placement/attack input, visual cages, second-PC LAN, and the
final integrated flow remain open. Cumulative gate counts before packaging: 44 builds,
22 automation runs, twelve packages, and fifteen packaged runtime attempts.

### Candidate19 remote quota defect

Candidate19's seven-phase runner completed, but the independent verifier failed eight of 157
checks across 1,177 samples. All failures were settled quota counts on the two clients that did
not own the placer. Cage attachment, 450-to-420 primary damage, movement/jail tags, death release,
respawn and round cleanup agreed. All three clients exited normally; no runtime error or
critical lines occurred. The 16 corrupted-evidence controls were rejected. Keep
`packaged-jail-capture01-{result,verification}.json` as failing evidence, not a promoted pass.

`GetBuildableCountForPlayer` compared `Buildable.Owner` with `PlayerState.Owner`. A remote
PlayerController is absent on other clients; ownerless cages and the remote player's controller
reference therefore both compared equal to null. A replicated `PlacingPlayerState` now records
stable ownership during authoritative initialization. Registry counts compare that identity;
cages retain null placement ownership. This also prevents unrelated remote-owned structures
from being conflated. It does not change once-per-round or active-cap rules.

Editor28 and `jail06-all-buildables.json` pass all eight tests, including new assertions that a
placed/persistent trap counts for its player and a PlayerState without a visible controller
never counts ownerless cages. The suite took 50.10 seconds with zero test errors/warnings.
Game18 and candidate20 will validate the unchanged seven-phase Jail verifier plus affected
remote placement/persistence and Siege active-cap/expiry regressions. Preserve candidate19's
frozen source and all failures. Counts before candidate20: 46 builds, 23 automation runs,
13 packages, and 16 packaged attempts.

### Candidate20 Jail pass and affected regression findings

`packaged-jail-capture02-verification.json` passes all 157 checks, 16 corruption controls and
1,176 samples using the unchanged candidate19 verifier. All four peers agree on cage identity,
attachment, health, jail/movement tags and placement counts through seven phases. The remote
trapped Argus used its real primary, reducing its cage from 450 to 420 health; friendly damage
was rejected. Destruction, victim death/respawn, recapture and production reset cleared the
appropriate cages while retaining the same floor trap. Three clients exited normally and
four logs had no runtime error/critical lines. All 131 staged hashes matched game18.

Affected regressions reused candidate20. `packaged-buildable07` stopped at the second-round
fixture ground search: 31 ground hits, 23 fixture-clearance rejections and eight geometry
rejections. Earlier malformed submissions, quota rejection, valid placement and production
persistence had completed. The development-only finder now retains its original search and
adds a bounded denser fallback. Every point must still pass the same production validation
and the same fixture pawn/spawn clearance. This changes synthetic target discovery, not
shipping placement rules. The failed run and partial independent verifier remain preserved.

`packaged-siege04` completed native cap/expiry behavior, but failed its held-pawn accounting
check. The host log proves a bot spawn collision at (-70,3500,362), and the round-reset count
was three versus four alive on team 0. Cleanup restored seven pawns, with eight expired from
the prior round. The team spawn selector chose randomly without occupancy checks. It now
prefers clear tagged team starts, then engine-adjustable starts, preserving team boundaries.
`spawn01.json` passes a deterministic initialized-world regression: a real capsule blocks one
start, 32 choices prefer the clear start, the blocker moves, and 32 more choices use the newly
clear start. Switching teams selects only the opposing team's tagged start. Editor29 passes;
this does not by itself prove packaged recovery.

Candidate21 will carry the spawn-selection repair and denser fixture search. Its Siege verifier
adds an explicit eight-restored-pawn requirement and rejects spawn-collision warnings; the
prior verifier and failing candidate20 evidence are retained. Recheck placement/persistence,
Siege active-cap/expiry, and repeated human/bot death and next-round recovery. The stock Lyra
next-frame retry helper does not recognize the modular relic bot controller; all-starts-blocked
retry recovery is a separate source lead, not proved by choosing a clear start.

Cumulative gate counts before candidate21 packaging: 48 builds, 24 automation runs,
14 packages and 19 packaged attempts. The current Jail result remains a scoped candidate20
pass. Ward effects, accumulated survivors, rendered/physical checks, second-PC LAN, full
roster/reference reconciliation and final integrated acceptance remain open.

### Candidate21 placement, Siege, and respawn regressions

Candidate21 passed all three affected four-process regressions at 200 ms packet lag. Game19
matched the staged executable and all 131 artifact hashes. BuildCookRun completed in 66.25
seconds. `candidate21-regressions.json` records the completed batch; no test process remains.

| Evidence in `Saved/Logs/buildable-authority-20260927/` | Verified result |
|---|---|
| `packaged-buildable08-verification.json` | 146 checks, 23 corruption controls, 1,034 samples; remote GAS placement/rejection, production round persistence, and second placement |
| `packaged-siege05-verification.json` | 91 checks, 17 controls, 1,222 samples; cap, natural expiry, renewed placement, all eight pawns restored, and no spawn-collision warning |
| `packaged-spawn-respawn01-verification.json` | 77 checks, 11 controls, 605 client snapshots; repeated real human/bot death, no PostRound restart, all-eight next-round recovery, and another human respawn without false elimination |

Nine clients exited normally; the three listen hosts were terminated by owned fixture cleanup
after client completion. All twelve logs passed runtime error/critical screening. These runs
use NullRHI on one PC and prove synthetic gameplay and transport only. Physical targeting,
appearance, performance, second-PC LAN, and final integrated acceptance remain open.

The placement run selected SunShrine and succeeded using the original sparse search. The
denser development-only fallback compiled but was not dynamically exercised; do not claim a
fallback runtime pass. The prior candidate20 failures remain preserved. Candidate20's Jail
pass remains bound to that frozen artifact; candidate21 does not add a new Jail run.

Current counts: 48 builds, 24 automation runs, 15 packages, and 22 packaged runtime attempts.
The next gate is Ward aura ownership/team filtering and effect cleanup through target avatar
replacement. Require actual Blueprint behavior before a production repair, then verify the
affected multiplayer state. All-starts-blocked modular-bot retry remains a separate recovery
lead; the clear-start regression does not prove that case.

### Ward owner and target lifecycle repair

`ward01.json` reproduced failures in all three actual-Blueprint owner-lifetime cases. An
unpossessed/destroyed placer could make the Ward slow allies. A detached victim pawn left its
infinite slow on the persistent PlayerState ASC: respawning outside retained 0.5 speed, and
reentering reduced it to 0.25. In-place team changes and destruction after target detachment
also failed cleanup. Initial enemy slow, radius, health, and independent overlapping sources
provided working controls.

The Ward now filters by its stored team. Effect removal resolves the owning ASC from the
active effect handle, independently of the old pawn. A server-only 0.2-second reconciliation
timer removes ineligible/stale effects and admits newly initialized avatars already inside
the aura. Begin/end overlaps still provide immediate ordinary entry/exit. EndPlay clears the
timer and every owned effect. Source context rejects a stale instigator without an ASC.

Editor31 and `ward02.json` pass all three lifecycle cases in 15.56 seconds, with zero test
errors/warnings. `ward03-all-buildables.json` passes all eleven tests in 40.22 seconds, with
normal framework/process exits and zero test errors/warnings. The test retains current
authored compound stacking; that does not establish final-alpha stacking parity. Timer-driven
transitions use separate latent steps because TimerManager ignores duplicate ticks in one
engine frame. No shared gameplay timer behavior was changed for the fixture.

The new development-only `BwayWardAuraAudit.cpp` selects a remote Korryn and opposing remote
victim, holds all pawns, and observes 14 phases across a listen host and three clients. Its
scope includes owner unpossession/death, target death/respawn, range and team changes, two
independent sources, lethal removal, and production round cleanup. Every peer logs its local
attributes, tags, pawn identities, deaths, registry IDs, quota, and source health. The runner
waits for all client exits before host teardown. The independent verifier uses settled sample
windows and 27 corrupted-evidence controls; a runner PASS alone cannot promote this gate.

Game20 failed a const parameter in the new fixture's network-ID sorter; game21 passed the
correction in 38.79 seconds. Candidate22 is prepared from game21. Packaged Ward behavior is
not yet verified. Counts entering packaging: 52 builds, 27 automation runs, 16 package
attempts including candidate22, and 22 packaged runtime attempts. Keep the failed ward01
and game20 records. Prevention: remove an infinite effect through its actual owning ASC;
an old avatar's component lookup cannot reliably clean a PlayerState-owned effect.

Candidate22 packaged in 96.97 seconds and verified all 131 staged hashes. The first three
Ward phases completed, but phase3 owner self-damage reported a missing source location and
never produced death. The observer timed out; `packaged-ward-aura01-{result,verification}.json`
preserves 1,371 samples and the failed scope. The independent verifier rejected the partial
run; no packaged Ward pass is claimed.

`ALyraPlayerController::OnUnPossess` explicitly clears the ASC avatar. The fixture reused its
already initialized pawn after possession without restoring that binding. Its restore helper
now calls `InitAbilityActorInfo` on the same PlayerState/pawn pair, checks the avatar before
self-damage, and stops a phase after 25 seconds so native cleanup precedes observer timeout.
This restores state disturbed by the test; production Ward code is unchanged. Candidate23
uses game22 and the same state assertions and corruption controls; only the verifier's artifact
lookup changes. Direct same-pawn re-possession is not claimed as a verified production flow.

Candidate22's source-identity difference was only a three-byte UTF-8 BOM added by Windows
PowerShell to the resume status/untracked text. Normalized text and all 236 frozen file hashes
matched; `candidate27-22-provenance-review.json` retains that review. Preserve the original
frozen snapshot. Invoke the next freeze through Windows PowerShell to match the resume helper.

Candidate23 packaged in 98.98 seconds and matched all 131 staged hashes. Its 14-phase runner
completed with three normal client exits and all eight surviving pawns restored. Independent
verification passed 573 of 574 checks across 2,358 samples; the sole failure was host runtime
error screening. The team-change probe redundantly called `SetGenericTeamId` on a possessed
pawn, which Lyra correctly rejects because the controller drives its team. The PlayerState
change already propagated correctly, and every independent speed, tag, identity, death,
registry, quota, source-health, and cleanup assertion passed. Do not promote the run while
the error-screening check fails.

Game23 removes only that redundant fixture setter; the PlayerState/controller route and pawn
network update remain. Candidate24 repeats the same 574 checks and 27 corrupted-evidence
controls. Preserve `packaged-ward-aura02-{result,verification}.json`; no error was reclassified
or suppressed. Generic web searches did not settle final-alpha Ward stacking semantics;
`ward-stacking-reference-search.json` records the queries and the narrower next research path.

### Candidate24 Ward lifecycle pass

`packaged-ward-aura03-verification.json` passes all **574 checks and 27 corruption controls**
across **2,356 samples**. The assertions match candidate23; only the artifact lookup changes.
All four processes use 200 ms packet lag. Each peer independently verifies speed multipliers,
slow tags, owner/victim identity and deaths, registry IDs, placing-player counts, and source
health in settled windows. The log's `quota` field is the registered buildable count for the
placer; this fixture does not prove a new physical placement or once-per-round input path.

The 14 phases cover baseline, one Ward, owner unpossession, real owner death/respawn, range
exit/reentry, two independent Wards, lethal removal of one, real victim death/respawn outside,
reentry, stationary team change in both directions, production round cleanup, and lethal
removal of the surviving Ward. Round cleanup replaces all eight pawns and retains the same
Ward identity; this does not claim a new orchestrator `StartRound` transition. Cleanup restored
eight pawns, accounted for ten expired avatars, and stopped every peer observer. Three clients
exited 0; the owned listen host was terminated separately after their completion.

All four logs have zero runtime errors and critical messages. Warning counts remain
234/17/16/16 (host/client1/client2/client3), preserved in the verifier and
`ward03-warning-inventory.json`. Duplicate pistol GameplayCues and hit-cue tag discovery remain
actionable polish leads; this gate does not reclassify or suppress them.

Candidate24 BuildCookRun completed in 95.41 seconds. All 131 staged hashes match;
the game SHA-256 is `14E634AB6B86117DE543A2A764B3FC10CE33E0DC429D8165DB8D607FBFE60B33`.
Frozen source identity: `6B2263A1FBEBD83348FC04116FC7946216697174A2B310CFA461AD0106151D69`.
Game23 contains the unchanged verified Ward production repair and the corrected development
fixture. Editor31/ward03 retain all eleven automation passes. Counts: 54 builds, 27 automation
runs, 18 packages, and 25 packaged attempts. Failed candidate22/23 records remain intact.

Scope remains NullRHI on one PC. Physical input, rendered effects/readability, second-PC LAN,
performance, final-alpha stacking/accumulation reconciliation, the full roster, and the final
integrated flow are not completed by this result. The next focused gate is modular-bot retry
when all tagged team starts are temporarily blocked: preserve team boundaries, avoid duplicate
pawns, and recover once a valid start clears.

## All-starts-blocked recovery reproduction (September 27)

Expected result: while a modular relic bot's tagged team starts are physically obstructed, its normal death timer must not produce a wrong-team or duplicate pawn. Clearing one start must produce exactly one living pawn and restore the team's actual alive count. Initial spawns, round resets, and cancellation at PostRound/teardown require separate coverage before accepting a shared retry repair.

The source audit is `Saved/Logs/buildable-authority-20260927/spawn-recovery-source-audit.json`. Lyra's retry helper handles player controllers and `ALyraPlayerBotController`, whereas relic bots derive from `AModularAIController`. The round component consumes its timer before restarting; an unsuccessful restart has no subsequent timer. An engine restart with no start can also return without calling `FailedToRestartPlayer`.

The development probe `bway.Test.SpawnRecoveryAudit` uses the live game mode, actual bot and production death timer. It temporarily moves tagged starts apart, surrounds them with fixture-owned collision, and restores starts and surviving pawn transforms/movement/brains on completion. This isolated geometry does not establish rendered targeting or original-map collision acceptance.

- Editor32 failed because `auto*` could not deduce an element of the `TObjectPtr` PlayerArray. Editor33 compiled after explicit pointer iteration. No gameplay source repair was included.
- `spawn-recovery01-result.json`: the standalone editor `-game` experiment did not dispatch the fixture, could not resolve the requested experience and produced 59 startup error lines including experimental Python toolset errors. The verified owned headless game PID was stopped; zero fixture samples were collected. This is setup failure, not bot recovery evidence. The runner's earlier UTC parse rejection launched no process; the comparison now preserves the parsed DateTime kind.
- Game24 / candidate `LAN-20260927-25`: BuildCookRun succeeded in 99.85 seconds, with all 131 staged file hashes verified. Stage recovered a transient Zen connection refusal through its own retry; the raw package log preserves it. `spawn-recovery02-result.json` exited normally with zero runtime errors, but setup rejected the original single-box geometry because the engine could adjust outside it. No death/recovery assertions ran.
- The corrected obstruction has neighboring boxes covering all first-step adjustment destinations. Editor34 and `spawn-geometry01.json` pass `Breakaway.Spawn.OccupiedTeamStart` with zero test errors/warnings, including actual overlap, rejection of teleport adjustment, no opposing-team fallback, and recovery of the same start after clearing the blocker.
- Game25 / candidate26 are the unchanged-production reproduction with corrected geometry. Retry reason: changed and independently verified fixture geometry, not an unchanged retry.

The completed engine-storage request was revalidated without new deletions; it did not advance gameplay acceptance. This turn resumed the pending spawn gate from the authoritative source/process inventory. No visual, physical-input, separate-PC or end-to-end proof is inferred from these checks.

### Valid baseline and shared recovery repair

Candidate26 (`compile-game25.json`) completed BuildCookRun in93.92seconds and passed all131 staged hashes. `spawn-recovery03-result.json` exited normally with no runtime errors but still rejected setup. The raw log identifies static Dorado PlayerStart capsule components: the requested fixture moves were rejected. The earlier interpretation that the first setup rejection was only teleport adjustment was incomplete; the geometry automation did not cover map component mobility.

The probe now records each start's original transform and mobility, makes it movable, checks relocation success, and restores both properties. Game26/candidate27 contain only this private native probe change over candidate26. `candidate27-27-native-stage.json` records inherited cooked content and the two changed staged files (game executable and matching symbols); all131 manifest files were verified. The staging copy initially included two runtime-generated config files from the previously executed base artifact. These copies were excluded from the new owned candidate before launch; the staging helper now copies only manifest-listed files. No production, asset, reflected type or configuration change was included in this native probe patch.

`spawn-recovery04-result.json` is the valid before-fix evidence. The live modular bot died normally with six team starts blocked. Its production respawn timer reported that no living pawn was produced. One start was cleared after the deadline; the bot remained dead for another10seconds. The fixture collected75samples, restored six start transforms/mobility and seven surviving pawns, and exited0 with zero runtime errors. This contradicts recovery completion.

The source repair gives `ABreakawayGameMode` one weak delayed retry per controller. It covers both engine failure callbacks and a no-start return without a failure callback, and replaces Lyra's controller-type-specific retry dispatch. Successful spawns cancel pending retries. Round reset, round/phase cleanup, logout and mode teardown cancel owned retries. `ABwayGameState::RecalculateTeamAliveCount` derives counts from living pawns; initial failed spawns and delayed successes no longer leave stale membership counts or double-credit a still-dead teammate. The existing death delay and upgrade clock remain owned by the round component.

The expanded native probe has death, initial, reset and cancel scenarios. Initial coverage uses the production bot component's trim and missing-bot creation methods, requires a new controller with no pawn or prior StartSpot, and does not manufacture a pawn. Reset coverage uses production `ResetRoundState`, checks blocked team count0, and permits four separate recovery attempts through the one newly cleared start. Cancellation ends the round before clearing the start and requires no PostRound replacement. Every scenario checks actual pawn/roster agreement and restores fixture-owned state. These are isolated collision and authoritative lifecycle scopes, not original-map geometry, input, rendering or peer replication proof.

Editor35 passes. `spawn-geometry02-fixed.json` and `spawn-respawn-upgrade01.json` both pass with zero test errors/warnings and normal framework/process exits. Game27/candidate28 fully recook the production repair; the probe-only native patch shortcut does not apply to that repair. Its live scenario evidence and multiplayer regression remain pending at this checkpoint.

### Candidate28 verified recovery and preserved regressions

Game27 and the full candidate28 BuildCookRun pass; cook/stage/pak took99.38seconds. The frozen executable hash is `B85EF22E3CC0C6DBEF80358C392364B08DCC07087B719EF2D1FBEE621C01D1D7`; all131 staged files match their manifest. Source/compile/cook provenance is under `Saved/Logs/buildable-authority-20260927/candidate27-28-*` and `source-snapshot-LAN-20260927-28.json`.

`spawn-recovery05-verification.json` passes all four standalone authority scenarios:114 checks,48 deliberately corrupted controls and131samples. Each process exits0 with zero runtime errors. The death, initial and reset cases recover exactly one pawn per controller; reset recovers all four blocked teammates through one cleared start. The cancellation case retains the dead bot through PostRound despite the cleared spawn. All starts recover their original transform/mobility and all surviving pawns recover their movement/brain ownership and appropriate map transforms.

`packaged-spawn-respawn02-verification.json` passes the established four-process200ms regression:77checks,11negative controls,607client snapshots. It includes two actual human/bot deaths, correct team counts, no PostRound restart, next-round recovery and a subsequent human death/respawn without false elimination. All three clients exit0; host termination is recorded as owned fixture cleanup. All four logs are free of runtime errors.

`packaged-spawn-countdown01-verification.json` passes102checks and17negative controls across694UMG samples in four rendered processes. Native timer/game clocks, displayed seconds, repeated deaths, widget reattachment, shop suppression/resume and round cleanup pass. Three clients exit0 and the logs contain zero runtime errors. This checks widget state and timing, not visual appearance or physical input.

For obstructed peer convergence, `run-packaged-spawn-obstruction28.ps1` reuses the already compiled native probe and the existing read-only client journal. Clients exit normally at journal sample100; the host remains alive until all clients exit. `verify-spawn-obstruction28.py` requires four-human/eight-player rosters, exact player/team/death identities, matching actual and reported alive counts, health, stable replacement ownership and phase. It evaluates at least two full peer snapshots during the final0.85seconds of obstruction and after1.2seconds of authority recovery/cancellation, retaining earlier samples as transients. All cases retain raw logs and compare against the same frozen artifact. The first death case passes95checks/9controls/300client snapshots; remaining case verdicts are pending at this checkpoint.

### Obstructed peer baseline correction (candidate29)

The first four-process obstruction family is preserved as `packaged-spawn-obstruction01-*-{result,verification}.json`. All four native authority fixtures and transports completed with normal client exits. Death passed95checks/9controls and cancellation passed89checks/9controls, each with300client snapshots. Initial failed two peer count-versus-pawn assertions: unrelated existing pawns were still absent in one early blocked-window sample on two clients, then present in their subsequent samples. Reset failed five assertions on client1 because its replacement was still absent in one settled-window sample1.288seconds after the authority's first stable sample; later rows contained it. These failures are retained, not reclassified as passing.

The fixture moved held pawns toX=100000/Z=30000 while the released start was atX=50000/Z=20000. This introduced a distant relocation and an unproved client baseline into a spawn-retry test. The next probe keeps held pawns near the released start, requests their movement update, and waits three seconds before changing the lifecycle. The independent verifier additionally requires two complete eight-living-pawn snapshots from every client before the scenario begins. Recovery windows and prior assertions remain unchanged. Missing and unsettled baseline corruption controls are added.

This change affects only `Private/Development/BwaySpawnRecoveryAudit.cpp`. Game28/candidate29 inherit candidate28's verified cooked content and unchanged production repair; `candidate27-29-native-stage.json` and the131-file manifest record the two replaced files (native executable and matching symbols). The fixture-only patch does not require another cook. All four peer scenarios are rerun as `packaged-spawn-obstruction02-*`, with the original family kept intact.

Workflow lessons: prove fixture collision geometry with the cheap engine-level test, but also verify real map component mobility and relocation. Establish a peer baseline before measuring a change; do not mix actor relocation/readiness with restart convergence. Copy only frozen manifest files when constructing a native-probe patch, because executed staged directories acquire runtime-generated files. Do not apply that optimization to changed production defaults, assets, reflected types or configuration without a separate artifact analysis.

Candidate29 death passes106checks/11controls/300client snapshots. Initial passes107checks/11controls/300snapshots in `packaged-spawn-obstruction02-initial-reconciled-verification.json`. Its polling wrapper initially reported `client1 exited early: 0` because it read the log immediately before the final marker was flushed. All three original client processes had already exited0 without cleanup intervention, and all three final logs contain sample100 plus the proper test-exit marker. The host completed its native fixture and was terminated last. `reconcile-obstruction02-initial.py` verifies these conditions and writes a separate derived result, preserving the original failed JSON and raw logs. No client, host, compile or cook was repeated. The current runner now drains a confirmed terminal client log once before deciding an exit was early. Baseline and recovery assertions were not changed.

### Candidate29 obstructed-spawn peer acceptance

All four scenarios pass: death (106 checks), initial spawn (107), round reset (107), and
PostRound cancellation (100). Each rejects 11 corrupted-evidence controls and contains 300
client snapshots. `spawn-recovery29-family.json` totals 420 checks, 44 controls, and 1,200
snapshots. The four attempts use one listen host and three clients with 200 ms packet lag.
All 12 clients exit normally without cleanup intervention. Host termination is recorded
separately after client completion, and all 16 runtime logs pass error screening.

The independent verifier requires a settled eight-player baseline, at least two complete
peer snapshots during obstruction and recovery, exact team/death identity, matching actual
and reported living counts, and stable replacement pawns. Round reset recovers all four
blocked teammates through one cleared start. Cancellation remains in PostRound without a
replacement. Initial-spawn reconciliation preserves its original wrapper failure and raw logs.

The aggregate is a scoped pass. Original-map collision, visual appearance, physical input,
second-PC LAN, and final integrated flow remain unproved. Restart scheduling/cancellation,
round lifecycle, pawn ownership, alive counts, or relevant replication changes invalidate
affected results. Current totals: 63 builds, 30 automation runs, 21 full packages, 42 packaged
attempts, two native probe stages, and one unsuccessful standalone editor-game attempt.

### Next gate: accumulated survivors

Expected result: a remote player places one structure in each of three production rounds.
All previous structures retain their identities, health, team, ownership, and registry entries
on every peer. A later round clears its placement quota but the configured active cap still
rejects another placement. Enemy destruction frees capacity, permits one replacement, and
must not reset a placement already consumed during that round.

The September 21 hands-on reference confirms persistence and accumulation, but does not
establish exact per-character caps or aura stacking arithmetic. `asset-config02.json` records
cap 3 for Shrine, Jail, and Ward, and cap 1 for Siege. The native default of 2 is overridden
by those assets. Preserve these values pending stronger final-alpha evidence; a current-cap
pass cannot establish exact reference parity. The earlier two-structure gate and Siege cap
scope remain valid within their frozen inputs. Extend the existing remote placement fixture
and peer journal rather than introducing a separate transport framework.

### Three-round accumulation probe and first result

Game29 compiles the private accumulation extension. Candidate30 inherits the cooked content
from candidate28 through native probe candidate29. Only `BwayBuildableAuthorityAudit.cpp`
changed among build inputs; only the executable and matching symbols changed among 131
staged files. Its executable SHA-256 is
`8358D97EC14BAF8D3BC302A0970A0AB9985E2DB6EF3862DB5EE7500923E54D1E`.

The first stage preflight rejected Windows PowerShell's nested JSON-array result before copying
files. Assigning the deserialized array directly restored the one-file comparison. The second
stage attempt passed all hashes; no compile or cook was repeated. Keep the failed command in
this history. Metadata distinguishes the immediate native base29 from the full cook28.

`packaged-accumulation01-shrine-result.json` completes the normal remote GAS sequence,
three production round transitions, damaged survivor persistence, configured cap rejection,
enemy destruction, replacement, and same-round quota rejection. All three clients exit0 and
the host is terminated afterward. The independent `-verification02.json` passes 541 checks,
20 corrupted-evidence controls, and 1,916 samples across four processes at 200 ms.

The first `-verification.json` is retained: all state checks passed, but three final sample-count
checks asked for journals after their observers had stopped. Each peer intentionally stops
after two seconds of verified empty state, before the host's five-second release. The corrected
verifier checks at least eight terminal snapshots over the final 1.8 seconds, requires completion
at least 1.9 seconds after authority cleanup, and preserves the same empty-registry, quota,
ability, targeting, phase, and actor assertions. No process was rerun for this correction.

Ward uses the same candidate and verifier. Jail remains next. Exact final-alpha numeric caps
remain unresolved; `buildable-cap-reference-search.json` records the bounded search and dated
persistence evidence. No asset cap or production gameplay value was changed for this test.

Ward accumulation also passes 541 checks, 20 controls, and 1,917 samples on candidate30.
The first Jail attempt did not enter fixture setup: the normal timeout selected Alona and
Korryn for all remote humans. Its log contains no matching remote Rawlins owner. The exact
owned headless host was stopped after its PID, executable, and log argument were verified;
the existing harness recorded the failure and cleaned its clients. Preserve the result and
`packaged-accumulation01-jail-setup-stop.json`; this is missing fixture setup, not a failed
Jail gameplay assertion.

The accumulation probe now selects its requested hero for the lowest-ID remote controller
while production selection is active, verifies the lock, and retains that PlayerState identity.
It fails explicitly if the selection window was missed. No live hero swap, asset override,
or production rule change is used. Game30 passes; candidate31 changes only the same private
probe file over candidate30 and retains cook28. All 131 staged hashes match; its executable
SHA-256 is `95486FDEA0B77C1129579587394558C488AC4152C81CCF0088893F0C7FCCE286`.
The Jail retry uses the original state assertions plus an explicit-selection assertion and
missing-selection corruption control. Earlier Shrine/Ward frozen passes remain valid.

### Jail-specific accumulation remains incomplete

`packaged-accumulation02-jail-result.json` passed setup with an explicitly selected remote
Rawlins, but the native fixture stopped at `accumulation-damage` in stage10 after two placements.
Its damage marker reports `applied=0 before=1 after=1 gone=0`. All three clients exited0 after
fixture cleanup; the host was terminated separately. No three-trap accumulation pass is claimed.

`BwayRawlinsJailBuildable::InitializeAbilitySystem` and `ArmTrap` deliberately disable damage
and assign nominal health1 to the armed floor trap. Its overlap handler consumes the trap and
spawns damageable cages. The generic Shrine/Ward damage-persistence assumption does not fit
that authored lifecycle. Do not make the trap damageable to pass this fixture. The next Jail
probe must retain three traps through production rounds, assert armed immunity, trigger one
through actual enemy overlap, verify the placing-player registry during the cage lifetime,
then destroy the cage through production damage before requesting replacement. Preserve the
consumed round quota after another trigger/removal. Restore all fixture-moved pawn transforms.
Existing Jail capture/cage damage evidence is supporting scope, not this combined gate's pass.

An intermediate progress message incorrectly treated a combined Setup/Accumulated/Complete
marker count as successful accumulation. The terminal result contradicted that reading and was
reported immediately. Future progress projections must distinguish successful phase markers
from failure completions; only the final independently checked verdict promotes a gate.

Current totals are 65 builds, 30 automation runs, 21 full packages, 46 packaged attempts,
four native probe stages, and one unsuccessful standalone editor-game attempt. Candidate31
contains only the deterministic selection probe change. Shrine/Ward passes and all earlier
scoped results remain frozen; no gameplay rule, asset cap, or trap immunity was changed.

## GameplayCue registration polish: read-only diagnosis

Current packaged logs still report duplicate `GameplayCue.Weapon.Pistol.Fire` registrations
and early discovery of unregistered goal, hit, and slide tags. Project `DefaultGame.ini` adds
`/Game/GameplayCueNotifies` at startup, while those three tags are declared under the later
BreakawayCore game-feature tag source. Inspect registration order before changing vocabulary.

The first five-asset commandlet attempted protected `GameplayCueName` CDO access. It failed
without saving; that surface is now recorded as unsupported. `cue-registration02-{run,report}`
uses the public `GameplayCueTag` plus searchable AssetRegistry metadata instead (the report
filename is `cue-registration02.json`). It exits0, reads all five assets without errors, and
verifies seven inspected/load-dirtied asset hashes are unchanged. Loading Niagara dependencies
marks two packages dirty in memory; the commandlet reports them and never saves them.

Both `/ShooterCore/Weapons/Pistol/GCN_Weapon_Pistol_Fire` and
`/BreakawayCore/GameplayCues/GCN_BW_Weapon_AlonaPrimary_Fire` declare
`GameplayCue.Weapon.Pistol.Fire`. The three project cues declare the exact goal, hit, and slide
tags named in the startup warnings. Empty package-referencer lists do not establish that a cue
is unused: gameplay calls route by tag. Next, inspect Alona's actual cue dispatch before
retagging the duplicate, and make startup cue tags available before discovery. Require a fresh
cooked package with no affected registration warnings and actual relevant cue delivery; asset
inspection alone does not prove visible effects or gameplay delivery. No cue/tag asset or
configuration repair has been applied yet.

### Jail accumulation contract correction

The source audit found that spawned cages have no owner or placing PlayerState by design.
`SpawnCageForVictim` initializes each cage with a null controller: global registry membership
persists, but cages do not consume the placer's active-trap cap. The earlier proposed requirement
that cage lifetime retain placer capacity was unsupported and is superseded. Exact final-alpha
trap/cage rules still require reference reconciliation; this is a test of current authored rules.

The new probe retains three traps through production rounds and explicitly verifies armed
immunity. It finds a point inside only the selected trap's actual trigger volume, excludes other
enemies from the capture radius, and moves one held enemy there. Production overlap consumes
the trap and creates the cage. After four-peer settling, replacement must be possible while the
cage is alive: three world actors (two traps and a cage), but two placer-owned traps. It then
places the replacement, destroys the first cage through production damage, restores the victim,
triggers the replacement, and verifies the consumed round quota still rejects placement before
and after cage destruction. It restores each moved transform and cleans every fixture cage.
No damageability, trigger radius, quota, or asset configuration is overridden.


### Candidate32: observed client immunity defect and replacement setup failure

`packaged-accumulation03-jail-result.json` is terminal, failed at the remote replacement
submission: `ClientNoGround sequence=23`. All four owned processes were cleaned up by the
harness; there is no normal-client-exit or completed accumulation claim. The logs prove
three persistent traps, armed authority immunity, a real overlap capture, and a globally
registered cage exempt from the placing-player cap. The verifier preserves the partial result.

The independent samples also reveal a production defect: every remote armed trap reports
`damageable=1`, while the authority reports0. Base `OnRep_BuildState` sets active structures
damageable; Jail only restored immunity in authority arming. Candidate33 overrides the build
state notification for Jail and establishes immunity in BeginPlay on every peer. The regression
test calls the actual reflected notification on each authored buildable, requiring ordinary
structures/cages to remain damageable and the armed floor trap to remain immune.

The replacement setup searched165 ground hits;119 were rejected by the test's one-thousand-unit
pawn margin and46 by production geometry. The captive occupied the cleared placement region.
The development ground finder now derives captive clearance from live trap trigger radius plus
pawn radius and50 units. Other pawns/spawns retain the broad margin. Production geometry/range,
trap volumes, quota, and damage rules remain unchanged. The test must still prove exactly one
live cage and an untriggered replacement on all peers. Candidate33 requires a fresh full cook;
this production repair cannot inherit cook28 as a private-probe-only patch.


### Candidate33: Jail client immunity and accumulation verified

Editor36 and game32 compiled with empty failure signals. Both affected editor DLLs advanced.
`jail04-all-buildables.json` reports all 11 tests passed, no warnings, framework/process exit0.
The full cook/stage33 exited0 and all 131 artifact hashes match. Its game executable SHA256 is
`DAD6EDB392512CD54DA67C52E57BEB5911F67626AE55CE442EFA9CDBA8A1394F`.

`packaged-accumulation04-jail-verification02.json` passes 747 checks and 29 negative controls
over 2,074 samples from four separate processes under 200 ms packet lag. All three clients
exit normally; the host remains alive until their release and is terminated by owned cleanup.
All four runtime logs have zero errors. Three armed traps survive actual production rounds;
authority and all clients agree on immunity, health, identity, team, ownership and bounds.
The fourth-round cap rejects placement without consuming quota. A real enemy overlap consumes
one trap, and replacement succeeds while its quota-exempt cage is still alive. Two production
cage destructions release captured tags; the victim's exact transform is restored twice.
Consuming/destroying the replacement does not reset the already-used round quota. Cleanup
restores all eight current pawns, releases both captured states, and leaves no fixture actors.

The first verifier output (`...-verification.json`) failed only its stale candidate allowlist:
all 746 gameplay checks passed. The correction explicitly admits33 and also requires the
artifact candidate identity plus staged/compiled hash equality. Re-extraction used the same
immutable logs; no build, cook or runtime retry. A targeted negative control reproduces the
observed remote damageable-trap defect and must fail. The original failed extraction is retained.

`accumulation-family.json` combines frozen candidate30 Shrine/Ward with candidate33 Jail:
1,829 checks, 69 negative controls, 5,907 samples, nine normal client exits. Each case retains
its exact artifact/source scope. This is not a final integrated-candidate, physical targeting,
rendered appearance, second-PC LAN or exact final-alpha tuning claim. Game32 changed production
Jail client state, so candidate33 used a full cook, not a private-probe patch. Overhead for this
repair: two target builds, one automation run, one full package, one four-process runtime attempt.

Next gate: missing/duplicate GameplayCue registrations. Native `BwayGameplayAbility_AlonaPrimary`
spawns/configures a projectile and does not itself dispatch the duplicated Pistol.Fire cue.
Inspect the actual Blueprint execution/inheritance and presentation path before retagging;
this native observation alone does not establish that the Blueprint cue is unused.


### Cue dispatch audit03: next repair is concrete

One editor lifecycle (PID27528, `cue-editor01-launch.json`) loaded editor36 and used the
structured bridge to read the exact Alona primary and cue assets. The ability's parent is
`BwayGameplayAbility_AlonaPrimary`; its EventGraph has only two unconnected event nodes,
with no execution/data edges. The native primary body spawns/configures the projectile and
contains no cue dispatch. The duplicate fire cue is a `GameplayCueNotify_Burst` Blueprint
with no EventGraph nodes; its authored burst supplies placeholder pistol audio, camera shake
and haptics, with a `weapon_r_muzzle` default and a `hand_r` sound-placement override.
`cue-alona-graph03.json` and `cue-alona-properties03.json` preserve the complete inspections.

`asset.read_properties` addresses the Blueprint asset and cannot read its CDO `BurstEffects`;
the bounded function-scoped Python inspection succeeded. This is a cached capability finding,
not a reason to retry the unsupported asset-property call. Final dirty-package inventory was
empty; no asset was saved. The bridge closed this exact editor normally. These observations
establish the missing Alona fire connection, not network cue delivery or perceived audio/VFX.

The next expected result is valid, unique startup registrations for goal/hit/slide/Alona fire
and actual production cue delivery to the correct peers. The three project-root cue tags are
currently declared only in a later game-feature config. Alona requires a dedicated fire tag
and a connection from the real primary ability. Preserve prediction behavior and owner-only
camera/haptic feedback; check the authored placement against placeholder sockets. Fresh asset
serialization/cook plus runtime dispatch evidence are required; do not count absence of warnings
alone as complete presentation. No cue asset, tag config or firing implementation changed here.

Final source comparison found two unrelated editor-bridge inputs changed at20:37:25 UTC: `FoliageHandlers.cpp` and `LevelHandlers.cpp`. They were preserved. `candidate33-after-cue-audit03-provenance-review.json` records their Editor-only module scope and the requirement to review them before a new editor compile. The original mismatch remains failed; do not claim current-source identity equality or rebuild just to conceal the warning. Candidate33 runtime evidence remains tied to its frozen sources. Exact editor PID27528 exited, and the final owned-process inventory is empty.
