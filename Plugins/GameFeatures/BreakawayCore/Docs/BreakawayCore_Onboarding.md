# Onboard to BreakawayCore

Use this tutorial to build a working mental model of the BreakawayCore codebase, assets, Game Feature Plugins, and verification workflow. You do not need to implement a feature to complete it.

**Last reviewed:** September 6, 2026

## Know what you will learn

By the end of this tutorial, you can:

- Locate the project, Lyra modules, BreakawayCore runtime module, and hero plugins.
- Translate disk paths such as `Plugins/GameFeatures/BreakawayCore/Content` into Unreal paths such as `/BreakawayCore/`.
- Start a deterministic DevMap session and identify the active experience.
- Trace a player from login to hero data, ability grants, and pawn visuals.
- Trace the relic from spawn to pickup, scoring, round reset, and replication.
- Choose the smallest correct owner for a code or content change.
- Run the verification tier that matches your change.

For the full architecture map, see [BreakawayCore Game Feature Plugin systems](./BreakawayCore_GFP_Systems.md). For project priorities, see the [vertical slice status ledger](../../../../AI_Planning/SLICE_STATUS.md).

## Prepare your workspace

You need the engine version resolved by `ProjectB.uproject`, a Windows Unreal development environment, and access to the Lyra source and content used by the project. Repository test evidence refers to Unreal Engine 5.8.1.

Before you open or edit assets, inspect the worktree. Existing changes can belong to another task.

```powershell
Set-Location 'E:\Unreal Projects\ProjectB'
git status --short
```

The root project modules are `LyraGame` and `LyraEditor`. BreakawayCore and the hero modules are plugin modules, so the native editor target is `LyraEditor`, not `ProjectBEditor`.

## Read the repository in order

Open these documents in sequence:

1. [Planning hub](../../../../AI_Planning/README.md) for project scope, status boundaries, and document ownership.
2. [BreakawayCore Game Feature Plugin systems](./BreakawayCore_GFP_Systems.md) for the canonical system map.
3. [Lyra integration](./Lyra_Integration.md) for the foundation that BreakawayCore extends.
4. [Match flow and phases](./MatchFlow_and_Phases.md) for the experience and round lifecycle.
5. [GAS and abilities](./GAS_and_Abilities.md) for the ASC, ability sets, tags, and placement rules.
6. [Relic system](./Relic_System.md), [Buildable system](./Buildable_System.md), and [UI system](./UI_System.md) for the main vertical-slice domains.
7. The relevant [hero plugin setup page](../../Heroes/README.md) when you work on a roster hero.

The planning hub owns priority and status. BreakawayCore docs own system behavior and editor contracts. Avoid copying status claims from an old slice checklist into a new change.

## Compile the native code

Run the compile gate after a C++ change. It uses the project resolver and the `LyraEditor` Win64 Development target.

```powershell
Set-Location 'E:\Unreal Projects\ProjectB'
.Scripts\Test\Run-Tier.ps1 -Tier 1 -ValidateOnly
.Scripts\Test\Run-Tier.ps1 -Tier 1
```

If engine discovery fails, set the project-specific `UE_ENGINE_ROOT` variable to the directory that contains `Engine/`, then rerun the gate.

```powershell
$env:UE_ENGINE_ROOT = 'E:\Github\UnrealEngine'
.Scripts\Test\Run-Tier.ps1 -Tier 1
```

Close the editor gracefully before a full rebuild when reflected headers or classes changed. Preserve unsaved editor work. A successful compile proves native code compatibility; it does not prove PIE, replication, or packaged behavior.

## Start the editor and find the first map

The editor startup map is `/BreakawayCore/Maps/L_BW_DevMap`. The normal project game default map is `/Game/System/FrontEnd/Maps/L_LyraFrontEnd`.

In the editor:

