# Relic placeholder effects and lifetime acceptance

Evidence root: `Saved/Logs/buildable-authority-20260927/`. Candidate `LAN-20260927-43` passes the affected rendered packaged gameplay/audio/component-attachment scope with zero unsupported Niagara references. Actual appearance and full acceptance remain separate.

**Current result:** rendered four-world PIE lifecycle and initial-flight/color scopes passed. `relic-vfx31-lifecycle.json` contains 16 settled/repeated samples across eight states (64 peer rows) under configured 100 ms latency, with one correct active component or none as appropriate, unchanged component identity on repeated notifications, and complete actor/effect destruction. `relic-vfx33-initial-flight.json` adds eight peer rows for initial Thrown replication and an independent blue-to-red team change without trail recreation. Both observers stopped and all owned objects were removed. These are synthetic production-actor fixtures, not physical throws/catches or separate-process LAN tests.

`relic-vfx31-*.png` shows the actual relic with the saved smaller glows, moving trail, and empty reset state. `relic-vfx33-{blue,red}.png` confirms visible team-colored trails. Renderer screenshots do not prove physical input or Pixel Streaming readiness. `relic-vfx35-validation.json` reports seven assets requested/checked/valid, zero warnings/invalid/skipped/unable-to-validate. `relic-vfx34-cleanup.json` and `relic-vfx36-closed-editor.json` confirm no PIE worlds, no dirty maps/content, no retained fixture objects, restored four-client listen settings, and editor shutdown.

## Required behavior

The relic must show its current state on the listen host and every relevant client, including its initial neutral state and an actor arriving while already in flight. Repeated presentation notifications must preserve the existing component. Every state exit and actor shutdown must remove effects it no longer owns. Cosmetic simulation must not change pickup, passing, damage, scoring, or replication authority.

| State | Persistent presentation |
| --- | --- |
| Neutral | Idle glow |
| Carried | Smaller carried glow |
| Dropped | Dropped glow |
| Thrown / BeingPassed | World-space trail |
| Scoring | Scoring glow; current state, not a historical goal burst |
| Resetting / PendingRequest | No state effect or trail |

`URelicSettings` owns the four state assets plus `TrailEffect` and `TrailLifetimeSeconds`. Replacement assets use centimeters, attach at the relic mesh origin, and loop until their actor-owned component is destroyed. The trail optionally consumes Niagara parameters `User.Team_Color` (LinearColor) and `User.Trail_Length_Second` (float, default 0.25 seconds, clamped 0.05–2). Particle age is cosmetic; the replicated relic state controls flight lifetime. Team RepNotify refreshes color independently of state RepNotify order. Dedicated servers skip cosmetic creation.

## Implementation and asset scope

`ARelicActor` reconciles local components on BeginPlay, state RepNotify, and settings initialization after BeginPlay. It preserves a matching component on repeated notification, removes other state components, and explicitly destroys all effects during EndPlay. The old unreliable throw/pass multicast was replaced by persistent state-driven trail ownership.

Six new assets live under `/BreakawayCore/Effects/Relic`: four `NS_BW_Relic{Idle,Carried,Dropped,Scoring}` systems, `NS_BW_RelicTrail`, and `NET_BW_Relic`. Only these assets and `DA_BW_RelicSettings_Default` are changed. Engine/Lyra source templates and maps are preserved.

State glows duplicate the one-sprite `MinimalLightweight` template. Inspection of `NiagaraStatelessEmitterInstance.cpp` confirms its local-space simulation. Each particle lives one second with a repeated one-particle burst. Bounds are ±200 cm. The effect type retains all platform quality levels and disables distance, visibility, instance-budget, and global-budget culling for this single objective; actor ownership bounds live instances. This is not a target-device performance pass.

The trail duplicates `NS_Grenade_Trail`, which has two ribbons plus arrow/line emitters. It needs system-level Infinite loop behavior before use as a persistent flight effect. `niagara.set_emitter_property` now supports the explicit `propertyName: "System.LoopBehavior"` operation with an enum display label such as `Infinite`. It refuses PIE edits, validates a unique SystemState input, uses the graph schema, recompiles, and saves only that system. Runtime validation remains required.

## Evidence and attempt history

