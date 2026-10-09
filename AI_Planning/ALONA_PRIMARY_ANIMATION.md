# Alona primary action animation acceptance

October 9, 2026. This gate checks the placeholder body action independently of projectile and
gameplay-cue delivery. It does not change primary damage, attack timing, or movement tuning.

## Expected result

A successful primary attack produces a visible action pose on the owning Alona and remote
hero meshes. Its compatible replacement animation is assigned through an asset interface.
Prediction must not double-play the action. Locomotion, death/respawn, and repeated attacks
must leave no stale playback. Physical input and packaged LAN remain separate proof scopes.

## Current evidence

The four-world listen PIE setup selected Alona for remote player 257 and reached Playing.
Live class defaults have no primary `ActivationOwnedTags`. The base AnimBlueprint maps
`Event.Movement.WeaponFire` to `GameplayTag_IsFiring`. The feminine unarmed layer uses a
ready idle pose and a 0.5-second hip-fire raise duration.

The fire cue's EventGraph is empty, but its `OnBurst` function is executable. It calls
`WeaponAudioFunctions.SendWeaponFire`, followed by weapon-dependent audio/effect operations.
`SendWeaponFire` routes to the music manager's `Receive Weapon Fire`. These graph observations
do not establish an attack pose. Inspect the evaluated runtime path before adding playback.

Evidence is under `Saved/Logs/alona-primary-animation-20261009/`: `class-defaults01.json`,
`cue-onburst-graph.json`, and `gate-contract.json`.

## Failed dispatch and retry condition

`baseline01.json` stopped before activation because `AbilitySystemBlueprintLibrary` is not
exposed as a Python module class in this editor. Its observer callback was removed.

The subsequent Python call to the remote ASC's `TryActivateAbilitiesByTag` caused recursive
prediction and editor exit. `editor01.log` records the Deep PredictionKey Chain ensure and
repeated `ServerTryActivateAbility` stack. No primary animation verdict is claimed.

This matches BF-066: Python's `GAllowActorScriptExecutionInEditor` guard makes
`AActor::GetFunctionCallspace` return Local, including nested native RPCs. Do not call remote
gameplay mutations directly or indirectly from Python. Queue the primary through a bounded
development command on a normal game tick, require that guard to be false, and retain Python
only for serialized observation. This is a changed dispatch hypothesis, not a gameplay fix.

The editor process exited, destroying its callbacks. No asset edits occurred. The closed-editor
PIE settings match their original values, and the temporary ShooterCTF module manifest was
restored byte for byte. See `editor01-failure-cleanup.json`.

## Queued native diagnostic and missing action

`bway.Test.PrimaryAnimation <PlayerId>` queues one owning-client primary activation on the
normal game tick. It waits up to five seconds for Playing, a live owned pawn, matching ASC
ownership, and a cleared editor script guard. `bway.Test.PrimaryAnimationCancel` cancels a
pending request. The helper is excluded from Shipping builds and leaves gameplay tuning unchanged.

`compile-editor01.json` records a successful `LyraEditor` build in 17.95 seconds, with exit
code 0 and no failure signals. The runtime DLL timestamp advanced to October 9, 2026,
20:53:36 UTC. `baseline03.json` then recorded three accepted attacks and three projectiles
on every peer. All 122 samples per peer had no firing tag, hip-fire weight, or montage.

## Replaceable action implementation

The existing Alona fire cue now derives from `BwayGameplayCueNotify_PrimaryAction`. Its
`PrimaryActionMontage` and `PrimaryActionPlayRate` defaults provide the replacement interface.
The existing cue tag, spawn conditions, placement, effects, and 26-node `OnBurst` graph remain.
The native primary ability, damage, projectile, and prediction transport are unchanged.

`AM_BW_Alona_Primary_Placeholder` uses the existing nonadditive `MM_Pistol_Melee` sequence,
without notifies or root motion. It plays at rate 2 through the evaluated `UpperBody` slot
and existing upper/lower-body mask. Replacement art must retain the compatible skeleton,
evaluated slot, finite playback, no root motion, and no gameplay notifies.

The pawn owns cosmetic playback independently of the ability's immediate end. It replaces
only its own montage instance, preserves unrelated montage groups, rejects dead avatars,
and stops that exact instance on death, ASC teardown, and EndPlay.

`compile-editor04.json` passed with empty failure signals in 7.86 seconds. The two intervening
builds found local-name shadowing and an unexported engine function; both were corrected.
The cue Blueprint validates with zero errors or warnings.

## Evaluated pose and lifecycle evidence

`normal02-verification.json` proves three attacks across four peers: 12 plays, no duplicates,
and no rejected actions. Each peer has 123 samples, its feminine linked layer remains present,
and the upper-body slot reaches weight 1 then returns to 0. Active hand excursions measure
81.4–83.6 cm against 12.0–16.7 cm idle variation; actor displacement remains zero.

The first `fixed-*-01` observer used `set_editor_property` to force bone refresh. That editor
notification re-registered the mesh and removed its dynamic linked animation layer, invalidating
pose acceptance. The corrected observer assigns the Blueprint-writable runtime property directly
and restores it the same way. Preserve the failed measurement; do not repair production animation
in response to that fixture side effect.

The corrected death run proves two pawn lifetimes per peer, immediate primary-action cleanup,
and renewed action playback after respawn. Death and hit-react montages are separate from the
primary montage and must not be counted as leaked primary playback.

`rapid-death02-verification.json` retains an unresolved rapid-delivery result: peer 2 missed the
middle action in a three-attack burst. The inherited editor Average profile enables 30–60 ms
incoming/outgoing latency and 1% packet loss. Packet loss is a possible cause, not a confirmed
diagnosis. The controlled no-loss repeat below separates this result from the current acceptance evidence.

`controlled04-verification.json` passes the explicit-network and moving checks in four-world,
same-process listen-server PIE. The moving attack fires at 600 cm/s with zero configured lag
or loss. All four peers retain their linked animation layers and show primary playback,
movement, and lower-body foot motion together. Input stops and every peer later returns to
zero velocity and inactive primary playback.

The rapid repeat uses an observed 100 ms lag and zero packet loss on all four drivers.
Three accepted attacks produce 12 plays and eight replacements, with three starts per peer.
Both controlled runs restore all four tick settings and remove their callbacks, with no
bounded errors or rejected actions. `editor04-cleanup.json` records editor exit and restored
PIE, network, and module-manifest settings.

`compile-editor05.json` and `compile-game01.json` both pass with exit code 0 and empty failure
signals. Packaged transport, physical input, and final appearance remain open. The bridge
screenshot captured the editor viewport rather than a gameplay window and supplies no
action-appearance proof. The next acceptance step is a fresh package containing this change.
