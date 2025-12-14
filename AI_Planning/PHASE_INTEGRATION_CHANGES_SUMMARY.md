# Hero Selection Phase Integration - Changes Summary

## What Was Changed

### ✅ C++ Files Modified

#### 1. **BwayHeroSelectionPhaseComponent.h**
**Location:** `Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/HeroSystems/BwayHeroSelectionPhaseComponent.h`

**Added:**
- `OnHeroSelectionPhaseStarted(const FGameplayTag& PhaseTag)` - Callback when Lyra starts the phase
- `EndPhaseAndProgressToNext()` - Signals completion to Lyra phase system

#### 2. **BwayHeroSelectionPhaseComponent.cpp**
**Location:** `Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/HeroSystems/BwayHeroSelectionPhaseComponent.cpp`

**Changed:**
- `BeginPlay()` - Now listens to Lyra Game Phase Subsystem for phase start events
- `HandleAllPlayersReady()` - Now calls `EndPhaseAndProgressToNext()` when done
- Added include for `LyraGamePhaseSubsystem.h`

**Added New Functions:**
- `OnHeroSelectionPhaseStarted()` - Starts hero selection when Lyra activates the phase
- `EndPhaseAndProgressToNext()` - Signals to Lyra that hero selection is complete

---

## How It Works Now

### Before (Problem):
```
Experience Loads
  ↓
Phases Start Automatically (Warmup phase starts immediately)
  ↓
Your Component tries to start hero selection
  ↓
CONFLICT: UI shows but phases progress in background ❌
```

### After (Solution):
```
Experience Loads
  ↓
Lyra Phase System starts GamePhase.HeroSelection
  ↓
Your Component receives callback → Shows UI
  ↓
Players select heroes
  ↓
All players ready → Component signals completion
  ↓
Lyra Phase System ends HeroSelection phase
  ↓
Lyra Phase System starts next phase (Warmup) ✅
```

---

## What You Need to Do Next

### Step 1: Compile the Code

**In Unreal Editor:**
1. Close Unreal Editor if it's open
2. Open Visual Studio (or your IDE)
3. Build the solution (Ctrl+Shift+B)
4. Or use **Tools → Recompile** in Unreal Editor

**Or via command line:**
```powershell
cd "E:\Unreal Projects\ProjectB"
"C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\Build.bat" LyraEditor Win64 Development -Project="E:\Unreal Projects\ProjectB\ProjectB.uproject" -WaitMutex -FromMsBuild
```

### Step 2: Configure Your Experience (CRITICAL!)

Your Experience needs to start the **HeroSelection phase FIRST**, not Warmup.

