# Korryn primary action animation acceptance

October 9, 2026. This gate continues the complete, replaceable placeholder presentation
requirement in `VERTICAL_SLICE_DEFINITION.md`. It preserves projectile authority, damage,
attack timing, movement, and the ability's immediate completion.

## Expected result

An accepted Korryn primary produces one evaluated action on its owner and observing peers.
Its animation is replaceable through asset properties. Repeated attacks, locomotion,
death/respawn, and avatar teardown must leave no duplicate or stale playback.

## Missing action baseline

Evidence is under `Saved/Logs/korryn-primary-animation-20261009/`.
The native source performs authoritative projectile spawning but has no firing presentation.
`class-defaults01.json` confirms the granted Blueprint has no activation-owned firing tags.
`ability-eventgraph01.json` contains only unconnected activation and end events.

The four-world listen PIE baseline selected remote Korryn player 257 and reached Dorado's
Playing phase. Three native queued activations were accepted and every peer observed three
projectiles. All 116 samples per peer had no montage, firing tag, or hip-fire override weight.
The observer removed its callback and restored all four mesh tick settings. See `baseline01.json`
and `baseline01-native-brief.json`.

The full editor log preserves two Pixel Streaming target-window errors and one failed initial
asset lookup using the wrong plugin mount. The valid mount is `/Hero_Morgan/`, derived from
`Hero_Morgan.uplugin`. These errors do not establish rendered acceptance; retain them separately
from the bounded primary baseline. `editor01-cleanup.json` records normal editor exit and
restoration of PIE/network settings and the temporary ShooterCTF module manifest.

## Integration

The source change registers `GameplayCue.Breakaway.Weapon.KorrynPrimary.Fire` in the existing
shared cue-tag module. The predicting owner executes the cosmetic cue before its existing
ability end. Authority emits it only after successful projectile spawning. GAS provides the
same prediction-key duplicate suppression used by Alona. No new RPC or montage ability task
is introduced, and the projectile configuration remains authority-only.

`compile-editor01.json` passes LyraEditor in 28.49 seconds with no failure signals and an
advanced HeroMorganRuntime DLL timestamp. The new cue Blueprint
`GCN_BW_Weapon_KorrynPrimary_Fire` uses `BwayGameplayCueNotify_PrimaryAction` as its parent.
Its replaceable `PrimaryActionMontage` points to `AM_BW_Korryn_Primary_Placeholder`.
This UpperBody placeholder uses the compatible, nonadditive `MM_Pistol_Melee` sequence,
RateScale 2, 0.06/0.12-second blends, automatic blend-out, and no gameplay notifies.
It provides a replaceable upper-body action until final art is available. Asset validation passed.

The first post-change probe (`fixed-normal01.json`) still produced projectiles without
invoking the new cue. The saved Blueprint tag, asset-registry tag, and montage were correct,
but the live editor cue registry had not refreshed after the native default-property write.
Applying the same tag through an explicit editor property-change notification and saving
the asset refreshed registration. `cue-refresh01.json` and the live cue-map entry at line 5092
of `editor02.log` preserve this changed retry hypothesis. The subsequent fresh PIE session
routed the cue and evaluated the montage on all four peers. This editor-cache correction is
separate from cold packaged discovery, which must still be proved.

## Evaluated PIE evidence

The successful probes use remote Korryn player 261 in four same-process listen PIE worlds
on Dorado. The source gate preserves the failed probe and the registration retry:

| Probe | Observed result |
| --- | --- |
| `fixed-normal02.json` | Three shots; 12 exact Korryn playback starts; evaluated montage samples on all four peers |
| `fixed-moving01.json` | One shot while native input drives movement; exact primary montage evaluated on every peer |
| `fixed-rapid01.json` | Three shots with explicitly applied 100 ms lag; native driver readbacks and replacement markers retained |
| `fixed-death01.json` | Two pawn lifetimes on every peer; immediate primary interruption and another primary after respawn |

