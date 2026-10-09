# Generic slide lifecycle acceptance

## Gate and scope

The ordinary input-triggered slide must end its GAS ability when Character Movement leaves slide
mode. Input release, low speed, jump, external interruption, cancellation, and avatar replacement
must remove the sliding tag and input task exactly once. A movement transition must retain its
destination mode. This gate does not change gravity, friction, speed, slope tuning, or hero defensive
abilities.

## Source findings

`UBwayGameplayAbility_Slide` remains active after Character Movement automatically leaves slide
mode. Its only normal end path was input release. `CanActivateAbility` also cached a movement
component during a query, permitting a later avatar's query to use an older component.

The repair binds the activation's character movement-mode delegate and removes it before ending
movement, so synchronous callbacks cannot recursively end the ability. Queries resolve the supplied
avatar without modifying execution state. GAS remains responsible for task/tag/input-cache cleanup.

Independent findings remain pending: `PhysSliding` integrates gravity with the timestep twice, and
the character drops its slide-shake handle without stopping a potentially looping replacement asset.
Those need separate physics/visual lifetime verification; this gate must not claim them repaired.

## Verification

`compile-editor06.json` and `automation03.json` pass the current editor build and lifecycle suite. The
authority suite uses the production humanoid slide grant and real PlayerState ASC/pawn ownership.
It covers release, low speed, jump, external movement modes, repeated cancellation, synchronous
restart from ability-end and cue callbacks, rejected avatar queries, and avatar teardown/replacement.
The latest suite adds held stationary behavior at zero threshold, with a separate temporary positive
threshold for automatic exit. It passes with zero test errors or warnings and process exit 0.

Candidate49 is the pre-repair artifact. Authority and four-world remote-owner verification now pass
the authored zero-threshold policy and the optional positive-threshold exit branch. The separate-process
probe and repair also pass `compile-game01.json`, Candidate50 packaging, and the four-process runtime
gate documented below. Physical keyboard/gamepad feel remains separate.

Expected assertions: one end event, zero remaining sliding tags/tasks, unchanged destination mode,
repeat activation without accumulation, and no observer from an old avatar affecting its replacement.

Evidence directory: `Saved/Logs/slide-lifecycle-20260930/`. Record compile, authority automation,
remote-owner topology/results, cleanup, retry reasons, and packaging coverage here after verification.

## Authored minimum speed and remote setup

The first remote attempt used the staging world without the required travel options and timed out
before setup. `runtime01.json` preserves that setup failure. The corrected Dorado attempt also
timed out before mutations (`runtime02.json`). Inspection found `MinSlideSpeed=0` on the humanoid
Blueprint and every live pawn, while the native default is 100 cm/s. Because the production exit
condition compares speed against that minimum, zero disables the low-speed exit.

An initial experiment changed the humanoid asset to the native 100 cm/s minimum. Cold load and all
16 pawn instances across four worlds read back 100, allowing the conditional low-speed exit to be
tested. That asset change was then reverted byte-for-byte after reference review: the supplied
`Supplemental_Examples/basecharacter_stateslide.lua` ties slide eligibility to held input and has
no automatic low-speed exit. Its version is unknown, but the native default alone is insufficient
reason to replace authored zero-threshold behavior. `asset-tuning-restored.json` proves the restored
asset matches its pre-edit backup. The current authored minimum remains zero; other tuning is unchanged.

`compile-editor04.json` passes added bounded setup diagnostics and a stronger remote fixture.
For each case the fixture requires one owner and one authority activation/end, actual sliding
mode during the production cue route, and settled clean state on every peer. It restores packet
settings, input, delegates, and its original authority transform.

`runtime03.json` passes the LowSpeed case with 100 ms packet lag on owner and authority. All four
peers settle without slide mode, ability, or tag. Moving cases stop at the spawn area's missing
flat runway; this is setup failure, not a moving-slide verdict. A traced Dorado lane at (0, 2000)
has eight flat floor samples across 1,400 cm. Any authority-only relocation for a retry must restore
the separate original transform in `runtime04-transform-backup.json` after fixture cleanup.

The editor03 log also retains two Pixel Streaming startup errors and an unsupported Python
HitResult property read. `HitResult.export_text()` supplies the supported trace inspection path.
Those errors are not hidden by the scoped lifecycle result; physical/UI acceptance remains open.

