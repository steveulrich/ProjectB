# Argus primary action animation acceptance

October 9, 2026. This gate implements the replaceable placeholder action required by
`VERTICAL_SLICE_DEFINITION.md`, while preserving Argus's melee sweep, authority, damage,
attack timing, and immediate ability completion.

## Expected result

Three accepted remote-owner primary activations produce three distinct evaluated actions on
all four peers. Preserve the linked locomotion layer, return to inactivity, and leave a
stationary pawn's transform unchanged. Moving attacks, rapid replacement, death/respawn, and
packaged delivery require separate captures. Physical input and final appearance remain open.

## Missing action baseline

Evidence is under `Saved/Logs/argus-primary-animation-20261009/`.
The effective ability is
`/Hero_Spartacus/Characters/Heroes/Argus/Abilities/GA_BW_Spartacus_PrimaryAttack`.
Its native class is LocalPredicted and InstancedPerActor. `class-defaults01.json` records an
empty ActivationOwnedTags container. `ability-graphs01.json` identifies the actual
`Gameplay Ability Graph`; `ability-graph-summary01.json` has two unconnected activation/end
events. The failed initial query for `EventGraph` is retained separately.

`baseline01-verification.json` establishes a valid reproduction, not an action pass. Remote
Argus player 261 produced three accepted owner activations and three authority activations in
four Dorado PIE worlds. All 124 samples per peer had no montage, Melee or WeaponFire state,
or UpperBody, FullBody, or hip-fire weight. All scalar reads were supported. Linked layers
remained present; all pawns were alive and stationary. Approximately 4.44 cm of idle hand
drift is retained and is not mistaken for an attack. All 12 native driver readbacks had zero
lag and loss. The callback was removed and all four mesh tick overrides restored.

The bounded runtime log window has zero errors, warnings, or critical lines. Three full-log
Pixel Streaming target-window errors remain separate and prevent a rendered-acceptance claim.
The first staging setup selected the host because it incorrectly used PlayerState array order
as ownership. No primary observer ran in that setup. The corrected setup derives the remote
owner from the local PlayerController in client PIE world 1 and verifies its hero again after
travel. See `selection01.json`, `selection02.json`, and `baseline01-preflight.json`.

`editor01-settings-cleanup.json` and `editor01-cleanup.json` record restored settings, no PIE
worlds or dirty assets, normal editor exit, and restored module metadata.

## Integration

The source registers `GameplayCue.Breakaway.Weapon.ArgusPrimary.Attack` in the shared startup
cue dictionary. A valid activation executes it through GAS on the predicting owner and
authority before the existing authority-only sweep. Missed swings also animate. This cosmetic
event adds no damage, RPC, ability task, or gameplay lifetime. Existing GAS prediction keys
suppress the owning client's authoritative duplicate.

The compatible UpperBody asset `AM_BW_Argus_Primary_Placeholder` uses the nonadditive
`MM_Pistol_Melee` sequence, RateScale 2, 0.06/0.12-second blends, no gameplay notifies, and a
finite Default section. The cue uses the existing pawn-owned `PrimaryActionMontage`
interface, which handles replacement, death, and avatar cleanup. The ability still ends
immediately; cosmetic recovery does not keep damage active or block locomotion.

`compile-editor01.json` passes LyraEditor in 28.76 seconds with no failure signals. Both linked
LyraGame and BreakawayCoreRuntime DLL timestamps advanced. `GCN_BW_Weapon_ArgusPrimary_Attack`
uses the existing native cue parent and points to the new Argus montage. Its tag was assigned
with an explicit editor property-change notification before the first runtime attempt, so
the live registry was refreshed during authoring. Cue validation reported zero errors/warnings.

## Evaluated lifecycle

All four `fixed-*-independent-verification.json` records pass for remote Argus player 257.