1. Open `ProjectB.uproject` with the engine resolved by the project configuration.
2. Let the editor finish loading plugins and Asset Manager scans.
3. Open `L_BW_DevMap` from the Content Browser if the startup map is not already active.
4. Use the World Settings and Play settings to identify the selected experience and net mode.
5. Use Listen Server for networking, team, relic, and hero checks. Standalone is sufficient for a first asset and match-flow smoke test.

The core experience assets are under `/BreakawayCore/Experiences/`:

| Asset | Role |
|---|---|
| `B_BW_Experience_Dev` | Development harness for direct PIE and short-match checks. |
| `B_BW_Experience_CaptureTheRelic` | Capture-the-Relic experience used by the mode flow. |
| `B_BW_Experience_HeroSelectStaging` | Hero-select staging experience. |
| `EAS_BW_CaptureTheRelic` | Match HUD and Capture-the-Relic feature actions. |
| `LAS_BW_SharedInput` | Shared input and common gameplay actions. |
| `LAS_BW_StandardComponents` | Standard component setup for the experience. |
| `LAS_BW_MatchHUD` | Match HUD layout and slot injection. |
| `DA_BW_MatchFlow_Dev` | Phase abilities, durations, match rules, and development bot default. |

## Run a deterministic DevMap session

Use the following URL in the Play settings when you want a short, focused match-flow session:

```text
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=3&PointsToWin=1&WarmupDuration=5&PostRoundDuration=3&RoundDuration=90
```

This URL selects the development experience, skips the staging selection flow, fills three bot slots, and shortens the match. The `ShortMatch` profile in `Scripts/Test/BwayTestConfig.psd1` is the machine-readable equivalent.

Use this baseline when the change is unrelated to hero presentation:

```text
L_BW_DevMap?Experience=B_BW_Experience_Dev&ForceHumanoid=1&SkipHeroSelection=1&NumBots=3&PointsToWin=1
```

`ForceHumanoid=1` keeps the common humanoid path active so you can isolate relic, round, score, or networking regressions from hero plugin setup.

When the change is about hero selection, use the staging map and the relevant hero plugins. Direct PIE can also use a `Hero=` URL or the direct-play hero UI. The active RM match phase sequence does not include a `HeroSelection` enum value.

## Trace the player startup path

Use this exercise to connect the major classes.

1. Open [`BreakawayGameMode.h`](../Source/BreakawayCoreRuntime/Public/BreakawayGameMode.h). Note that its contract is player lifecycle, team assignment, pawn restart, hero application, and initial world-object spawning.
2. Open [`BwayGameState.h`](../Source/BreakawayCoreRuntime/Public/BwayGameState.h). Find the C++ default subobject references for round management, scoring, relic, team bridge, buildables, hero selection, and combat feedback.
3. Open [`BwayPlayerState.h`](../Source/BreakawayCoreRuntime/Public/BwayPlayerState.h). Identify replicated hero selection, lock state, relic flags, player number, match stats, and the upgrade component.
4. Open [`BwayCharacterWithAbilities.h`](../Source/BreakawayCoreRuntime/Public/BwayCharacterWithAbilities.h). Follow `InitializeHeroData()` in the matching `.cpp` file.
5. In the editor, open a hero data asset, such as `/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona`.
6. Compare its mesh, animation Blueprint, stats, attribute-set class, ability sets, buildable data asset, and movement configuration with the fields in `BwayHeroDataAsset.h`.
7. Follow the ability-set references into the hero plugin's `Abilities/` folder. Each ability grant maps an input tag to a native or Blueprint ability class.

The server applies hero data after the pawn restarts. It grants hero sets to the persistent PlayerState ASC. Clients use the replicated hero ID to load the same data and apply visual state.

## Trace the relic loop

Use a listen-server DevMap session for this exercise.

