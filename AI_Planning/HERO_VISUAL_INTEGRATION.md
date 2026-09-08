# Hero placeholder mesh and animation integration

September 8, 2026. This records the body/locomotion integration repair. Ability-specific montages, sockets, effects, physical inputs, final art, and performance still require their own acceptance.

## Authoring contract

The four playable `UBwayHeroDataAsset` definitions own their body visuals. Replace these fields together when introducing a new rig:

| Field | Requirement |
|---|---|
| `HeroMesh` | Rigged skeletal mesh with the intended physics, LODs, and required sockets |
| `AnimationClass` | Generated animation class, such as `ABP_Mannequin_Base_C`; a Blueprint asset object is not a runtime class |
| `AnimationLayerClass` | Linked locomotion/action layer when the base class uses linked layers; otherwise optional |
| `HeroMeshRelativeTransform` | Absolute transform relative to the capsule, including floor offset, facing axis, and scale; current humanoid convention is Z=-88, yaw=-90, unit scale |
| `ExpectedSkeleton` | Intended mesh skeleton; change it deliberately when retargeting to a different rig |
| Existing validation fields | Required sockets, physics asset, and LOD expectations remain available |

Argus and Rawlins use `SKM_Manny` with `ABP_UnarmedAnimLayers_C`. Korryn and Alona use `SKM_Quinn` with `ABP_UnarmedAnimLayers_Feminine_C`. Both use `ABP_Mannequin_Base_C` and `SK_Mannequin`. These are replaceable placeholders; hero IDs, stats, kits, portraits, input mappings, and gameplay rules are retained.

The generated classes and meshes are hard references from the hero definitions, so the existing hero discovery/cook path retains them. Runtime state stays on pawns and PlayerStates. Do not mutate definition assets during play. `AnimationBP` remains an editor-only deprecated migration field and is no longer used by runtime code or the cheat diagnostic. New or external definitions must populate `AnimationClass` before packaging; the four playable definitions have been migrated.

Data validation now rejects a hero mesh without a skeleton, a missing runtime animation class when a custom mesh is assigned, and an animation or linked-layer skeleton incompatible with that mesh. Existing expected-skeleton and socket validation remains. Run validation after replacing assets, then inspect locomotion, ground alignment, facing, actions, and remote peers in a cooked build.

## Causes and implementation

- All four authored mesh transforms were identity. Applying them replaced the humanoid pawn's floor offset and forward-axis rotation, producing floating/sideways bodies. The defaults and four definitions now follow the pawn's existing convention.
- Argus referenced the engine tutorial mesh with a different skeleton; Alona's stub mesh had no skeleton. Both were paired with the mannequin animation Blueprint. The four definitions now use compatible Manny/Quinn pairs.
- The old field stored `UAnimBlueprint`. Unreal's `UBlueprint::NeedsLoadForClient/Server` returns false, while the generated class is retained. Thus the editor showed an assigned Blueprint while packaged Rawlins logged a null reference. Runtime code now stores/uses `TSubclassOf<UAnimInstance>`.
- Reinitialization lost the human pawn's linked locomotion layer. Applying hero visuals now initializes the animation class and explicitly relinks the hero's layer. The before-fix runtime probe found the unarmed layer on bots but not the four human pawns.
- Both Breakaway experiences added `B_BW_PickRandomCharacter` to controllers. Its cosmetic parts could replace the main mesh with an invisible Lyra body, competing with the hero definition. Exactly that request was removed from Dev and CaptureTheRelic; the composition audit preserved all other requests, order, flags, and actions.
- `ApplyHeroDataToNewPawn` accepted `AController` but returned immediately for AI controllers. Selected bot heroes therefore never received the same hero initialization. Removing the player-only cast applies the already-selected, locked hero through the same path for human and AI controllers, including round respawns. Existing tracked ability grants are removed before replacement.

## Verification and limits

Three C++ builds passed: 55.40 seconds for runtime class/schema, 45.96 seconds for linked layers, and 26.06 seconds for AI initialization. The additional builds followed distinct runtime findings. Six changed assets passed validation with zero errors/warnings after a fresh editor launch. No unrelated dirty assets were saved.

The final four-peer DevMap run completed two natural rounds, Team 1 winning 2–0 at 17:07:07 UTC. Current-state assertions passed for all 32 pawn instances. A 40-second sampler recorded 182 samples and 96 pawn lifetimes across round respawns: every recorded pawn had a visible mesh, linked layer, and changing foot poses. The seven distinct world paths in the aggregate include three temporary startup worlds; this was four simultaneous PIE peers, not seven players. Initial diagnostic runs and their failures remain in the evidence directory.

A separate postmatch fixture assigns all four hero definitions to the four authoritative human pawns and checks replicated Manny/Quinn meshes and layers. It intentionally exercises visual replication; it does not update or validate selection/results identity. Do not interpret the retained Argus results from that already-completed match as fixture hero-selection evidence.

The final fixture passed on all 32 current pawn instances (six Manny and two Quinn per world). Its 40-second observer recorded 193 samples, changing poses on all 32 instances, zero missing layers, and zero invisible meshes. Current-state evidence is `current-natural-bot-fixed.json` and `current-all-four-final.json`; observer evidence is `runtime-natural-final.json` and `runtime-all-four-final.json`.

Attempt accounting: four editor launches, four four-peer PIE runs, and three builds. Later runs followed the linked-layer, cosmetic-ownership, and bot-initialization findings. The initial plugin mount lookup and protected-property sampler failed without asset changes or retained world wrappers. A guarded shell edit rejected its expected text before writing; the bot fix was then applied as a scoped patch. Git LFS inspection required normal sandbox escalation for its local pipe. The editor logs contain the separately known PixelStreaming2 startup/PIE-start error; the final gameplay/teardown check must not relabel that as an animation failure. BF-114 records prevention.

Evidence: `Saved/Logs/hero-visuals-20260908/` (authored exports, migrations, asset validation, natural samples, current-state checks, and fixture records); `Saved/Logs/cosmetic-ownership-20260908/{before,after}.json`; `codex-hero-visuals-{build,layer-build,bot-build}-20260908.log`; corresponding editor logs. Scripts are `Saved/inspect-hero-visuals-20260908.py`, `migrate-hero-visuals-20260908.py`, `verify-hero-visuals-20260908.py`, `validate-hero-visuals-20260908.py`, and `fix-hero-cosmetic-ownership-20260908.py`.

Packaged proof is pending in a new candidate. Candidate 0804 predates these changes. Same-process PIE does not establish the required one-PIE-plus-three-process topology, second-PC LAN, physical all-kit inputs, or complete placeholder-art integration.
