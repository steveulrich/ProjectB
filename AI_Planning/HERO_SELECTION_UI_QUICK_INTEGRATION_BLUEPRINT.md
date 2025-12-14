# Hero Selection - Quick Integration (Blueprint Only)

## Goal

Get hero selection working in 30 minutes using only Blueprint.

## What You Need

- ✅ Build compiles (hero selection system installed)
- ✅ Widget created (`WBP_BW_HeroSelect`)
- ✅ One hero data asset (`DA_Hero_Spartacus` or similar)

---

## Step 1: Create Phase Manager Blueprint (10 minutes)

### A. Create the Actor

1. **Content Browser:**
   - Navigate to: `BreakawayCore/Content/Experiences/Phases/`
   - Right-click → Blueprint Class
   - Parent: **Actor Component**
   - Name: `BP_HeroSelectionPhaseManager`

### B. Add Variables

```
Variables:
├── SelectionManager (Type: BwayHeroSelectionManager, Instance Editable: false)
├── bPhaseActive (Type: bool, Default: false)
├── HeroSelectionWidgetClass (Type: Widget Class Reference)
│   └─ Default: WBP_BW_HeroSelect
└── DefaultHeroDataAsset (Type: BwayHeroDataAsset, Instance Editable: true)
    └─ Default: DA_Hero_Spartacus
```

### C. Create Functions

**Function: Start Hero Selection Phase**

```
Start Hero Selection Phase
├─ Branch: Has Authority?
│  └─ False: Return (client doesn't run this)
│
├─ Set bPhaseActive = true
│
├─ Get Game State → Cast to BwayGameState
├─ Get Component by Class: BwayHeroSelectionManager → SelectionManager
│
├─ Register All Players:
│  └─ Get Game State.PlayerArray
│  └─ ForEach PlayerState:
│     └─ SelectionManager.RegisterPlayer(PlayerState)
│
├─ Bind Event: SelectionManager.OnAllPlayersReady → Handle All Players Ready
│
├─ SelectionManager.StartHeroSelection()
│
└─ Show UI to All Players:
   └─ Get All Player Controllers
   └─ ForEach PlayerController:
      ├─ Branch: Is Valid?
      └─ Create Widget: HeroSelectionWidgetClass
         ├─ Owning Player: PlayerController
         ├─ Add to Viewport
         └─ Set Input Mode UI Only
            - Show Mouse Cursor: true
```

**Event: Handle All Players Ready**

```
Handle All Players Ready
├─ Print String: "All Players Ready!"
│
├─ Hide UI from All Players:
│  └─ Get All Player Controllers
│  └─ ForEach PlayerController:
│     └─ Get All Widgets of Class: WBP_BW_HeroSelect
│        └─ ForEach Widget:
│           └─ Remove from Parent
│
├─ Set Input Mode Game Only (for all players)
│
├─ Spawn Heroes For All Players (call function)
│
└─ Start Match:
   └─ Get Game Mode → Cast to BreakawayGameMode
   └─ Call: StartRound()
```

**Function: Spawn Heroes For All Players**

```
Spawn Heroes For All Players
├─ Get Game State.PlayerArray
│
└─ ForEach PlayerState → Cast to BwayPlayerState:
   ├─ Get PlayerState.GetSelectedHeroId() → HeroId
   │
   ├─ Branch: Is HeroId Valid?
   │  ├─ False: Use DefaultHeroDataAsset
   │  └─ True: Continue
   │
   ├─ Get Hero Data by ID (from HeroRegistry)
   │  └─ If null: Use DefaultHeroDataAsset
   │
   ├─ Get PlayerController from PlayerState
   │
   └─ Respawn Player with Hero Data:
      └─ Get Game Mode
      └─ Call: RestartPlayer(PlayerController)
         └─ (Game mode will handle spawning)
```

---

## Step 2: Add to Game State (2 minutes)

### Open BP_BwayGameState

1. **Open:** Your game state blueprint
2. **Add Component:** `BP_HeroSelectionPhaseManager`
3. **Configure in Details:**
   ```
   Hero Selection Widget Class: WBP_BW_HeroSelect
   Default Hero Data Asset: DA_Hero_Spartacus
   ```
4. **Compile & Save**

---

## Step 3: Trigger Phase from Game Mode (5 minutes)

### Option A: Auto-Start on BeginPlay

**In BP_BreakawayGameMode (or your game mode):**

```
Event BeginPlay
├─ Delay: 3.0 seconds (let players load)
│
├─ Get Game State → Cast to BwayGameState
├─ Get Component: BP_HeroSelectionPhaseManager
│
└─ Call: Start Hero Selection Phase
```

### Option B: Manual Trigger (for testing)

Add a console command or key binding:

```
Event: Key Press (F8)
├─ Get Game State
├─ Get HeroSelectionPhaseManager
└─ Start Hero Selection Phase
```

---

## Step 4: Test (5 minutes)

### Solo Test

1. **PIE Settings:**
   - Net Mode: Play As Listen Server
   - Number of Players: 1