## Completed four-world lifecycle gate

`compile-editor05.json` passes the corrected fixture. The previous run left an inactive authority
spec's `InputPressed` bit set, which is normal for Lyra when movement ends the ability before input
release. The fixture no longer rejects that inactive server bit. Owner input checks, inactive
authority checks, true slide entry, exact end counts, and all-peer convergence remain required.

`runtime05.json` passes LowSpeed, Release, and Jump with the temporary 100 cm/s minimum and 100 ms
packet lag on the authority and remote owner. It records six true owner/server slide entries, three exact activation/end pairs per role,
twelve settled peer samples with no slide mode/ability/tag, and immediate owner Falling with
slide-jump state preserved. All four packet configurations and the fixture transform were restored.
The separately relocated pawn was then restored exactly from its serialized original transform.
PIE stopped, dirty maps/content were empty, the editor closed normally, and original closed-editor
PIE options were restored. Three Pixel Streaming errors remain in the complete editor04 log; the
scoped fixture passes without claiming physical/UI acceptance or suppressing those errors.

The first PIE start in editor04 exposed an independent late-arrival selection race before the
slide fixture ran. The successful run waited for all four PlayerStates on all staging peers before
locking. See [late-arrival acceptance](./LATE_ARRIVAL_SELECTION_ACCEPTANCE.md); this setup change
does not fix that production admission path.

## Authored configuration verified

[runtime06.json](<E:/Unreal Projects/ProjectB/Saved/Logs/slide-lifecycle-20260930/runtime06.json>) passes
AuthoredLowSpeed, LowSpeed, Release, and Jump in four-world listen-server PIE, with 100 ms packet lag
on the authority and remote owner. The authored minimum remains zero. Owner and authority sustain
an active stationary slide while input is held for at least 0.5 seconds, then end cleanly on actual
input release. Only the optional LowSpeed case temporarily sets each peer's minimum to 100 cm/s.
Release and Jump use the original thresholds.

The run records eight actual owner/server slide entries and 16 settled peer samples, with exactly
one activation and end per case on each role. The first three cases require Walking on all peers;
Jump preserves its immediate Falling assertion. All four thresholds and packet configurations are
restored. The fixture reports zero location, rotation, and scale restoration error; the separate
relocation is also restored exactly. PIE stops, dirty maps/content are empty, the editor exits,
and the original closed-editor settings are restored.

Two Pixel Streaming startup errors remain in `editor05.log`, with zero critical lines. This scoped
pass does not claim physical/UI, trajectory, packaged, or second-PC acceptance. It supersedes
`runtime05.json` for authored-configuration evidence; runtime05 remains the temporary-100 experiment.

Overhead so far: six LyraEditor build attempts (one failed; five passed), three authority automation
runs, five editor launches, eight PIE starts including rejected setup, and six remote fixture
invocations. The first LyraGame build (`compile-game01.json`) passes with empty `failureSignals`; no new package pass exists for this gate.
Each retry's changed condition remains in runtime01 through runtime06.

The repeated setup is saved in `Saved/slide_lifecycle_setup.py`: prepare, staging readiness,
Dorado readiness, authority-only relocation, exact transform restoration, and settings cleanup.
It returns serialized values and retains no Unreal wrappers in module globals. Import the module
through `sys.path`; this bridge misparses multiline console text containing a literal script filename.

## Candidate50 packaged verification

`compile-game01.json` and the Candidate50 ACL preflight, cook, stage, and pak pass. The frozen
artifact contains 131 hashed files. Its source snapshot includes the complete tracked patch;
`codex-packaged-LAN-20260930-50/untracked-source-sha256.csv` and `untracked-source/` also preserve
the 141 untracked source inputs. The snapshot comparison records an encoding-only BOM difference
from the resume snapshot, with identical source text and tracked patch.

`packaged-LAN-20260930-50-slide01-result.json` passes all four cases in four independent processes
on one PC with 100 ms fixture-owned packet lag. The host records AuthoredLowSpeed, LowSpeed,
Release, and Jump case passes. Each peer restores its authored zero threshold, packet settings,
and callbacks. Success requires four cleanup acknowledgments, four final acknowledgments, and
the verified authority transform restoration before the host emits `COMPLETE passed=1`.
All three clients then exit normally with code 0. The runner terminates the intentionally retained
headless host during owned cleanup; that termination is not a normal host-quit claim.