1. Open `/BreakawayCore/GameModes/BallMode/Relic/DA_BW_RelicSpawn` and `/BreakawayCore/GameModes/BallMode/Relic/DA_BW_RelicSettings_Default`.
2. Inspect the goal data assets under `/BreakawayCore/GameModes/BallMode/Gameplay/Goals/`.
3. Open [`BwaySpawnPoint.h`](../Source/BreakawayCoreRuntime/Public/SpawnSystem/BwaySpawnPoint.h) and [`BwaySpawnPointData.h`](../Source/BreakawayCoreRuntime/Public/SpawnSystem/BwaySpawnPointData.h).
4. Open [`BwayRelicManagerComponent.h`](../Source/BreakawayCoreRuntime/Public/GameState/BwayRelicManagerComponent.h). It owns the active relic and reset operations.
5. Open [`RelicActor.h`](../Source/BreakawayCoreRuntime/Public/Relic/RelicActor.h). Follow `CurrentState`, `CurrentCarrier`, `LastPossessingTeam`, and `bHasScoredThisRound`.
6. Pick up, drop, throw, or pass the relic in PIE. Observe the HUD and the `OnRep` reactions on the relic and PlayerState.
7. Enter a goal with the relic. Follow `ABwayGoalVolume` to `ARelicActor::OnEnteredGoal()`, then to `UBwayRoundManagementComponent::OnRelicScored()` and `UBwayScoringComponent::AddScore()`.
8. Start another round and verify that the relic resets while persistent buildables remain unless destroyed.

For a network exercise, enable packet lag and repeat throw, pass, pickup, and fumble. The relic uses `URelicMovementReplicationComponent`; do not use standard actor movement replication as a diagnostic shortcut.

## Explore the asset tree

Start with these folders in the Content Browser:

| Unreal path | What to inspect |
|---|---|
| `/BreakawayCore/Experiences/` | Experience definitions, action sets, and phases. |
| `/BreakawayCore/GameModes/` | GameMode and GameState Blueprint defaults. |
| `/BreakawayCore/GameModes/BallMode/` | Relic, goals, scoring, and carrier ability assets. |
| `/BreakawayCore/Characters/` | Humanoid pawn data, common ability set, and character Blueprints. |
| `/BreakawayCore/Input/` | Input Actions and Mapping Contexts. |
| `/BreakawayCore/UI/Match/` | Score, relic, health, portraits, ability bar, gold, and result widgets. |
| `/BreakawayCore/Maps/` | DevMap, Dorado, frontend, and hero-select staging. |
| `/Hero_Spartacus/Characters/Heroes/Argus/` | Argus data, abilities, buildable, and UI. |
| `/Hero_Alona/Characters/Heroes/Alona/` | Alona data, abilities, Sun Shrine, and UI. |
| `/Hero_Morgan/Characters/Heroes/Hexweaver/` | Korryn data, abilities, Cursed Ward, and UI. |
| `/Hero_Rawlins/Characters/Heroes/Gunslinger/` | Rawlins data, abilities, Jail, and UI. |

When an asset is missing, check three things in order:

1. The disk folder exists under the expected plugin.
2. The asset uses the expected Unreal mount.
3. The owning plugin is registered and active for the current experience.

For primary asset discovery, inspect `Config/DefaultGame.ini`. It registers map, experience, action-set, hero, match-flow, movement, combat-readability, and hero kit primary asset types.

## Make a safe first change

Start with a URL-only experiment. It changes no asset and teaches you where match-flow settings are resolved.

1. Launch the DevMap with `PointsToWin=1`, `WarmupDuration=2`, and `RoundDuration=30`.
2. Observe the resolved values in the Breakaway match-flow log and the round HUD.
3. Change one value, restart the session, and confirm that only the expected phase or rule changed.
4. Remove the temporary URL overrides before you evaluate the authored defaults.

For a content-only change, choose one existing Blueprint child of a C++ widget base and change a presentation property. Keep the parent class, BindWidget names, slot tag, and action-set registration unchanged. Test the same map after a cold editor restart when the change touches plugin registration, reflected classes, or `DefaultGame.ini`.

For a native change, identify the state owner before editing:

