# GameplayCue registration and production delivery

This gate preserves the final-public-alpha gameplay, placeholder-art integration and four-player
LAN objective. Registration checks alone do not prove visible or audible presentation.

## Expected result

The goal, hit, slide and Alona fire cue tags exist before startup cue discovery. Each tag resolves
to exactly one intended cue, with the stock pistol mapping preserved. Production actions route
the intended cue to relevant peers; predicted owner feedback must not repeat on server echo.
Local camera and haptic feedback must stay with the correct player. Perceived presentation,
physical input, second-PC LAN and the final integrated match loop remain separate requirements.

## Candidate46 production route result

The authored production hit-confirm cue is `GameplayCue.Breakaway.Combat.HitConfirm` at
`/BreakawayCore/GameplayCues/GCN_BW_CombatFeedback_HitConfirm`. It is now declared in the
primary game-module cue dictionary and included in the registration test alongside the existing
project hit template, slide, goal, Alona fire and stock pistol entries. The slide ability emits
the registered slide cue on activation, and the authoritative relic emits the goal cue after a
validated goal transition.

Candidate `LAN-20260929-46` compiled `LyraEditor` and `LyraGame`, passed Cook/Stage/Pak, and
completed the four-process Connected browser fixture with the existing remote damage/pass/goal
fixture. The production observer recorded exactly one `HitConfirm` route and one `Scoring.Goal`
route on each of the host and three clients, with all mappings resident and zero fixture errors.
The route evidence is `Saved/Logs/cue-route-20260930-02/production-cue-route-result.json` and
the raw peer logs in that directory. The slide route remains unobserved until a physical or
dedicated slide activation is available; NullRHI transport evidence does not prove perceived
VFX/audio, camera, haptics or final visual quality.

## Candidate47 slide activation result

Candidate `LAN-20260929-47` adds the `Movement.Slide` cue dispatch to the production Argus and
Rawlins slide implementations, then compiles both `LyraEditor` and `LyraGame` before a fresh
Cook/Stage/Pak. The existing four-process all-kit fixture completed all 16 ability families and
32 natural cooldown cycles, including two ArgusSlide and two RawlinsSlideShot activations, with
512 peer rows and zero cleanup failures. With `bway.Test.CueAudit` enabled, the Argus activation
routed `GameplayCue.Breakaway.Movement.Slide` once on the host and each client; every observed
route had `ready=1` and `instigatorMatches=1`. Evidence is in
`Saved/Logs/all-kit-cooldown-cue-20260929-47/runtime-brief.json` and
`Saved/Logs/all-kit-cooldown-cue-20260929-47/production-slide-route-result.json`.

Candidate `LAN-20260930-49` reran the same four-process all-kit fixture with the development
observer extended to 900 seconds. Both real RawlinsSlideShot cooldown cycles routed
`GameplayCue.Breakaway.Movement.Slide` on the host and all three clients: eight route rows,
`ready=1`, `instigatorMatches=1`, and the expected `Executed` event. The complete fixture still
passed all 16 families, 32 cycles, 512 peer rows, 64 HUD samples, and clean teardown. Evidence is
in `Saved/Logs/rawlins-cue-audit-20260930/rawlins-slide-route-result.json` and
`Saved/Logs/rawlins-cue-audit-20260930/runtime-brief.json`.

This proves production routing and cue residency, not perceived VFX/audio, camera shake,
haptics, or physical input.

## Diagnosis and repair

Evidence is in `Saved/Logs/buildable-authority-20260927/`.

- `cue-registration02.json` records three project-root cues whose tags were declared only in
  the later BreakawayCore game-feature configuration. Startup skipped those cue registrations.
- Both the Alona burst and the stock pistol burst used `GameplayCue.Weapon.Pistol.Fire`, so
  discovery skipped a duplicate mapping.
- `cue-alona-graph03.json` proves the Alona ability Blueprint inherits the native primary and
  contains only unconnected events. The base/native primary did not dispatch its fire cue.
- `cue-alona-properties03.json` records the existing Alona placeholder audio, camera shake and
  haptics. Its EventGraph is empty, but the live October 9 inspection found an `OnBurst`
  function with shooter audio and weapon-dependent calls. An empty EventGraph does not prove
  that a cue has no executable Blueprint functions. These effects are preserved.