`packaged-slide01-log-audit.json` and `packaged-slide01-flow-brief.json` retain all four log counts:
zero error and critical lines. The runner's separate review includes ten synthetic controls that
reject missing final barriers, incorrect role markers, and late failures. This gate used one
LyraGame build, one package, and one packaged runtime attempt in addition to the PIE work above.

Structured authority, authored-policy PIE, and packaged lifecycle verification are complete for
this repair. No production slide tuning changed. Physical keyboard/gamepad behavior, perceived
slide feel, trajectory parity, rendered effects/camera, and second-PC LAN remain unverified.
Next: repair the independently observed late-arrival selection/Prematch hole, then return to
physical and presentation acceptance. Changes to slide ability/movement/probe behavior invalidate
the corresponding evidence; subsequent selection changes require their own fresh package.

## Gravity and camera-shake follow-up

The October 1 gate is `Saved/Logs/slide-physics-shake-20261001/gate-contract.json`.
Candidate53 remains a frozen selection-recovery reference and cannot prove these new source changes.
The expected result is one timestep integration of gravity and cleanup of only the slide-owned shake,
including release, owner changes, death, and teardown. Authored thresholds and tuning remain unchanged.

The physics repair removes the extra timestep from gravity acceleration. New tests call production
movement dispatch on actual flat and ramp collision at 30, 60, and 120 Hz. Fixture-only settings
isolate gravity; zero-gravity controls, floor identity, nonpenetration, and floor loss remain asserted.
These checks do not establish authored-friction trajectory parity or network prediction.

Pre-build review caught a fixture error: Unreal's `VInterpTo` returns its target at zero interpolation
speed. The fixture now uses a tiny positive wall-friction value with a bounded velocity effect below
0.003 cm/s under standard gravity. The existing gravity and trajectory tolerances were preserved.

Camera review also identified expired-pool object reuse: a weak reference to a finished shake can
refer to a later same-class effect. The repair must preserve exact ownership across that boundary,
finite-duration behavior through garbage collection, and normal authored blend-out. Same-class reuse
and unrelated-shake controls are required before acceptance.

`compile-editor01.json` passes the production repair and new tests. The gravity tests report matching
vertical velocity of approximately -490 cm/s after 0.5 seconds at all three rates. Actual downhill
distances are 65.333, 63.292, and 62.271 cm, converging toward the continuous 61.25 cm expectation.
Flat contact, floor loss, zero-gravity controls, existing slide lifecycle, and jump velocity also pass.
These five per-test results remain in `automation01.json`; that combined run failed camera setup.

Two camera-fixture issues were corrected without changing production assertions: the controller
needed its real local-player flag initialized, and the world needed initialized actors for dynamic
death-delegate dispatch. `automation01.json` and `automation02-camera.json` preserve both failures.
`compile-editor03.json` and `automation03-camera.json` now pass all six camera cases with zero test
errors or warnings, framework exit zero, and process exit zero. The cases include finite duration
through garbage collection, same-class object reuse, authored single-instance behavior, release and
blend-out, owner/class/manager changes, death, and teardown. The owned shake is transient and excluded
from the engine's expired pool; original manager/controller references remain weak.

`runtime01.json` passes all four existing lifecycle cases in four-world listen-server PIE, with
100 ms lag on authority and remote owner. Every case records clean owner/authority activation and
ending, all-peer convergence, and complete restoration. The separately relocated pawn was restored
exactly; no content or maps were dirty. The editor closed, no owned processes remained, and the
three PIE settings were verified on disk. Two Pixel Streaming target-window errors are retained;
there are no other errors or critical lines. This is structured lifecycle evidence, not a visual pass.

`runtime-slide-config01.json` records equal authored movement parameters on all four local pawns,
including minimum speed zero. All four `SlideCameraShakeClass` values are null. Camera automation
therefore proves replacement-instance safety; it does not prove an authored runtime shake is active.
The humanoid asset hash remains unchanged.

Read-only collision traces found Dorado's existing 26-degree ramps. The finer profile records a
plateau, a short ramp, another plateau, a second roughly 25-degree ramp, and the arena floor. These
are leads for a sustained terrain-transition fixture, not proof of capsule clearance or prediction.
The first trace helper incorrectly expected a tuple; this engine returns `HitResult` or `None`.
Its handled exception retained no world wrappers; the corrected scan completed in the same session.

