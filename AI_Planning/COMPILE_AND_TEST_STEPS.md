# Compile and Test Steps

## ✅ Code Status: Ready to Compile

All C++ changes are complete and naming conflicts resolved.

---

## Step 1: Compile the Code

### Option A: In Visual Studio

1. **Open** your solution in Visual Studio
2. **Build** → **Build Solution** (or press `Ctrl+Shift+B`)
3. **Wait** for compilation to complete
4. **Check** Output window for "Build succeeded"

### Option B: In Unreal Editor

1. **Close** Unreal Editor if it's open
2. **Right-click** on `ProjectB.uproject`
3. **Select** "Generate Visual Studio project files"
4. **Open** the .sln file
5. **Build** the solution
6. **Open** Unreal Editor

### Option C: Command Line

```powershell
cd "E:\Unreal Projects\ProjectB"
"C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\Build.bat" LyraEditor Win64 Development -Project="E:\Unreal Projects\ProjectB\ProjectB.uproject" -WaitMutex
```

---

## Step 2: Verify Compilation

**Expected output:**
```
Build succeeded.
Time Elapsed: XX:XX:XX.XX
```

**If you get errors:**
- Check that all includes are correct
- Make sure Lyra's phase system files exist
- Try "Rebuild Solution" instead of "Build Solution"

---

## Step 3: Configure Experience (CRITICAL!)

**Before testing, you MUST configure your Experience to start HeroSelection phase first!**

See: `QUICK_FIX_EXPERIENCE_CONFIGURATION.md`

Quick version:
1. Open `B_BW_Experience_Dev.uasset`
2. Find Actions array
3. Make sure `BW_Phase_HeroSelection` starts before `BW_Phase_Warmup`
4. Save

---

## Step 4: Test in PIE

### Setup:
1. **Play Settings:**
   - Number of Players: 2
   - Net Mode: Play as Listen Server
   - Window Size: 800x600 (for both to fit on screen)

2. **Launch:** Click Play (or Alt+P)

### What You Should See:

**✅ Success Indicators:**

**Output Log:**
```
LogTemp: BwayHeroSelectionPhaseComponent: Listening for Lyra phase: GamePhase.HeroSelection
LogLyraGamePhase: Starting phase: GamePhase.HeroSelection
LogTemp: BwayHeroSelectionPhaseComponent: HeroSelection phase started by Lyra phase system!
```

**In Game:**
- Hero selection UI appears on both windows
- No background gameplay happening
- Can select and lock heroes
- UI disappears when both players lock
- Match starts (warmup/gameplay begins)

**❌ Failure Indicators:**

**If Warmup starts immediately:**
```
LogLyraGamePhase: Starting phase: GamePhase.Warmup  ← Wrong!
```
→ Experience not configured correctly. Go to Step 3.

**If no UI appears:**
- Check that `HeroSelectionWidgetClass` is set in component
- Verify Blueprint override for `ShowHeroSelectionUI` is implemented
- Check Output Log for errors

**If code doesn't compile:**
- Share the exact error message
- Check all files were saved
- Try "Clean Solution" then "Rebuild Solution"

---

## Step 5: Debug Output Log Monitoring

### Add Debug Keys for Testing

In your Game Mode or HUD, add these for debugging:

```blueprint
Event Key: F7
└─ Get World → Get Subsystem: Lyra Game Phase Subsystem
   └─ K2_Start Phase: BW_Phase_HeroSelection
      └─ (Manually trigger phase for testing)

Event Key: F8  
└─ Get Game State.HeroSelectionPhaseComponent
   └─ Start Hero Selection Phase
      └─ (Manually trigger your component)

Event Key: F9
└─ Get World → Get Subsystem: Lyra Game Phase Subsystem
   └─ Is Phase Active: GamePhase.HeroSelection
      └─ Print String: Result
```

---

## Common Issues and Solutions

### Issue 1: "Cannot find LyraGamePhaseSubsystem"

**Cause:** Include path issue or Lyra files missing

**Solution:**
```cpp
// Make sure this is in your .cpp file:
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
```

If the file doesn't exist, your Lyra installation might be incomplete.

### Issue 2: Linking errors

**Error:** "unresolved external symbol"

**Solution:**
Check `BreakawayCore.Build.cs` includes these modules:
```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core",
    "CoreUObject",
    "Engine",
    "LyraGame",  // ← Make sure this is here!
    // ... other modules
});
```

### Issue 3: Compilation succeeds but changes don't appear

**Solution:**
1. Close Unreal Editor completely
2. Delete `Binaries` and `Intermediate` folders
3. Right-click .uproject → "Generate Visual Studio project files"
4. Recompile
5. Launch editor

---

## Verification Checklist

After compilation and configuration:

**Code:**
- [ ] No compilation errors
- [ ] No linker errors
- [ ] Unreal Editor opens successfully
- [ ] No crashes on startup

**Experience Configuration:**
- [ ] Opened Experience asset
- [ ] Found phase configuration
- [ ] Set HeroSelection to start first
- [ ] Saved asset

**Runtime Testing:**
- [ ] PIE launches with 2 players
- [ ] Output Log shows phase listening message
- [ ] Hero Selection phase starts
- [ ] UI appears for both players
- [ ] Can select and lock heroes
- [ ] UI disappears when ready
- [ ] Next phase (Warmup) starts

---

## Next Steps After Successful Test

1. **Implement Blueprint Overrides**
   - ShowHeroSelectionUI (create widgets)
   - HideHeroSelectionUI (remove widgets)
   - SpawnHeroForPlayer (use hero data)

2. **Add Phase Polish**
   - Timer for selection
   - Fade transitions
   - Title cards

3. **Test Edge Cases**
   - Late joiners
   - Player disconnect during selection
   - No hero selected (fallback)

---

## Getting Help

If you encounter issues:

1. **Share Output Log** - Especially lines with:
   - `LogTemp: BwayHeroSelection...`
   - `LogLyraGamePhase: ...`
   
2. **Share Compilation Errors** - Full text of any errors

3. **Describe What You See** - Is UI showing? Are phases progressing?

---

**Estimated Time to Complete:**
- Compilation: 2-5 minutes
- Experience configuration: 5-10 minutes
- Testing: 5 minutes
- **Total: 15-20 minutes**

---

Good luck! 🚀



