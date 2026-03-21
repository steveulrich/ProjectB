# Breakaway Vertical Slice: Blueprint Integration Guide

This guide walks you through the remaining Blueprint/Editor work to complete your Vertical Slice.

---

## Phase 1: Compile and Verify C++ Code

### Step 1.1: Compile the Project
1. Open your project in Unreal Editor or Visual Studio
2. Build the project (Ctrl+Shift+B in VS or click Compile in UE)
3. Verify no compilation errors

### Step 1.2: Verify New Classes Exist
1. In Content Browser, click **Settings** → **Show Engine Content** and **Show Plugin Content**
2. Open the Class Viewer (Window → Developer Tools → Class Viewer)
3. Search for and confirm these classes exist:
   - `BwayCoreHUDWidget`
   - `BwayScoreboardWidget`
   - `BwayPauseMenuWidget`
   - `BwayResultsScreenWidget`

---

## Phase 2: Create Core HUD Widget

### Step 2.1: Create the Blueprint Widget
1. Navigate to `Plugins/GameFeatures/BreakawayCore/Content/UI/`
2. Right-click → **User Interface** → **Widget Blueprint**
3. In the parent class picker, search for `BwayCoreHUDWidget`
4. Name it `WBP_BW_CoreHUD`

### Step 2.2: Design the HUD Layout
Open `WBP_BW_CoreHUD` and create this layout in the Designer:

```
┌─────────────────────────────────────────────────────────────┐
│  [Team 1 Score]          [Timer]           [Team 2 Score]   │  ← Top bar
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                      (Gameplay Area)                        │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│  [Health Bar]        [Relic Status]     [Ability Icons]     │  ← Bottom bar
└─────────────────────────────────────────────────────────────┘
```

**Required UI Elements:**

| Element | Widget Type | Binding |
|---------|-------------|---------|
| Health Bar | Progress Bar | `GetHealthPercent()` |
| Team 1 Score | Text Block | `GetTeam1Score()` |
| Team 2 Score | Text Block | `GetTeam2Score()` |
| Round Timer | Text Block | `GetRoundTimeFormatted()` |
| Relic Status | Text Block | `GetRelicPossessingTeam()` |

### Step 2.3: Implement Blueprint Events
In the Event Graph, implement these events:

**OnHealthChanged:**
```
Event OnHealthChanged (NewHealth, MaxHealth, HealthPercent)
    → Set HealthBar Percent = HealthPercent
    → Set HealthText = FormatText("{0}/{1}", NewHealth, MaxHealth)
```

**OnScoreChanged:**
```
Event OnScoreChanged (Team1Score, Team2Score)
    → Set Team1ScoreText = Team1Score
    → Set Team2ScoreText = Team2Score
```

**OnRoundTimeUpdated:**
```
Event OnRoundTimeUpdated (SecondsRemaining, FormattedTime)
    → Set TimerText = FormattedTime
    → If SecondsRemaining < 30, Set TimerColor = Red
```

**OnLocalPlayerDied:**
```
Event OnLocalPlayerDied
    → Play "Death" animation on Health Bar
    → Show "Respawning..." text overlay
```

---

## Phase 3: Create Scoreboard Widget

### Step 3.1: Create the Blueprint Widget
1. In `Content/UI/`, create new Widget Blueprint
2. Parent class: `BwayScoreboardWidget`
3. Name: `WBP_BW_Scoreboard`

### Step 3.2: Design the Layout

```
┌─────────────────────────────────────────────────────────────┐
│                      SCOREBOARD                              │
├─────────────────────────────────────────────────────────────┤
│  TEAM 1 (Score: X)                  TEAM 2 (Score: Y)       │
├─────────────────────────────────────────────────────────────┤
│  Player | Hero | K | D | A | Ping   Player | Hero | K | D   │
│  ────────────────────────────────   ────────────────────────│
│  [List View]                        [List View]              │
└─────────────────────────────────────────────────────────────┘
```

### Step 3.3: Create Player Row Widget
1. Create `WBP_BW_ScoreboardPlayerRow` (parent: `CommonUserWidget`)
2. Add bindings for `FScoreboardPlayerData` struct:
   - PlayerName (Text Block)
   - HeroName (Text Block)
   - Kills/Deaths/Assists (Text Blocks)
   - Ping (Text Block)
   - LocalPlayer highlight (Border color)