Current overhead is three editor builds, three automation runs, one editor launch, and one successful
remote lifecycle invocation. No new package has been made. Sustained authored-tuning trajectories,
packaged behavior, and physical/rendered feel remain pending. The bounded plan and terrain evidence
are in `trajectory-proof-plan.json`, `dorado-terrain-audit01.json`, and `dorado-ramp-profile01.json`.
An optional trajectory mode is being added to the existing packaged coordinator; preserve the prior
lifecycle cases. Finish its independent review and editor/runtime checks before freezing a new package.

## October 5 resume

The prior trajectory authoring stopped at a usage limit before review, compilation, or runtime
verification. The saved native probe, runner, and analyzer remain unverified. Resume inspection
found no surviving agent, editor, game, or build process. Finish the source and parser reviews
before compiling; Candidate53 remains a frozen pre-repair artifact.

The refreshed Computer Use plugin initializes through `node_repl` and `@oai/sky`, and returns
targetable native windows. This supersedes the earlier unavailable-tool assumption. No Unreal
window or physical gameplay input had been tested at initialization. The capability
record is `Saved/Logs/slide-physics-shake-20261001/ui-channel-20261005.json`.

The resumed trajectory fixture compiled successfully in `compile-editor04.json` (35.74 seconds,
empty failure signatures, linked DLL advanced). The first four-world PIE attempt used requested
30 Hz and 100 ms simulated latency. It failed geometry setup before any movement case executed.
The original flat route left the arena; 27 of 57 visibility samples missed the floor. A centered
1,400 cm route has 57 flat hits, with native capsule verification still pending. The corrected
fixture preserves authored slide support policy and reports exact collision-query failures.

That run also exposed a production base-speed replication defect: player257's rating was10.5 on
all four peers, but `MaxWalkSpeed` was630 on authority and600 on each client. The movement fix
observes rating changes and seeds the current rating during ability-system attachment. Three
tests cover binding lifetime, actual rating RepNotify, fallback values, and multiplier composition.
Independent source review passed; this newer batch has not yet compiled or run. See
`source-review-hero-base-speed.json` and `hero-base-speed-before01.json` in the gate folder.

The revised analyzer passes64 synthetic controls; the unchanged runner retains14 controls.
These checks validate evidence handling, not gameplay. Further review found that ramp run-up
can finish too early, before sliding can reach the first slope. Resolve activation timing through
observed position/contact while preserving real input, map geometry, and authored tuning.

All four peers acknowledged cleanup and final restoration. PIE stopped, the editor exited,
and closed-editor settings were restored. No maps or content remained dirty. The full editor log
retains two Pixel Streaming errors and the fixture setup error, with no critical lines. Native
Computer Use captured the editor and one client window; physical input acceptance remains open.
The detailed result is `trajectory-runtime01.json`, with cleanup in `editor02-cleanup.json`.
Current gate overhead is four editor builds, three automation runs, two editor lifecycles, two
runtime attempts, and no new package. Candidate53 remains frozen before these repairs.

### Verified hero base-speed repair

`compile-editor05.json` passed, and `automation04-hero-speed.json` passed all three tests with
zero errors or warnings and normal framework/process exits. The first fresh selection used only
rating10 heroes, so its matching600 values did not reproduce the distinct-speed case. A proposed
temporary stat-application probe stopped at preflight without mutation; normal passive effects and
owner-only combat attributes made that broader probe unsuitable.

The next session selected Rawlins for remote player257 through the production selection path
before locking and traveling. `hero-base-speed-runtime02.json` verifies rating10.5 and630cm/s on
authority, owner, and both simulated proxies. This closes the live PIE replication defect observed
as630/600 before the fix. It does not replace fresh packaged transport or physical selection proof.

### Sustained trajectory diagnosis

Trajectory02 completed two native flat cases but failed the ramp support assertion. Its actual
42–44Hz samples also failed the requested30Hz band: PIE travel had reset `t.MaxFPS` to0. Apply
and read the cap after Dorado readiness. No frame-rate tolerance was widened.