2. **Play**
3. **Expected:**
   - Game loads
   - After 3 seconds, hero selection UI appears
   - Select a hero
   - Click Lock
   - Hero spawns
   - Match starts

### Multiplayer Test

1. **PIE Settings:**
   - Net Mode: Play As Client
   - Number of Players: 2
2. **Play**
3. **Expected:**
   - Both players see hero selection
   - Can select different heroes
   - When both lock, heroes spawn
   - Match starts

---

## Step 5: Quick Fixes for Common Issues

### Issue: UI Doesn't Appear

**Fix 1:** Check console for errors
```
LogTemp: BwayHeroSelectionManager: Initialized on server
LogTemp: Starting hero selection phase
```

**Fix 2:** Manually test UI
```
Event Key Press (F9):
├─ Create Widget: WBP_BW_HeroSelect
└─ Add to Viewport
```

### Issue: Heroes Don't Spawn

**Fix:** For now, just use default respawn:
```
In Handle All Players Ready:
├─ Get All Player Controllers
└─ ForEach Controller:
   └─ Get Game Mode.RestartPlayer(Controller)
```

### Issue: Selection Doesn't Replicate

**Check:**
- Playing as "Client" not "Standalone"
- BwayHeroSelectionManager is on Game State
- Build.cs has ModularGameplay module

---

## Minimum Viable Implementation (15 minutes)

If you just want it working **right now**:

### Ultra-Simple Version

**In Game Mode BeginPlay:**

```
Event BeginPlay
├─ Delay: 5 seconds
│
├─ Get All Player Controllers
└─ ForEach PlayerController:
   ├─ Create Widget: WBP_BW_HeroSelect
   │  - Owning Player: PlayerController
   ├─ Add to Viewport
   └─ Set Input Mode UI Only
```

**In WBP_BW_HeroSelect (add to Lock Button):**

```
Lock Button Clicked
├─ Call: LockSelection()
│
├─ Remove from Parent (hide UI)
│
├─ Get Owning Player
├─ Set Input Mode Game Only
│
└─ Branch: Is Server?
   └─ Get Game Mode
   └─ StartRound()
```

This skips the "wait for all players" logic but gets you started.

---

## Full Implementation Blueprint Graph

### BP_HeroSelectionPhaseManager

**Variables:**
```cpp
- SelectionManager: BwayHeroSelectionManager
- bPhaseActive: bool
- HeroSelectionWidgetClass: TSubclassOf<UserWidget>
- DefaultHeroDataAsset: BwayHeroDataAsset (Object Reference)
```

**Start Hero Selection Phase:**
```
1. Has Authority? → If False, Return
2. Set bPhaseActive = true
3. Get Game State → Get Component by Class → BwayHeroSelectionManager
4. ForEach PlayerState in GameState.PlayerArray:
   → SelectionManager.RegisterPlayer(PlayerState)
5. Bind SelectionManager.OnAllPlayersReady → HandleAllPlayersReady
6. SelectionManager.StartHeroSelection()
7. ForEach PlayerController:
   → Create Widget(HeroSelectionWidgetClass)
   → Add to Viewport
   → Set Input Mode UI Only
```

**Handle All Players Ready:**
```
1. Print "All Ready!"
2. Get All Widgets of Class(WBP_BW_HeroSelect)
   → ForEach: Remove from Parent
3. ForEach PlayerController:
   → Set Input Mode Game Only
4. Call: SpawnHeroesForAllPlayers
5. Get Game Mode → Cast BreakawayGameMode
6. Call: StartRound()
```

**Spawn Heroes For All Players:**
```
1. ForEach PlayerState in GameState.PlayerArray:
   → Cast to BwayPlayerState
   → Get SelectedHeroId
   → If Valid: Continue
   → If Invalid: Use DefaultHeroDataAsset
   → Get PlayerController
   → Get Game Mode
   → Call: RestartPlayer(Controller)
```

---

## Testing Checklist

- [ ] Blueprint compiles
- [ ] Component added to Game State
- [ ] Phase starts automatically
- [ ] UI appears for all players
- [ ] Can select hero
- [ ] Lock button works
- [ ] UI disappears when ready
- [ ] Heroes spawn
- [ ] Match starts

---

## Next Steps

Once basic flow works:

1. **Implement proper hero spawning:**
   - Spawn correct mesh based on hero data
   - Apply ability sets
   - Set correct animations

2. **Add phase transitions:**
   - Warmup phase before selection
   - Countdown timer
   - "Match Starting" screen

3. **Polish:**
   - Loading screens
   - Transition animations
   - Sound effects

---

## Blueprint vs C++ Decision

**Start with Blueprint because:**
- ✅ Faster to iterate
- ✅ No recompile needed
- ✅ Visual debugging
- ✅ Perfect for prototyping

**Move to C++ when:**
- ❌ Blueprint getting too complex
- ❌ Need better performance
- ❌ Want to share with other projects
- ❌ Ready for production

For now, **stick with Blueprint** until you have the flow working!

---

**Time Estimate:**
- Setup: 15 minutes
- Testing: 15 minutes
- Fixing issues: 15 minutes
- **Total: ~45 minutes**

Good luck! 🚀
