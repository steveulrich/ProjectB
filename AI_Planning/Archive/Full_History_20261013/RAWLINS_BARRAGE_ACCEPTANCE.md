# Rawlins Blazing Barrage acceptance

Updated October 13, 2026. The targeted fix restores all 12 shots and per-shot body-action
presentation across four listen-PIE peers at zero and measured 100 ms lag. Two earlier
baselines reproduced 11 shots and no action presentation. A subsequent fix makes
health loss interrupt the channel after healing, verified at zero and 100 ms lag.
Walking/aim, death/respawn, and mid-slide activation now have scoped four-world PIE
evidence. Damage and airborne juggling now pass at zero and measured 100 ms lag.
Candidate07 now passes scoped cooked normal delivery at zero added latency.
The 100 ms gate, cooked interruption/reset, changing aim, and final-alpha parity
remain open.

## Required behavior and configuration

The supplied hero stats sheet requires 12 rapid shots, stationary but aimable firing,
airborne juggling, interruption, and a 25-second cooldown. The loaded
`GA_BW_Rawlins_BlazingBarrage` and `DA_BW_RawlinsKitConfig` use 12 shots at approximately
0.1-second intervals, base damage 4 plus 0.3 attack strength, projectile speed 3600,
lifespan 1.5 seconds, and upward impulse 350. Exact cadence and the final-alpha
damage-interruption trigger still need independent reference confirmation.

The ability is LocalPredicted and InstancedPerActor. Its activation owns
`Gameplay.MovementStopped`. Its effective Blueprint contains only two unconnected
event nodes, and `BarrageProjectilePresentation` is null. These asset readbacks are
recorded in the preceding Rawlins primary gate's `barrage-class-defaults01.json`
and `barrage-graph-summary01.json`.

## Reproduced baseline

Evidence is under `Saved/Logs/rawlins-barrage-animation-20261009/`.
`baseline-zero01` and `baseline-lag01` each contain one accepted native queued
remote-owner activation, one authoritative activation, and 11 authoritative shot
records despite the loaded count of 12. The native driver readbacks establish
zero and 100 ms lag respectively; these are observed settings, not command-line intent.

Each peer supplied 173 samples over six seconds. No montage or weighted UpperBody
or FullBody action appeared. MovementStopped appeared during the channel and cleared
afterward; owner and authority became inactive. Both bounded logs contain zero
errors, warnings, or critical lines. Observers removed callbacks and restored mesh
tick settings. Short-lived projectile samples corroborate spawning but are not the
sole shot-count proof.

The `baseline-*-independent-verification02.json` audits preserve `validSetup: true`,
`missingShotReproduced: true`, and `missingActionReproduced: true`.
The shot-count requirement fails; an otherwise clean observation is not a gameplay pass.

## Targeted correction

The predicting owner retains its committed channel and predicts the first cosmetic
cue. Authority alone advances the shot timer and ends both instances after the final
shot. This removes the competing predicted completion timer. Each successful
authoritative shot emits `GameplayCue.Breakaway.Weapon.RawlinsBarrage.Fire`.
The activation prediction key suppresses the owner's duplicate first cue; later
timer cues reach that owner as well. Both fixed captures verify one play per shot
on each peer, including exactly 12 on the predicting owner.

`AM_BW_Rawlins_Barrage_Placeholder` is a dedicated UpperBody montage using the existing
project-owned, nonadditive Rawlins pistol-fire sequence. It has a finite 0.6667-second
section, rate 1, blend-in 0.03, blend-out 0.1, and no gameplay notifies.
`GCN_BW_Weapon_RawlinsBarrage_Fire` binds the registered Barrage fire tag to this
montage with play rate 1. `cue-authored01.json` records the saved binding and registry
name; `cue-validation01.json` reports zero errors and zero warnings.

The initial shot-completion correction preserved damage, cooldown, shot interval,
movement tags, and the health-interruption rule. The subsequent health gate below
reproduces and corrects the separate comparison against health at activation.

## Fixed normal-channel evidence

`compile-editor01.json` records a passing LyraEditor build in 32.50 seconds, exit
code 0, and empty `failureSignals`. The before/after DLL records preserve the
updated LyraGame and HeroRawlinsRuntime binaries used by the restarted editor.

`fixed-zero01-independent-verification.json` and
`fixed-lag01-independent-verification.json` each pass all 67 checks. They independently
match captured native-log bytes against the original log window and its hash. Remote
owner 257 activates the actual granted Barrage through the queued native selector.

| Capture | Observed lag | Samples per peer | Authority shots | Exact plays across four peers | Replacement stops |
| --- | ---: | ---: | ---: | ---: | ---: |
| `fixed-zero01` | 0 ms | 169 | 12 | 48 | 44 |
| `fixed-lag01` | 100 ms | 170 | 12 | 48 | 44 |

Both runs contain exactly 12 authority plays, 12 owner plays, and 24 simulated-proxy
plays. Each peer supplies 12 evaluated montage starts with advancing positions and
positive UpperBody weight. Weighted hand poses differ from idle, linked locomotion
layers remain present, and the final montage finishes naturally. This verifies the
dedicated Barrage action rather than counting the preceding primary montage.

Authority and owner show `Gameplay.MovementStopped` during the active channel, then
release the tag and become inactive. All four native driver readbacks match each
capture's requested lag, with zero packet loss and no random lag. The second run
waits for the real cooldown; the fixture does not reset or shorten it.

Both bounded windows contain zero errors, warnings, or critical lines. Two earlier
Pixel Streaming target-window errors remain in the full editor log and supply no
rendered-acceptance proof. Each observer removes its callback and restores all four
mesh tick settings. The editor subsequently closed normally.
`editor01-settings-cleanup.json` and `editor01-cleanup.json` verify that PID 3236
exited, temporary PIE/network settings were restored, and the original ShooterCTF
module manifest was restored.

`verifier-controls01.json` records 30 rejected corruptions and five positive controls.
The checks reject wrong identities, assets, counts, network settings, cleanup,
unclassified diagnostics, and command-line text impersonating native events. Positive
controls preserve player-ID independence and accept incomplete projectile sampling
when independent native shot evidence is complete. These are offline evidence checks,
not additional gameplay runs.

## Health interruption reproduction

The reviewed development command
`bway.Test.BarrageAnimationHealth <id> <heal-damage|damage-only|heal-only>` prepares
the exact remote owner through production damage/heal effects, activates Barrage
once, records health changes after the first authoritative bullet, and restores
health after inactivity. It preserves the real cooldown, begins cleanup at eight
seconds, and has a ten-second hard bound. The observer allows twelve seconds.
`compile-editor02.json` passes LyraEditor in 8.28 seconds with exit 0 and empty
`failureSignals`; `editor-dll-after-compile02.json` verifies that diagnostic binary.
The following baselines retain the original production health rule and require
explicit health restoration and owner convergence evidence.

The first health attempt, `heal-damage01`, stopped before activation. Production
`DamageSelf(100)` reduced health from 400 to 240; regeneration then raised it to
246.25 while the fixture awaited owner convergence. The fixture rejected that
change against its frozen preparation value. Its bounded log contains no Barrage
activation or shot, and `HealthCleanup` confirms both endpoints restored to 400.
This is setup evidence, not a test of interruption. The next fixture revision must
use the converged activation baseline and sufficient measured health headroom.
Editor PID 37164 closed normally; `editor02-cleanup.json` records settings and
module restoration.

The corrected fixture accepts only upward regeneration bounded to 25% of the
observed preparation loss and captures an exact shared health baseline immediately
before activation. It uses the loaded damage contribution and actual effect deltas
to preserve nonlethal headroom. `compile-editor03.json` passes in 8.09 seconds;
the runtime DLL timestamp advanced before editor PID 7852 started.

| Baseline capture | Result | Authority shots | Four-peer recoil plays | Audit checks |
| --- | --- | ---: | ---: | ---: |
| `heal-damage02` | Health 240 → 400 → 336; channel incorrectly continues | 12 | 48 | 62 |
| `damage-only01` | Health loss below activation interrupts | 1 | 4 | 61 |
| `heal-only01` | Healing permits natural completion | 12 | 48 | 60 |

The authoritative audits are `heal-damage02-independent-verification03.json`,
`damage-only01-independent-verification02.json`, and
`heal-only01-independent-verification.json`. The reproduction requires the final
health to exceed both the dispatch and first-shot baselines. The damage-only
control requires no later shot. All captures retain the real 25-second cooldown,
observed zero lag/loss, four-peer recoil/pose evidence, inactive endpoint/tag state,
restored health, and callback cleanup. Their bounded logs contain no errors,
warnings, or critical lines. The editor closed normally with settings and module
restoration recorded in `editor03-cleanup.json`.

The verifier accounts for the source-referenced finite hit-reaction montages and
does not count a return from a hit reaction to an advancing Barrage montage as
another recoil start. Earlier stricter verifier reports remain preserved.
The health/order controls in `heal-damage02-verifier-controls03.json`
reject 48 corruptions and pass three positive cases.

The targeted production correction compares each new health value with its
immediately previous value. The obsolete activation-health snapshot is removed;
damage, cooldown, cadence, and cancellation cleanup are unchanged.

## Fixed health interruption evidence

`compile-editor04.json` records a passing LyraEditor build in 18.76 seconds with
exit 0 and empty `failureSignals`. The HeroRawlinsRuntime DLL timestamp advanced
before editor PID 21804 launched with the captured source patch.

| Fixed capture | Observed lag | Authority shots | Four-peer recoil plays | Audit checks |
| --- | ---: | ---: | ---: | ---: |
| `fixed-heal-damage01` | 0 ms | 1 | 4 | 62 |
| `fixed-damage-only01` | 0 ms | 1 | 4 | 61 |
| `fixed-heal-only01` | 0 ms | 12 | 48 | 60 |
| `fixed-heal-damage-lag01` | 100 ms | 1 | 4 | 62 |

Each capture has a passing `-independent-verification.json`; the damage-only result
uses `-independent-verification02.json`. At zero lag, healing then damage repeats
240 → 400 → 336 and stops after the first shot. Damage-only changes 240 → 176 and
also stops. Healing-only changes 240 → 400 and completes naturally. No damage case
produces another authoritative shot after the health decrease.

At 100 ms lag, regeneration changes the activation baseline to 246.005646 and
first-shot authority health to 252.198486. Healing reaches 400 and damage reduces
it to 336, above both baselines. Authority ends at the damage event, 0.187289
seconds after the cast; both endpoints are inactive at 0.336881 seconds.
All four native driver records show `PktLag=100` and zero other lag/loss values.
This proves end-state propagation for the recorded topology, not a latency budget.

All fixed audits require real cooldown retention, exact native recoil counts,
evaluated poses, final ability/tag cleanup, health restored to 400 on both endpoints,
four restored mesh tick settings, and callback removal. The zero-lag damage-only
capture first exposes the Barrage montage around position 0.356 seconds on three
peers because the current-montage query selects their earlier hit reaction. The
audit bounds that observation by the native montage duration and requires preceding
production hit reactions, advancing positions, weighted poses, and natural end;
it does not claim the obscured initial pose was sampled.
`fixed-damage-only01-verifier-controls.json` rejects 46 corruptions and passes four
positive cases, including controls for that observation gap.

The four bounded logs contain zero errors, warnings, or critical lines. Two earlier
Pixel Streaming target-window errors remain in the full editor log; there is no
rendered-appearance acceptance claim. `editor04-cleanup.json` verifies normal editor
exit and restored PIE/network settings and module manifest.