`compile-editor06.json` adds only rejected-sample diagnostics. Trajectory03 used actual30Hz and
passed the checked flat release/jump thresholds, including timestamp matching, contact, terminal
state, and bounded corrections. Ramp preflight correctly rejected another player's capsule in
the lane. The exact blocker is logged before `GeometryRejected`; this is a setup failure.

After temporarily relocating that nonparticipant on authority and verifying peer convergence,
trajectory04 reached the ramp. `MovementRejected` records18.12775cm floor distance, a valid25-degree
blocking ramp hit, no overlap or penetration, and active custom sliding. The existing15cm criterion
still fails. This does not alone prove a production defect: the supplied Lua reference includes an
off-ground slide branch, and its exact final-alpha version remains unverified. The next diagnostic
must retain failure while observing whether contact recovers or sliding ends naturally.

Both attempts restored all four fixture peers. The separately moved pawn's exact original transform
was restored. Editor26172 exited normally, no maps/content remained dirty, and the closed-editor
settings and frame cap were restored. See `editor04-cleanup.json`. Current gate overhead is six
editor builds, four automation runs, four editor lifecycles, five fixture attempts, and no new package.

### Bounded support diagnostic

`compile-editor07.json` passed the opt-in diagnostic. `support-diagnostic01-analysis.json`
records a failed, explicitly nonacceptance run at actual 30 Hz. Owner and authority peaked at
33.321396 cm floor distance, then recovered custom support after 0.199997 movement seconds.
They remained in custom sliding with the ability and tag active. There was no penetration or
capsule overlap. All 29 matched post-rejection timestamps agreed on position, velocity, and floor
distance; no corrections occurred inside the measured post-rejection windows.

The recovered slide then lost horizontal speed unusually quickly. Retained downward velocity
made the flat floor satisfy the frontal-wall dot-product condition. Applying the authored wall
friction reproduces the observed velocity drop to within 0.001 cm/s. A production-dispatch
regression must confirm this before a contact-response repair. Keep the authored tuning and
existing acceptance thresholds unchanged while distinguishing this defect from brief air gaps.

The owner diagnostic ended after 1.032151 wall seconds, one frame beyond the one-second target.
All four peers released input, removed callbacks, and restored packet settings. Only three peers
acknowledged the final transform: the owner had yaw 0 while the authority target was yaw 90.
The fixture never recorded the owner's original orientation, so this is incomplete cleanup proof,
not proof of a gameplay rotation defect. Phase 9 timed out; no final COMPLETE or acceptance pass
was emitted. Preserve that failure when fixing per-peer orientation capture and restoration.

The separately relocated pawn was restored exactly. PIE stopped, editor 32068 exited normally,
no maps or content remained dirty, and all four closed-editor settings plus the frame cap were
restored. See `editor05-cleanup.json`. Gate overhead is seven editor builds, four automation runs,
five editor lifecycles, six fixture attempts, and no package. The verifier's 103 synthetic controls
also pass independent review; these prove evidence handling, not gameplay.

### Reproduced contact-response defect

`compile-editor08.json` passes the authored-humanoid contact regression and the fixture's per-peer
orientation repair. Production movement remained unchanged for `automation05-contact-red.json`.
The flat-floor case fails both tangential-speed and inward-velocity assertions at 30, 60, and 120 Hz.
After 0.2 seconds, authored floor friction predicts 262.727, 270.915, and 274.810 cm/s forward speed.
Actual speeds are 0.411, 3.312, and 5.115 cm/s, with negative vertical velocity retained into the floor.
The genuine opposing-wall control passes at all three rates. The automation failure is expected
regression evidence, not a setup pass or an accepted gameplay result. See `contact-regression-red01.json`.

The production repair must reconcile velocity with actual collision response, including secondary
contacts, preserve airborne gravity and authored tuning, and keep genuine wall response covered.
Earlier gravity tests require stored downward velocity to accumulate while grounded; revise those
assertions to physically meaningful contact/tangent motion while retaining explicit gravity controls.
The per-peer orientation repair compiles but still needs a new four-peer runtime cleanup result.

### Contact repair verification and remaining cleanup ordering