`BwayGameplayCueTags` now registers the three existing semantic tags and the dedicated
`GameplayCue.Breakaway.Weapon.AlonaPrimary.Fire` tag from the primary game module. The three
late configuration definitions were removed. Their serialized tag names do not change.
Only the Alona burst changes its tag; there is no global redirect of the stock pistol tag.

Alona emits the cue with GAS's current prediction key. The local owner predicts the launch
feedback; authority publishes it only after creating the actual projectile. Both paths use the
same configured launch position. The authoritative projectile/damage path is unchanged.
The native burst implementation targets local camera/haptics when its authored world-play
flags are false; perceived output and accessibility behavior still require runtime inspection.

## Compile and asset evidence

Editor37 failed on three concurrently edited editor-bridge JSON key conversions. Those keys use
the engine's shared-string storage type; direct `FString` assignment/JSON string construction
does not compile. Restoring explicit `FString(*Key)` conversion fixes the errors. Editor38 passes,
and the LyraGame, BreakawayCore and HeroAlona runtime DLLs advanced during that build family.
The original failure and successful retry remain in `compile-editor37.json`/`compile-editor38.json`.

`cue-retag04-run.json` records a failed attempt to write Python's read-only `GameplayTag.TagName`.
It changed no asset. Engine `PyWrapperStruct.cpp` confirms the supported `import_text` API;
`cue-retag05-run.json` succeeds through that API and saves only the reviewed Alona cue. Its six
other inspected cue/Niagara assets have unchanged hashes. Loading Niagara dependencies dirtied
two packages in memory; neither was saved. Normalized before/after burst properties are equal.

`cue-registration05-cold.json` passes the fresh-process mapping test: all five dictionary entries
are valid before asset loading, their serialized cue tags match, and the inspected mappings are
unique. The test has no failures/warnings and both framework and process exit0. The cold log has
no missing-tag or duplicate-cue registration warning. Game33 compiled successfully.

## Packaged verification in progress

Candidate `LAN-20260927-34` will use a fresh full cook. `bway.Test.CueAudit` is a bounded development
observer of the production cue registry and the engine's actual routing delegate. It does not
dispatch synthetic cues or force cue classes to load. The existing four-player primary combat
fixture provides two real Alona owner activations against friendly/enemy structures under 200 ms
packet lag. Its cleanup removes each local cue listener before clients report release.

`verify-cue-primary.py` retains the existing combat assertions and adds exact mapping, loading,
cue identity, owner/simulated-peer, prediction-order, duplicate-route and cleanup checks, with
corruption controls. Registration of goal/hit/slide does not yet prove those production actions
dispatch their cues. Continue their wiring/runtime audit after this Alona verification.

Invalidate affected evidence after changing cue tags, registry paths, cue assets, ability
prediction/activation/feedback, avatar ownership, gameplay-cue suppression or relevant transport.
Retain exact candidate/source manifests; do not promote headless evidence to visual/audio proof.

## Candidate34 result and readiness-observer correction

The four-process run completed normally for all three clients and passed its combat assertions.
Each peer received exactly two real Alona fire routes; the remote owner preceded authority by
about 0.2 seconds and did not repeat on the server echo. The initial verifier nevertheless failed
four residency assertions (`packaged-cues01-alona-cue-verification.json`); preserve that result.

Engine `GameplayCueManager.cpp::OnGameplayCueNotifyAsyncLoadComplete` retains preloaded classes
without populating each cue-set entry. `GameplayCueSet.cpp::HandleGameplayCueNotify_Internal`
resolves that entry lazily immediately before execution. An empty `LoadedGameplayCueClass` alone
therefore does not establish an unloaded class. The corrected development observer also checks
`ResolveObject` (which does not load) and pending cue loads, both at Playing and at each actual
route. Candidate35 must pass those stronger observations; candidate34 is not retroactively passed.

Read-only `cue-properties06.json`/`cue-properties06-run.json` prove the current combat config asks
for `GameplayCue.Breakaway.Combat.HitConfirm`, while the authored hit cue uses `.Combat.Hit`.
All four effect slots (sound, camera shake, haptics, Niagara) in each hit/slide/goal cue are empty.
No asset was saved, and eight inspected asset hashes stayed unchanged. These are inspection leads, not yet proven
integration defects: a separate /BreakawayCore/GameplayCues/GCN_BW_CombatFeedback_HitConfirm asset exists and may be the intended mapping. Inspect its tag, authored effects and Blueprint dispatch before changing config. Empty native slots also do not exclude Blueprint effect logic.