Through the health gate, totals were four builds, four owned editor lifecycles plus one reused lifecycle,
11 completed runtime captures, one failed preactivation setup capture, and zero
packages. The extra health-fixture build corrected measured regeneration/damage
assumptions; the following build applied the reproduced gameplay fix. Verifier
revisions reused saved captures and did not launch additional gameplay runs.

## Movement and aim correction — October 11, 2026

The first native walking-input capture exposed two gameplay defects that idle
presentation checks could not detect. `move-zero01-diagnosis.json` and
`move-zero01-independent-verification02.json` preserve the failing evidence:

- All four peers continued at 200 cm/s with `Gameplay.MovementStopped` active.
  Native owner/authority `GetMaxSpeed` returned zero, but Unreal's `CalcVelocity`
  applied the loaded 200 cm/s `MinAnalogWalkSpeed` floor afterward. The movement
  persisted on flat ground before reaching the slope.
- All 12 projectile headings disagreed with the authority camera. A 90-degree
  view produced a 180-degree velocity. Shared projectile configuration supplied
  world-space velocity, then deferred component initialization interpreted it as
  local space and rotated it again.

Lyra movement now returns zero minimum analog speed while the stop tag is active.
The custom slide speed path also checks that tag before computing its own cap.
Normal movement settings and speed multipliers are preserved. Both shared
world-space projectile assignments now explicitly disable local-space initial
velocity; constructor defaults and paths that do not assign world velocity remain
unchanged. This correction affects Alona, Korryn, and Rawlins projectiles.

The initial fixture also restored the authority transform before queued client
movement had drained, leaving a horizontal 1.17 cm offset. Cleanup now stops input
once after `EvidenceEnd`, waits for the configured network round trip plus 0.25
seconds and at least 0.1 seconds of measured quiet convergence, then performs one
authority teleport. It retains the 1 cm authority tolerance and makes no client
transform writes. Cleanup motion cannot satisfy gameplay recovery.

`compile-editor05.json` passed in 10.26 seconds for the first diagnostic.
`compile-editor06.json` passed in 33.43 seconds for the reviewed corrections and
shared projectile regression fixture. Both have exit zero and empty
`failureSignals`; affected DLL timestamps advanced. Frozen source snapshots and
the two owned editor launch records are beside the captures.

| Corrected capture | Checks | Shared stationary interval | Settlement after cast | Shots / recoil plays |
|---|---:|---:|---:|---:|
| `fixed-move-zero01-independent-verification.json` | 110 passed | 0.891 s | 0.240 s | 12 / 48 |
| `fixed-move-lag01-independent-verification.json` | 110 passed | 0.765 s | 0.483 s | 12 / 48 |

The later run measures `PktLag=100` on all four drivers, with other lag/loss fields
zero. Both captures show moving warmup, zero measured positional drift after
settling despite continued native movement input, changing owner/camera yaw,
matching authoritative projectile headings, and resumed movement before cleanup.
The settlement bounds derive from recorded speed/braking, sampling, and transport;
this is not a claim of an instantaneous stop. Each run also verifies 44 montage
replacement stops, evaluated poses on four peers, continuous channel coverage,
the loaded 25-second cooldown against elapsed time, and cleared ability/tags.
Final authority position error is zero in both; owner error is at most 0.000011 cm.

The observer and verifier passed independent source review. Offline
`fixed-move-zero01-verifier-controls02.json` rejects 53 corruptions and passes
three positive controls, including rejection of the old 200 cm/s movement,
double-rotated projectile headings, late-only stationary evidence, inactive
moving gaps, false cooldown duration, and missing or premature cleanup drain.
`movement-final-independent-summary.json` links both captures, controls, and the
reviewed helper hashes; no additional gameplay runs were needed for these checks.

`projectile-direction-fixed01-verification.json` independently passes 13 checks
over 24 native deferred-spawn cases: three projectile families, four yaw/pitch
rotations, and the configured-movement and presentation-fallback routes. All
directions and speeds match after `FinishSpawning`; all 24 fixture actors are
destroyed in that frame. This proves authority initialization, not damage,
collision, hero-input routing, or network transport for all three families.
The attempted Python equivalent could not access deferred spawning; two setup
records preserve that limitation, with no actors created. The native fixture
replaces that unsupported route.

Both gameplay captures and the bounded projectile regression contain zero errors,
warnings, or critical lines. The two earlier Pixel Streaming target-window errors
remain in the full editor log. `editor06-cleanup.json` confirms normal exit,
restored PIE/network settings and module manifest, and removed observers. These
are structured same-process PIE results; there is no rendered-appearance claim.

Gate totals are six builds, six owned editor lifecycles plus one reused lifecycle,
14 completed Barrage captures, one failed health setup, two unsupported Python
projectile setup attempts, one native 24-case projectile run, and zero packages.
The second movement build followed reproduced gameplay and cleanup failures;
offline verifier revisions reuse immutable captures.

## Death interruption, production respawn, and cooldown

The development-only `bway.Test.BarrageAnimationDeath <playerId>` fixture waits
for the exact remote owner's authority Barrage to be active with one new owned
bullet, then invokes production self-destruct before the second-shot deadline.
Weak pawn, controller, PlayerState, ASC, ability-spec, and round identities prevent
the queued request from following a replacement pawn. The native request has a
five-second bound; the scalar observer has a 40-second bound and owns its callback
and mesh-tick restoration. This iteration changes diagnostics, not production
death, respawn, or cooldown behavior.

`compile-editor07.json` records a successful LyraEditor build in 8.64 seconds with
no failure signals and an advanced runtime DLL. `source-snapshot-runtime07.json`
records identity `BB29E4215A338F4D6D0B4CF1D819CA4038738E624F6057119D0F1B22238A49B7`.
The only difference from the build snapshot is the recorded temporary ShooterCTF
module BuildId alignment. One owned editor hosted all three captures.

The first capture, `death-zero01`, proved the interrupted shot, four deaths and
normal respawns, but could not prove the persistent effect clock. The observer
queried effect asset tags with `GetActiveEffectsWithAllTags`; the cooldown uses
granted tags. Its failed audit and `death-zero01-independent-partial-review.json`
preserve that distinction. The corrected observer uses `GameplayEffectQuery`
with `OwningTagQuery` and `ASC.GetActiveEffects`. Only the observer changed before
the fresh capture; there was no rebuild or editor restart.

`death-zero02-independent-verification03.json` and
`death-lag01-independent-verification03.json` pass 67 checks each.
`death-zero02-verifier-controls03.json` rejects 46 corruptions and accepts three
benign mutations plus both original captures. A control exposed an extra shot
outside the expected cast groups; the final verifier requires exact bounded
activation and shot totals as well as per-cast counts. Both immutable captures
pass that stricter check without another gameplay run. A compensated collapsed
shot interval is also rejected by a lower cadence bound derived from the loaded
interval and measured frame allowance. `death-final-independent-review.json`
links the independent review and frozen helper hashes.

`death-zero02` and `death-lag01` each record one interrupted authority shot,
four old-pawn recoil plays followed by four immediate death stops, and a fresh
12-shot cast with 48 plays and 44 replacement stops. The latter records 100 ms
packet lag on all four drivers. Neither run exercises a late dead-avatar cue
rejection; that guard has source review only. Every peer observes the old pawn dead and exactly
one replacement while remaining in round 1 and the Playing phase. The replicated
respawn deadline matches the loaded three-second delay with zero reduction.
Old ability specs end and the movement-stop tag clears before replacement.

The PlayerState ASC and its actual cooldown effect persist through ability-spec
removal and regrant. Both owner and authority observe the loaded 25-second effect
duration, its countdown, expiry, and an idle new spec before the second cast.
The fresh cast commits a new cooldown and ends normally. The observer accounts
for all eight pawn lifetimes: four old pawns are destroyed, four live mesh-tick
settings are restored, and its callback is removed. Timer/delegate
cleanup remains source-grounded and corroborated by the absence of later
old-pawn shots; private timer state is not directly reflected.

Each bounded capture has zero errors or critical lines and seven classified
`InitializeHeroData` banner warnings. The full editor log retains two earlier
Pixel Streaming target-window errors; these captures make no rendered-appearance
claim. `editor07-cleanup.json` proves normal editor exit, restored module manifest,
restored network/PIE settings, and the closed-editor one-client configuration.

Gate totals are seven builds, seven owned editor lifecycles plus one reused
lifecycle, 17 completed Barrage captures, one failed health setup, two unsupported
Python projectile setup attempts, one native 24-case projectile run, and zero
packages. The death gate added one build, one editor, and three captures; the
extra capture followed the demonstrated observation-query defect. Offline audit
and corruption-control revisions reuse the immutable captures.

## Mid-slide baseline and restoration defect

The reviewed `bway.Test.BarrageAnimationSlide` diagnostic compiles in
`compile-editor08.json`; its source snapshot is `source-snapshot-build08.json`.
One fresh owned editor reused the four-world staging-to-Dorado flow. Cold loaded
`MinSlideSpeed=0` confirms the authored held-stationary slide policy.

An initial bridge request had an unclassified transport delay and no observed
editor dispatch. Before retrying, a read-only bridge check confirmed that the
observer module had not loaded. `slide-zero01` then failed setup with zero
activations because Python cannot directly read protected `FHitResult` fields.
The corrected observer uses the source-verified `GameplayStatics.BreakHitResult`
output contract; the setup failure remains preserved. No rebuild was needed.

`slide-zero02` reached actual moving slide on owner and authority, then produced
12 authority shots, 48 recoil plays, and 44 replacement stops. Thirty common-active
samples show all four peers in Custom/Slide with the movement-stop tag, zero speed,
and zero three-dimensional drift. This is partial channel evidence, not acceptance.

The production slide exit changes `bOrientRotationToMovement` from the captured
false value to true on both endpoints. Capsule, floor angle, rotation rate, tags,
and ability state restore, but the orientation policy does not. The diagnostic
therefore never admits normal walking recovery. Continued movement reaches a
25-degree ramp, violating its flat-ground scope. The same restoration assertion
also blocks safe cleanup despite settled endpoints, leaving their transforms
about 466 cm from the starting point. Preserve the failed audit and
`slide-zero02-first-causal-diagnosis.json`; do not count the channel samples as a
passing recovery test.

The coherent correction must restore the pre-slide orientation on grounded exit,
preserve existing non-grounded behavior, keep flat support strict during slide
and channel, and allow actual walkable ground during ordinary walking recovery.
Post-evidence safety cleanup must not depend on a gameplay assertion passing;
the failed assertion must remain visible. Source and helper review, a fresh
LyraEditor compile, and zero/100 ms runtime evidence are required before acceptance.

`editor08-cleanup.json` records normal editor exit and restored settings/module
manifest. It explicitly retains `fixtureRestorationPassed=false`; stopping PIE
removed the affected worlds. This attempt adds one build, one editor, one setup
failure, and one completed failing gameplay capture, with no package. Cumulative
counts are eight builds, eight owned editors plus one reused lifecycle, and
18 completed Barrage captures.

## Fixed mid-slide evidence

The production correction saves the pre-slide orientation policy and restores it
on grounded exit. Existing non-grounded exit behavior is unchanged. The native
diagnostic now requires flat support during the slide/channel, permits actual
walkable terrain during walking recovery, and drains input safely even when a
gameplay assertion fails. Safe transform restoration cannot turn that failure
into a pass. `compile-editor09.json` passes with no failure signals and an advanced
runtime DLL; `source-snapshot-build09.json` freezes the tested source.