`compile-editor09.json` passes the reviewed production contact repair. The component now derives
velocity from the complete collision-resolved displacement. A stack-scoped impact context captures
intermediate frontal walls, then applies authored wall damping after reconciliation. Actual floor
contacts no longer receive wall braking. No authored tuning or pre-contact gravity integration changed.
Exact before/after sources and independent review are recorded in `contact-response-freeze09/` and
`source-review-contact-response09-independent.json`.

`automation06-slide-contact.json` passes all 16 slide tests with zero errors or warnings and both
framework and process exit 0. Coverage includes real floor contact, primary and secondary glancing
walls with collision-identical damping controls, a corner, grounded ramp kinematics, off-contact
gravity, floor loss, lifecycle, slide-jump velocity, and camera-shake ownership. The original 0.1
cross-rate velocity tolerance remains unchanged.

`support-diagnostic02-analysis.json` records actual 30 Hz on four peers. The brief ramp gap still
peaks at 33.321396 cm and recovers after 0.199997 movement seconds. The owner travels 109.02635 cm
farther than diagnostic01 and retains forward motion throughout the observation window. All 28
matched owner/server timestamps agree, without corrections inside the measured window. This is
bounded diagnostic evidence; the existing 15 cm contact assertion still rejects acceptance.

Cleanup again receives four local-cleanup acknowledgments but only three final-restoration
acknowledgments. This time the original owner actor and controller yaw were both recorded as 90.
The actor later has yaw 0 while its controller remains at 90. After the fixture times out, one
manual restoration to the recorded original yaw remains stable. Engine saved-move combining and
position correction can restore earlier actor rotation; the fixture must wait for spatial/motion
convergence before its one-time orientation restoration. Do not disable prediction or discard saved
moves to pass cleanup. The failed run and missing final COMPLETE remain part of the record.

The separately moved pawn was restored exactly; PIE and editor 32060 stopped normally, with no dirty
content or maps. Frame cap and closed-editor INI keys were restored (`editor06-cleanup.json`). Current
gate overhead is nine editor builds, six automation runs, six editor lifecycles, seven fixture
attempts, and no package. The next build is fixture-only; the passing production tests remain valid
unless their source changes.

### Verified diagnostic cleanup

`compile-editor10.json` passes the fixture-only ordering repair in 9.34 seconds, with no failure
signatures and an advanced runtime DLL. Production movement and its passing automation tests are
unchanged. The fixture waits for stable position/motion before restoring orientation once, then
requires the existing independent orientation readback interval. It preserves normal movement
delivery and does not disable correction or discard saved moves.

`support-diagnostic03-analysis.json` proves all four original actor/controller orientations and
motion states, four acknowledgments at each cleanup/restoration/final barrier, and the native final
record with `cleanupAcks=4`, `finalAcks=4`, and `authorityTransformRestored=1`. The native verdict
remains `passed=0`: successful cleanup does not convert this support diagnostic into acceptance.
Raw evidence is preserved in `support-diagnostic03.log` and its byte-slice record.

The run used four Dorado PIE worlds at the requested 30 Hz, with player257 selected through the
production Rawlins path. All four peers still reported 630 cm/s. No extra pawn needed relocation.
PIE stopped, editor5632 exited normally, and frame cap plus four closed-editor settings were
restored. No content or maps were dirty (`editor07-cleanup.json`). Gate overhead is ten editor
builds, six automation runs, seven editor lifecycles, eight fixture attempts, and no package.

The next decision separates actual ground contact from a nearby floor query and an airborne slide.
The supplied Lua reference has distinct ground and air branches; its final-alpha provenance and
the missing `Jump_Physics` implementation remain unresolved. Keep the existing support band and
record air intervals truthfully. Review airborne friction, slope acceleration, and floor-query-loss
behavior before changing production code or claiming sustained two-ramp acceptance.

`Saved/analyze-slide-support-diagnostic.py` now extracts these repeated diagnostics using pinned
verifier05 primitives. It preserves existing output files and always reports `acceptancePassed=false`.
Its 69 baseline metric comparisons and 13 author controls pass; independent review replayed 12
data-corruption controls and inspected exclusive output creation. The review is
`source-review-support-diagnostic-helper01-independent.json`. Diagnostic03 has 28 matched movement
timestamps within the unchanged 10 cm / 60 cm/s bounds, four measured 30 Hz bands, and no corrections
inside the post-rejection window. This comparison does not establish full route or reference parity.