## Candidate35: packaged Alona scope passed

Game34 compiled successfully with empty failure signals. Candidate35 completed a fresh
Cook/Stage/Pak with exit0; all131 staged-file hashes match and no build inputs changed during
its freeze. Its executable SHA256 is
`028D823F7C9CC2C3463079E45E1CB636962ADFE0B08230DFCA24159F98A9F0F0`.

`packaged-cues02-alona-cue-verification.json` passes244 assertions and30 corruption controls.
Each of four peers has five unique resident mappings with no pending cue loads before use.
The two actual remote Alona primary activations route exactly once on every peer, ready on the
first shot; the owner precedes authority under200ms latency and receives no duplicate echo.
The original combat fixture still checks all four heroes against friendly/enemy structures.
Three clients exit normally; the host's owned teardown is separate. Runtime errors are zero.

The initial extraction attempt had a Python int-conversion error from an overly broad text
replacement in the verifier. Fixing that expression allowed extraction from the same immutable
logs; no game rerun occurred. Candidate34's failed cache-only result is retained. The ledger's
UTF-8 write failure was recovered from candidate34's immutable patch and checked against its
file-hash manifest before applying current state. No gameplay/asset state was lost.

This scope proves registration, residency and production routing/prediction, not perceived
sound, camera motion, haptics or particles. The authored Alona burst still has no concrete
particle system. Goal/hit/slide dispatch and local feedback ownership need their own inspection.

## Authored feedback inspection and local camera repair

Read-only editor02 (PID37100, closed normally) inspected the actual feature hit cue and root
hit/slide/goal templates. `cue-graphs07.json`, `cue-functions07.json` and `cue-properties07.json`
prove the feature HitConfirm cue has the correct config tag, no Blueprint override, and authored
melee-impact sound, weapon-fire camera shake and Niagara RadialBurst. Haptics are unassigned.
The three root cues have empty native effects and no Blueprint execution/functions. They are
separate templates, so their `.Combat.Hit` tag does not prove the live `.HitConfirm` config is wrong.
No tag/config/asset edit was made on that mistaken lead. The editor reported no dirty packages;
its startup Pixel Streaming target-window error remains an editor/UI limitation, not a passed
rendered gate. Goal Blueprint inspection is in `goal-graphs07.json`: only disconnected events
and the default construction graph. Its parent draws a server-side debug sphere; relic state
replication separately owns scoring VFX and must be audited before adding duplicate effects.

The actual hit router passes location/magnitude to ExecuteGameplayCue without an EffectContext.
Engine ASC/cue-manager parameter overloads forward those parameters unchanged to the ASC avatar.
The previous native feedback handler therefore skipped the configured camera shake. It also used
client camera/haptic RPCs without local ownership checks, which could duplicate feedback if a
context were supplied later. The repaired handler uses the cue target's locally controlled pawn
and starts its camera shake locally. Haptics keep the local guard; accessibility skip flags and
world-position sound/VFX remain intact. The opt-in, non-shipping `bway.Combat.DebugCueFeedback`
records actual notify execution, target identity, configured assets and camera-instance creation.

Game35 passed; candidate36 cooked cleanly and all131 hashes verified. `hit01-fixture-brief.json`
and the retained hit01 result show a setup failure, not a feedback verdict: the old URL forced
all four humans to Argus, and correct per-team duplicate rejection left two players without
locked heroes/pawns. The harness later timed out waiting for results. Do not bypass this rule.

Candidate37 uses normal hero selection instead. The no-world delayed fixture now waits up to
180 seconds for Playing plus four locked human pawns, allowing the normal60-second selection
phase and travel/warmup to finish. Authority and client fixture selection use already loaded
native primary-ability families instead of Blueprint-name substrings. Game36 passed the initial
readiness change; game37 adds that required family-selection correction. This extra build is
retained as workflow overhead; no second gameplay run preceded the final coherent fixture build.
The retry also stops promptly on an explicit fixture error and preserves expected test-exit
markers separately from host teardown. Candidate37 package/runtime evidence remains pending.

## Staging correction and missing owner hit confirmation

Hit02 was stopped after its logs proved that opening Dorado directly enters late-join pickers,
not the shared staging selection timer. `hit02-owned-setup-stop.json` records the four exact
candidate37 executable/PID checks and owned teardown. Reusing the unchanged artifact from the
staging URL already observed in candidate35 fixed setup without another compile/cook.