### Step 3.4: Implement OnScoreboardDataRefreshed
```
Event OnScoreboardDataRefreshed (Team1Players, Team2Players)
    → Clear Team1ListView
    → For Each player in Team1Players:
        → Create WBP_BW_ScoreboardPlayerRow
        → Set data from player struct
        → Add to Team1ListView
    → (Repeat for Team2)
```

### Step 3.5: Bind Scoreboard to Tab Key
1. Open your **Input Actions** folder
2. Create or find `IA_ShowScoreboard` (Input Action)
3. In `WBP_BW_CoreHUD`, add Input Action binding:
   - **Started**: Push `WBP_BW_Scoreboard` to Game layer
   - **Completed**: Pop `WBP_BW_Scoreboard` from stack

---

## Phase 4: Create Pause Menu Widget

### Step 4.1: Create the Blueprint Widget
1. Parent class: `BwayPauseMenuWidget`
2. Name: `WBP_BW_PauseMenu`

### Step 4.2: Design the Layout

```
┌─────────────────────────────────────┐
│           GAME PAUSED               │
├─────────────────────────────────────┤
│         [ RESUME ]                  │  ← Default focus
│         [ SETTINGS ]                │
│         [ QUIT TO LOBBY ]           │
│         [ QUIT TO DESKTOP ]         │
└─────────────────────────────────────┘
```

### Step 4.3: Button Bindings

| Button | On Clicked |
|--------|------------|
| Resume | Call `ResumeGame()` |
| Settings | Call `OpenSettings()` |
| Quit to Lobby | Call `QuitToMainMenu()` |
| Quit to Desktop | Call `QuitToDesktop()` |

### Step 4.4: Configure Settings Widget
1. In the Class Defaults of `WBP_BW_PauseMenu`:
   - Set **Settings Widget Class** to Lyra's `W_LyraSettingsScreen` or your custom settings widget
   - Set **Main Menu Level** to your frontend/lobby map

### Step 4.5: Wire Pause Menu to HUD Layout
1. Open your HUD Layout Blueprint (extends `LyraHUDLayout`)
2. Set **Escape Menu Class** to `WBP_BW_PauseMenu`

---

## Phase 5: Create Results Screen Widget

### Step 5.1: Create the Blueprint Widget
1. Parent class: `BwayResultsScreenWidget`
2. Name: `WBP_BW_ResultsScreen`

### Step 5.2: Design the Layout

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│                    TEAM 1 WINS!                             │
│                                                             │
│              Final Score: 3 - 1                              │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│                         MVP                                  │
│                   [Player Name]                              │
│                   [Hero Name]                                │
├─────────────────────────────────────────────────────────────┤
│         [ PLAY AGAIN ]        [ RETURN TO LOBBY ]           │
└─────────────────────────────────────────────────────────────┘
```

### Step 5.3: Implement OnResultsReady
```
Event OnResultsReady (Results)
    → If Results.bLocalPlayerWon:
        → Set HeaderText = "VICTORY!"
        → Play Victory animation/sound
    → Else:
        → Set HeaderText = "DEFEAT"
        → Play Defeat animation/sound
    → Set ScoreText = FormatText("{0} - {1}", Results.Team1Score, Results.Team2Score)
    → Set MVPNameText = Results.MVPPlayerName
    → Set MVPHeroText = Results.MVPHeroName
