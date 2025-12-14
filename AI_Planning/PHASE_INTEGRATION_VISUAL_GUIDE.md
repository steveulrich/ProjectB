# Hero Selection Phase Integration - Visual Guide

## 🎯 The Problem (Before)

```
┌─────────────────────────────────────────────┐
│  Experience Loads                           │
└────────────┬────────────────────────────────┘
             │
             ├─> Scoring Component Auto-Starts Warmup ❌
             │   
             ├─> Warmup Phase Begins
             │   ├─> Players spawn
             │   ├─> Match timer starts
             │   └─> Game is "playing"
             │
             └─> Meanwhile... Your component tries to show UI
                 └─> UI appears but game ignores it!
```

**Result:** Hero selection UI shows up AFTER the match has started, phases progress in background.

---

## ✅ The Solution (After)

```
┌─────────────────────────────────────────────┐
│  Experience Loads                           │
└────────────┬────────────────────────────────┘
             │
             v
┌─────────────────────────────────────────────┐
│  Lyra Phase Subsystem                       │
│  Starts: GamePhase.HeroSelection            │
└────────────┬────────────────────────────────┘
             │
             v
┌─────────────────────────────────────────────┐
│  BwayHeroSelectionPhaseComponent            │
│  - Receives: OnHeroSelectionPhaseStarted()  │
│  - Shows UI                                 │
│  - Waits for all players                    │
└────────────┬────────────────────────────────┘
             │
             v
        Players Select Heroes
             │
             v
┌─────────────────────────────────────────────┐
│  All Players Lock Selection                 │
└────────────┬────────────────────────────────┘
             │
             v
┌─────────────────────────────────────────────┐
│  Component: EndPhaseAndProgressToNext()     │
│  - Hides UI                                 │
│  - Spawns heroes                            │
│  - Signals phase complete                   │
└────────────┬────────────────────────────────┘
             │
             v
┌─────────────────────────────────────────────┐
│  Lyra Phase Subsystem                       │
│  Ends: GamePhase.HeroSelection              │
│  Starts: GamePhase.Warmup                   │
└────────────┬────────────────────────────────┘
             │
             v
        Match Begins! ✅
```

---

## 📦 Component Architecture

```
┌──────────────────────────────────────────────────┐
│  ABwayGameState                                  │
│                                                  │
│  ┌────────────────────────────────────────────┐ │
│  │ UBwayHeroSelectionManager (C++)            │ │
│  │ - RegisterPlayer()                         │ │
│  │ - IsHeroAvailableForTeam()                 │ │
│  │ - AreAllPlayersReady()                     │ │
│  │ - OnAllPlayersReady → Fires delegate      │ │
│  └────────────────────────────────────────────┘ │
│                                                  │
│  ┌────────────────────────────────────────────┐ │
│  │ BP_BW_HeroSelectionPhase                   │ │
│  │ (Blueprint child of C++ component)         │ │
│  │                                            │ │
│  │ C++ Base:                                  │ │
│  │ ├─ OnHeroSelectionPhaseStarted() ← NEW!   │ │
│  │ ├─ EndPhaseAndProgressToNext() ← NEW!     │ │
│  │ └─ Listens to Lyra Phase Subsystem        │ │
│  │                                            │ │
│  │ Blueprint Overrides:                       │ │
│  │ ├─ ShowHeroSelectionUI()                  │ │
│  │ ├─ HideHeroSelectionUI()                  │ │
│  │ └─ SpawnHeroForPlayer()                   │ │
│  └────────────────────────────────────────────┘ │
│                                                  │
└──────────────────────────────────────────────────┘
```

---

## 🔄 Phase Lifecycle

```
┌─────────────────────────────────────────────────┐
│ 1. Experience Definition                        │
│    Defines: What phases exist and in what order │
│    Example: HeroSelection → Warmup → Playing    │
└────────────┬────────────────────────────────────┘
             │
             v
┌─────────────────────────────────────────────────┐
│ 2. Lyra Game Phase Subsystem (World Subsystem) │
│    - Manages active phases                      │
│    - Starts/ends phase abilities                │
│    - Broadcasts phase change events             │
└────────────┬────────────────────────────────────┘
             │
             v
┌─────────────────────────────────────────────────┐
│ 3. Phase Ability (ULyraGamePhaseAbility)       │
│    - One instance per phase                     │
│    - Activated when phase starts                │
│    - Deactivated when phase ends                │
│    - Has GameplayTag (e.g. GamePhase.Warmup)    │
└────────────┬────────────────────────────────────┘
             │
             v
┌─────────────────────────────────────────────────┐
│ 4. Your Component (Listens to phases)          │
│    - WhenPhaseStartsOrIsActive()                │
│    - Receives callback when phase tag matches   │
│    - Does game-specific logic                   │
│    - Signals when done                          │
└────────────┬────────────────────────────────────┘
             │
             v
┌─────────────────────────────────────────────────┐
│ 5. Phase Ends, Next Phase Starts               │
│    - Subsystem ends current phase ability       │
│    - Subsystem starts next phase ability        │
│    - Cycle repeats                              │
└─────────────────────────────────────────────────┘
```

---

## 🔧 Code Flow Diagram

### Before (Manual Starting):
```
Game Mode BeginPlay
    │
    └─> Manually call: StartHeroSelectionPhase() ❌
        │
        └─> Show UI, wait for players
            │
            └─> Match continues in background!
```

