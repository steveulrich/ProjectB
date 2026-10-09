# Rawlins primary action animation acceptance

October 9, 2026. Rawlins's primary now passes normal, moving, rapid, and death/respawn
presentation checks on four same-process PIE peers. Candidate06 also passes remote-owner
two-shot delivery across four packaged processes. Both project targets compile successfully.
Physical input, final appearance, and expanded packaged lifecycle checks remain unverified.

This gate continues the replaceable placeholder presentation required by
`VERTICAL_SLICE_DEFINITION.md`. It preserves projectile authority, damage, shot timing,
movement, and the authority-controlled lifetime of the predicted burst.

## Expected result

Three separated remote-owner requests produce three accepted bursts and six authoritative
shots at the loaded kit interval. Each shot produces one evaluated action on its owner and
observing peers. Preserve linked locomotion layers and eventual ability inactivity.

Two shots are expected within each activation; they are not duplicate input. Moving bursts,
rapid bursts with measured latency, death/respawn, and packaged delivery use separate captures.
Cancellation before the second shot, physical input, and final appearance remain open.

## Effective configuration

Evidence is under `Saved/Logs/rawlins-primary-animation-20261009/`.
`class-defaults01.json` records the effective ability:
`/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_PrimaryAttack`.
It is LocalPredicted and InstancedPerActor, with empty ActivationOwnedTags. Its loaded kit
sets `PrimaryShotCount` to 2 and `PrimaryShotInterval` to approximately 0.12 seconds.
`PrimaryProjectilePresentation` is null.

`ability-graph-summary01.json` records two unconnected events in `EventGraph`:
ActivateAbility and OnEndAbility. Neither the effective Blueprint graph nor its loaded
projectile presentation supplies an action animation.