`fixed-slide-zero01-independent-verification02.json` and
`fixed-slide-lag01-independent-verification02.json` each pass 163 checks at zero
and measured 100 ms lag. Each capture records one remote-owner activation,
12 authority shots, 48 recoil plays, and 44 replacement stops. Owner and authority
are moving in the production slide before activation. All four peers settle into
the authored held, stationary slide during Barrage, then return to normal walking
after production input release. Original orientation, capsule, floor angle, and
rotation settings restore during recovery, before fixture cleanup. Input drains,
transforms restore, and all delegates, observer callbacks, and tick overrides are
removed without cancellation or timeout. Both bounded logs have zero errors,
warnings, or critical lines. Editor startup still has the separately recorded
Pixel Streaming target-window errors; this gate provides no rendered/UI proof.

The initial 100 ms audit passed 162 of 163 checks because two projectile ages were
negative by 13 and 15 microseconds. Engine source explains this: actor creation
time is stored as a float, while world time is double precision. The revised
verifier derives a half-float-ULP allowance from each recorded world time, adds
one microsecond for log formatting, and rejects allowances above 32 microseconds.
It preserves exact projectile identity, count, order, heading, and the existing
upper age bound. The original audit remains saved. This correction reuses the
immutable captures and does not require another build or runtime attempt.

`fixed-slide-zero01-verifier-controls02.json` rejects 55 corrupted inputs and
accepts four benign mutations plus both real captures. Controls cover false flat
support, motion during the channel, orientation leaking into recovery, failed
gameplay hidden by cleanup, unreleased input, missing delegates, and timestamps
outside the source-derived precision bound. The frozen verifier hash is
`EB0199BF71C487C2912A3B2C477F2194CF1FC78EC34B948A2B13DC85D31434C2`.
The combined independent review is `slide-independent-review.json`.

`editor09-cleanup.json` records normal exit of the exact owned editor, restored
network/PIE settings, the verified closed-editor INI, and the canonical module
manifest. No content or maps were dirty at exit. The earlier failed fixture keeps
its failed restoration verdict; these corrected runs do not overwrite it.

This correction adds one build, one editor lifecycle, and two completed captures,
with no package or runtime retry. Cumulative Barrage counts are nine builds, nine
owned editors plus one reused lifecycle, 20 completed captures, one failed slide
setup, one failed health setup, two unsupported projectile setup attempts, one
native projectile-direction run, and zero packages.

Invalidate this slide proof when slide movement/entry/exit, Barrage movement-stop
or activation behavior, relevant tuning/assets, or the tested topology changes.
Changes to diagnostic sampling or verification require review and the affected
revalidation. Preserve the authored zero minimum slide speed and existing airborne
policy; this gate does not establish those policies as final-alpha parity.

## Damage and airborne juggling

The development-only `BwayBarrageDamageJuggleTest.cpp` drives one remote-owner
Barrage against an opposing listen-host player. It selects a flat, unobstructed
lane, settles the owner's observed aim and the authority view, and starts the
victim's real Jump ability. Barrage starts only after the victim is airborne and
descending. No health, armor, gravity, launch, or cooldown override supplies the
combat result. Synthetic aim remains separate from physical targeting proof.

Each authoritative projectile is paired with the actual health-change effect
context and its terminal destruction. The observer checks the pending upward
launch, a subsequent velocity increase and height gain for every hit, replicated
health, natural landing on all four peers, and restoration. Current live values
produce `(4 + 0.3 * 60) * (1 - 3/100) * 1 = 21.34` damage per hit; the verifier
derives this from loaded tuning and attributes instead of fixing the result.

`juggle-zero03-verification02.json` and `juggle-lag02-verification02.json` each
pass 186 checks at zero and measured 100 ms. Each records 12 distinct shots,
damage hits, and launch responses, 48 action plays, 44 replacement stops, and
exact cleanup. Each hit deals approximately 21.34 damage (256.08 total before
restoration). The final verifier rejects 60 corrupted inputs, accepts one benign
mutation and three actual captures, including the earlier zero02 baseline.
`juggle-final-helper-audit-summary.json` links the final audits and controls.
`juggle-runtime12-independent-review.json` independently recomputes all 24
hit/launch responses, verifies exact source/helper hashes, and confirms the
100 ms repeat admission after 68.409 seconds on the same endpoints.

The 100 ms repeat capture's initial audit passes 185 of 186 checks:
the remote cooldown rises once by 0.118591 seconds shortly after predicted
activation, then declines normally. The original failed audit is preserved in
`juggle-lag02-verification01.json`; `juggle-lag02-cooldown-diagnosis01.json` records
the samples and activation anchors. Unreal permits temporary overlap of predicted
and authoritative effects, remaps the server effect's start time onto the client
clock, and reports the greatest remaining matching cooldown. The observed change
is consistent with that mechanism; the effect-handle replacement itself was not
sampled. `cooldown-reconciliation-source-review.json` preserves the source
references and hashes.

The corrected verifier permits one early remote-owner correction only when its
arrival, inferred expiry shift, and before/after clocks agree with the recorded
owner and authority activation times and measured transport/sampling bounds.
Authority, zero-lag, and settled countdowns remain monotonic. The exact 25-second
duration, continuous cooldown tag, single activation, and late/multiple/excessive
reset rejection remain required. Five added corruption cases exercise these
limits. This re-audit reused the saved captures without another Unreal run.

Two setup failures remain in the record. `juggle-zero01` fired no shots because
another player's capsule blocked the forward lane. A read-only trace identified
that exact actor; the diagnostic now tries at most five nearby directions while
preserving collision with unrelated pawns. `juggle-lag01` also fired no shots:
the first remote activation left the authority ability spec's `InputPressed`
flag set, as Unreal's server activation code does. The diagnostic incorrectly
required that flag to be clear for direct Barrage activation. The correction keeps
unique-spec, inactivity, and real cooldown checks, and retains the released-input
requirement for the input-driven Jump. It never resets the flag or cooldown.
The successful `juggle-lag02` admission observes the authority flag still set.

These are diagnostic changes; production damage and juggle behavior did not
change. `compile-editor10.json` through `compile-editor12.json` pass, with linked
DLL timestamps recorded. Final native source is frozen at
`45FE4769AE61CDDFD84F76F3433BE27BDD433C1110D44405CF0C15FA43546920`.
Numbered source snapshots are supplemented by `untracked-source-build*.json`
and exact C++ copies because the generic snapshot records untracked names, not
their contents. `juggle-helper-freeze12-cooldown02.json` archives all seven final
helper inputs; prior helper archives and failed audits remain immutable.

`editor10-cleanup.json`, `editor11-cleanup.json`, and `editor12-cleanup.json`
confirm normal owned-editor exits, restored network/PIE settings, closed-editor
INI checks, and the canonical module manifest. No assets were dirty. The bounded
combat captures have no error or critical lines; startup Pixel Streaming errors
and failed read-only Python capability probes remain separately recorded. These
runs do not certify rendered quality or exact cadence/performance.

This gate used three builds, three owned editor lifecycles, five captures
(two setup failures and three combat captures), and no package. The second zero
capture after the readiness fix deliberately established the prior remote cast
needed to exercise repeat admission at 100 ms. Cumulative Barrage counts are
12 builds, 12 owned editors plus one reused lifecycle, and 25 captures.

## Remote-owner GAS cancellation

On October 11, 2026, health-independent owner cancellation passed 103 checks
each at zero and measured 100 ms in four-world listen PIE. The final results are
`cancel-zero01-verification03.json` and `cancel-lag01-verification03.json` in the
Barrage evidence directory. The verifier rejects 45 corrupted cases, accepts
two benign controls, and accepts both real captures. See
`cancel-lag01-verifier-controls03.json`.

The development fixture activates the exact granted owner spec, then calls
`CancelAbilitiesByFunc` with replication enabled after the first authority shot.
It matches the original pawn, controller, PlayerState, ASC, ability instance,
spec, round, and prediction key. Loaded policy permits remote cancellation:
`LocalPredicted`, `ClientOrServer`, and `bServerRespectsRemoteAbilityCancellation`
are verified, together with active cancellation flags.

Zero lag produces one authority shot and four recoil plays. At 100 ms, a second
shot fires before cancellation reaches authority, producing eight plays and four
replacement stops. Neither run spawns a shot after the matching authority
cancellation event. Both ability instances end, full health remains unchanged,
and the committed 25-second cooldown continues. Saved effect handles distinguish
the owner's initial prediction reconciliation from the stable authoritative
cooldown. Existing projectiles and presentation finish within the observation
window. The captures contain 171 and 174 scalar samples respectively.

The latency capture preserves a brief replicated movement-stop tag pulse after
local cancellation. The owner remains inactive throughout the unambiguous
post-end samples. Four samples retain tag count 1 from 1.047 to 1.157 seconds;
the first subsequent clear sample is at 1.188 seconds. Authority ended at
1.049224 seconds. Unreal's replicated activation-owned tag count can arrive
after local removal and update the count independently of ability activity.
This explanation is consistent with source and timing; individual replication
packets were not captured. The verifier requires immediate native local removal,
continuous post-end inactivity, paired ASC readback agreement, at most one binary
pulse, and final release within the measured transport and sampling bound.

The exact bundled Python reports 15.625 ms resolution for both its monotonic and
UTC clocks. Comparing elapsed samples with native UTC log events therefore uses
their summed uncertainty plus 2 ms log precision: 33.25 ms. No sample-interval
grace period applies to inactivity. The tag-clear deadline is 1.229474 seconds,
derived from authority end, 100 ms return latency, the observed 47 ms maximum
sample interval, and that clock uncertainty. The original lag audit failed
99/100 on unconditional tag release. A later zero-lag audit failed 101/102 when
it treated a clock-ambiguous sample as post-end. Both remain saved; the corrected
audits reuse their original captures. `owner-cancel-clock-capability01.json`
retains the measured metadata and its superseded one-clock proposal.

`cancellation-helper-freeze13-clock03.json` archives the six final helper files
and clock metadata. `owner-cancel-independent-review.json` verifies the final
audits, source/helper hashes, original log bytes, and corrected clock bound.
`untracked-source-build13.json` binds the exact native inputs;
the cancellation source SHA-256 is
`F75C5078B79AF9C12567CB4013C98071F6DC0123D65A42DA64A2D9279F88BEF8`.
`compile-editor13.json` passed in 21.28 seconds with no failure signatures and an
advanced runtime DLL. Both runs remove five native delegates, the observer
callback, and four temporary mesh tick overrides without fallback cancellation.
`editor13-cleanup.json` confirms normal editor exit, restored network/PIE settings,
the closed-editor INI, and the canonical module manifest. No assets were dirty.
The bounded captures have no errors or warnings. Two startup Pixel Streaming
target-window errors remain recorded, so this is not rendered UI acceptance.

This gate used one build, one editor lifecycle, two captures, and no package.
There were no gameplay changes or runtime retries for verifier corrections.
Cumulative Barrage counts are 13 builds, 13 owned editors plus one reused
lifecycle, and 27 captures. This proves the synthetic production GAS cancellation
route; it does not establish a player-facing cancel binding or broader
final-alpha interruption policy.

## Healthy round teardown: initial partial capture