Hit03 (`packaged-LAN-20260927-37-hit03-staging-*`) completed production selection/travel,
remote damage, teammate pass, natural goal, objective credit and fixture transform restoration.
All three processes exited0. Its stricter feedback verifier remains failed: the host and two
simulated peers executed the hit notify, while the actual remote attacker did not. Client1's
final test-exit lines were also absent from the buffered log despite its process exit0; preserve
that missing evidence instead of inferring UI completion. `hit-family.json` records all attempts.

The router's server-only ExecuteGameplayCue inherited the attacking ASC's current prediction
key. GAS's NetMulticast_InvokeGameplayCueExecuted_WithParams skips a local client's prediction
key, assuming that client already predicted the cue. Only muzzle feedback is predicted here;
authority-confirmed hit feedback must use an empty key. A scoped empty FPredictionKey now covers
only the confirmation call and restores the original ability prediction context afterward.
The opt-in dispatch record captures previous/applied key validity so the runtime test can check
this cause. Candidate38 adds that repair; its next run adds the engine-supported FORCELOGFLUSH
flag to preserve terminal evidence. `verify-hit-feedback02.py` keeps the original gameplay and
camera assertions and adds one authority dispatch with an empty key and no client dispatch.

## Candidate38 confirmed-hit delivery

`packaged-LAN-20260927-38-hit04-staging-feedback-verification.json` passes 54 assertions and
13 corruption controls. The actual authority dispatch reports `priorPrediction=1` and
`appliedPrediction=0`, confirming the suppressed-owner cause. Each of the four peers executes
the authored HitConfirm exactly once. The real remote attacker (client2, player257, Spartacus)
creates one camera shake; the host and other clients create none. No EffectContext is supplied.
The authored sound and Niagara slots resolve; the haptic slot is empty and no haptic is sent.

The same run completes normal timed hero selection, travel, remote damage, teammate passing,
a natural goal, exact score/objective credit, and fixture transform restoration. All three
clients log Results and the real TestExit marker and exit 0. All four logs contain zero runtime
errors or cue-registration warnings. `hit04-fixture-brief.json`, `hit04-preflight.json`, the
runner result, and full logs preserve the evidence. The host is torn down separately after the
client exits. Candidate38's 131 staged hashes match the frozen manifest and compiled executable.

This is four-process structured evidence on one PC with 200 ms latency and no rendering/audio.
It does not prove perceived camera, sound, VFX, physical input, configured haptics, host-owned
camera behavior, reduced-motion variants, second-PC LAN, or the final integrated acceptance.
Invalidate this scope after changes to hit dispatch, prediction, cue-target ownership, notify
implementation, authored hit assets, or the relevant transport. Alona's separate candidate35
prediction proof remains frozen; this run exercises a different primary ability family.

Overhead through hit04: 76 builds, 32 automation runs, 27 full packages, 54 packaged attempts.
This continuation reused the completed candidate38 package and launched one four-process run.
The next gate inspects actual relic settings, client configuration, and pickup/drop/score/reset
feedback before changing dispatch; existing replicated state VFX must not be duplicated.

## Relic audio integration

Read-only asset inspection (`relic-feedback03.json`) confirms that the active relic Blueprint
has the default settings asset on its CDO. Current peers therefore have that configuration
fallback even though `RelicSettings` is not itself replicated. A runtime settings override is
not covered by this evidence. The Blueprint has only three disconnected default events and
an empty construction script; it contains no alternate sound or state-effect implementation.
All four audio and four state-Niagara slots were empty. The earlier comment claiming RepNotify
updated audio was inaccurate: pickup/drop/goal/reset called `PlaySoundAtLocation` only on
authority. Visual state dispatch exists but has no authored effects to spawn.

`relic-feedback01/02` preserve two failed read-only commandlets. Both used asset-registry class
lookup before that feature path was indexed. The changed hypothesis in attempt03 used direct
generated-class loading and succeeded with no asset changes or dirty packages. Do not repeat
the unchanged registry lookup. The subsequent editor inspection used PID40004 and closed it
normally before compilation. Its single Pixel Streaming target-window startup error remains
an open rendered/UI issue; these calls prove only read-only asset/graph inspection and scoped
asset saves.