#### Open Your Experience:
```
Plugins/GameFeatures/BreakawayCore/Content/Experiences/B_BW_Experience_Dev.uasset
```
(or whichever experience you're using for testing)

#### What to Look For:

**Look for one of these patterns:**

##### Pattern A: "Actions" List with "Add Abilities"
If you see an action of type **"GameFeatureAction_AddAbilities"**:
1. Open it
2. Look for "Abilities To Grant" or similar
3. Find phase abilities (they'll reference `BW_Phase_*`)
4. Make sure `BW_Phase_HeroSelection` is granted FIRST
5. Or make sure there's a "Start Phase" action that starts HeroSelection first

##### Pattern B: Direct Phase Starting
If you see something like "Default Phase" or "Initial Phase":
1. Set it to: `BW_Phase_HeroSelection`
2. Or make sure the first phase started is HeroSelection

##### Pattern C: Scoring Component Auto-Start
Based on your screenshot, you might have a scoring component that auto-starts Warmup:
1. Look for **B_BW_ScoringBase** or similar in the Experience
2. Check if it has an "Auto Start Phase" setting
3. Change it to start HeroSelection instead
4. Or remove auto-start and let Experience control phases

### Step 3: Verify Phase Configuration

#### Check BW_Phase_HeroSelection Asset:
```
Plugins/GameFeatures/BreakawayCore/Content/Experiences/Phases/BW_Phase_HeroSelection.uasset
```

**Required Settings:**
- **Game Phase Tag:** `GamePhase.HeroSelection` (must match!)
- **Ability Class:** Can be base `LyraGamePhaseAbility` or custom

#### Check BW_Phase_Warmup Asset:
```
Plugins/GameFeatures/BreakawayCore/Content/Experiences/Phases/BW_Phase_Warmup.uasset
```

**Required Settings:**
- **Game Phase Tag:** `GamePhase.Warmup` or `GamePhase.Playing.Warmup`
- Should NOT auto-start

### Step 4: Update Game Mode BeginPlay

**REMOVE** any manual phase starting from your Game Mode:

#### In BP_BWayGameMode Event Graph:

**REMOVE THIS:**
```
❌ Get Game State.HeroSelectionPhaseComponent.StartHeroSelectionPhase()
```

**Instead, the flow should be:**
```
Event BeginPlay
├─ Parent: BeginPlay
└─ (That's it! Let Experience handle phases)
```

**Or if you want to manually control:**
```
Event BeginPlay
├─ Parent: BeginPlay
│
├─ Bind Event: On Experience Loaded
│  └─ When experience finishes:
│     ├─ Get World → Get Subsystem: Lyra Game Phase Subsystem
│     └─ Start Phase: BW_Phase_HeroSelection
│
└─ Done
```

---

## Testing the Integration

### Test Checklist:

1. **Compile Code** ✓
2. **Configure Experience** ✓
3. **Launch PIE** (2 players, Listen Server)
4. **Expected Behavior:**
   - Game starts
   - Hero Selection UI appears immediately
   - Output Log shows: `"Listening for Lyra phase: GamePhase.HeroSelection"`
   - Output Log shows: `"HeroSelection phase started by Lyra phase system!"`
   - Select heroes → Lock
   - UI disappears
   - Output Log shows: `"Hero selection complete. Lyra phase system will progress to next phase."`
   - Warmup phase starts (players spawn, etc.)

### Debug in Output Log:

Look for these messages:
```
LogTemp: BwayHeroSelectionPhaseComponent: Listening for Lyra phase: GamePhase.HeroSelection
LogLyraGamePhase: Starting phase: GamePhase.HeroSelection
LogTemp: BwayHeroSelectionPhaseComponent: HeroSelection phase started by Lyra phase system!
LogTemp: BwayHeroSelectionPhaseComponent: All players ready - ending phase
LogTemp: BwayHeroSelectionPhaseComponent: Hero selection complete.
LogLyraGamePhase: Ending phase: GamePhase.HeroSelection
LogLyraGamePhase: Starting phase: GamePhase.Warmup
```

---

## Common Issues & Solutions

### Issue 1: "Could not find LyraGamePhaseSubsystem"

**Cause:** Phase subsystem not available (very rare)

**Solution:**
- Check that Lyra's phase system is enabled
- Make sure you're testing in PIE or packaged game, not editor preview

### Issue 2: "Warmup still starts immediately"

**Cause:** Experience is starting Warmup phase first

**Solution:**
- Check your Experience configuration
- Make sure HeroSelection phase starts first
- Remove any auto-start from Warmup phase ability

### Issue 3: "UI never appears"

**Cause:** Phase callback not triggering

**Debug:**
1. Check Output Log for "Listening for Lyra phase" message
2. If missing, compilation failed or component not initialized
3. If present, check that phase tag matches exactly: `GamePhase.HeroSelection`
4. Verify Experience actually starts that phase

### Issue 4: "Phase starts but UI not showing"

**Cause:** Blueprint override not working

**Solution:**
- Check that your `BP_BW_HeroSelectionPhase` blueprint properly overrides `ShowHeroSelectionUI`
- Make sure widget class is set in the component
- Add debug Print String nodes to verify Blueprint is executing

---

## Verification Steps

### 1. Compile Verification
After compiling, check for:
- ✅ No compile errors
- ✅ "Build succeeded" message
- ✅ Unreal Editor can open project

### 2. Blueprint Verification
Open `BP_BW_HeroSelectionPhase`:
- ✅ Can override `ShowHeroSelectionUI`, `HideHeroSelectionUI`, `SpawnHeroForPlayer`
- ✅ `GetHeroDataById` function available in Blueprint

### 3. Runtime Verification
Play in PIE:
- ✅ Output Log shows phase listening message
- ✅ Output Log shows Lyra phase start messages
- ✅ UI appears when phase starts
- ✅ Phases progress in correct order

---

## Files Reference

### Modified Files:
1. `Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/HeroSystems/BwayHeroSelectionPhaseComponent.h`
2. `Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Private/HeroSystems/BwayHeroSelectionPhaseComponent.cpp`

### Need to Configure:
1. `Plugins/GameFeatures/BreakawayCore/Content/Experiences/B_BW_Experience_Dev.uasset`
2. `Plugins/GameFeatures/BreakawayCore/Content/Experiences/Phases/BW_Phase_HeroSelection.uasset`
3. `Content/BreakawayContent/B_BWayGameMode.uasset` (remove manual phase start)

### Already Correct:
1. `BwayHeroRegistry.h` - GetHeroDataById is already Blueprint callable ✅
2. `BP_BW_HeroSelectionPhase` - Blueprint overrides should work ✅
3. Hero selection UI widget - Should display when phase starts ✅

---

## Next Steps After It Works

Once hero selection integrates properly with phases:

1. **Add Phase Timer**
   - Auto-lock selections after X seconds
   - Show countdown in UI

2. **Add Phase Transitions**
   - Fade in/out between phases
   - "Hero Selection" title card
   - "Match Starting" transition

3. **Polish Hero Spawning**
   - Actually use selected hero data
   - Apply hero mesh, abilities, etc.
   - Not just `RestartPlayer()`

4. **Handle Edge Cases**
   - Late joiners during hero selection
   - Players disconnecting during selection
   - No hero selected (use fallback)

---

## Need Help?

If you encounter issues:
1. Share Output Log messages (especially phase-related)
2. Check your Experience configuration
3. Verify phase tags match exactly
4. Test with minimal Experience (remove other features temporarily)

Refer to **HERO_SELECTION_PHASE_INTEGRATION_GUIDE.md** for detailed explanations.

---

**Status:** Code changes complete, awaiting configuration in Experience  
**Next Action:** Compile, configure Experience, test!


