# Breakaway project architecture overview

Vertical slice of Breakaway: team arena, hero selection, relic capture, and persistable buildables. Gameplay lives in the `BreakawayCore` Game Feature plugin plus per-hero plugins, on top of Lyra.

**Last reviewed:** August 19, 2026  
**Planning hub:** [Planning docs](./README.md)  
**System class tables:** [BreakawayCore systems index](../Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md)  
**Current priorities:** [Project roadmap](./PROJECT_ROADMAP.md)

This document explains how systems fit together. It is not the live task list.

---

## System Map

```mermaid
graph TB
    subgraph Lyra["Lyra Framework (Engine Layer)"]
        LyraGM["ALyraGameMode"]
        LyraGS["ALyraGameState"]
        LyraPS["ALyraPlayerState"]
        LyraPC["ALyraPlayerController"]
        LyraChar["ALyraCharacter"]
        ExpDef["ULyraExperienceDefinition"]
        PhaseSubsys["Game Phase Subsystem"]
        TeamSubsys["ULyraTeamSubsystem"]
        AbilitySet["ULyraAbilitySet"]
        PawnData["ULyraPawnData"]
        AssetMgr["UAssetManager"]
        CommonUI["CommonUI / Activatable Widgets"]
        FrontEnd["B_LyraFrontEnd_Experience"]
    end

    subgraph Bway["BreakawayCore Plugin"]
        subgraph Core["Core Framework"]
            BwGM["ABreakawayGameMode"]
            BwGS["ABwayGameState"]
            BwPS["ABwayPlayerState"]
            BwPC["ABwayPlayerController"]
            BwChar["ABwayCharacterWithAbilities"]
            BwCMC["UBwayCharacterMovementComponent"]
        end

        subgraph GSComps["GameState Components"]
            RoundMgmt["UBwayRoundManagementComponent"]
            Scoring["UBwayScoringComponent"]
            RelicMgr["UBwayRelicManagerComponent"]
            TeamBridge["UBwayTeamBridgeComponent"]
            BuildReg["UBwayBuildableRegistryComponent"]
        end

        subgraph Hero["Hero Systems"]
            HeroData["UBwayHeroDataAsset"]
            HeroReg["UBwayHeroRegistry"]
            HeroSelMgr["UBwayHeroSelectionManager"]
            HeroPhase["UBwayHeroSelectionPhaseComponent"]
            HeroWidget["UBwayHeroSelectWidget"]
        end

        subgraph RelicSys["Relic System"]
            RelicActor["ARelicActor"]
            GoalVol["ABwayGoalVolume"]
            RelicSettings["URelicSettings"]
            RelicMoveRep["URelicMovementReplicationComponent"]
        end

        subgraph SpawnSys["Spawn System"]
            SpawnMgr["UBwaySpawnPointManagerComponent"]
            SpawnPt["ABwaySpawnPoint"]
            SpawnData["UBwaySpawnPointData"]
        end

        subgraph Build["Buildable System"]
            BuildBase["ABuildableActor"]
            Turret["ATurretBase"]
            Trap["ATrapBase"]
            BuildData["UBwayBuildableDataAsset"]
        end

        subgraph Econ["Economy (GAS)"]
            GoldAttr["UBwayGoldAttributeSet"]
        end

        subgraph UILayer["UI Layer"]
            MatchHUD["Slot widgets (score, relic, health, ability bar)"]
            ScoreW["UBwayScoreboardWidget"]
            ResultsW["UBwayResultsScreenWidget"]
            CurrencyW["UBwayCurrencyDisplayWidget"]
            AbilityBar["UBwayAbilityBarHUDWidget"]
            PauseW["UBwayPauseMenuWidget"]
        end

        Exp["B_BW_Experience_CaptureTheRelic"]
    end

    %% Inheritance
    BwGM -->|extends| LyraGM
    BwGS -->|extends| LyraGS
    BwPS -->|extends| LyraPS
    BwPC -->|extends| LyraPC
    BwChar -->|extends| LyraChar

    BwGS --> RoundMgmt
    BwGS --> Scoring
    BwGS --> RelicMgr
    BwGS --> TeamBridge
    BwGS --> BuildReg
    TeamBridge -->|syncs to| TeamSubsys
    HeroPhase -->|integrates with| PhaseSubsys
    Exp -->|loaded by| ExpDef
    AssetMgr -->|ULyraAssetManager scans| HeroData
    RelicMgr -->|spawns/tracks| RelicActor
    GoalVol -->|triggers| RoundMgmt
    SpawnMgr -->|manages| SpawnPt
    BwChar --> BwCMC
    GoldAttr -->|lives on| BwPS
    BuildData -->|referenced by| HeroData
```