The first healthy-reset capture, `reset-zero01.json`, preserves 925 scalar samples
across four listen PIE worlds. The production reset replaced the four authority
pawns after one Barrage projectile. Both exact old activations ended inactive,
with full 400 health, movement tags cleared, and the committed cooldown retained.
This is partial evidence; the follow-up cast did not dispatch. The corrected
captures described next supersede its failed diagnostic assumptions.

Two diagnostic assumptions blocked the run. The native observer required
`bWasCancelled=true`, but both exact end callbacks reported false. Authority log
ordering places the end inside the replacement hero's grant teardown:
`TakeFromAbilitySystem` calls `ClearAbility`, whose `OnRemoveAbility` ends the old
instance with `bWasCancelled=false`. The precise owner callback route was not traced.
The corrected diagnostic must retain exact old activation identity, explicit reset
context, detached old avatar, full health, and bounded teardown timing. It must
also prove old-spec removal, new grants, and no further old-pawn shots.

The Python observer also treated a discoverable old UObject as a live pawn.
All 16 original paths remained discoverable, so fresh-cast readiness stayed false.
Live-world actor enumeration is the required lifetime check; retain the original
object-presence and destruction-flag observations without using them as proof of
world membership. The new capture must prove every original pawn leaves its world
and exactly one replacement appears for each player on each peer.

`compile-editor14.json` records two diagnostic compile failures: conditional
`TObjectPtr` deduction and an unsupported console-command lookup. The reviewed
compatibility fixes passed `compile-editor15.json`, with no failure signatures
and an advanced runtime DLL. The original native bytes and complete tracked
patches remain in the build14/build15 source archives.

Editor14 closed normally after the bounded observer ended. Five native delegates
and the Python callback were removed; temporary settings and the canonical module
manifest were restored, with no dirty assets. See `editor14-cleanup.json`.
The two startup Pixel Streaming target-window errors remain recorded; this run
provides no rendered or physical-input proof. Cumulative overhead: 15 builds,
14 editor lifecycles, 28 runtime attempts, and zero packages in this Barrage gate.

## Verified healthy round teardown

The corrected healthy-reset gate passes 175 checks each at zero and measured
100 ms latency. `reset-zero02.json` contains 917 samples; `reset-lag01.json`
contains 893. Their `-verification01.json` records contain the complete checks.
The final verifier rejects 38 corrupted cases on each capture, accepts the benign
control, and accepts both real captures. See `reset-controls01.json` and
`reset-lag01-controls01.json`.

Both runs prove all 16 original pawn replicas leave their live-world actor
inventories. Each player has exactly one replacement on each peer, with the same
PlayerState, ASC, team, scores, and round state. The old Barrage instances end with
exact captured handles and prediction keys, unchanged full health, and cleared
movement tags. Authority ends inside the production reset call; the owner end
arrives within the recorded transport bound. Normal ends are accepted only with
the old avatar detached. The old ability specs disappear and new handles differ.

Each original activation fires one authoritative projectile and never fires again
after teardown. Its 25-second cooldown remains on the persistent ASC, with the
same authority effect identity and expiry. New specs fail the actual cooldown
check while it remains. After natural expiry, the replacement performs a normal
12-shot Barrage: 48 recoil plays across four peers, 44 replacement stops, and
finite presentation and projectile recovery. No cooldown, input, or transform
override creates that recovery.

The 100 ms capture exercises delayed old-pawn cues. Proxy plays arrive after the
authority reset returns, then their exact old montage instances stop during
end-play. No replacement plays before the fresh-cast dispatch. This is observed
delivery behavior in the captured topology, not a guarantee for every packet order.

The observer records high-resolution collection ranges and synchronous dispatch
anchors. Native-to-Python clock calibration is derived from those brackets;
elapsed uncertainty is approximately 47 microseconds at zero lag and 61
microseconds at 100 ms. Legacy UTC-log comparisons retain approximately 33.3 ms
uncertainty. Raw clocks, samples, and ambiguous boundaries remain in evidence.

Both bounded logs have no errors or unclassified warnings. Each retains 28
classified hero-initialization warning lines. The two editor-startup Pixel
Streaming target-window errors remain outside these captures and preclude any
rendered/UI claim. Independent review verifies the original editor-log byte
ranges, raw capture hashes, lifecycle identities, cooldowns, presentation, and
controls in `reset-independent-review01.json`.

`compile-editor16.json` passes with an advanced runtime DLL. The complete tracked
patch and all four untracked native files are archived in `source-snapshot-build16`
and `untracked-source-build16.json`. The final reset diagnostic hash is
`73AF7A9878555554B8586AB6FA0132F87CB55B751FD98D48826A6DEA069C203B`.
`reset-helper-freeze15-final01.json` freezes all seven observer/verifier/control
dependencies. Earlier failed compile and runtime records remain unchanged.

Editor15 closed normally. Each run removes five native delegates, its fresh-cast
ticket callbacks, and its Python callback. Four surviving mesh tick overrides are
restored; the four original Rawlins actors are confirmed absent from their worlds.
Original network/PIE settings and the canonical module manifest are restored,
with no dirty assets. See `editor15-cleanup.json`. This gate added three builds,
two editor lifecycles, and three captures, including the failed setup assumptions.
Cumulative Barrage overhead is 16 builds, 15 editor lifecycles, 30 runtime attempts,
and zero packages.

Scope is direct production `ResetRoundState` during healthy Barrage in four-world,
same-process listen PIE. It does not establish a scored round transition,
packaged transport, physical input, or second-PC LAN acceptance. Changes to reset,
grant removal, avatar ownership, Barrage end/cooldown behavior, cue routing, or the
recorded diagnostic inputs invalidate their affected evidence.

## Cooked Barrage transport gate in preparation

The next candidate is `LAN-20261011-01`. Its development-only replicated probe
uses a listen-host coordinator and three connection-owned witnesses. Every
process must acknowledge a captured baseline before one remote Rawlins performs
two normal Barrage activations separated by the real cooldown. Each cast requires
12 authoritative projectiles and 12 correctly routed montage plays per process.
Early reuse must fail, the actual cooldown must expire naturally, and every
witness must acknowledge projectile, montage, movement-tag, and observer cleanup.
The fixture does not write health, transforms, cooldowns, or input state to obtain
a pass. Zero-lag and measured 100 ms transport are separate attempts.

The new reflected probe requires both `LyraEditor` for the cooker and `LyraGame`
for the packaged executable. The gate-specific compile and package wrappers bind
both targets to the same frozen tracked patch and raw untracked source bytes.
They verify compile records, linked binaries, copied source evidence, and the
staged executable. Source-binding negative controls reject changed untracked
bytes and a changed manifest. Helper review and controls are saved under the
Barrage evidence directory. Runtime evidence remains pending; this preparation
does not promote any gameplay acceptance result.

The first editor build (`compile-editor17.json`) failed before producing an
admitted binary. The new probe used a replication-macro parameter name that
must be `OutLifetimeProps`, and three local tag containers shadowed an actor
member. A changed unity-file grouping also exposed ambiguous `ASC` helpers in
the existing relic diagnostics. The complete compiler output and grouped causes
remain in `compile17-diagnosis.json`; the retry requires corrected source and
a fresh frozen manifest. These failures are diagnostic compilation results,
not gameplay regressions or runtime evidence.

The corrected `compile-editor18.json` and `compile-game19.json` both pass against
the same 19-file source manifest. Editor18 advanced the runtime DLL. UBT changed
only the build ID in the tracked ShooterCTF module manifest; its generated bytes
and diff were archived, the exact original bytes restored, and the full source
binding rechecked before accepting that build. `compile18-binding-recovery.json`
and `package-helper-review02.json` document this recovery without another build.

Candidate `LAN-20261011-01` passed Cook/Stage/Pak with 131 staged files totaling
4,273,755,627 bytes. Its executable hash is
`42DBA0227D6DA680D83016B55A7A18A19E68440894369EBE0A8257A1C4B5A36B`,
matching the compiled game. Artifact evidence is in
`Saved/Logs/codex-packaged-LAN-20261011-01/package-result.json`.

The first four-process capture, `packaged-LAN-20261011-01-normal-zero01`, failed
before any Barrage cast. The authority timed out at the aggregate resident
spec/kit/zero-cooldown-reduction setup predicate. All four owned processes were
closed by the failure cleanup; their forced exits are recorded separately from
the setup error. The verifier correctly rejects the capture because it contains
no accepted plan or casts. No actual-data verifier controls or 100 ms run were
attempted on this failed baseline.

Source inspection identifies a likely fixture assumption: its soft kit lookup
required residency before first use, whereas production resolves that reference
during activation. The old aggregate marker cannot prove which subcondition
failed. The next diagnostic revision must distinguish those conditions and
verify the exact primary instance's configured kit after production activation,
without loading the asset in the fixture. Initial 12-shot/0.1-second/1.5-second/
25-second values are an expected gate contract until the loaded values verify.
This source change requires a fresh candidate; an unchanged retry of candidate01
is not justified. Cumulative Barrage overhead is 19 builds, 15 editor lifecycles,
31 runtime attempts, and one package.

The deferred-load revision also compiled and packaged successfully:
`compile-editor20.json`, `compile-game21.json`, and candidate
`LAN-20261011-02` (131 staged files; executable
`523ED4AF020ADED97598C66E703990BD516C69369423B0C83664B20F03FB3967`).
Its `normal-zero02` capture reaches an authority plan and owner readiness with
the configured kit explicitly nonresident and cooldown reduction zero. It then
fails before casting because the authority's unlimited forward-ray lane check
includes an opponent beyond the nominal projectile range. The owner reports
view yaw 0 degrees; authority reports 90 degrees. Both observations remain in
the raw logs. All four owned processes exited during recorded failure cleanup.
See `packaged-normal-zero02-diagnosis.json` and `packaged-normal-family01.json`.

The next fixture revision uses an explicit finite expected envelope, then verifies
actual loaded speed/offset and initialized projectile behavior. It accounts for
the lifespan timer's pending-start and terminal-frame slack by admitting only
authority frames at or below 0.2 seconds while armed and recording the observed
peak. Exceeding that bound invalidates this test's evidence; gameplay timing
is not changed. Exact health, shot, cue, cooldown, and cleanup requirements remain.
No input, view, transform, or health is changed to make the fixture pass.

The initial aim difference is not resolved by this delivery fixture. Source
inspection identifies the stationary client camera update policy as relevant,
but the capture does not establish the packet-level cause. Packaged aim
convergence remains a separate proof gap, along with physical input and final
presentation. Cumulative Barrage overhead is now 21 builds, 15 editor lifecycles,
32 runtime attempts, and two packages; neither packaged setup failure proves
normal Barrage delivery or cooldown.

Candidate `LAN-20261011-03` passed LyraEditor22, LyraGame23, and Cook/Stage/Pak
against identical frozen inputs. Its 6,920 cm lane envelope admitted the first
normal 12-shot cast. Zero03 then failed at elapsed 8.06 seconds: an authority
frame of 0.4 seconds exceeded the 0.2-second bound while the activation was
inactive and owned bullet count was zero. No second cast or complete cooldown
acceptance was reached. All four owned processes exited during failure cleanup.
The exact final bullet retirement and safe guard lifetime are under review;
the threshold remains unchanged. See `packaged-normal-zero03-diagnosis.json`,
its failed verifier result, and `packaged-normal-family02.json`. Cumulative
Barrage overhead is 23 builds, 15 editor lifecycles, 33 runtime attempts, and
three packages. No actual-data controls ran against a failed baseline.