```

### Step 5.4: Configure Level References
In Class Defaults:
- **Play Again Level**: Set to your gameplay map
- **Lobby Level**: Set to your frontend/lobby map
- **Button Enable Delay**: 2.0 seconds (prevents accidental clicks)

---

## Phase 6: Configure Match Phase Flow

### Step 6.1: Open Your Experience Definition
1. Navigate to `Plugins/GameFeatures/BreakawayCore/Content/Experiences/`
2. Open your main Experience asset (e.g., `B_BW_Experience_CaptureTheRelic`)

### Step 6.2: Configure Phase Abilities in Experience
Add these **Game Phase Abilities** to the Experience's Action Set:

| Phase | Ability Asset | Order |
|-------|---------------|-------|
| Warmup | `BW_Phase_Warmup` | 1 |
| Hero Selection | `BW_Phase_HeroSelection` | 2 |
| Playing | `BW_Phase_Playing` | 3 |
| Post Round | `BW_Phase_PostRound` | 4 (loops back to 3) |
| Post Game | `BW_Phase_PostGame` | 5 |

### Step 6.3: Configure HeroSelectionPhaseComponent
1. Open `BP_BW_HeroSelectionPhase` or your GameState Blueprint
2. Find the `BwayHeroSelectionPhaseComponent`
3. Set these properties:
   - **Hero Selection Widget Class**: `WBP_BW_HeroSelect` (your existing widget)
   - **Default Hero Data**: Select Argus as fallback
   - **Phase Tag**: `ShooterGame.GamePhase.HeroSelection`

### Step 6.4: Wire Results Screen to Match End
1. Open your `BW_Phase_PostGame` asset
2. In the phase's Action Set, add:
   - **Add Widgets** action
   - Widget Class: `WBP_BW_ResultsScreen`
   - Layer Tag: `UI.Layer.Game`

---

## Phase 7: Configure Audio Assets

### Step 7.1: Create/Import Sound Files
Prepare or import these sound files:
- `SFX_Relic_Pickup.wav`
- `SFX_Relic_Drop.wav`
- `SFX_Relic_Score.wav`
- `SFX_Relic_Reset.wav`
- `SFX_Ability_ShieldBash.wav`
- `SFX_Ability_WarCry.wav`
- `SFX_Ability_Leap_Launch.wav`
- `SFX_Ability_Leap_Land.wav`

### Step 7.2: Configure RelicSettings Data Asset
1. Open `Plugins/GameFeatures/BreakawayCore/Content/Relic/DA_RelicSettings`
2. In the **Audio** section, assign:
   - **Pickup Sound**: `SFX_Relic_Pickup`
   - **Drop Sound**: `SFX_Relic_Drop`
   - **Scoring Sound**: `SFX_Relic_Score`
   - **Reset Sound**: `SFX_Relic_Reset`

### Step 7.3: Configure Ability Sounds (Blueprint)
For each ability Blueprint (e.g., `GA_BW_Spartacus_ShieldBash`):
1. Add a **Sound Cue** or **MetaSound** node in the activation logic
2. Use **Play Sound at Location** or **Spawn Sound 2D**

---

## Phase 8: Create Missing Gameplay Effects

### Step 8.1: Create Stun Effect
1. Navigate to `Content/Characters/Heroes/Argus/Abilities/GameplayEffects/`
2. Create new **Gameplay Effect** Blueprint
3. Name: `GE_BW_Spartacus_ShieldBash_Stun`
4. Configure:
   - **Duration Policy**: Has Duration
   - **Duration Magnitude**: 1.5 seconds
   - **Gameplay Effect Granted Tags**: Add `State.Stunned`
   - **Application Tag Requirements**: Block activation if already stunned

### Step 8.2: Create War Cry Buff Effect
1. Create `GE_BW_Spartacus_WarCry_Buff`
2. Configure:
   - **Duration Policy**: Has Duration
   - **Duration Magnitude**: 5.0 seconds
   - **Modifiers**:
     - Attribute: MovementSpeed, Modifier Op: Multiply, Magnitude: 1.2
     - Attribute: BaseDamage, Modifier Op: Multiply, Magnitude: 1.15
   - **Granted Tags**: `State.Buffed.WarCry`

### Step 8.3: Create Defensive Stance Effect
1. Create `GE_BW_Spartacus_DefensiveStance`
2. Configure:
   - **Duration Policy**: Infinite
   - **Modifiers**:
     - Attribute: IncomingDamage, Modifier Op: Multiply, Magnitude: 0.5
     - Attribute: MovementSpeed, Modifier Op: Multiply, Magnitude: 0.7
   - **Granted Tags**: `State.DefensiveStance`

### Step 8.4: Assign Effects to Ability Blueprints
For each ability Blueprint:
1. Open the ability (e.g., `GA_BW_Spartacus_ShieldBash`)
2. In Class Defaults, find the effect class property (e.g., `StunGameplayEffectClass`)
3. Assign the corresponding GameplayEffect

---

## Phase 9: Final Integration Checklist

### Step 9.1: Experience Configuration
- [ ] Experience has all 5 phase abilities in correct order
- [ ] Experience has HUD widget action (`WBP_BW_CoreHUD`)
- [ ] Experience has correct GameMode class (`BreakawayGameMode`)
- [ ] Experience has correct GameState class (`BwayGameState`)

### Step 9.2: HUD Configuration
- [ ] `WBP_BW_CoreHUD` has health, score, timer bindings
- [ ] Scoreboard bound to Tab key input
- [ ] Pause menu bound to Escape (via HUD Layout)

### Step 9.3: Hero Configuration
- [ ] Argus has all 4 abilities in AbilitySet
- [ ] Each ability has cooldown GameplayEffect configured
- [ ] Each ability has effect GameplayEffect assigned (stun, buff, etc.)

### Step 9.4: Map Configuration
- [ ] Map has Team1 and Team2 PlayerStart actors with correct tags
- [ ] Map has RelicSpawn point
- [ ] Map has two BwayGoalVolume actors (one per team)
- [ ] Map uses the correct Experience asset

---

## Phase 10: Testing Protocol

### Step 10.1: Single Player Test
1. PIE with 1 player
2. Verify:
   - [ ] HUD displays correctly
   - [ ] Health bar updates when taking damage
   - [ ] Scoreboard appears on Tab
   - [ ] Pause menu appears on Escape
   - [ ] Relic can be picked up/dropped
   - [ ] All 4 Argus abilities activate

### Step 10.2: Local Multiplayer Test
1. PIE with **Number of Players: 2** (Net Mode: Play As Listen Server)
2. Verify:
   - [ ] Both players spawn on different teams
   - [ ] Relic possession updates correctly
   - [ ] Scoring triggers correctly
   - [ ] Results screen appears at match end

### Step 10.3: LAN Test (Optional)
1. Package a Development build
2. Run on 2+ machines on same network
3. Verify all networking works correctly

---

## Quick Reference: Key Paths

| Asset Type | Location |
|------------|----------|
| UI Widgets | `Plugins/GameFeatures/BreakawayCore/Content/UI/` |
| Heroes | `Plugins/GameFeatures/BreakawayCore/Content/Characters/Heroes/` |
| Phases | `Plugins/GameFeatures/BreakawayCore/Content/Experiences/Phases/` |
| Experiences | `Plugins/GameFeatures/BreakawayCore/Content/Experiences/` |
| Relic Config | `Plugins/GameFeatures/BreakawayCore/Content/Relic/` |

---

## Quick Reference: C++ Base Classes

| Widget Purpose | C++ Base Class | Key Methods |
|----------------|----------------|-------------|
| Core HUD | `UBwayCoreHUDWidget` | `GetHealthPercent()`, `GetTeam1Score()`, `GetRoundTimeFormatted()` |
| Scoreboard | `UBwayScoreboardWidget` | `GetTeamPlayers()`, `RefreshScoreboardData()` |
| Pause Menu | `UBwayPauseMenuWidget` | `ResumeGame()`, `OpenSettings()`, `QuitToMainMenu()` |
| Results Screen | `UBwayResultsScreenWidget` | `GetMatchResults()`, `PlayAgain()`, `ReturnToLobby()` |

---

## Quick Reference: Blueprint Events to Implement

### BwayCoreHUDWidget Events
| Event | Parameters | Purpose |
|-------|------------|---------|
| `OnHealthChanged` | NewHealth, MaxHealth, HealthPercent | Update health bar display |
| `OnScoreChanged` | Team1Score, Team2Score | Update score displays |
| `OnRoundTimeUpdated` | SecondsRemaining, FormattedTime | Update timer display |
| `OnRoundStateChanged` | NewState (FName) | Handle phase transitions |
| `OnRelicPossessionChanged` | TeamIndex, bIsCarried | Update relic indicator |
| `OnLocalPlayerDied` | (none) | Show death overlay |
| `OnLocalPlayerRespawned` | (none) | Hide death overlay |

### BwayScoreboardWidget Events
| Event | Parameters | Purpose |
|-------|------------|---------|
| `OnScoreboardDataRefreshed` | Team1Players, Team2Players | Rebuild player lists |

### BwayResultsScreenWidget Events
| Event | Parameters | Purpose |
|-------|------------|---------|
| `OnResultsReady` | FMatchResultsData | Populate results display |
| `OnPlayAgainRequested` | (none) | Optional pre-travel logic |
| `OnReturnToLobbyRequested` | (none) | Optional pre-travel logic |

---

This guide covers all Blueprint work needed for the Vertical Slice. Work through each phase sequentially, testing as you go. The C++ infrastructure is complete - focus on visual design and asset configuration.