The four `*-independent-verification.json` records pass. Movement fired at owner speed 600,
preserved foot motion during positive UpperBody weight, and ended at zero speed. Rapid fire
produced exactly 12 plays and eight replacements, with three starts per peer and all 12 native
driver records showing 100 ms lag and zero loss. Death produced four immediate primary stops,
no exact primary on or after the old pawn's death, and successful playback on each new pawn.
All four bounded runtime windows contain zero errors, critical lines, or warnings.

Every observer completed, removed its callback, and restored mesh tick overrides. The
generic `deadWithMontage` counter includes the legitimate death montage; cleanup acceptance
must compare the exact Korryn primary path rather than reject all montage playback on a dead
pawn. `editor02-settings-cleanup.json` and `editor02-cleanup.json` record restored PIE/network
settings, no dirty assets or PIE worlds, normal editor exit, and byte-exact module restoration.

The full second editor log retains 13 error lines: three Pixel Streaming target-window errors,
two prohibited editor-asset reads during PIE, and eight traceback lines from the play-mode and
protected-property inspection failures. These are outside the bounded successful probes.
No rendered or physical-input acceptance
is inferred. The earlier CueAudit fixture enumerates other cue tags and cannot substitute for
Korryn's own mapping and runtime evidence.

Overhead through PIE: one LyraEditor build, two editor lifecycles, six primary observer attempts
(baseline, initial unrouted cue, and four successful scenario captures), and no package yet.
On the registration retry, normal hero-selection timeout completed while context was restored;
the selected remote hero was explicitly checked before issuing the valid probe.

## Packaged candidate

`compile-game01.json` passes LyraGame in 46.14 seconds with empty failure signals.
Candidate `LAN-20261009-04` freezes commit `0592b324` plus the recorded source patch and three
archived untracked files, including both Korryn assets. Its source identity is
`6F052E52BCD2214577CC2B552FEC8303C22D8047A41750D48D2033CD9207B899`.
Cook/Stage/Pak passed in 48.30 seconds. `artifact-verification04.json` verifies all 131 staged
hashes, the compiled executable, and unchanged frozen build inputs. The temporary module
manifest was restored after cooking.

The four-process run uses the existing all-kit leech fixture to issue a real remote-owner
Korryn primary and verify its damage. The runner records client exit status separately from
owned host teardown. Its verifier requires the exact selected hero, ability, montage,
network/local roles, damage and healing, ability end, and fixture cleanup. Thirty-six corrupted
records are rejected; positive controls permit changed player/owner identity and unrelated
Alona playback. See `packaged04-verifier-controls.json`.

`package04-delivery-verification.json` passes for remote Korryn player 259 in client3. Each
process recorded exactly one matching primary montage, with positive duration and the correct
owner/observer role. The authority's loaded damage plan, damage event, healing, and result agree.
Both authority and owning-client ability termination passed. All four fixtures passed;
the host restored four players and two bases and removed its listeners. Three clients exited
with code 0 before the host's owned teardown. All four logs contain zero errors or critical
lines. `package04-cleanup.json` confirms no owned editor/game process remains and the module
manifest is restored. Cold packaged discovery therefore no longer depends on the live editor's
cue-cache correction.

The legacy CueAudit tag list omits Korryn's new fire cue, so its missing route marker is not
claimed as coverage. Exact production montage playback establishes delivered action behavior;
direct Korryn cue event observation remains omitted. The fixture freezes and repositions
players and requests `-PktLag=200` without a native driver readback. Packaged evaluated
movement, natural montage completion, death cleanup, physical input, final appearance, and
second-PC LAN remain separate proof gaps. Candidate03 remains the frozen Alona reference.

Gate totals: two target builds, two editor lifecycles, one package, and seven runtime observer
or packaged attempts. Reuse the passing records unless a relevant source, asset, configuration,
fixture, or topology change invalidates their stated scope. Next audit: Argus's effective primary
Blueprint/CDO and a remote-owner evaluated baseline, including its Melee and WeaponFire tag
paths. Its authoritative damage proof does not establish action presentation.