- `relic-vfx14-authored.json`: six assets saved; no dirty content remained. First authoring attempt stopped because Python's Box constructor accepts only min/max; recovery touched only the two outputs created before that failure.
- `relic-vfx15-settings.json` and `relic-vfx17-cold-settings.json`: all five settings slots assigned, state settings reloaded after editor restart, no dirty content.
- `compile-editor40.json`: LyraEditor passed in 32.30 seconds, no failure signatures; linked gameplay DLL advanced. This includes the gameplay-side lifetime changes.
- `relic-vfx18-setup.json`: isolated replicated production relic in four PIE worlds, separate from the match relic. Initial neutral glow was observed on all four original match relics.
- `relic-vfx20-neutral.png`: actual relic through a temporary game camera, captured using the engine renderer. The first glow was too large relative to the mesh. `relic-vfx23-polish.json` reduces size/alpha to idle 55/0.30, carried 45/0.20, dropped 60/0.40, and scoring 85/0.50. Updated visual verification remains required.
- `relic-vfx21-lifecycle.json`: fixture failure, not gameplay evidence. A nonexistent `unreal.is_valid` call also broke its error cleanup and produced repeated callback errors. The observer was explicitly unregistered; the corrected helper uses `unreal.SystemLibrary.is_valid` and unregisters before error cleanup. API preflight now checks validity, attachment, active state, reflected RepNotify, and net update before registering the observer.
- `relic-vfx22-lifecycle.json`: neutral and carried convergence/repeated notifications passed, then the thrown trail existed but was inactive. The observer stopped and requested owned cleanup; PIE was stopped afterward. The engine reported natural Niagara completion. `relic-trail24.t3d` shows a finite system loop overriding the emitter lifecycle. Do not promote this attempt as a complete VFX pass.
- Setup capability corrections, scoped to this editor lifecycle: static deferred spawn is available through reflected `GameplayStatics` calls; `bAlwaysRelevant` cannot be edited on an actor instance; Transform construction takes a Rotator; CameraActor uses `get_component_by_class(CameraComponent)`; PlayerController uses `get_controlled_pawn`; screenshots use `SystemLibrary.execute_console_command(world, 'Shot filename="..." -nosuffix', controller)`. No global live-world wrappers are retained.
- `compile-editor41.json`: bridge link failed because Niagara's `NotifyGraphNeedsRecompile` helper is not exported across DLLs. Its public virtual pin-change callback already performs that work. Removing the redundant direct call produced passing `compile-editor42.json`; `editor42-linked-dll.json` verifies the new bridge DLL. No game compile/package was launched for this editor authoring repair.
- `relic-vfx25-loop-authoring.json`: changed the copied trail's system loop from Once (`NewEnumerator1`) to Infinite (`NewEnumerator0`). An invalid enum label and an attempted lifetime edit during PIE were rejected. `relic-vfx27-lifecycle.json` then passed all eight states and EndPlay cleanup, but its screenshots exposed the unsaved glow resize.
- `relic-vfx23-polish.json` recorded in-memory values only: changing a stateless subobject had not marked its owning asset dirty, so a default save skipped persistence. The corrected script calls `modify()` on asset/module/emitter, notifies the emitter, and explicitly saves that exact asset with `only_if_is_dirty=False`. `relic-vfx29-polish.json` records the corrected save; `relic-vfx30-cold-reload.json` confirms all four sizes after package reload; run31 provides the matching rendered/runtime proof. Never treat `save_loaded_asset` returning true alone as proof that an unmarked subobject edit reached disk.
- An initial-flight probe selected actors by height and failed before collecting peer data; its error is retained in `relic-vfx-editor04.log`. Runs32/33 instead identify newly spawned actors against a per-world baseline. Run32 passed structured state/color/cleanup, but a gameplay camera handoff invalidated its screenshots. Run33 owns its camera, reapplies the view target before each capture, and separates the blue capture from the later red mutation; both structured and visual scopes pass.
- Full editor04 log screening: seven error lines (three unresolved Pixel Streaming target-window lines and four lines from the failed Python selection probe), no other error lines and no fatal/assert/ensure lines. Earlier editor03 retains its callback API failure flood; it is not a clean-runtime pass. Do not suppress or relabel these errors as gameplay evidence.