Independent review of zero03 confirms 12 authority shots, 48 total montage
plays, matching production tuning, unchanged health, and settled recovery on
all four processes. Eleven later samples show displacement; one projectile had
already collided. The last live bullet was observed at world time 26.587131;
the first final zero count was at 26.672998. The failing idle frame occurred at
31.539364. This bounds retirement between samples; it does not record an exact
destruction callback. See `packaged-normal-zero03-independent-review01.json`
and `packaged-normal-zero03-partial-first-cast-review02.json`.

The next diagnostic revision confines the unchanged frame bound to each cast
through all-witness settled-zero acknowledgement. It also distinguishes the
first-shot RPC observation before the world clock advances from the subsequent
world tick, which can share a frame counter. Continuous health and cooldown
observation remains required during idle periods; expiry and recast still need
a complete passing capture.

Candidate `LAN-20261011-04` passed LyraEditor24, LyraGame25, and packaging.
It remains unrun: helper review identified that its separate 0.3-second wall
sampling limit would also reject the recorded cooldown-idle gap. The next
diagnostic adds numbered observations with engine frame and world delta.
A narrowly qualified settled-idle gap must retain consecutive observations,
stable identities and counters, positive cooldown, and exact paired owning
effect readbacks. Active-flight, recovery, and expiry boundaries retain their
strict limits. Health and activation callbacks remain bound; roster/team and
some effect properties are sampled and do not prove every transient mutation.
Both native and helper reviews must finish before another build. Overhead is
25 builds, 15 editor lifecycles, 33 runtime attempts, and four packages.

## Cooked normal transport: zero added latency

Candidate `LAN-20261011-05` passed LyraEditor26, LyraGame27, and packaging from
the same frozen inputs. Normal-zero04 passes 301 checks: 24 authoritative shots,
96 montage plays, 88 replacement stops, real 25-second cooldown, rejected early
reuse, natural expiry, and an ordinary second cast. All three clients exited 0;
the host's owned forced teardown is recorded separately. Independent review
accepts 132 rejected corruptions, six benign controls, and one actual capture.

The original failed audit is preserved. Three offline verifier corrections use
the latest contiguous settled sample pair before acknowledgement, GAS's signed
remaining-time query before effect removal, and the actual same-frame world
phase that starts the fire timer. The revised per-shot cadence bound is tighter
than a global allowance. No gameplay rerun or rebuild was needed for these
interpretation corrections. See `packaged-normal-zero04-verification02.json`,
`packaged-normal-zero04-controls01.json`,
`packaged-normal-zero04-offline-acceptance02.json`, and
`packaged-normal-zero04-independent-review01.json`.

Two qualified settled-idle gaps remain explicit: 0.322 seconds on the host and
0.334 seconds on client3. These are sampled-state evidence, not performance
acceptance or proof of every transient mutation. A separate stationary-aim
cross-check finds zero direction difference for all 24 shots; initial owner yaw
0 converges to 90 before both casts. Changing aim, physical input, and camera RPC
timing remain unproved. Current overhead: 27 builds, 15 editor
lifecycles, 35 runtime attempts, five packages.

## Cooked transport at 100 ms: observation cadence failed

Normal-lag01 completed both casts in all four processes, with 24 authoritative
shots and 96 montage plays. All three clients exited 0; the host's owned teardown
remains separate. The unchanged verifier passes 299 of 301 checks. Client1 and
client3 fail observation cadence, so this capture does not pass the latency gate.

Client1 has a 0.315-second gap just before activation. Client3 has four active
delivery gaps of 0.443, 0.358, 0.346, and 0.390 seconds. Consecutive observation
sequences and matching frame deltas establish slow actual frames, with no missing
log rows. The second cast has no equivalent gap. CPU, GPU, and streaming causes
are not established. Independent review agrees that the 0.3-second active,
recovery, and expiry limit must remain unchanged; the 0.2-second authority flight
bound also remains unchanged. No controls were run against this failed baseline.

Evidence: `packaged-normal-lag01-verification.json` and
`packaged-normal-lag01-cadence-diagnosis01.json`, beside the immutable runner
result and four raw logs. A separate stationary-aim cross-check also matches all
24 shots; this does not change the failed cadence verdict.

## Reduced rendering request: startup frame guard rejected

Normal-lag02-screen50 reused the same candidate after all 131 staged file hashes
and 19 frozen source paths were checked. Only two progress documents differed.
The reviewed runner requested `r.ScreenPercentage 50` and queried dynamic
resolution settings. It preserved all four rendered processes, native gameplay
checks, and timing limits. No rebuild or recook was needed.

The authority armed during a 213.102 ms startup quantum, exceeding the unchanged
200 ms guard. One deferred projectile had been allocated, but no initialized
projectile or motion record was accepted. Cleanup then cancelled the activation
and closed all four owned processes. These forced failure exits do not prove
normal client or host shutdown.

The requested rendering setup also failed its independent admission check.
Each process reported screen percentage 50, dynamic operation mode 1, and test
screen percentage 0. Mode 1 depends on user settings; it does not establish that
dynamic resolution was active. Saved settings show it disabled, but those files
do not prove effective runtime state. No GPU, CPU, or loading cause is established.

Evidence: `packaged-normal-lag02-screen50-diagnosis01.json`,
`packaged-screen50-independent-diagnosis01.json`, the runner result, and four raw
logs. The original screen50 runner, review, and parser controls remain frozen.
No gameplay-verifier controls were run against this failed baseline.

The next proposed setup change measures a bounded stable-frame window on all
four peers before the first cast. Its source and evidence schema require review
before implementation. Active flight, delivery, and expiry limits stay unchanged;
first-use performance remains a separate acceptance gap. Current overhead is
27 builds, 15 editor lifecycles, 36 runtime attempts, and five packages.

After normal transport, prioritize the current candidate's end-to-end LAN flow,
as required by `VERTICAL_SLICE_DEFINITION.md`: host/find/join, selection, full
match/results, and return/rehost. Retest the recorded travel ASC GUID concern.
Keep physical input, natural four-human play, bots filling 4v4, and second-PC
evidence explicit; structured test commands cannot substitute for those scopes.

Synthetic cooked remote-owner GAS cancellation remains optional diagnostic
coverage in `packaged-barrage-lifecycle-next-gate-plan.json`. It would need to
prove the exact cancelled authority end, no later shots, and retained cooldown.
It does not establish a player-facing manual-cancel binding or an explicit
final-alpha requirement. Select it when a concrete interruption path or defect
justifies the additional fixture and regression work.

## First-cast startup readiness: source reviewed, evidence pending

The reviewed schema is `packaged-barrage-startup-readiness-schema01.json`.
Each locally owned witness temporarily samples every world frame while preparing
the first cast. Admission requires at least 0.5 seconds of advancing wall and
world time, at least five distinct observations, frame deltas at most 0.2 seconds,
and observation gaps at most 0.3 seconds. Preparation has a 15-second deadline.

The verifier must retain every raw preparation row and prove unchanged identities,
full health, and no ability, cue, projectile, montage, or cooldown activity.
Only those idle, pre-admission rows have a separate preparation scope. The final
stable window and all post-admission rows retain the strict timing bounds,
including the transition to casting and first cue. An owned-witness heartbeat
gives the authority current evidence from all four peers without comparing
different processes' clocks. Local and authority checks remain active through
handoff; cue arrival may precede a proxy's replicated stage property.

The original witness tick interval must be restored on handoff or cleanup.
This change applies only before the first cast. The existing second-cast,
cooldown, flight, recovery, and expiry checks remain required. It does not prove
startup performance, responsiveness to the earliest possible player input,
physical presentation, or the cause of previous slow frames.

Native source review passed at CPP hash `7291A6CF…1ED16` and header
`909F7E8F…FA1B6`; the full hashes and review are in
`packaged-barrage-startup-readiness-native-review01.json`. The review fixed a
stale activation-time check: after publication, real tick observations continue,
and the authority separately checks the actual activation callback's current
phase. The callback cannot refresh its own evidence clock. Planned candidate
`LAN-20261011-06` required fresh Editor/Game compilation and cook. The completed
build and runtime result follow.

## Candidate06 readiness result — October 12

Editor28 and Game29 passed from the same frozen source, with empty failure
signals. The runtime DLL advanced, and the package passed Cook/Stage/Pak with
131 recorded artifact hashes. The reviewed helper set is frozen in
`Saved/Logs/rawlins-barrage-animation-20261009/helpers-packaged06/`.

The zero05 run failed before any cast: client2 never reached stable readiness.
Its 39 consecutive frame observations contained 38 world deltas above 200 ms;
the median was 372.535 ms. The other three peers admitted readiness. Independent
review found a valid rejection, not a stale-window or RPC defect. All four
witnesses cleaned up and restored their tick intervals. All four processes
closed through owned failure cleanup; this is not normal-exit evidence.

Evidence remains under `Saved/Logs/rawlins-barrage-animation-20261009/`:
`compile-editor28.json`, `compile-game29.json`,
`packaged-barrage-helper-freeze06.json`,
`packaged-LAN-20261011-06-normal-zero05-result.json`, and
`packaged-normal-zero05-independent-diagnosis01.json`.
Package evidence is in `Saved/Logs/codex-packaged-LAN-20261011-06/`.

This iteration added two target builds, one package, and one failed runtime
attempt. No verifier control suite ran against the failed baseline. Keep the
200 ms frame and 300 ms observation limits. Before retrying, capture bounded
CPU/frame/GPU or scheduling evidence on this unchanged artifact and identify a
specific workload correction. No performance cause is established. The separate
[current-candidate frontend regression](./CONNECTED_FRONTEND_TRAVEL.md) can
advance independently; it does not close Barrage or performance acceptance.

## Candidate06 profiling — October 13

Two bounded diagnostic runs reused the frozen candidate, with no build or cook.
Both kept four rendered 800 × 450 processes, the existing visual settings, and
the 200 ms frame and 300 ms observation limits. Host and client2 captured
CPU, frame, GPU, bookmark, and log channels. GPU and process counters provide
supporting context; neither capture establishes final performance acceptance.

The baseline `profile-zero01` failed startup readiness before any cast.
Complete main-loop frames within the measured interval averaged 520.11 ms on
the host and 451.80 ms on client2. Clipped `Sync_RenderingThread` time accounted
for 98.914% and 99.263% of their main-loop intervals. Client3 ran about 193 FPS
despite the startup `t.MaxFPS=30` query. Later Lyra settings updates can replace
that cap. The actual writer and foreground-state cause remain unproven.
GPU queue waits and analyzer warnings prevent a specific GPU-pass conclusion.

The single-variable `profile-cap30-01` comparison added a per-process
`GameUserSettings` override for `FrameRateLimit=30`. Readback from each actual
transient settings instance confirmed 30 on all four peers. However, random
spawn placement put an enemy inside the fixture's conservative projectile
envelope: the host measured −16 units of clearance, versus +46.315 in the
baseline. The existing safety check rejected this arrangement before host
readiness or any cast. This is a setup failure, not proof of a projectile hit.

Client3 produced only three approximately 33.33 ms observations. The host had
no Ready interval, and client2's Ready-to-Complete interval was only 81.049 ms.
There is no valid sustained comparison with the baseline. Do not report a
performance improvement or run the full acceptance verifier on this capture.
Keep the decoded traces for the setup diagnosis; another timing export cannot
recover the missing interval.