The default settings now reference four existing non-looping placeholder SoundWaves: base
pickup, short melee impact for drop, a chime for scoring, and item replenish for reset.
`relic-audio-placeholder05.json` records the before/after fields and clean package state. Only
this settings asset was saved. Their audible balance, spatial attenuation, and art direction
still need a rendered/audio pass; state-Niagara slots remain an explicit pending gate.

The native audio path now sends a transient unreliable multicast from the accepted authority
event. Its resolved sound and event position travel together, independently of carrier,
movement, and settings replication ordering. The listen host plays locally, relevant clients
submit the same sound, and dedicated servers skip playback. State RepNotify never plays audio,
so late arrivals cannot replay old pickups or goals. Repeated drop/reset calls preserve their
existing cleanup behavior but play sound only when the state actually changes.

The non-shipping diagnostic reports actual event delivery, sound, position, settings, and local
submission. An opt-in, one-shot development fixture repeats reset and drop before the existing
remote pass/score sequence, then clears its own flag. This keeps normal gameplay unaffected.
Game39 passed in 69.79 seconds with empty failure signals. Candidate39's full package and
four-peer delivery plus post-score-arrival results are recorded below.

## Candidate39 audio transport and arrival result

Candidate39 Cook/Stage/Pak passed in 107.29 seconds. All 131 staged hashes match, with executable
SHA256 `D2D48D057CF234978143DA56EB734F091C61AC771EA3C9072F4066CB16ECE3BE` matching game39.
`packaged-LAN-20260927-39-relic01-staging-audio-verification.json` passes 82 assertions and nine
corruption controls. Events 3–7 cover one reset, one drop, the initial pickup, the real pass
receiver's pickup, and the natural goal. All four original peers receive each event once with
the expected SoundWave, configured settings, and event position within the net-quantization
tolerance. Two reset requests and two drop requests produce one event each. The separate late
join receives the current Scoring state and configured settings, reaches Results, and receives
no historical audio. All four client processes exit 0 through their real Results TestExit.

The retained hit verifier also passes 54 assertions and 13 controls on the original four peers.
Actual enemy damage, teammate pass, natural goal, exact team/objective credit, owner-only hit
camera creation, and fixture transform restoration remain passed. All five logs contain zero
runtime errors. The host's owned teardown is recorded separately. Full logs, result, audio/hit
verifiers, `relic01-fixture-brief.json`, and `relic01-runtime-preflight.json` preserve the proof.

The audio verifier decodes Unreal's compact vectors by named axes, including omitted zero
axes and `V(0)`. It rejects missing/duplicate events, wrong assets/settings/positions, suppressed
submission, absent arrival state, and historical replay. This run uses NullRHI/nosound and
proves transport plus the local audio call, not audibility, attenuation, visual quality, physical
input, second-PC LAN, packet-loss guarantees, or integrated final acceptance. Unreliable audio
events deliberately are not durable gameplay state. Runtime settings overrides remain untested.

One further presentation defect is visible in the production call order: `ResetRelic` calls
`BeginResettingState` (and its sound) before moving the relic to the spawn point, then calls
`OnDropped`, which plays the drop sound. The next gate must make a completed reset sound once
at its destination, retain one sound for a genuine voluntary drop, and preserve late-join and
gameplay behavior. The transport pass above does not establish those reset semantics.
All four relic state-Niagara slots, audio balance/attenuation, slide feedback, and rendered
presentation remain pending. No Niagara asset was changed in this continuation.

Current overhead: 77 builds, 32 automation runs, 28 packages, 55 packaged attempts; nine read-only
commandlets, five editor lifecycles, three earlier asset-mutation commandlets, and one scoped
editor asset save. This continuation added one build/package, two packaged runs (hit04/relic01),
three read-only commandlets (two registry failures), one editor lifecycle, and one asset save.

## Candidate40 reset sound semantics

The production reset manager now resolves a spawn point before changing state. Begin-reset is
silent; the existing completion/drop path distinguishes Resetting from an ordinary drop and
emits the configured reset sound only after relocation. Repeated completion calls remain silent.
The missing-destination branch preserves the prior state; that branch is source/compile checked,
not a dedicated runtime-failure claim.