| Probe | Verified scope |
| --- | --- |
| Normal | Three accepted attacks, 12 exact montage plays, three starts per peer, evaluated hand movement, intact masculine linked layers, final inactive |
| Moving | One accepted attack, four plays, owner speed 600 at fire, foot motion beneath positive UpperBody weight, native input stop and final zero speed |
| Rapid | Three accepted attacks, 12 plays, eight replacements, three starts per peer; all 12 driver samples show 100 ms lag with zero loss |
| Death/respawn | Two accepted attacks, eight plays, four immediate stops; no exact primary during or after the old pawn's death; each new pawn replays successfully |

Callbacks and mesh tick settings were restored after every probe. Both editor lifecycles exited
normally, and the settings/module cleanup records verify restoration. Bounded test windows have
zero errors or critical lines. The death window retains seven hero-initialization warning lines;
the full second editor log retains two earlier Pixel Streaming target-window errors. This is
structured pose and lifecycle evidence, not physical-input or final-appearance acceptance.

The [art replacement guide](../Plugins/GameFeatures/BreakawayCore/Docs/Hero_Art_Replacement_Guide.md)
now documents the cue properties, slot/skeleton contract, root-motion restriction, finite
playback, gameplay-notify boundary, and required replacement checks for Alona, Korryn, and Argus.

## Packaged gate

`compile-game01.json` passes LyraGame in 36.36 seconds with empty failure signals.
Candidate `LAN-20261009-05` freezes base commit `0592b324`, the recorded source patch,
and six archived untracked assets/documents. Its source identity is
`6BA009829FA2A10EC174F5ED4176665BF862D4129786AEE3E56CAEE461674FFE`.
Cook/Stage/Pak passed in 55.03 seconds. `artifact-verification05.json` confirms all 131
staged hashes, the matching compiled executable, and unchanged frozen build inputs.
The temporary module manifest was restored after cooking.

`package05-delivery-verification.json` passes with remote Argus player 257 in client1.
The role-order marker identifies host 256 separately. The real owner activation produces
exactly one matching primary montage on each process, with positive duration and correct
network/local roles. The loaded one-hit damage plan, damage event, self-healing, and final
result agree. Both authority and owner report the ability inactive afterward. All four
fixtures pass; the host restores four players and two bases and removes its listeners.
Three clients exit with code 0 before owned host teardown. All four logs have zero errors
or critical lines. `package05-cleanup.json` confirms no owned process remains.

The development-only `bway.Test.AllKitLeechHostRemoteArgus` variant selects Argus on a remote
player before travel. It preserves the existing client's hero-based resolution and all damage,
upgrade, and cleanup logic. The original host command retains its existing roster order.
This allows the package to prove real remote melee activation rather than reuse a host-owned
Argus result. A native role-order marker records the distinct host and Argus identities.

The packaged verifier requires those identities to agree with the real activation, exact hero,
ability, kit, and montage paths, one-hit damage/healing, ability end, cleanup, and client exits.
Forty-seven corrupted records are rejected. CueAudit omits the new Argus tag, so direct cue-event
observation remains outside this verifier's claim. The test requests `-PktLag=200`; its
command-line record is not a native driver readback. Movement is frozen by this damage fixture,
so packaged moving poses, natural montage completion, death cleanup, physical input, final
appearance, and second-PC LAN remain separate proof gaps. Candidate04 remains the frozen
Korryn reference.

Gate totals: two builds, two editor lifecycles, one package, six primary observer or packaged
attempts, and two initial staging setup attempts. No compile or package retry was needed.
Use the final artifact verification and source snapshot to distinguish subsequent documentation
updates from changes that invalidate gameplay evidence.

Next gate: Rawlins's effective primary Blueprint/CDO, loaded projectile presentation, and
evaluated remote-owner baseline. Preserve its two-shot burst and predicting-client lifetime:
three separated activations must remain three inputs producing six shots, with eventual
owner/authority inactivity. Its existing packaged damage proof does not establish an action pose.