The predicting client must remain active until authority completes the burst. An earlier
immediate client `EndAbility` cancelled the authority timer before shot two. The presentation
change preserves that repair and the existing cancellation cleanup. For more information,
see [Mixed-roster primary gate and Rawlins burst repair](./LIFE_LEECH_ACCEPTANCE.md#mixed-roster-primary-gate-and-rawlins-burst-repair).

## Missing action baseline

`baseline01-verification.json` passes all 36 reproduction checks:
`validBaseline: true`, `missingActionReproduced: true`, and `actionPass: false`.
Remote owner 257 activated its actual granted primary three times in four Dorado PIE worlds.
Each request began after the previous burst became inactive.

The authority recorded three activations and six shots, grouped as two shots per burst.
Observed intervals were 0.132, 0.126, and 0.129 seconds, consistent with the loaded timer interval
and frame scheduling. Every peer observed six distinct owner-filtered bullet actors. This
corroborates spawning rather than treating input requests or generic damage records as shots.

Each peer supplied 129 samples. All samples had no montage, firing state, melee state, or
weighted UpperBody, FullBody, or hip-fire action layer. The linked locomotion layer remained
present, and the stationary pawn remained alive without actor displacement. Recorded hand
and foot movement is idle motion; it does not establish an attack pose.

Authority and owner each entered and left three active bursts. Immediate owner `active=1`
is expected for this ability. The two simulated peers do not own replicated ability specs;
their absent specs are expected and do not substitute for owner/authority lifetime checks.
All 12 native network-driver readbacks reported zero lag and loss.

The observer removed its callback and restored all four mesh tick settings. The bounded
runtime window, editor log lines 4587–4641, contains zero errors, warnings, or critical lines.
Two earlier Pixel Streaming target-window errors remain in the full log and prevent a
rendered-acceptance claim. `editor01-settings-cleanup.json` records zero remaining PIE worlds,
restored in-memory PIE/network settings, and no dirty maps or content at cleanup. It does not
by itself prove editor exit or closed-editor INI restoration.

## Per-shot presentation integration

The source change adds `RawlinsPrimaryFire` to the shared native cue-tag module with value
`GameplayCue.Breakaway.Weapon.RawlinsPrimary.Fire`. The predicting owner executes the first
cosmetic cue during activation and retains its existing active burst lifetime. Authority
executes the cue in `SpawnShot` after each successful projectile spawn.

The network behavior uses the activation prediction key to suppress the owner's
duplicate first-shot multicast. The timer-driven second shot runs outside that activation
scope with an empty prediction key, so its cue also reaches the owner. Fixed runtime evidence
confirms two plays per accepted burst on every peer, without an extra owner play. No new
RPC, montage ability task, client shot timer, or
gameplay notify is added. Existing damage, projectile configuration, burst timing, end, and
cancellation paths remain responsible for gameplay.

The placeholder uses a project-owned copy of `MM_Pistol_Fire`:
`/BreakawayCore/Animations/A_BW_Rawlins_PrimaryFire_Placeholder`.
The original sequence is preserved. The copy uses `AAT_NONE`, clears its additive reference
pose, and receives an `ALWAYS` property-change notification before saving.
`fire-sequence-authored02.json` records the saved nonadditive state and sampled recoil poses.

`/BreakawayCore/Animations/AM_BW_Rawlins_Primary_Placeholder` contains that single recoil
sequence in the `UpperBody` slot. `montage-authored01.json` records approximately 0.6667 seconds,
RateScale 1, a 0.03-second blend-in, a 0.1-second blend-out, a finite Default-to-None section,
and no notifies. The replacement contract requires no root motion and finite automatic
completion. The fixed PIE checks verify evaluated playback and cleanup within their scopes.

## Editor compile and fixed PIE results

`compile-editor01.json` records a passing LyraEditor build in 25.46 seconds, exit code 0, and
empty `failureSignals`. `editor-dll-after-compile01.json` confirms that both LyraGame and
HeroRawlinsRuntime DLL timestamps advanced before the fixed editor launch.

The four `fixed-*-independent-verification.json` records pass independently checked production
activations from remote owner 257. Shot counts describe authority gameplay; play and replacement
counts are totals across all four peers.

| Scenario | Authority shots | Exact montage plays | Replacement stops | Independent checks |
| --- | ---: | ---: | ---: | ---: |
| Normal, three bursts | 6 | 24 | 12 | 62 |
| Moving, one burst | 2 | 8 | 4 | 58 |
| Rapid, three bursts with measured 100 ms lag | 6 | 24 | 20 | 62 |
| Death during recoil, then a burst after respawn | 4 | 16 | 8 | 59 |

Every case preserves two shots per burst and the loaded cadence. The exact montage advances
and restarts once per shot on every peer. Positive UpperBody weight and hand movement distinct
from idle establish evaluated action; linked layers remain intact. Authority and owner return
to inactivity, and the final sampled primary montage is inactive. The moving run also verifies
weighted action during locomotion and input cleanup. The rapid run verifies actual native
network-driver `PktLag=100` readbacks, with zero packet loss and no random lag.

The death run records four additional immediate cleanup stops and two pawn lifetimes on each
peer. The old pawn loses its exact primary montage, and the new pawn plays both shots again.
Death occurs approximately 0.2 seconds after activation, after both shots and during recoil.
This proves action cleanup and respawn; it does not prove cancellation before the second shot.

Each observer removes its callback. Normal, moving, and rapid runs restore four mesh tick
settings each; the death run restores eight across the old and new pawns. All bounded fixed
runtime windows contain zero errors or critical lines. Normal, moving, and rapid windows have
zero warnings. The death window retains seven character-initialization warnings. Two earlier
Pixel Streaming target-window errors remain separate in the full editor log.

The samples establish finite observed hand/foot positions and preserved layers. They do not
measure bone-chain lengths or independently certify anatomical proportions or final aesthetics.
Physical input and appearance still require their own acceptance checks.

`editor01-cleanup.json` and `editor02-cleanup.json` confirm that editor PIDs 31396 and 31816
exited, the ShooterCTF module manifest was restored byte for byte, and closed-editor PIE
settings match their originals. The corresponding settings-cleanup records confirm restored
network settings, no remaining PIE worlds, stopped observers, and no dirty maps or content.

## Packaged primary delivery

`compile-game01.json` records a passing LyraGame build in 35.14 seconds with empty
`failureSignals`. Candidate `LAN-20261009-06` completed Cook/Stage/Pak in 47.92 seconds.
Its frozen source uses base `8e2ce403` plus the recorded patch and four archived untracked
inputs. The source identity is
`A9D2DF0E6D9FB275051ABAD70F54B37E14A0A7417206D4C6F40A6863645236C0`.
`artifact-verification06.json` passes all 131 staged hashes, the compiled-input relationship,
and archived-input integrity, with no changed build inputs or errors. The restored ShooterCTF
module manifest is recorded separately from build inputs.

`package06-delivery-verification.json` derives Rawlins owner 257 in `client1`, distinct from
listen host 256. One accepted primary burst produces two exact montage instances on each
process, without an extra owner play. The loaded kit produces two 18.43 damage events and
two 0.9215 healing events, totaling 36.86 damage and 1.843 healing. Authority and owner both
return to ability inactivity. All four fixtures pass, and authority records fixture cleanup.

All four full logs contain zero errors or critical lines; warning counts remain in the
verifier evidence. `packaged06-flow-brief.json` records one normal exit marker per client.
The runner result records all three clients exiting with code 0 before owned host teardown.
The host's teardown exit is separate from normal client exits and does not prove a graceful
frontend exit. The result is saved as
`packaged-LAN-20261009-06-rawlins-primary01-result.json`.
`package06-cleanup.json` verifies that the four exact recorded process IDs are absent and
the temporary module manifest is restored. The earlier draft with invalid PID fields is
retained separately and supplies no cleanup proof.

The verifier rejects 53 corrupted evidence variants. These include missing second shots or
plays, duplicate owner presentation, wrong identity/assets, invalid arithmetic, failed ability
cleanup, command-line text posing as playback, and invalid client exit records. Positive
controls preserve acceptance when player identifiers change consistently or unrelated playback
is added. These controls test the evidence verifier; they do not launch additional game runs.

The fixture freezes and repositions players. It proves cooked per-shot delivery and gameplay
events, without proving evaluated moving poses, bone proportions, natural montage completion,
or death cleanup in the package. `BwayCueAudit` does not include the Rawlins primary tag, so
there is no direct cue-route/readiness assertion. Actual ability/damage events and exact
montage plays establish delivery. Requested command-line lag is 200 ms; this run does not
observe effective packet simulation settings or replace the measured PIE latency evidence.

Gate totals are two passing builds, three editor lifecycles, one package, and six runtime
attempts. The third editor lifecycle ended before the cancellation probe when the Git
publication request took priority. `editor03-cleanup.json` records editor exit, restored
settings, and byte-exact module restoration. No build, package, or fixed runtime retry was needed.

## Next acceptance work

`next-gate-audit.json` identifies an existing source-supported route for primary interruption:
issue `Cheat CancelActivatedAbilities` through the exact remote owner after the first
authoritative projectile and before the loaded second-shot deadline. Confirm the owner/server
cheat managers, observed zero lag, and active primary before dispatch. The expected result is
one authoritative shot, both ability instances inactive, and no second shot or recoil start.
This route remains runtime-unverified. `PrimaryAnimationCancel` stops only the diagnostic
ticker, and the 0.2-second death fixture cannot prove cancellation before shot two.

After that check, inspect the effective Blazing Barrage Blueprint/CDO and loaded
`BarrageProjectilePresentation` before declaring an animation defect. Its next baseline must
cover configured shots, damage/juggle, movement lock with aim, owner/proxy presentation, and
channel/tag/timer cleanup. The source audit also identifies a health-interruption hypothesis:
test healing followed by damage that leaves health above the activation baseline. Confirm the
intended interruption rule and runtime behavior before changing it.

Physical input, visual quality, packaged evaluated motion and lifecycle, and second-PC LAN
remain separate proof gaps. Completing this primary-action gate does not complete all-kit
presentation or the broader final-public-alpha parity and four-player frontend acceptance.