---

## System-by-System Breakdown

### 1. Core Framework (Lyra Extension Layer)

These classes extend Lyra's base classes to add Breakaway-specific logic:

| Class | Extends | Role |
|-------|---------|------|
| [`ABreakawayGameMode`](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/BreakawayGameMode.h) | `ALyraGameMode` | Player lifecycle, team assign, hero apply, initial spawns. Round and relic logic live on GameState components. |
| [ABwayGameState](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/BwayGameState.h) | `ALyraGameState` | Replicated match state: teams, scores, round state, round timer, relic tracking. Hosts all `UGameStateComponent`s |
| [ABwayPlayerState](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/BwayPlayerState.h) | `ALyraPlayerState` | Per-player replicated data: hero selection/locking, relic possession, player number |
| [ABwayPlayerController](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/BwayPlayerController.h) | `ALyraPlayerController` | Client-side hero selection UI toggling, cheat manager hookup |
| [ABwayCharacterWithAbilities](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/BwayCharacterWithAbilities.h) | `ALyraCharacter` | In-match pawn with hero data initialization, relic pickup, team appearance, custom movement |

---

### 2. GameState Components (Modular Systems)

Following Lyra's `UGameStateComponent` pattern, these components live on `ABwayGameState` and are individually responsible for specific match concerns:

| Component | File | Role |
|-----------|------|------|
| [UBwayRoundManagementComponent](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/GameState/BwayRoundManagementComponent.h) | Round lifecycle | Start/end rounds, check win conditions (goal scored, team eliminated, timer expired), pre-round delay |
| [UBwayScoringComponent](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/GameState/BwayScoringComponent.h) | Score tracking | Replicated per-team score array, [AddScore()](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/GameState/BwayScoringComponent.h#28-29), [ResetScores()](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/GameState/BwayScoringComponent.h#36-37), broadcasts `OnTeamScoreChanged` |
| [UBwayRelicManagerComponent](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/GameState/BwayRelicManagerComponent.h) | Relic lifecycle | Spawns relic via SpawnPointManager, tracks carrier changes, resets relic between rounds |
| [UBwayTeamBridgeComponent](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/GameState/BwayTeamBridgeComponent.h) | Lyra team sync | Bridges BwayGameState team data → `ULyraTeamSubsystem` |
| [UBwayBuildableRegistryComponent](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/GameState/BwayBuildableRegistryComponent.h) | Buildable registry | Server-side tracking of placed buildables for caps and late-joiner queries |

> **Note:** Round, Scoring, Relic, TeamBridge, and BuildableRegistry are **C++ default subobjects** on `ABwayGameState`. Blueprint GameState subclasses must not duplicate them.

> **Important:** Some round and relic methods still exist on `ABreakawayGameMode` from an older extraction. Prefer the GameState components as the source of truth. Consolidating the leftover GameMode path is tracked as tech debt.

---

### 3. Hero Systems

| Class | File | Role |
|-------|------|------|
| [UBwayHeroDataAsset](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/HeroSystems/BwayHeroDataAsset.h) | `UPrimaryDataAsset` | Data definition for a hero: display name, class, portrait, mesh, animation BP, stats, GAS ability sets, buildable data, voice bank, ability display info |
| [UBwayHeroRegistry](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/HeroSystems/BwayHeroRegistry.h) | `UGameInstanceSubsystem` | Discovers all [HeroDataAsset](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/HeroSystems/BwayHeroSelectWidget.h#171-172) via Asset Manager. Provides [GetAllHeroSoftObjects()](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/HeroSystems/BwayHeroRegistry.h#28-29) and [GetHeroDataById()](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/HeroSystems/BwayHeroRegistry.h#36-37) |
| [UBwayHeroSelectionManager](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/HeroSystems/BwayHeroSelectionManager.h) | `UGameStateComponent` | Tracks per-player selection state, enforces rules (no duplicate heroes per team), manages selection timer, broadcasts readiness |
| [UBwayHeroSelectionPhaseComponent](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/HeroSystems/BwayHeroSelectionPhaseComponent.h) | `UGameStateComponent` | Phase orchestrator: integrates with Lyra's Game Phase system, shows/hides UI, assigns defaults, spawns heroes, advances to next phase |
| [UBwayHeroSelectWidget](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/HeroSystems/BwayHeroSelectWidget.h) | `UCommonActivatableWidget` | C++ base for hero selection UI. Provides Blueprint-callable API for selecting/locking heroes and Blueprint-implementable events for UI updates |

**Hero selection data flow (direct PIE):**

```
HeroRegistry ──discovers──→ HeroDataAssets
       ↓
HeroSelectWidget ──reads──→ HeroRegistry
       ↓ (user picks)
HeroSelectWidget ──calls──→ BwayPlayerState::ServerSetSelectedHeroId()
       ↓ (replicates)                              ↓
HeroSelectionManager ←──listens──→ BwayPlayerState.OnSelectedHeroChanged
       ↓ (all ready?)
GameMode::ApplyHeroDataToNewPawn()
```

**Production path (Section 3):** front-end tile → `L_BW_HeroSelect_Staging` → lock → seamless travel → match map. Round management does **not** run an in-match `HeroSelection` phase on that path. Direct PIE uses `Hero=` URL or auto hero UI, then holds Warmup until humans lock. For more information, see [Hero select staging setup](../Plugins/GameFeatures/BreakawayCore/Docs/HeroSelect_Staging_Setup.md) and [Core loop implementation plan](../Plugins/GameFeatures/BreakawayCore/Docs/CoreLoop_Implementation_Plan.md) Section 3 principles.

---

### 4. Relic System

| Class | File | Role |
|-------|------|------|
| [ARelicActor](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Relic/RelicActor.h) | Core relic | 8 states (`Neutral`, `Carried`, `Dropped`, `Thrown`, `BeingPassed`, `PendingRequest`, [Scoring](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/BwayGoalVolume.h#53-54), `Resetting`). Has its own ASC, grants carrier abilities via `ULyraAbilitySet`, physics-based throw/pass with network replication |
| [URelicSettings](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Relic/RelicSettings.h) | Data asset | Configuration: throw/pass force, socket names, VFX/SFX references, pickup rules |
| [URelicMovementReplicationComponent](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Relic/RelicMovementReplicationComponent.h) | Component | Smooth network replication of physics-based relic movement |
| [ABwayGoalVolume](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/BwayGoalVolume.h) | Trigger volume | Detects relic overlap with goal area → calls `RelicActor::OnEnteredGoal()` → triggers scoring |

---

### 5. Spawn System

| Class | Role |
|-------|------|
| [UBwaySpawnPointManagerComponent](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/SpawnSystem/BwaySpawnPointManagerComponent.h) | Auto-discovers all `ABwaySpawnPoint` actors, provides tag-based/team-based spawn queries, spawns/despawns objects at points |
| [ABwaySpawnPoint](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/SpawnSystem/BwaySpawnPoint.h) | Level-placed actor with gameplay tags defining its role (relic spawn, team spawn, goal spawn, etc.) |
| [UBwaySpawnPointData](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/SpawnSystem/BwaySpawnPointData.h) | Data asset attached to spawn points defining what to spawn and associated configuration |

---

### 6. Buildable System

| Class | Role |
|-------|------|
| [ABuildableActor](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Buildable/BuildableBase.h#L16) | Base class extending `ABwayActorWithAbilitiesAndHealth` — has build time, invulnerability during build, round persistence |
| [ATurretBase](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Buildable/BuildableBase.h#L91) | AI Perception-driven auto-targeting turret with configurable fire rate and attack radius |
| [ATrapBase](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Buildable/BuildableBase.h#L147) | Trigger-based trap with overlap detection and [ApplyTrapEffect()](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Buildable/BuildableBase.h#171-172) |
| [UBwayBuildableDataAsset](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Buildable/BuildableBase.h#L70) | Data asset defining buildable class, meshes. Referenced by `UBwayHeroDataAsset` |

---

### 7. Economy System (GAS-driven)

[UBwayGoldAttributeSet](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Economy/BwayGoldAttributeSet.h) — lives on the PlayerState's ASC:
- [CurrentGold](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Economy/BwayGoldAttributeSet.h#57-58) / [MaxGold](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Economy/BwayGoldAttributeSet.h#60-61) / [GoldPerSecond](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Economy/BwayGoldAttributeSet.h#63-64) (passive income)
- Modified exclusively via GameplayEffects
- Displayed on the match HUD gold slot
- **Not** spent on buildables in the vertical slice (once-per-round free placement). Spend is deferred to post-slice items. For more information, see [Economy — Gold](../Plugins/GameFeatures/BreakawayCore/Docs/Economy_Gold.md).

---

### 8. Movement System

[UBwayCharacterMovementComponent](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/BwayCharacterMovementComponent.h) — extends Lyra's `ULyraCharacterMovementComponent` with:
- Slide mechanic with FOV changes
- Wall running with gravity curves
- Custom movement modes

---

### 9. Content / Data-Driven Configuration

| Asset | Path | Role |
|-------|------|------|
| Experience Definition | `Content/Experiences/B_BW_Experience_Dev` (core-loop harness); `B_BW_Experience_CaptureTheRelic` (playlist) | GameMode, PawnData, action sets, `GameFeaturesToEnable`, match-flow action |
| Game Phases | `Content/Experiences/Phases/` | `BW_Phase_Warmup` → `BW_Phase_HeroSelection` → `BW_Phase_Playing` → `BW_Phase_PostRound` → `BW_Phase_PostGame` |
| GameMode BP | `Content/GameModes/B_BWayGameMode_Default` | Blueprint subclass of `ABreakawayGameMode` with configured defaults |
| GameState BP | `Content/GameModes/BP_BW_GameState` | Blueprint subclass with component configuration |
| PawnData | `Content/Characters/DA_BW_PawnData_Humanoid` | Points to character class, ability sets, input config |
| Maps | `Content/Maps/L_BW_DevMap`, `L_BW_Dorado` | Level assets with placed spawn points and goal volumes |

---

## Game flow: main menu → match → main menu

### Match entry (Section 3)

```
Production:  Front-end tile → L_BW_HeroSelect_Staging → lock → SeamlessTravel → L_BW_DevMap
Direct PIE:  DevMap → (Hero= URL | auto hero UI) → lock → RM Warmup → Playing
Regression:  ForceHumanoid=1 → humanoid only, relic loop unchanged
```

The following sections describe the systems that run after the match map loads. The in-match Lyra `HeroSelection` phase still exists on GameState, but Section 3 production travel skips it (`SkipHeroSelection=1` after staging).

### Phase 1: Main Menu (Lyra Frontend)

```
Engine Boot
  └→ ULyraAssetManager — registers primary asset types (incl. Breakaway paths in DefaultGame.ini)
  └→ Lyra loads B_LyraFrontEnd_Experience
       └→ L_LyraFrontEnd map loads
       └→ W_LyraFrontEnd widget displayed           — experience selection screen
       └→ Player selects "Capture the Relic"
            └→ Lyra's session/travel system initiates server travel
```

**Systems involved:** `ULyraAssetManager`, `ULyraExperienceDefinition`, `UCommonSession` (Lyra)

---

### Phase 2: Experience Loading & Initialization

```
Server Travel to L_BW_Dorado (or L_BW_DevMap)
  └→ ABreakawayGameMode::InitGame()
       └→ Reads map options
  └→ ABreakawayGameMode::InitGameState()
       └→ ABwayGameState is created
       └→ GameState components (C++ defaults on ABwayGameState):
            ├── UBwayRoundManagementComponent
            ├── UBwayScoringComponent
            ├── UBwayRelicManagerComponent
            ├── UBwayTeamBridgeComponent
            ├── UBwayBuildableRegistryComponent
            ├── UBwayHeroSelectionManager
            ├── UBwayHeroSelectionPhaseComponent
            ├── UBwayBotCreationComponent
            └── UBwaySpawnPointManagerComponent (added by GameMode)
  └→ B_BW_Experience_CaptureTheRelic loaded by Lyra
       └→ Injects action sets (LAS_BW_SharedInput)
       └→ Configures game phases
  └→ UBwaySpawnPointManagerComponent::BeginPlay()
       └→ DiscoverSpawnPoints() — finds all ABwaySpawnPoint in the level
  └→ UBwayTeamBridgeComponent::BeginPlay()
       └→ Binds to team change events on BwayGameState
       └→ SyncAllTeamsToLyra()
```

**Systems involved:** `ABreakawayGameMode`, `ABwayGameState`, all `UGameStateComponent`s, `ULyraExperienceDefinition`, `UBwaySpawnPointManagerComponent`

---

### Phase 3: Player Joins & Team Assignment

```
Player connects
  └→ ABreakawayGameMode::PostLogin(PlayerController)
       └→ ABwayPlayerState created (from BP_BW_PlayerState)
       └→ AssignPlayerToTeam() — auto-balances teams
            └→ ABwayGameState::AddPlayerToTeam()
            └→ UBwayTeamBridgeComponent::SyncPlayerTeamToLyra()
       └→ UBwayHeroSelectionManager::RegisterPlayer()
```

**Systems involved:** `ABreakawayGameMode`, `ABwayPlayerState`, `ABwayGameState`, `UBwayTeamBridgeComponent`, `UBwayHeroSelectionManager`

---

### Phase 4: Hero Selection Phase

```
Lyra Game Phase system activates "HeroSelection" phase
  └→ UBwayHeroSelectionPhaseComponent::HandleLyraPhaseActivated()
       └→ StartHeroSelectionPhase()
            └→ UBwayHeroSelectionManager::StartHeroSelection()
            └→ ShowHeroSelectionUI() — creates UBwayHeroSelectWidget on each client
  
Client-side:
  └→ UBwayHeroSelectWidget::NativeOnActivated()
       └→ Queries UBwayHeroRegistry for all available heroes
       └→ Displays hero grid with portraits, stats, abilities
  └→ Player clicks a hero
       └→ SelectHero() → ABwayPlayerState::ServerSetSelectedHeroId() (Server RPC)
            └→ Replicates SelectedHeroId to all clients
            └→ Broadcasts OnSelectedHeroChanged
            └→ UBwayHeroSelectionManager::UpdatePlayerSelection()
  └→ Player clicks "Lock In"
       └→ LockSelection() → ABwayPlayerState::ServerLockHeroSelection() (Server RPC)
            └→ UBwayHeroSelectionManager checks AreAllPlayersReady()

When all ready (or timer expires):
  └→ UBwayHeroSelectionManager::OnAllPlayersReady broadcast
       └→ UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady()
            └→ AssignDefaultHeroes() — for any player who didn't pick
            └→ SpawnHeroesForAllPlayers()
                 └→ For each player:
                      └→ GameMode::RestartPlayer() → spawns ABwayCharacterWithAbilities
                      └→ GameMode::ApplyHeroDataToNewPawn()
                           └→ ABwayCharacterWithAbilities::InitializeHeroData()
                                └→ Sets mesh, animation BP, grants ability sets
            └→ HideHeroSelectionUI()
            └→ EndPhaseAndProgressToNext() — tells Lyra to advance phases
```

**Systems involved:** `UBwayHeroSelectionPhaseComponent`, `UBwayHeroSelectionManager`, `UBwayHeroSelectWidget`, `UBwayHeroRegistry`, `ABwayPlayerState`, `ABreakawayGameMode`, [ABwayCharacterWithAbilities](../Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/BwayCharacterWithAbilities.h#18-19), Lyra's Game Phase system

---

### Phase 5: Round Start & Active Gameplay

```
Phase advances to "Playing"
  └→ ABreakawayGameMode::StartRound() (or UBwayRoundManagementComponent::StartRound())
       └→ ABwayGameState::SetRoundState(RoundActive)
       └→ UBwayRelicManagerComponent::SpawnRelic()
            └→ Queries SpawnPointManager for relic spawn tag
            └→ Spawns ARelicActor at spawn point
            └→ ARelicActor::InitializeRelicData(RelicSettings)
       └→ Round timer starts (default 180s)
       └→ Broadcasts OnRoundStarted

During gameplay:
  ├── UBwayGoldAttributeSet — passive gold income via GoldPerSecond
  ├── ABwayCharacterWithAbilities — movement, abilities, relic interaction
  ├── ARelicActor — state machine (Neutral→Carried→Dropped→Thrown→etc.)
  │    └→ OnPickedUp() — attaches to carrier, grants carrier abilities
  │    └→ OnDropped() — physics enabled, pickup cooldown
  │    └→ Server_ThrowRelic() / Server_PassRelic() — network RPCs
  ├── ABwayGoalVolume — overlap detection for scoring
  └── ABuildableActor / ATurretBase / ATrapBase — placed structures
```

**Systems involved:** All gameplay systems active simultaneously (Relic, Scoring, Round Management, Economy, Buildables, Abilities, Movement)

---

### Phase 6: Win Condition & Round End

Three possible win conditions trigger `EndRound()`:

```
1. Goal Scored:
   ARelicActor enters ABwayGoalVolume
     └→ ABwayGoalVolume::OnGoalOverlapBegin()
          └→ ARelicActor::OnEnteredGoal(ScoringTeam)
          └→ UBwayRoundManagementComponent::OnRelicScored(ScoringTeam)
               └→ UBwayScoringComponent::AddScore()
               └→ EndRound(ScoringTeam, GoalScored)

2. Team Eliminated:
   Player dies → ABreakawayGameMode::OnPlayerDied()
     └→ ABwayGameState::OnPlayerDied() — updates alive count
     └→ UBwayRoundManagementComponent::CheckTeamElimination()
          └→ If all players on a team dead → EndRound(OtherTeam, TeamEliminated)

3. Timer Expired:
   Round timer reaches 0
     └→ UBwayRoundManagementComponent::OnRoundTimerExpired()
          └→ DetermineRelicPossessionTeam()
          └→ EndRound(PossessingTeam, TimeExpired)
```

```
EndRound():
  └→ ABwayGameState::SetRoundState(RoundEnding)
  └→ Broadcasts OnRoundEnded
  └→ Phase advances to "PostRound" (BW_Phase_PostRound)
       └→ Shows round results
  └→ CheckMatchEnd() — has a team reached PointsToWin (default 3)?
       ├── No → ResetRoundState() → StartRound() (next round)
       └── Yes → Phase advances to "PostGame" (BW_Phase_PostGame)
```

**Systems involved:** `UBwayRoundManagementComponent`, `UBwayScoringComponent`, `ARelicActor`, `ABwayGoalVolume`, `ABwayGameState`, Lyra's Game Phase system

---

### Phase 7: Match End & Return to Menu

```
PostGame phase:
  └→ BW_Phase_PostGame activates
       └→ Shows UBwayResultsScreenWidget (final scores, MVP, etc.)
       └→ After delay or player confirmation
            └→ Server travel back to L_LyraFrontEnd
            └→ B_LyraFrontEnd_Experience loads
            └→ Player is back at the main menu
```

**Systems involved:** Lyra's Game Phase system, `UBwayResultsScreenWidget`, Lyra's session/travel system

---

## Architectural notes

### What works well

- Lyra integration: `UGameStateComponent`, `UPrimaryDataAsset`, `UGameInstanceSubsystem`, and `UCommonActivatableWidget`
- Data-driven heroes: `UBwayHeroDataAsset` plus per-hero Game Feature plugins
- Relic state machine with custom movement replication
- GAS economy: attribute-set gold with GameplayEffect modifications

### Vertical slice readiness (August 19, 2026)

| Dimension | Status |
|-----------|--------|
| C++ match loop | Complete for DevMap core loop (Steps 0–11) |
| Relic / rounds / gold | Functional. Fumble-on-damage is Step 22. |
| Buildable persistence | C++ flag, registry, once-per-round free placement |
| 4 heroes | Argus, Alona, Korryn, Rawlins C++ kits + one buildable each |
| Match HUD | Steps 12–16 and 19.5 complete |
| 4v4 listen + bots | Bot scaler to 8 implemented |
| Editor content | `.uasset` not in git; [Blueprint asset audit](./BLUEPRINT_ASSET_AUDIT.md) is stale |
| Documentation | [Planning docs](./README.md) + [systems index](../Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md) |

See [Vertical slice definition](./VERTICAL_SLICE_DEFINITION.md).

### Known architectural debt

**Dual responsibility between GameMode and GameState components:** leftover round and relic methods on `ABreakawayGameMode` overlap `UBwayRoundManagementComponent` and `UBwayRelicManagerComponent`. Treat components as authoritative.

**Duplicate replicated data:** `ABwayGameState` still holds teams, scores, relic references, and round state that also live on scoring and relic components. Consolidate over time.

### Multiplayer and listen server

Server-authoritative logic on GameMode and GameState components is the same for listen server and dedicated server. Replication is a no-op in standalone PIE. Skip hero apply with `ForceHumanoid=1`. Bots fill empty slots through `UBwayBotCreationComponent`.
