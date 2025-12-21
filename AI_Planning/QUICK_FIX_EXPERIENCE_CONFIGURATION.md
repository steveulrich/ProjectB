# Quick Fix: Experience Configuration for Hero Selection

## The Critical Step You Need to Do

Your C++ code is now ready and integrated with Lyra's phase system. The **only thing left** is to configure your Experience to start the HeroSelection phase FIRST.

---

## Step-by-Step in Unreal Editor

### 1. Find Which Experience You're Using

Check your game mode or world settings to see which experience loads. Likely one of:
- `B_BW_Experience_Dev`
- `B_BW_Experience_CaptureTheRelic`
- `B_BW_Experience_BallMode`

### 2. Open the Experience Asset

**Path:** `Plugins/GameFeatures/BreakawayCore/Content/Experiences/`

Double-click your Experience asset to open it.

### 3. Look for "Actions" Array

In the Details panel, find the **"Actions"** array. This contains all the Game Feature Actions.

### 4. Find the Phase-Related Actions

Scroll through the Actions list looking for:

#### Option A: Look for "Add Abilities" Action
- Type: `GameFeatureAction_AddAbilities`
- Or: `Add Abilities to Avatar`
- This might grant phase abilities

#### Option B: Look for Scoring Component
- Type: Something with "Scoring" in the name
- Check if it has a "Default Phase" or "Start Phase" setting

#### Option C: Look for Experience Definition
- Some experiences have a separate "Experience Definition" asset
- Check that asset for phase configuration

### 5. Configure to Start HeroSelection First

Depending on what you found above:

#### If Using Add Abilities Action:

**In the Action details:**
```
Abilities To Grant:
├─ [0] BW_Phase_HeroSelection (should be FIRST!)
├─ [1] BW_Phase_Warmup
├─ [2] BW_Phase_Playing
└─ [3] Other phases...
```

**Or if using a different structure:**
```
Default Starting Phase: BW_Phase_HeroSelection
```

#### If Using Scoring Component:

Look for something like `B_BW_ScoringBase` in the Actions:
```
Component Class: B_BW_ScoringBase
└─ Check its properties for:
   ├─ Auto Start Phase: FALSE (disable this!)
   ├─ Or: Initial Phase: BW_Phase_HeroSelection
   └─ Or: Remove this component temporarily for testing
```

---

## Quick Test Method

### Minimal Test Configuration:

1. **Create a Test Experience** (optional but recommended):
   - Duplicate `B_BW_Experience_Dev`
   - Name it `B_BW_Experience_HeroTest`
   - Remove ALL actions except essentials

2. **Add Only Phase Actions:**
   ```
   Actions:
   ├─ [0] Add Abilities (with phase abilities)
   │      └─ Start with: BW_Phase_HeroSelection
   └─ [1] Your other essential actions
   ```

3. **Set Your Game Mode to Use This Experience**

4. **Test:** Launch PIE and see if hero selection appears

---

## Alternative: Manual Phase Start in Game Mode

If you can't figure out the Experience configuration, you can temporarily bypass it:

### In BP_BWayGameMode:

```blueprint
Event BeginPlay
├─ Parent: BeginPlay
│
├─ Delay: 1.0 second (wait for experience to load)
│
├─ Get World
├─ Get Subsystem: Lyra Game Phase Subsystem
│
└─ K2_Start Phase
   └─ Phase: BW_Phase_HeroSelection (select the ability class)
   └─ Phase Ended: (bind to callback if needed)
```

**This ensures HeroSelection starts first**, regardless of Experience configuration.

---

## How to Know It's Working

### Success Indicators:

**Output Log should show:**
```
LogTemp: BwayHeroSelectionPhaseComponent: Listening for Lyra phase: GamePhase.HeroSelection
LogLyraGamePhase: Starting phase: GamePhase.HeroSelection
LogTemp: BwayHeroSelectionPhaseComponent: HeroSelection phase started by Lyra phase system!
```

**In Game:**
- Hero selection UI appears immediately
- Game doesn't progress to Warmup until you're done
- No background phase changes while selecting

**Failure Indicators:**
```
LogLyraGamePhase: Starting phase: GamePhase.Warmup  ← Started Warmup instead!
```

---

## If You're Still Stuck

### Debug: Check What Phases Are Starting

Add this to your Game Mode to see what phases start:

```blueprint
Event BeginPlay
├─ Get World → Get Subsystem: Lyra Game Phase Subsystem
│
└─ Bind: K2_When Phase Starts Or Is Active
   ├─ Phase Tag: GamePhase (parent tag to catch all phases)
   ├─ Match Type: Partial Match
   │
   └─ When Phase Active:
      └─ Print String: "Phase Started: " + [Phase Tag]
```

This will print EVERY phase that starts, helping you see the order.

---

## Common Experience Structures

### Lyra Shooter Pattern:
```
Experience Actions:
├─ Add Input Mapping
├─ Add Abilities (includes phases)
│  └─ Grant on spawn: Phase abilities
├─ Add UI Layout
└─ Add Teams/Scoring
   └─ This often auto-starts Warmup! ← Problem
```

### Your Breakaway Pattern (likely):
```
Experience Actions:
├─ Load Hero System
├─ Add Scoring Component
│  └─ Check if this starts Warmup automatically!
├─ Add Abilities
└─ Add UI
```

---

## Final Checklist

- [ ] Compiled C++ code successfully
- [ ] Found which Experience is being used
- [ ] Opened Experience asset in editor
- [ ] Found Actions array
- [ ] Located phase-starting configuration
- [ ] Changed to start HeroSelection first
- [ ] Saved Experience asset
- [ ] Tested in PIE
- [ ] Output Log shows correct phase start
- [ ] Hero selection UI appears
- [ ] Phases progress in correct order

---

## Estimated Time

- **Finding Experience:** 2 minutes
- **Configuring it:** 5 minutes
- **Testing:** 2 minutes
- **Total:** ~10 minutes

---

## What If Experience Files Are Too Complex?

### Nuclear Option: Direct Game Mode Control

Put this in your Game Mode and bypass Experience phase control entirely:

```blueprint
Event BeginPlay
├─ Parent: BeginPlay
│
├─ Branch: Has Authority?
│  └─ False: Return
│
├─ Delay: 2.0 seconds (let everything initialize)
│
├─ Get World → Get Subsystem: Lyra Game Phase Subsystem
│
├─ Is Phase Active?
│  └─ Phase Tag: GamePhase.HeroSelection
│  └─ Result: If FALSE, start it manually
│
└─ If Not Active:
   └─ K2_Start Phase: BW_Phase_HeroSelection
```

This **forces** hero selection to start, overriding whatever the Experience does.

---

**Bottom Line:** You need to make the Experience (or Game Mode) start `GamePhase.HeroSelection` BEFORE any other phase. That's the only missing piece!