A bounded local and primary-reference search did not recover `Jump_Physics` or state-selection
priority. The September 2017 trailer lead was not accessible for frame/input verification. Preserve
floor-query-loss cancellation and current production air forces until stronger evidence justifies
a change. The next safe step is contact/plane and movement-step telemetry, with exact capsule
preflight geometry; proposed ground/air criteria remain distinct from approved acceptance.
The source-backed decision and missing evidence are recorded in `ground-air-trajectory-review01.json`.

### Corrected contact telemetry and fresh capture

Compile11 added diagnostic-only capsule, floor-plane, movement-callback, and preflight geometry
telemetry. Diagnostic04 is invalid: the new response loop used `ECC_MAX` (65) to index a 64-entry
collision-response array. The initial source review missed that enum/storage distinction. Preserve
the failed capture and extraction result; neither can establish gameplay or telemetry correctness.

The correction uses `UE_ARRAY_COUNT` on the actual response array, matching engine practice.
Independent review passed, and compile12 passed in 12.27 seconds with no failure signals, an advanced
runtime DLL, and the frozen source hash. The contact analyzer now requires exactly 64 responses;
all 59 independent controls pass. The pinned support analyzer and trajectory verifier are unchanged.
This prevents accepting a truncated array or the deprecated pseudo-channel as a real response.

`support-diagnostic05.log` is a fresh, immutable 6,733,152-byte capture from the corrected binary.
`support-diagnostic05-analysis.json` successfully extracts the new telemetry while keeping
`acceptancePassed=false`. All observed preflight batches are complete. It includes 749 contact
samples, 545 classified as primary and 58 as replay. The four measured frame-rate bands pass the
requested 30 Hz check; the post-rejection window has no correction rows. All four peers pass the
cleanup, restoration, and final barriers. The native final verdict remains `passed=0`.

Production selection again verified Rawlins at 630 cm/s on all four peers. No nonparticipant needed
relocation. PIE stopped, editor15764 exited normally, no content or maps were dirty, and the frame
cap plus all four closed-editor settings were restored. See `editor09-cleanup.json`.

Contact-plane clearance and reconstructed chord sweeps describe geometry; they do not record the
actual production impact history or prove contact with finite mesh edges. Use stable-plane callback
intervals to test force hypotheses without converting diagnostic observations into ramp acceptance.
The source and parser reviews are `source-review-contact-channel-bound12-independent.json` and
`source-review-contact-diagnostic-helper03-independent.json`. Gate overhead is 12 editor builds,
six automation runs, nine editor lifecycles, ten fixture attempts, and no new package. Compile12 and
diagnostic05 specifically replace invalid telemetry from diagnostic04; production tests were not
rerun because production source was unchanged.

`support-diagnostic05-interpretation.json` identifies six stable sweep-floor callbacks matching the
loaded production force model within 0.000223 cm/s. Two separated flat-plane steps match within
0.000069 cm/s: ground friction reduces downward speed despite gravity. The interpretation excludes
six owner line-trace rows because Unreal can combine a line normal with a retained sweep impact
point; that hybrid does not define a coherent contact plane. It also records 29 matching owner/server
timestamp pairs with zero position or velocity difference in the measured window. Full-route contact
and final-alpha air behavior remain unproved.

The next regression separates nearby query geometry from physical support. The reviewed correction
would gate only slope acceleration and ground friction using a valid, nonpenetrating floor result
within Unreal's existing `MAX_FLOOR_DIST` contact tolerance. It must retain authored ramp support,
gravity, steering, speed limits, held slide state, collision response, and query-loss behavior.
See `ground-force-contact-design13.json`. This is a bounded correction to ground-force leakage, not
a reconstruction of the missing `Jump_Physics` implementation.

The analyzer now exposes floor-query distance provenance and plane coherence. It retains raw hybrid
line/sweep values but excludes those planes from derived separation bands. All 62 controls pass in
an independent rerun, and `source-review-contact-diagnostic-helper04-independent.json` records the
engine-backed review. `support-diagnostic05-analysis-v2.json` reuses the same immutable capture;
extraction and cleanup still pass, while gameplay acceptance remains false. No native rebuild or
runtime rerun was required for this evidence correction.

### Distant-floor ground-force regression