All owned game processes exited. The staged `GameUserSettings.ini` was backed
up, archived after the run, and restored byte for byte. An independent elevated
process inventory and SHA-256 check confirmed cleanup. The next fixture change
uses an explicit development process flag to assign the existing Dorado starts
by hero. It must validate the full four-human roster and reserved start mapping
before spawning, reject occupied or ambiguous targets without fallback, and
retain actual endpoint lane checks. Default spawn selection stays unchanged
when the flag is absent. This avoids temporary view or pawn-transform edits.
Preserve the existing lane geometry, health, cooldown, projectile, and timing
checks. Changing-aim replication remains a separate acceptance requirement.

Evidence is under `Saved/Logs/rawlins-barrage-animation-20261009/`:

- `profile-zero01-independent-review01.json` and the associated trace exports.
- `profile-cap30-01-independent-review01.json` and both saved traces.
- `profile-cap30-01-lane-review.json` for the spawn and lane diagnosis.
- `profile-cap30-runner-guard-review02.json` and
  `profile-cap30-01-terminal-verification.json` for settings and process cleanup.

This profiling work added two runtime attempts, zero builds, and zero packages.
The total gate counts are 39 runtime attempts, 29 target builds, and six packages.

## Deterministic start fixture: candidate07 zero-lag acceptance

The development-only `-BwayBarrageTransportDeterministicStarts` flag selects
existing Dorado starts for Argus, Rawlins, Alona, and Korryn. The GameMode validates
all four human players, their locked heroes, team pairings, and unique tagged
starts before spawning. Each requested slot must be clear. A failed reservation
stops the restart before the engine can substitute an old start or world origin.
The default path is unchanged when the flag is absent or in Shipping.

The fixture creates no replacement starts and changes no pawn transform or
camera state after spawning. Existing pawn initialization and hero setup still
run. Reservation, selection, and actual pawn/control/view markers retain the
observed geometry; the Barrage probe's safety checks remain unchanged.

Production warmup and the Playing transition each spawn all four pawns. The
captured `deterministic-zero01` run passed all 153 setup checks: both batches,
each request's complete unique reservation map, selected and actual positions,
final pawn paths bound to the Barrage roster, and actual transient settings
readbacks of `FrameRateLimit=30` on all four processes. Immediate authority view
readback remains diagnostic. This proves deterministic fixture setup; default
random-spawn behavior and sustained performance remain outside that scope.
The supplemental checker requires a separate pass from the unchanged normal
transport verifier on the same capture before it can report overall acceptance.
The later deterministic-zero02 capture supplies that passing baseline and
its negative controls; the failed zero01 evidence remains unchanged.

Candidate `LAN-20261013-07` packages this source change. Editor30 failed
with C3535 because this engine returns `TObjectPtr<APawn>` from `GetPawn()`;
`auto*` could not deduce the type. The readback now declares `APawn*` explicitly.
Editor31 and Game32 passed with exit 0 and empty `failureSignals` in 9.11 and
40.32 seconds, respectively. Cook/Stage/Pak passed with 131 hashed artifacts.
The executable SHA-256 is
`5B98A805A493B83DF7593EB71F0FFF5AE31055A31806AA5B136E015C4C14899F`.
The original candidate06 artifact cannot support the new process flag.

### Deterministic-zero01 terminal result

The four-rendered-process, zero-lag run failed the unchanged authority flight
frame limit during the second cast's final projectile flight. Host log lines
4143–4144 record `delta=0.267262`, `limit=0.200000`, `cycle=2`, `armed=1`, and
`ownedBullets=1`, followed by `authority-world-frame-outside-lane-bound`.
This is a valid safety rejection after successful setup; it does not establish
the cause of the hitch.

Preserved partial evidence includes 24 initialized authority shots, 96 montage
plays and 88 replacement stops across the four peers. The first cast settled
on all peers and retired its guard with zero projectiles. Early reuse was
rejected; the real 25-second cooldown expired at 25.089897 world seconds before
the second ordinary activation. Both casts naturally ended, but the second
flight did not finish within the required bound. No full normal-verifier pass
or acceptance controls were claimed for this failed capture.

All four native cleanup records removed callbacks and restored tick state.
One projectile still existed at the host cleanup marker. The runner then closed
all four owned processes as failure teardown; these were not normal client
gameplay exits. The settings guard restored the original absence of the staged
`GameUserSettings.ini`, with no remaining processes or cleanup error.

Evidence under `Saved/Logs/rawlins-barrage-animation-20261009/`:

- `compile-editor31.json`, `compile-game32.json`, and their input bindings.
- `packaged-LAN-20261013-07-deterministic-zero01-result.json` and its four logs.
- `deterministic-zero01-starts-extraction01.json` and
  `deterministic-zero01-independent-diagnosis.json`.
- `LAN-20261013-07-deterministic-zero01-settings-guard.json`.
- `deterministic-zero01-render-workload-review01.json`: engine-emitted 800×450
  resolution on all peers; startup VSM queue warnings and new PSOs, but none
  in the five seconds before the failure. These observations do not identify
  the hitch's critical path.

The package result and artifact hashes are under
`Saved/Logs/codex-packaged-LAN-20261013-07/`.

### Host-profile01: completed diagnostic capture

The follow-up reuses candidate07, deterministic starts, the loaded 30 FPS cap,
and four rendered peers, adding host CPU/frame/GPU/log tracing. Session `59920`
finished successfully. All four native probes reported success, all three clients
exited normally with code 0, and all four game logs have zero errors or critical
lines. Owned host teardown completed, the process inventory is empty, and the
original settings-file absence was restored. The 137,954,545-byte trace is retained.

The host recorded 68 armed frame samples and a 124.927 ms peak across both casts.
The second cast's own peak was 105.290 ms. The 153 supplemental setup checks
passed again; overall acceptance remains false without a strict normal-run pass.
The trace does not reproduce or explain the earlier 267.262 ms hitch.

Inventory and timing exports completed with exit 0. The saved second-cast window
contains 43 complete engine frames with a mean of 90.418 ms and maximum of
105.289 ms. Game-thread rendering synchronization covers 95.578% of the window;
world updates take at most 4.884 ms.

Worker events establish the visibility dependency: `OcclusionCullPipe` waits in
`RHIGetRenderQueryResult_GPU_Wait`, overlapping the render thread's visibility
wait. A separate RHI-thread `SyncPoint_Wait` belongs to `EndDrawingViewport` and
GPU frame completion. The measured Present calls are short; these waits must not
be added across concurrent threads or attributed to one GPU pass.

The first worker export exited 3 before producing a log or CSV because engine-side
writes were restricted. A single retry with the permissions used by the earlier
exports succeeded. This is an export setup failure, not gameplay evidence.
GPU ordering, negative-duration, pending-event, and memory-tag analysis diagnostics
remain preserved. They limit GPU and allocation conclusions.

Evidence: `host-profile01-independent-timing-review.json` binds the trace,
inventory, timing files, and both worker-export attempts. The native and cleanup
records remain `host-profile01-native-summary.json` and
`host-profile01-terminal-verification.json`.

### HZB comparison: no supported optimization

`hzb-profile01` changes only the four processes' occlusion mode to
`r.HZBOcclusion=1`, with actual Console readbacks on every peer. It retains the
same candidate, deterministic starts, cap, tracing, and safety limits. All four
native probes pass; three clients exit normally, and settings and processes are
cleaned up.

Hardware-query worker waits disappear, but blocking moves into
`MapOcclusionResults`. Its longest measured call includes RHI command submission
and flush waiting, then a sync-point wait. The engine's HZB readback path can block
there. Game-thread rendering synchronization remains 95.602% of the window.
The second-flight native mean changes from 90.419 to 90.544 ms, and its peak from
105.290 to 106.511 ms. These 35-sample populations are separate from the complete
engine-frame measurements in the padded trace window.

This comparison does not justify adopting HZB. It neither reproduces nor explains
the earlier 267.262 ms hitch. No production renderer setting changed, and no
visual-quality or sustained-performance pass is claimed. See
`hzb-profile01-independent-comparison.json`, `hzb-profile01-capture-contract.json`,
and `hzb-profile01-terminal-verification.json`.

### Deterministic-zero02: normal transport accepted within scope

One explicitly declared, uninstrumented reproducibility run returned to the
default renderer on candidate07. It passes all 352 normal transport checks and
153 deterministic setup checks. The observed result includes 24 authority shots,
96 montage plays, 88 replacement stops, early cooldown rejection, natural
25-second expiry, and ordinary reuse. Both flight guards retire after all four
witnesses settle with zero projectiles. The 76 armed samples peak at 117.973 ms,
below the unchanged 200 ms limit.

The corrected offline suite rejects 169 negative controls and accepts seven
benign controls. All five supplemental setup controls pass. The original
`deterministic-zero02-controls01.json` remains preserved: its repeated-frame
mutation targeted a discarded world-delta reset, not an accepted readiness
window. The corrected control selects consecutive counted samples in the
admitted generation and still requires the unchanged verifier to reject them.
Neither the normal verifier nor gameplay was changed for that correction.

`deterministic-zero02-terminal-scope-review01.json` independently binds the
352-check result, 153-check setup result, both control suites, exact executable,
raw logs, and allowed documentation-only source differences. All three clients
exit normally with code 0. Host teardown is recorded separately; callbacks, tick
state, owned processes, and the original settings-file absence are restored.
The passing run does not resolve the intermittent 267.262 ms failure.

Primary evidence is `deterministic-zero02-verification01.json`,
`deterministic-zero02-starts-verification01.json`,
`deterministic-zero02-controls02.json`, and
`deterministic-zero02-startup-control-fix01-review.json`.

### Deterministic-lag01: obsolete controller restart rejected

The 100 ms run fails during Warmup before normal Barrage acceptance. Three
obsolete seamless-travel controllers reach the deterministic restart path with
`request=-1`, producing `deterministic-start-four-player-roster-required`.
This is not an incomplete valid roster: the current host and all four valid
reservations appear in the same frame, 455. The setup extractor passes 152 of
153 checks; only `no_deterministic_start_failure` fails. The obsolete controllers are awaiting
connection swap acknowledgement and no longer hold their original PlayerStates.

The proposed production correction checks `PendingSwapConnection` at the start
of `RestartPlayer`, cancels the obsolete controller's queued restart, and returns
before hero preparation or start selection. This matches the engine's existing
restart exclusion. Valid controllers still require the complete roster and exact
spawn checks. Candidate07 does not contain this correction.

Preserve `packaged-LAN-20261013-07-deterministic-lag01-result.json`, its four logs,
`deterministic-lag01-starts-extraction01.json`, and the settings guard. All owned
processes closed and settings were restored; failure teardown is not a normal
client-exit pass. No latency acceptance or controls pass is claimed for this run.

### Candidate08 preparation and overhead

`LAN-20261013-08` is planned and unbuilt. Freeze the restart guard, documentation,
and complete source snapshot, then run Editor33 and Game34 before a fresh
Cook/Stage/Pak. The new candidate must pass normal zero/100 ms transport,
deterministic setup, and applicable controls with the existing 200 ms flight and
300 ms observation limits. Candidate07 retains its exact historical zero-lag
scope; it cannot validate the new restart guard.

Current cumulative Barrage overhead is 32 target build attempts, seven packages,
44 runtime attempts including four profiling captures, and 15 editor lifecycles.
No new build, package, or runtime is included in the planned candidate08 work.