### After (Phase System Integration):
```
Component BeginPlay
    │
    ├─> Register listener: WhenPhaseStartsOrIsActive()
    │   └─> Listening for: "GamePhase.HeroSelection"
    │
    └─> Wait...

Experience Loads
    │
    └─> Start Phase: GamePhase.HeroSelection

Phase Subsystem
    │
    └─> Activates phase ability with tag: GamePhase.HeroSelection

Component Receives Callback
    │
    ├─> OnHeroSelectionPhaseStarted() ✅
    │   └─> Calls: StartHeroSelectionPhase()
    │       ├─> Show UI
    │       ├─> Register players
    │       └─> Wait for ready
    │
    └─> All ready?
        └─> EndPhaseAndProgressToNext()
            └─> Phase subsystem ends phase
                └─> Next phase starts automatically
```

---

## 📊 Data Flow

```
┌──────────────┐
│  Player 1    │──┐
└──────────────┘  │
                  │    ┌──────────────────────────┐
┌──────────────┐  ├───>│ Hero Selection Manager   │
│  Player 2    │──┤    │ (Tracks selections)      │
└──────────────┘  │    └─────────┬────────────────┘
                  │              │
┌──────────────┐  │              │ All Ready?
│  Player 3    │──┤              │
└──────────────┘  │              v
                  │    ┌──────────────────────────┐
┌──────────────┐  │    │ OnAllPlayersReady        │
│  Player 4    │──┘    │ (Delegate fires)         │
└──────────────┘       └─────────┬────────────────┘
                                 │
                                 v
                       ┌──────────────────────────┐
                       │ Phase Component          │
                       │ HandleAllPlayersReady()  │
                       └─────────┬────────────────┘
                                 │
                   ┌─────────────┴─────────────┐
                   │                           │
                   v                           v
         ┌─────────────────┐        ┌─────────────────┐
         │ Hide UI         │        │ Spawn Heroes    │
         └─────────────────┘        └─────────────────┘
                   │                           │
                   └─────────────┬─────────────┘
                                 │
                                 v
                   ┌──────────────────────────┐
                   │ EndPhaseAndProgressToNext│
                   └─────────┬────────────────┘
                             │
                             v
                   ┌──────────────────────────┐
                   │ Lyra Phase Subsystem     │
                   │ → Next Phase             │
                   └──────────────────────────┘
```

---

## 🎮 User Experience Flow

```
Player launches game
    │
    ▼
Loading screen
    │
    ▼
┌──────────────────────────────────┐
│  HERO SELECTION SCREEN           │  ← GamePhase.HeroSelection
│                                  │
│  [Hero 1]  [Hero 2]  [Hero 3]   │
│     ↓                            │
│  [LOCK BUTTON]                   │
│                                  │
│  Waiting for other players...    │
└──────────────────────────────────┘
    │
    ▼ (All players locked)
Screen fades out
    │
    ▼
┌──────────────────────────────────┐
│  MATCH STARTING                  │  ← Transition
│         3...2...1...             │
└──────────────────────────────────┘
    │
    ▼
┌──────────────────────────────────┐
│  WARMUP PHASE                    │  ← GamePhase.Warmup
│  [HUD with timer, scores, etc]  │
│  Players can move around         │
└──────────────────────────────────┘
    │
    ▼
┌──────────────────────────────────┐
│  MATCH ACTIVE                    │  ← GamePhase.Playing
│  [Full gameplay]                 │
└──────────────────────────────────┘
```

---

## 🔍 Debug Visualization

### What to Look For in Output Log:

```
✅ GOOD SEQUENCE:
─────────────────────────────────────────────────
LogTemp: BwayHeroSelectionPhaseComponent: Listening for Lyra phase: GamePhase.HeroSelection
LogLyraGamePhase: Starting phase: GamePhase.HeroSelection
LogTemp: BwayHeroSelectionPhaseComponent: HeroSelection phase started by Lyra phase system!
LogTemp: BwayHeroSelectionPhaseComponent: Starting hero selection phase
LogTemp: BwayHeroSelectionManager: Initialized on server
... players select heroes ...
LogTemp: BwayHeroSelectionPhaseComponent: All players ready - ending phase
LogTemp: BwayHeroSelectionPhaseComponent: Hero selection complete.
LogLyraGamePhase: Ending phase: GamePhase.HeroSelection
LogLyraGamePhase: Starting phase: GamePhase.Warmup
─────────────────────────────────────────────────

❌ BAD SEQUENCE (Not Integrated):
─────────────────────────────────────────────────
LogLyraGamePhase: Starting phase: GamePhase.Warmup  ← Started wrong phase!
LogTemp: Players spawning...
LogTemp: Match timer starting...
LogTemp: BwayHeroSelectionPhaseComponent: Starting hero selection phase  ← Too late!
─────────────────────────────────────────────────
```

---

## 🎓 Key Concepts

### 1. Phase Tags are Hierarchical
```
GamePhase
├── GamePhase.HeroSelection     ← Sibling phases
├── GamePhase.Warmup            ← Cannot be active together
├── GamePhase.Playing           ← Only one at a time
│   ├── GamePhase.Playing.Round1  ← Can be active WITH parent
│   └── GamePhase.Playing.Round2  ← Siblings cancel each other
└── GamePhase.PostGame
```

### 2. Phase Abilities are Gameplay Abilities
- Granted to the Game State's Ability System Component
- Activated when phase starts
- Can have their own logic (or be simple markers)

### 3. Your Component is a "Phase Observer"
- Listens for specific phase tags
- Reacts when those phases become active
- Does NOT control the phases directly

---

## 🏁 Final Checklist

Integration Complete When:
- [ ] Code compiles ✓
- [ ] Component listens to phase subsystem ✓
- [ ] Experience starts HeroSelection phase first ← YOUR ACTION
- [ ] UI shows when phase starts ✓
- [ ] Phases progress in correct order ✓

---

**You're almost there! Just need to configure that Experience asset!** 🚀