Compile13 passes four new `Breakaway.CombatPolish.Slide.Physics.AirContact` tests, each covering
30, 60, and 120 Hz. Against unchanged production, `automation07-air-contact-red.json` fails three
tests with exactly 27 intended behavioral assertions. The grounded 25-degree ramp control passes.
Capsule-clear separated steps above real flat and sloped surfaces incorrectly receive ground forces;
the setup, real sweeps, natural recontact, and grounded-ramp checks succeed. This is an expected red
regression, not gameplay acceptance. See `air-contact-regression-red01.json`.

The production correction now gates only slope acceleration and ground friction on a valid,
nonpenetrating, finite floor result within `MAX_FLOOR_DIST`. It retains engine-tolerated negative
distances and does not require walking-angle acceptance, so authored slide ramps remain eligible.
Gravity, steering, speed limits, held slide state, impact response, and query-loss transitions are
unchanged. The frozen change is `ground-force-contact-freeze14/`; independent review, compile14,
the full Slide suite, and fresh four-peer runtime evidence are pending.

Independent source review passed with no findings (`source-review-ground-force-contact14-independent.json`).
Compile14 passes the frozen production correction with an advanced runtime DLL and no failure
signals. `automation08-slide-air-contact.json` passes all 20 Slide tests with zero errors or warnings,
framework exit 0, and process exit 0. This includes all four new AirContact tests and every prior
grounded, wall, corner, gravity, floor-loss, lifecycle, jump, and camera-shake test. Fresh Dorado
multiplayer evidence is the next check; the older package still predates the correction.

### Early flat-slide failure under severe editor slowdown

Diagnostic06 used compile14 and the unchanged native observer. It failed flat case0 before reaching
the ramps: the remote owner entered slide once, then correction/replay callbacks showed Walking
with the ability and tag inactive. The authority also reported `trajectory-held-slide-ended-early`.
No movement sample recorded active custom sliding, so this capture cannot evaluate the repaired
ground-force integration. The held-slide failure remains unresolved replication evidence.

Run-up was already near 3 Hz before activation: median 2.92 Hz on authority and 2.95 Hz on owner,
despite the post-travel `t.MaxFPS=30` readback. A 44.128 cm server correction predates owner entry;
the run-up-to-failure correction maximum is 147.693 cm. The larger whole-fixture maximum includes
deliberate setup relocation and is not a gameplay-correction metric. No Hold, Terminal, or release
phase occurs before failure. See `support-diagnostic06-early-end-analysis-v2.json`.

Source review identifies a plausible route: applying the server's Walking correction invokes the
slide ability's movement-mode callback, which ends the active ability. Saved moves currently restore
slide-jump state, not held-slide intent. The capture lacks reason-specific traces to exclude a
coincident release or activation rejection. Do not change correction handling or reassert slide
without distinguishing those causes. `support-diagnostic06-source-review.json` records the source
references and the unresolved cause.

Unreal's editor can cap background or all-hidden windows at 3 Hz; `t.MaxFPS` does not override that
branch. An unattended structured run bypasses this editor throttle. The next attempt must measure
actual frame cadence before dispatch and capture bounded movement/GAS verbosity at the same 100 ms
latency. Reuse compile14 after verifying source and DLL identity. The existing launch helper exposes
`-Unattended` through `-SkipTurnkeyValidation`; compile14's UBT log lines56-57 and91 separately prove
the installed, buildable Win64 SDK/toolchain. This launch variation proves no physical UI or packaging
behavior. Preserve diagnostic06 even if the next correctly timed attempt succeeds.

The immutable capture is `support-diagnostic06.log` (1,239,666 bytes). The ordinary support wrapper
correctly rejected extraction because no ramp rejection exists; its rules were not relaxed. The
separate early-failure analysis uses pinned verifier primitives and proves all four original
transforms/controller orientations plus cleanup, restoration, and final acknowledgments. Native
completion remains `passed=0`, `cases=0`. The relocated nonparticipant was restored exactly,
editor43124 exited normally, and all frame/PIE settings were restored with no dirty content or maps.

Gate totals are 14 editor builds, eight automation runs, ten editor lifecycles, 11 fixture attempts,
and no new package. `ground-air-next-gate-proposal02.json` contains proposed ground/air coverage
criteria only; none have been adopted as acceptance. Final two-ramp, packaged, physical, and
reference-parity proofs remain open.