### Candidate08: build passed, final observation window incomplete

Editor33 and Game34 passed in 10.04 and 22.08 seconds with empty failure signals.
Cook/Stage/Pak passed with 131 hashed artifacts. The executable hash is
`885323DFDD18AE1BC1E095FA3C4708A8993FC25F41D8697F6AF88302AA48561A`.
The independent source review confirms the engine-aligned obsolete-controller
guard applies in Shipping; only its fixture diagnostic is development-only.

The zero-lag run completed both native casts and cleanup. Three clients exited
normally with code 0, the owned host exited, and settings were restored.
All 153 deterministic setup checks passed. The unchanged normal verifier passed
351 of 352 checks, failing only `client1_cycle2_settled_movement_release_persists`.
This is not an accepted transport run.

The owner recorded a clear movement state at line 2402, then Report4 at line 2404.
Cleanup and Report5 followed 40.673 ms later, before another observer sample.
The verifier requires a real clear-state sample between those reports, so its
observation set was empty. No recorded state demonstrates a lingering movement
tag. Preserve this proof gap; do not weaken the verifier or accept native success
alone. Candidate09 will make terminal cleanup wait for real settled samples.

Evidence: `candidate08-zero01-verification01.json`,
`candidate08-zero01-starts-extraction01.json`,
`candidate08-obsolete-controller-guard-independent-review.json`, and
`packaged-LAN-20261013-08-deterministic-zero01-result.json` in the Barrage evidence
directory. Current totals are 34 builds, eight packages, 45 runtime attempts
including four profiling captures, and 15 editor lifecycles. The earlier
267.262 ms performance failure and the separate 100 ms acceptance gap remain open.

### Candidate09: observed terminal cleanup and zero-lag transport pass

Editor35 and Game36 passed in 11.46 and 23.91 seconds with empty failure signals.
Cook/Stage/Pak passed with 131 hashed artifacts. The executable SHA256 is
`6534E8F9390C25A1A9E67BCFB88CCBB1D473626B30540CDC148EA02F363A689A`.
`source-snapshot-build35.json`, `source-manifest-build35.json`, and
`source-inputs-build35/` preserve the exact compiled inputs. Candidate09 contains
the obsolete-controller restart guard and the terminal observation correction.

The development fixture now waits for two ordinary observer samples after
Report4, on advancing engine frames and world times, before final cleanup.
It retains a one-second settlement timeout and the existing observer cadence.
This fixes candidate08's unobserved 40.673 ms terminal window without altering
gameplay, cooldown duration, or acceptance timing limits.

The zero-lag capture passes the unchanged normal verifier's 352 checks,
153 deterministic setup checks, and 97 supplemental terminal-sample checks.
Controls reject 169 normal corruptions, five setup corruptions, and 19
terminal-sample corruptions; six applicable benign normal controls pass.
All eight terminal markers bind to actual clear-state observations: no montage,
slot contribution, movement tag, or owned projectile remains. The owner and
authority ability states are inactive. The supplemental helper checks these
state values as well as sample identity, order, and frame advancement.

The four peers record 24 shots and 96 presentation plays. Three clients exit
normally with code 0; owned host teardown is recorded separately. Settings
return to their original absent-file state, and no owned process remains.
Zero lag triggers no obsolete-controller skip, so that branch is not covered
by this successful capture. This pass does not fix the earlier 267.262 ms hitch.

Evidence in `Saved/Logs/rawlins-barrage-animation-20261009/`:

- `candidate09-zero01-verification01.json`
- `candidate09-zero01-starts-verification01.json`
- `candidate09-zero01-controls01.json`
- `candidate09-zero01-cleanup-samples02.json`
- `candidate09-zero01-skipped-controllers01.json`
- `candidate09-zero01-independent-review.json`
- `packaged-LAN-20261013-09-deterministic-zero01-result.json`

### Candidate09 at 100 ms: rejected capture and separate diagnoses

The 100 ms run completes the native fixture, normal client exits, owned host
teardown, and settings restoration. All 153 setup checks and 97 terminal-sample
checks pass individually. Three obsolete controllers emit the expected skip
markers. The combined gate remains rejected: the original normal verifier
passes 348 of 352 checks. A successful native fixture or supplemental subset
does not override that result.

Three failures concern one owner cooldown observation. At observation 314,
the exact stable effect is absent and the ability is ready 44.305 ms before
the locally reconstructed expiry. The matching authority effect records a
natural removal after 25.113907 seconds of the loaded 25-second duration.
Definition, server start, prediction key, and duration agree. The next owner
activation occurs 370.926 ms after its reconstructed local expiry.

Engine source permits replicated removal while the client's estimated remaining
duration is positive. The client reconstructs its local effect start from an
estimated server-clock offset. Consequently, the original predicate is not a
reliable authority-expiry model. This capture does not demonstrate a gameplay
refund, but it also lacks exact event-time evidence for the client's removal.
Queued log-prefix UTC timestamps cannot establish cross-process event order.
Preserve the original failed verifier result; do not add a blanket tolerance or
claim the source-grounded replication explanation as a passed assertion.

The fourth failure is independent and real. Client2 observations 356 and 357
span 653.228 ms during the second flight, exceeding the unchanged 300 ms bound.
Only one engine frame advances, with world delta clamped to 0.4 seconds. A
projectile and active montage are present before the gap, so the settled-idle
exception does not apply. Nearby UTC log ranges show the other three processes
advancing. Because those prefixes can be queued, they corroborate continued
progress without proving exact cross-process event overlap. Client2's elapsed
and frame counters establish its stalled frame; they do not identify its cause.
The earlier host trace's render wait cannot establish the cause of this event.

Retain `candidate09-lag01-verification01.json`,
`candidate09-lag01-starts-extraction01.json`,
`candidate09-lag01-cleanup-samples01.json`,
`candidate09-lag01-skipped-controllers01.json`,
`candidate09-lag01-cooldown-independent-diagnosis.json`,
`candidate09-lag01-observation-gap-diagnosis01.json`, and
`packaged-LAN-20261013-09-deterministic-lag01-result.json`.
No full corruption suite is claimed for this rejected baseline.

`candidate09-lag01-cooldown-offline-correction-feasibility.json` records the
source-backed logging limitation and the proposed exact event-clock contract.
The normal verifier and its controls remain byte-for-byte unchanged. A future
diagnostic should capture same-PC QPC counters at authority expiry and exact
owner replicated-effect removal, plus state/effect sampling brackets and
frequency metadata. It must bind the effect identity and retain strict duration,
no-early-reuse, and interior-state continuity checks. This proposal does not
establish a clock contract for different PCs.

`candidate09-lag01-independent-gap-review02.json` records one client2-only
CPU/frame/GPU trace as the next discriminating experiment. Reuse the frozen
candidate, original four rendered processes, 100 ms latency, 30 FPS cap, and
default renderer. Inspect the complete frame and its thread dependencies. If the
gap does not reproduce under tracing, record that limitation and keep the
intermittent defect open; do not repeat unchanged runs or claim a fix.

The contract records 36 target build attempts, nine packages, 48 runtime
attempts including four profiling captures, and 15 editor lifecycles.
Evidence-only diagnoses do not add a build, package, or runtime attempt.

### Client2-only profiling and candidate10 event-clock instrumentation

One client2-only trace reused candidate09 at 100 ms with the same four rendered
processes, cap30, resolution, renderer, and deadlines. All four native fixtures
completed, but the runner recorded client2 exit 777003 after it requested status
0 and closed its log. Engine source identifies 777003 as termination from an
exception in the in-process crash-reporting thread. No exception stack or
address was recovered from candidate crash records or the bounded Windows event
query. The cause and any relationship to tracing remain unknown. All owned
processes exited, and settings were restored. This is a failed exit scope.

The trace and both export stages completed. Exact raw/trace payload matching
binds 49 second-cast State samples and adjacent observations. The largest native
gap is 104.046 ms, so the earlier 653.228 ms stall did not recur. Across 62
complete GameThread frames, mean time is 80.259 ms and maximum is 105.996 ms.
Rendering synchronization covers 4810.339 ms of the selected 5013.947 ms interval.
These measurements describe the captured interval; they do not identify the
cause of an absent hitch. Thread IDs are checked directly because aggregate
timer exports can include other tracks. Overlapping worker times are not added
into a frame-time or dependency claim.

All 207 analyzer warning/error lines remain recorded: 100 invalid-memory-tag,
102 GPU ordering/duration, and five other diagnostics. GPU timing is not treated
as pure execution time. Preserve the trace, `client2-profile01-intervals.json`,
`client2-profile01-timing-summary01.json`,
`client2-profile01-independent-timing-review02.json`,
`client2-profile01-exit777003-diagnosis01.json`, the terminal runner result, and
settings guard. No unchanged profile retry or performance fix is justified.

The independently reviewed candidate10 fixture now adds `clockSchema=1` with
same-PC Windows QPC frequency/process metadata. Activation, authority expiry,
and exact owner cooldown-removal callbacks record counters before log queuing.
Existing State and Effect rows bracket their GAS reads with counters. Effect
origin distinguishes replicated-owner objects from predicted objects; it does
not assert packet delivery or a specific removal execution path. Authority
expiry additionally requires exact effect context and age at least the loaded
duration. The new owner delegate is removed during existing cleanup, increasing
the expected owner cleanup count from seven to eight. No gameplay duration,
RPC, reflected layout, timer, or shipping behavior changes.

`Saved/new-cooldown-clock01/design02.json`, `cooldown-clock02.patch`, and
`independent-native-review02.json` preserve the design and review. The applied
source SHA256 is `2CA1D2B61AAB384138BA8FC0F5F88EE497F55322F5C9794855EA11187AAADE2E`.
The isolated patch passes 52 log-format checks and applicability review.
Editor37, Game38, and candidate10 Cook/Stage/Pak subsequently passed, with
131 artifacts and executable SHA256
`143E053423505ED85196089B34DE6F6654DF2ACAD5169567404E82735D03761E`.
`source-manifest-build37.json` and `source-inputs-build37/` freeze the inputs.
Cross-machine clock synchronization remains outside this same-PC diagnostic.

Candidate10 `deterministic-zero01` completed the native fixture, all three
clients exited normally with code zero, owned processes closed, and settings
were restored. The full revised verifier still rejects the run: 375 of 381
checks pass in `candidate10-zero01-verification01.json`. The original failure
record remains immutable.

All 29 new event-clock checks pass. The independent 44-check review in
`candidate10-zero01-independent-qpc-review01.json` binds four owned process IDs
and 10 MHz QPC clocks. Exact authority expiry occurred at effect age
25.187262 seconds; matching replicated-origin owner removal followed by
342,084 ticks (34.2084 ms), and actual reuse followed removal by 146.6528 ms.
Predicted-object removals cannot satisfy that expiry proof. Cleanup delegate
counts are 9/8/5/5, with eight real final settled samples. These scoped results
do not override the six normal-verifier failures.

Two failures expose a fixture observation gap: the host's first zero-bullet
settled sample during cast two immediately preceded Report4, leaving only one
pre-ack sample. Later cleanup samples cannot repair evidence at that earlier
boundary. Candidate11's development fixture waits for two ordinary samples,
with advancing frame/world clocks and unchanged settlement predicates, before
either cast acknowledgement. It resets the sample count if settlement is lost
and has a one-second observation deadline from first settlement. No gameplay
timing, replicated layout, RPC, quality setting, or acceptance limit changes.