| Change | Start with |
|---|---|
| Round, phase, timer, or win rule | `UBwayRoundManagementComponent` and `UBwayMatchFlowConfig`. |
| Team score | `UBwayScoringComponent`. |
| Relic state or carrier transition | `ARelicActor` and `UBwayRelicManagerComponent`. |
| Hero data or grants | `UBwayHeroDataAsset` and `ABwayCharacterWithAbilities`. |
| Hero-specific behavior | The owning hero runtime module under `Plugins/GameFeatures/Heroes/`. |
| Buildable placement | `UBwayGameplayAbility_PlaceBuildable`, the targeting actor, and `UBwayBuildablePlacementLibrary`. |
| HUD data binding | The relevant C++ widget base and its Blueprint child. |
| Bot behavior | The `AI/` source folder and `/BreakawayCore/Bots/` assets. |

Do not add the same state to the GameMode, GameState, a Blueprint default subobject, and a Data Asset. Choose one runtime owner and one authored definition.

## Add a new hero GFP

Use this procedure when the change is a new roster hero, not a variant of an existing hero.

1. Copy an existing hero plugin folder under `Plugins/GameFeatures/Heroes/` and rename the plugin, module, and content mounts consistently.
2. Keep the plugin dependent on BreakawayCore. Add `GameplayAbilities` when the runtime module uses GAS types directly.
3. Create a `UBwayGameFeatureData` asset at the plugin root. Add the hero data asset to its `HeroDataAssets` array.
4. Create a `UBwayHeroDataAsset`, an ability set, and the hero content under `Content/Characters/Heroes/<Codename>/`.
5. Add the hero plugin's `HeroDataAsset` scan path to `Config/DefaultGame.ini`.
6. Add the plugin to the target experience `GameFeaturesToEnable` list. Use one hero at a time while proving a new integration.
7. Add native abilities or buildables to the plugin runtime module. Keep shared rules in BreakawayCore.
8. Compile `LyraEditor`, cold-start the editor, and confirm the GFD, hero registry, hero selection, pawn visuals, and ability grants.
9. Run listen-server PIE with the hero selected, then run the relevant hero setup and parity checklist.

For the existing roster's naming conventions, use [Hero Game Feature plugins](../../Heroes/README.md). The plugin folders are not the official display names: `Hero_Spartacus` presents Argus, `Hero_Morgan` presents Korryn, and the codename folders are part of the asset path contract.

## Match a change to a verification tier

Use the lowest tier that proves the changed boundary, then add gameplay evidence when the change affects runtime behavior.

| Change | Minimum evidence |
|---|---|
| C++ header or implementation | Tier 1 `LyraEditor` compile. |
| Data Asset, Blueprint, action set, input, or phase | Cold-start PIE on `L_BW_DevMap`. |
| Match state, score, relic, buildable, ability authority, or team logic | Listen-server PIE with a client or bots. |
| Relic movement or RPC behavior | Listen-server PIE with packet lag, then a separate-client check. |
| Experience, plugin activation, or Asset Manager settings | Cold-start PIE and packaged standalone smoke. |
| Packaging or cooked-content change | Packaged `ShortMatch` profile and acceptance summary. |

The live scripts are under `Scripts/Test/`:

```powershell
Set-Location 'E:\Unreal Projects\ProjectB\Scripts\Test'
.Run-Tier.ps1 -Tier 1
```

For packaged work, use the project workflow recorded in `Scripts/Test/README.md`:

```powershell
Set-Location 'E:\Unreal Projects\ProjectB\Scripts\Test'
.Package-Preflight.ps1 -Candidate '<candidate-name>' -OutputJson '.\preflight.json'
.New-AcceptanceSummary.ps1 -Candidate '<candidate-name>' -EvidenceLog 'E:\Unreal Projects\ProjectB\Saved\Logs\packaged.log' -OutputJson '.\acceptance.json'
```

Replace `<candidate-name>` with a unique local candidate identifier. Packaging still requires the project-specific build and cook step between preflight and acceptance summary.

Keep these results separate:

- Compile pass: native code built.
- Editor or PIE pass: the selected experience loaded and the behavior worked in that session.
- Multiplayer pass: authority, replication, and client presentation worked across the tested roles.
- Packaged pass: the cooked build contained and executed the required assets.

## Use the common troubleshooting paths

### The expected experience does not load

Confirm that the URL names `B_BW_Experience_Dev` or the intended production experience. Confirm that the map, experience, and `GameFeatureData` primary asset types are registered in `Config/DefaultGame.ini`. If match-flow values are missing, confirm that `UBwayGameFeatureAction_MatchFlowConfig` is attached to the experience Actions list. Do not reparent the experience Blueprint.

### The hero list is empty

Confirm that the hero plugin is registered and active, its GFD contains the hero data asset, and its scan path is present. Then cold-start the editor and reopen the experience. Check the Unreal mount, not just the Windows folder name.

### A hero pawn has the wrong mesh or no abilities

Confirm that `UBwayHeroDataAsset::HeroMesh`, `AnimationBP`, `AttributeSetClass`, and `AbilitySets` are set. Confirm that the ability set uses the expected `InputTag.Ability.*` tags. Check the server log from `InitializeHeroData()` and verify that the PlayerState ASC exists.

### The relic or goals do not spawn

Confirm the spawn data assets, actor classes, spawn tags, placed spawn points, and authority world. `ABreakawayGameMode` adds the spawn manager during GameState initialization and defers initial spawning until the next tick.

### Placement starts but does not finish

Confirm the per-hero PlaceBuildable grant, the common confirm and cancel grants, the placement target actor, and the `State.BuildablePlacement` and `Ability.Buildable.*` tags. Test rejected, canceled, and successful submissions. A rejected submission must end the placement ability and clear the session.

### The HUD is empty or duplicated

Confirm the C++ parent class, required BindWidget names, slot tag, action-set registration, and the active HUD layout. Do not add the deprecated monolithic core HUD alongside the slot widgets. Check the current HUD setup guide after a cold editor restart.

### A multiplayer test looks correct only on one machine

Check authority first. A local Blueprint event or dispatcher does not replicate. Confirm the server RPC, replicated property, `OnRep` callback, and client-side presentation binding separately. For relic movement, inspect the custom movement component and packet-lag behavior.

## Use the contribution checklist

Before you hand off a change, confirm the following:

- The state owner and lifetime are documented in the code or the relevant system page.
- The change follows the dependency direction from Lyra to BreakawayCore to optional hero plugins.
- The Unreal asset mount and disk path agree.
- Optional content uses a feature dependency or soft reference instead of an accidental core hard reference.
- C++ changes pass the Tier 1 compile gate.
- Content and runtime changes pass the relevant PIE or listen-server check.
- Replication changes include server, owning-client, observing-client, late-join, and teardown checks where applicable.
- The correct canonical document is updated when a contract, asset path, tag, or verification requirement changes.
- Temporary URL overrides, debug flags, and editor-only assets are not part of the final claim.

## Learn the key terms

| Term | Meaning |
|---|---|
| GFP | Game Feature Plugin. A plugin whose content and code can be registered and activated through Lyra's Game Features system. |
| GFD | Game Feature Data. The feature asset that can register hero data and other feature actions. |
| Experience | A `ULyraExperienceDefinition` asset that composes game mode, pawn data, action sets, and Game Feature plugins. |
| Action set | A Lyra data asset that injects input, components, widgets, or other feature actions into an experience. |
| ASC | Ability System Component. In this project, the PlayerState ASC is the persistent owner for player abilities and attributes. |
| RM | `UBwayRoundManagementComponent`, the canonical match-flow and round orchestrator. |
| Primary asset | An Asset Manager-discoverable asset with a stable type and ID, such as `HeroDataAsset` or `BwayMatchFlowConfig`. |
| Display team | The HUD's friendly or enemy slot. It can differ from authoritative game team index `0` or `1`. |
