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

## Queued native diagnostic compile

`bway.Test.PrimaryAnimation <PlayerId>` queues one owning-client primary activation on the
normal game tick. It waits up to five seconds for Playing, a live owned pawn, matching ASC
ownership, and a cleared editor script guard. `bway.Test.PrimaryAnimationCancel` cancels a
pending request. The helper is excluded from Shipping builds and leaves gameplay tuning unchanged.

`compile-editor01.json` records a successful `LyraEditor` build in 17.95 seconds, with exit
code 0 and no failure signals. The runtime DLL timestamp advanced to October 9, 2026,
20:53:36 UTC. Runtime dispatch and owner/remote animation observation remain pending.

Gate overhead so far: one compile, one editor lifecycle, two incomplete observer attempts,
and no packages. The next attempt must use native queued dispatch and retain read-only
Python observation; it must not repeat direct Python client ability activation.