This iteration used three editor builds (two passes, one corrected link failure), two editor launches, and three PIE lifecycles. Seven asset-mutation batches include the partial authoring attempt and the resize that did not persist; the final seven-asset set was reloaded/validated. No package or game build was needed for the editor authoring repair itself.

## Open proof

Candidate41 relic03-rendered completed with a D3D12 offscreen listen host, three initial clients at configured 200 ms latency, and an actual post-score late join. All four clients reached Results and exited 0; the host was terminated separately after client completion. Audio/reset verification passes 101 checks and 11 corruption controls; hit/damage/pass/goal verification passes 54 checks and 13 controls. Five logs contain zero error/critical lines. The 131 staged hashes match the frozen artifact and executable SHA256 `FC610B24F2F435290AFEAA23152803F25069FCA6F1C1B97B1269C6DF91E03F48`.

The host log contains `FNetGUIDCache::SupportsObject: NiagaraComponent ... NOT Supported` warnings. The replicated RelicMesh sends `USceneComponent::AttachChildren`; attaching locally spawned Niagara systems directly to it exposes unsupported dynamic object references. The correction adds a stable default `VisualEffectsRoot` scene component under the mesh, keeps that parent nonreplicated, and attaches local effects beneath it. Mesh replication and authoritative relic state remain intact. The new packaged gate requires unchanged production gameplay/audio, correct local effect parents on every peer and late join, and zero unsupported Niagara references. Do not classify the candidate41 warnings as a clean replication pass.

The packaged component markers prove construction and current-state delivery, not sustained simulation or perceived appearance. Actual throw/fumble timing, carried animation attachment, normal game-camera readability, physical actions, second-PC LAN, full-match presentation, audio balance, target-device performance, and final end-to-end acceptance remain open. The candidate41 late join proves current Scoring state and no historical audio; it does not prove a player joining during flight.

### Candidate42 cooker mismatch

`compile-game42.json` passed in 55.01 seconds. Candidate42 cooked/staged in 98.98 seconds and matched all 131 hashes, but `relic04-rendered` crashed before the listen socket opened. The first causal log sequence contains shifted RelicActor property types and `ObjectSerializationError ... Bad export index 278/9` while loading `BP_BW_RelicActor`. No clients launched; its vacuous `clientExitZero` field is not client coverage.

The new `VisualEffectsRoot` reflected field changed the game's property layout, while the cooker still loaded the older editor DLL (23:15:55 UTC, before the header edit at 00:02:04 UTC). This is a cook/game schema mismatch, not evidence against the attachment fix. Build LyraEditor, verify its runtime DLL advanced, and recook a new candidate using the already passing game42 binary. Keep the failed candidate/log intact. The testing README and ladder now document the additional cooker build when reflected layouts or native defaults change.

### Candidate43 local attachment verification

`compile-editor43.json` passed in 35.05 seconds. `candidate27-43-cooker-verification.json` confirms the gameplay editor DLL advanced to 00:12:22 UTC, after the header change. The game42 executable is unchanged. Candidate43 cooked/staged in 59.56 seconds, and all 131 staged hashes match. Executable SHA256: `910D4BC37B2A6B7A5CC8F9065328D15D09FD955DA6E0A7B878B6712F170FD14F`.

`packaged-LAN-20260927-43-relic05-rendered-result.json` records the same D3D12 offscreen listen topology, three original clients, and post-score late join. The three verifier records beside it pass 101 audio checks/11 corruption controls, 54 hit/gameplay checks/13 controls, and 55 VFX checks/9 controls. All four nonhost processes reach Results and exit 0; the host teardown is recorded separately. All five logs have zero error/critical lines, zero Niagara warnings, and zero unsupported Niagara object references (candidate41 had 19). Every state or trail component attaches to the nonreplicated VisualEffectsRoot while the relic mesh remains replicated. Late join settles to the current scoring glow without historical flight or audio replay.

This continuation added two builds, two packages, and two packaged attempts. The first package failed before gameplay because of the stale cooker; it remains failed evidence. No assets changed during this attachment repair. The synthetic PIE lifecycle screenshots remain evidence of the unchanged assets' appearance, not proof of the new hierarchy's animated carry behavior. Next: four-hero socket validation, carried attachment in motion, and normal-camera readability. Invalidate the attachment result if relic component ownership, state reconciliation, replicated mesh settings, assets, or cooked schema change.
