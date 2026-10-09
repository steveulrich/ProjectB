# Rawlins Blazing Barrage acceptance

October 9, 2026. The targeted fix restores all 12 shots and per-shot body-action
presentation across four listen-PIE peers at zero and measured 100 ms lag. Two earlier
baselines reproduced 11 shots and no action presentation. The fixed normal-channel
scope passes; broader Barrage behavior and final-alpha parity remain open.

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

Damage, cooldown, shot interval, movement tags, and the existing health-interruption
rule are unchanged. The health rule compares current health with health at activation;
healing followed by smaller damage remains a separate reproduction hypothesis.

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

## Remaining evidence

The reviewed development command
`bway.Test.BarrageAnimationHealth <id> <heal-damage|damage-only|heal-only>` prepares
the exact remote owner through production damage/heal effects, activates Barrage
once, records health changes after the first authoritative bullet, and restores
health after inactivity. It preserves the real cooldown, begins cleanup at eight
seconds, and has a ten-second hard bound. The observer allows twelve seconds.
`compile-editor02.json` passes LyraEditor in 8.28 seconds with exit 0 and empty
`failureSignals`; `editor-dll-after-compile02.json` verifies the updated diagnostic
binary. These three health cases have not run, and the production health rule is
unchanged. Require explicit health restoration and owner convergence evidence.

Verify movement lock while aim changes, damage/juggle, cancellation, healing followed
by damage, death/respawn, and cooked transport. The stationary normal-channel runs
observe movement tags and poses; they do not submit movement/aim input or establish
the damage-interruption rule. Physical input, final appearance, bone proportions,
second-PC LAN, and the complete multiplayer match flow remain separate requirements.