The other four failures concern existing one-cadence bounds on signed overdue
cooldown samples. Host values reached -0.187262 seconds against a 0.161061-second
bound; the owner reached -0.255894 against 0.193523. The subsequent source review
and corrected event-based predicates are documented below. The
authority's existing maximum expiry lateness remains 250 ms, and no early
reuse or missing effect may pass. `clock-schema1-verifier-independent-review01.json`
records independent helper review; legacy candidate09 retains all 352 original
checks plus 169 negative and six benign controls. New clock controls were then
verified against candidate11's full passing real baseline.

Cumulative counts before candidate11 builds are 38 target builds, 10 packages,
50 runtime attempts including five profiling captures, and 15 editor lifecycles.

### Candidate11: normal transport accepted at zero and 100 ms

Editor39 and Game40 passed with empty failure signals; Cook/Stage/Pak produced
131 hashed artifacts. `source-manifest-build39.json` and `source-inputs-build39/`
freeze all 21 dirty inputs. The executable SHA256 is
`54AA9FA99B53798BB00B0BC85E769465B6B2AE86A9064E27760691536B34F9F6`.
The reviewed native pre-ack change is recorded in
`cast-settled-preack-native-independent-review01.json`.

Both `deterministic-zero01` and `deterministic-lag01` pass 392 normal transport,
153 startup/spawn, and 97 cleanup checks. Each captures 24 authority shots,
96 peer plays, two actual pre-ack samples for each cast on all four peers,
and two actual final cleanup samples per peer. All three clients in each run
actually exit 0; owned processes close and exact settings are restored.
The 100 ms capture also passes 19 obsolete-controller checks covering three
skips; zero lag has no such branch coverage. Raw independent reviews pass
66 and 69 checks respectively.

| Measurement | Zero added latency | 100 ms added latency |
|---|---:|---:|
| Authority effect age at natural expiry | 25.156406 s | 25.081676 s |
| Owner removal after authority expiry | 64.1606 ms | 134.4534 ms |
| Measured removal-delay allowance | 230.4807 ms | 449.3780 ms |
| Armed-flight frame peak | 104.424 ms | 67.289 ms |

The earlier one-observer-cadence expiry bound was not an engine invariant.
In candidate10, a timer queued before the world tick inherited a 117.108 ms
start-phase offset; the first timer tick crossing its deadline added 70.153 ms.
That explains the authority's 187.262 ms expiry lateness while retaining the
existing 250 ms ceiling. Engine source ranges and exact sample correspondence
are recorded in `candidate10-signed-overdue-invariant-independent-review01.json`.

The final verifier binds its clock schema to archived native source, checks
exact effect identity and callbacks, rejects early authority expiry/reuse, and
requires signed State/Effect reads to precede their matching removal callbacks.
Owner-local remaining time may be positive when replicated removal arrives;
candidate11 zero lag observes +0.019615 seconds at that event. Readiness must
follow the actual event, and signed arithmetic, tags, and effect presence must
remain consistent throughout.

The finite removal-delay policy is `F + L + (H when L > 0) + O`: `F` is the
measured authority callback-to-next-read interval, `L` is configured outgoing
latency, and `H`/`O` are eligible protected host/owner read intervals, each still
bounded by 300 ms. `F` must also fit `H`. The 100 ms values are 55.7123 + 100 +
119.5480 + 174.1177 = 449.3780 ms. This is a bounded scheduling acceptance policy,
not proof of a packet's route, an OS scheduling guarantee, or cross-PC clock
synchronization. Flight frames retain their 200 ms ceiling.

Zero-lag controls reject 220 corruptions and accept six benign variations;
five startup and 19 cleanup controls also pass. All 51 new clock controls pass.
`candidate11-zero01-normal-controls-joined04.json` preserves 219 negative and six
benign results from suite03, then replaces exactly one reporting mismatch with
a focused proof: the unchanged verifier's actual
`client1_bounded_observation_cadence` check returns false before a later
fail-closed IndexError. The original mutation and required check are retained,
and tracing is restored. This is a reviewed composite result, not a claim that
the entire suite was rerun. The original failed suite remains on disk.

At 100 ms, `candidate11-lag01-qpc-budget-controls01.json` rejects two additional
cap-plus-one-tick mutations. One shifts the entire owner's clock consistently,
preserving its local intervals and ordering, and fails only the service-budget
check. Legacy candidate09 still passes all 352 original checks and its 169
negative/six benign controls. The normal verifier is frozen at SHA256
`CDE0D817DF0F381619B570B5CCB5013C0263C97801A3E0B76F45A3D40A9906C1`;
`clock-schema1-helper-freeze-final04.json` preserves exact helper bytes and diffs.

Primary records use the `candidate11-{zero01,lag01}-` prefixes:
`verification02.json` for zero lag, `verification01.json` for 100 ms,
`starts-verification01.json`, `cleanup-verification01.json`,
`skipped-verification01.json`, and `independent-raw-review01.json`.
`clock-schema1-offline-acceptance04.json` links the zero-lag control history.
`candidate11-normal-transport-independent-closure01.json` closes the combined
zero/100 ms normal-transport scope after checking the actual artifacts, logs,
exits, settings restoration, and focused latency controls.
Two mutation-setup failures and a mixed self-hash record are preserved; the
final legacy run verifies the same script hash before and after execution.
For future control changes, construct all mutations before the expensive suite,
freeze scripts while any suite is running, and rerun only an affected case when
unchanged results can be joined with explicit provenance.

This candidate adds two target builds, one package, and two runtime captures
after candidate10. Cumulative counts are 40 builds, 11 packages, 52 runtime
attempts including five profiling captures, and 15 editor lifecycles. No new
editor lifecycle or quality-setting reduction was needed. The earlier
267.262/653.228 ms stalls and exit777003 remain historical failures without a
proved cause or fix; these passing captures do not establish sustained
performance or clean-device acceptance.

## Remaining evidence

Normal stationary Barrage transport now passes at zero and 100 ms on one PC.
Complete cooked changing-aim acceptance using captured owner aim,
authority shot directions, and replicated peer presentation, followed by cooked
interruption/reset. Preserve the passed scopes and strict timing limits; do not
repeat profiling without a changed hypothesis for the intermittent stalls.
`cooked-changing-aim-next-gate-source-map01.json` maps the next source decision:
the candidate11 cooked probe does not change owner aim, and the existing animation
movement command needs authority in the same process. Production shots use
the player camera view. The initial movement-only camera-send inference is
superseded by `cooked-changing-aim-camera-source-audit02.json`: ordinary
stationary movement packets can request a camera update, and changed control
rotation affects their send cadence. Camera send eligibility still depends on
the loaded timing policy and cache comparison. This is a test lead, not a
demonstrated defect. The selected test adds one measured 12-degree owner yaw
input during the first real cast, captures its consumption and cached view,
and requires at least two later first-cast shots to match the changed view.
Record actual delay without assuming a camera RPC deadline from packet lag
alone. Preserve the normal shot, cooldown, lane, health, timing, and cleanup
checks. Do not force camera RPCs or server view state; new instrumentation
requires a new build/package and does not prove physical input.
Natural full-match LAN flow, exact
results statistics, physical input, final appearance, bone proportions,
second-PC LAN, slide case2 policy, and final-alpha parity remain separate
requirements.

## Candidate12: changing aim observed; normal frame gate failed

Candidate `LAN-20261013-12` is frozen to `source-manifest-build41.json` and
executable SHA256 `CABD91104797EF6D4870614DE3E3E071130C553C3AA27B42FC13855DCF49D429`.
LyraEditor41 passed in 15.18 seconds and LyraGame42 in 28.72 seconds, with empty
failure signals, unchanged source bindings, and an updated editor DLL.
Cook/Stage/Pak passed with 131 hashed artifacts. This gate used two builds,
one package, and one runtime attempt; cumulative counts are 42/12/53.

The opt-in development fixture queues one ordinary owner yaw input without
forcing a camera update, control rotation, RPC, or pawn transform. Its native
SHA256 is `8DAF55154096A7AB39B8A99F38EDAA28845E2A4B39919D47F3FCE3DC4DA4C36E`.
The existing safety/timing functions remain unchanged. The normal verifier
only registers this native hash; its new SHA256 is
`4622EBD5C42E6CE22B0BDB8B72AF3F380E71D3ED10981E6E8C1C865383FD14ED`.
The independently reviewed aim verifier is
`C345F5BAD64FB2617E0F5B33D23A5742ECD90D928AB8E0DE2B08B3D97E8BDDD0`.
Prototype, unchanged-function checks, and review records are under
`Saved/new-barrage-changing-aim01/`.

Zero-lag capture `packaged-LAN-20261013-12-changing-aim-zero01-result.json`
**failed overall**. Its 69 aim-specific checks and 27 independent raw-evidence
checks establish partial correspondence:

- Owner yaw changed from 90 to 102 degrees. Input was consumed after
  82.5998 ms; the first sampled camera plateau appeared after 164.8500 ms.
- Authority shots 1–3 preceded input at 90 degrees; shots 5–12 read
  102.002563 degrees, consistent with 16-bit rotation quantization. Shots 7–12
  clearly followed two owner plateau samples. All twelve second-cast shots
  held the changed direction. The first clearly later post-initialization
  readback was 334.6172 ms after input; this is not an exact spawn/RPC latency.
- All 68 host and 65 owner samples show zero observed 3D motion and movement
  input. These synthetic observations do not prove physical input or appearance.

The host then recorded a **299.810 ms** frame during the second cast, exceeding
the unchanged 200 ms bound while one owned projectile remained and the flight
guard was armed. It is not eligible for an idle exemption. Client2 recorded an
overlapping 302.072 ms native frame and 302.4057 ms QPC sample gap; the overlap
is 264.8285 ms. This supports a shared stall window, without identifying an OS,
GPU, rendering, or gameplay cause. Nearby host logs add no nonfixture warning
or error that explains it. The missing final host report is a consequence of
the native failure, not its cause.

All four processes required owned failure teardown (forced kill, exit -1),
and exact settings restoration passed. These are not normal client exits.
No 100 ms acceptance run or corruption-control suite was attempted because
the combined normal-plus-aim baseline failed. The 27 prepared aim controls
remain pending.

Evidence in `Saved/Logs/rawlins-barrage-animation-20261009/`:

- `candidate12-changing-aim-zero01-failure-summary01.json`
- `candidate12-changing-aim-zero01-normal-verification01.json`
- `candidate12-changing-aim-zero01-aim-verification01.json`
- `candidate12-changing-aim-zero01-independent-partial-review01.json`
- `candidate12-changing-aim-zero01-independent-frame-diagnosis01.json`

Next prepare one bounded host CPU/frame trace with the other three processes
unprofiled, after proving a safe trace flush and owned cleanup. This changes
the observation point from the earlier client2 profile. Reuse the source-bound
candidate with its reviewed documentation-only difference; no recook or
unchanged retry is justified. Preserve the 200/300 ms limits and require a
complete zero-lag normal-plus-aim pass before controls and 100 ms acceptance.

Healthy direct round teardown now passes within the recorded same-process scope.
The stats sheet does not establish a manual cancel binding or grounded-target
immunity. Confirm reference behavior before changing those gameplay policies.

Candidate `LAN-20261009-06` remains frozen to its older primary-delivery inputs and
cannot prove these Barrage changes.