Game40 passed in 48.1 seconds. Full Cook/Stage/Pak passed in 96.12 seconds and all 131 staged
hashes match the compiled executable. `packaged-LAN-20260927-40-relic02-staging-audio-verification.json`
passes 101 assertions and 11 corruption controls. The fixture moves the relic to
`(1020, -2410, 2130.40)`, calls begin-reset twice without sound, and invokes the production
manager. Exactly one reset event is emitted at the actual destination `(0, 0, 540)` with no drop
event for that reset. A genuine pickup/release then emits one drop sound despite two release
calls. All original peers receive the same event assets and positions, and the post-score
arrival sees current Scoring state without historical audio.

The retained hit/gameplay verifier passes 54 assertions and 13 controls. Damage, real teammate
pass, natural goal, exact credit, local hit camera ownership, fixture transform restoration,
four normal Results/test exits, and zero errors across all five logs remain verified. Negative
controls additionally reject a reset sound at the old position even if every peer agrees, and
an early reset sound. Full logs, `relic02-runtime-preflight.json`, the two verification JSONs,
and `relic02-fixture-brief.json` retain the proof. NullRHI/nosound limitations still apply.

This continuation used one game build, one full package, and one packaged attempt with no game
retry. Totals after this gate: 78 builds, 32 automation runs, 29 packages, 56 packaged attempts.
Next is relic VFX lifetime and placeholder integration. The editor bridge implements
`capture_screenshot` with a required `filename` and an asynchronous renderer-written file;
verify the file before viewing it. This is a possible visual evidence channel despite general
Computer Use being unavailable, and does not establish physical-input acceptance.

## Relic VFX preparation and visual channel

Editor39 compiled the current source successfully; `editor39-linked-dll.json` proves the
BreakawayCoreRuntime editor DLL advanced after compile start. Editor PID42560 was launched
through `Start-ProjectBEditor.ps1` (`relic-vfx-editor02-launch.json`) and its bridge is live.
The DevMap and all content packages were clean before inspection. The Pixel Streaming startup
target-window error remains recorded in `relic-vfx-editor02.log`; it has not been fixed or
discarded. Independently, three fresh renderer screenshots were written and inspected:
`relic-vfx06-baseline.png`, `relic-vfx07-comparison.png`, and `relic-vfx08-comparison.png`.
This verifies editor viewport capture, not physical interaction, audio, or the Pixel Streaming path.

`relic-niagara06-info.json` inventories the existing choices. GunPad/Loading use eight/ten
emitters and the preview shows a large red beam. MinimalLightweight and FountainLightweight
each use one emitter. The second comparison moves all previews to Z540 to clear the ramp;
Fountain at uniform scale0.15 produces a compact white effect, and Minimal shows a small glow.
The first comparison was partially occluded and is not sufficient for final scale selection.
No Niagara asset or relic VFX slot has been changed yet. Judge the selected placeholder on the
actual relic and during movement before accepting its appearance.

Minimal's stateless emitter has infinite one-second loops, one burst particle per loop, and
fixed bounds of +/-100 (`relic-niagara11-lifetime.json`). Particle lifetime, sprite size, owner
space behavior and scalability still need inspection/selection. The authored template is not
yet a verified relic effect. `SpawnTrailEffect` is also still a stub with no settings slot;
throw/pass trail wiring and its cleanup remain part of integration.

Cached bridge capabilities: Python cannot read the protected `NiagaraSystem.EmitterHandles`
property. Native `asset.read_properties` with `propertyName: EmitterHandles` exports the
stateless subobject path, after which `unreal.find_object` and public editor properties work.
`EmitterState` and `SpawnInfos` support `export_text`; `bLocalSpace` is absent on this stateless
emitter and must not be repeatedly probed. The initialize module exposes `LifetimeDistribution`,
`ColorDistribution`, and `SpriteSizeDistribution`. Native emitter-property editing expects
versioned stateful emitter data, so do not use it blindly for a stateless template.

All three temporary preview components were destroyed and verified invalid; both dirty-map and
dirty-content inventories are empty (`relic-vfx12-preview-cleanup.json`). The original viewport
was restored. Python's bound `destroy_component` needs a caller object in this engine; passing
the component owner fixed the single failed no-argument cleanup call. Keep the live editor for
the next asset operation, revalidating its exact PID and bridge. There is no active PIE session.
Current counts:79 builds,32 automation runs,29 packages,56 packaged attempts and six editor
lifecycles; the other counts are unchanged. The next step is scoped placeholder authoring and
real relic state/lifetime integration, followed by rendered and multiplayer proof.
