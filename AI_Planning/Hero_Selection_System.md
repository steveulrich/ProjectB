# Hero Selection System — Architecture & Integration Guide

> Consolidated reference for the hero selection phase. Covers C++ architecture, Lyra phase system integration, Blueprint wiring, and Experience configuration.

---

## 1. Architecture Overview

```
Lyra Game Phase Subsystem
  ├─ Activates: GamePhase.HeroSelection
  │  └─ Fires callback → HandleLyraPhaseActivated()
  │     └─ Component calls StartHeroSelectionPhase()
  │
  ├─ Waits for: Component completion
  │  └─ Component calls EndPhaseAndProgressToNext()
  │
  └─ Activates: Next phase (Warmup)
```

### Component Hierarchy (on ABwayGameState)

| Component | Type | Role |
|:---|:---|:---|
| `UBwayHeroSelectionManager` | C++ | Tracks player selections, validates picks, fires `OnAllPlayersReady` |
| `UBwayHeroSelectionPhaseComponent` | C++ (BP overrideable) | Listens to Lyra phases, manages UI lifecycle, spawns heroes |

### Key Classes

- **C++ Base:** `BwayHeroSelectionPhaseComponent.h/.cpp` — `BeginPlay()` registers a listener with `ULyraGamePhaseSubsystem::WhenPhaseStartsOrIsActive()` for `GamePhase.HeroSelection`.
- **Blueprint Child:** `BP_BW_HeroSelectionPhase` — overrides `ShowHeroSelectionUI()`, `HideHeroSelectionUI()`, `SpawnHeroForPlayer()`.
- **Widget:** `WBP_BW_HeroSelect` — player-facing UI.

---

## 2. Phase Flow

```
1. Experience loads → Phase subsystem starts GamePhase.HeroSelection
2. HandleLyraPhaseActivated() → StartHeroSelectionPhase()
3. UI shown, players registered with HeroSelectionManager
4. Players select + lock heroes
5. OnAllPlayersReady → HandleAllPlayersReady()
6. Hide UI, spawn heroes, call EndPhaseAndProgressToNext()
7. Phase subsystem progresses to GamePhase.Warmup
```

### Events vs Callbacks

| Name | Direction | Purpose |
|:---|:---|:---|
| `HandleLyraPhaseActivated()` | **Inbound** (Lyra → Component) | Private callback, never call manually |
| `OnHeroSelectionPhaseStarted` | **Outbound** (Component → World) | Public delegate, bind in Blueprint for UI/audio/camera |

---

## 3. Experience Configuration

### Required Phase Order

The Experience asset must start `GamePhase.HeroSelection` **before** `GamePhase.Warmup`.

**Path:** `BreakawayCore/Content/Experiences/B_BW_Experience_Dev.uasset`

1. Open Experience asset → find **Actions** array
2. Look for `GameFeatureAction_AddAbilities` or phase configuration
3. Ensure `BW_Phase_HeroSelection` is granted/started first
4. Remove any auto-start of Warmup from scoring components

### Phase Asset Configuration

| Asset | Game Phase Tag | Path |
|:---|:---|:---|
| `BW_Phase_HeroSelection` | `GamePhase.HeroSelection` | `Content/Experiences/Phases/` |
| `BW_Phase_Warmup` | `GamePhase.Warmup` | `Content/Experiences/Phases/` |

> **Critical:** Remove any manual `StartHeroSelectionPhase()` call from `BP_BWayGameMode::BeginPlay`. Let the Experience drive phases.

### Manual Override (Testing Only)

If the Experience is difficult to configure, bypass in Game Mode:
```
Event BeginPlay → Delay 2.0s → Get LyraGamePhaseSubsystem → K2_Start Phase: BW_Phase_HeroSelection
```

---

## 4. Blueprint Quick Integration

### Minimum Viable Setup (30 min)

1. **Create widget:** `WBP_BW_HeroSelect` (parent: `BwayHeroSelectWidget`)
2. **Override in `BP_BW_HeroSelectionPhase`:**
   - `ShowHeroSelectionUI`: Create widget → Add to Viewport → Set Input Mode UI Only
   - `HideHeroSelectionUI`: Remove widget → Set Input Mode Game Only
   - `SpawnHeroForPlayer`: Get hero data → RestartPlayer with hero
3. **Add Component:** `BP_BW_HeroSelectionPhase` on `BP_BwayGameState`
4. **Configure:** Set `HeroSelectionWidgetClass` = `WBP_BW_HeroSelect`, `DefaultHeroData` = `DA_Hero_Spartacus`

### Events to Bind in Blueprint

| Event | Source | Use For |
|:---|:---|:---|
| `OnHeroSelectionPhaseStarted` | Phase Component | Title cards, music |
| `OnHeroSelectionPhaseEnded` | Phase Component | Transition animations |
| `OnAllPlayersReady` | Selection Manager | Match-starting countdown |

---

## 5. Debugging

### Expected Output Log Sequence
```
LogTemp: BwayHeroSelectionPhaseComponent: Listening for Lyra phase: GamePhase.HeroSelection
LogLyraGamePhase: Starting phase: GamePhase.HeroSelection
LogTemp: BwayHeroSelectionPhaseComponent: HeroSelection phase started by Lyra phase system!
... players select heroes ...
LogTemp: BwayHeroSelectionPhaseComponent: All players ready - ending phase
LogLyraGamePhase: Ending phase: GamePhase.HeroSelection
LogLyraGamePhase: Starting phase: GamePhase.Warmup
```

### Common Issues

| Problem | Cause | Fix |
|:---|:---|:---|
| Warmup starts immediately | Experience starts Warmup first | Reorder phases in Experience Actions |
| UI never appears | Phase tag mismatch | Verify `GamePhase.HeroSelection` matches everywhere |
| Phase starts but no UI | Blueprint override missing | Check `ShowHeroSelectionUI` is overridden in BP child |
| Linker errors | Missing module | Ensure `LyraGame` is in `Build.cs` PublicDependencyModuleNames |
