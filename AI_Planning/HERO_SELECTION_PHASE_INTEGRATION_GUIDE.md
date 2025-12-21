# Hero Selection Phase - Lyra Phase System Integration

## The Problem

Your hero selection UI shows up, but the game's phase system (controlled by the Experience) continues running phases in the background, starting Warmup phase immediately and bypassing hero selection.

## Root Cause

Two separate systems not communicating:
1. **BwayHeroSelectionPhaseComponent** - Manually manages hero selection
2. **Lyra Game Phase Subsystem** - Automatically progresses through phases defined in Experience

## Solution: Integrate with Lyra's Phase System

---

## Part 1: Update the Phase Component to React to Lyra Phases

The component should LISTEN for when the HeroSelection phase starts, not start itself.

### Changes Needed in C++

**File: `BwayHeroSelectionPhaseComponent.h`**

Add these to the public section (around line 31):

```cpp
protected:
	//~UActorComponent interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~End of UActorComponent interface

	// NEW: React to Lyra's phase system
	/** Called when the HeroSelection game phase becomes active */
	UFUNCTION()
	void OnHeroSelectionPhaseStarted(const FGameplayTag& PhaseTag);
	
	/** Called when we need to end the phase and progress to next */
	void EndPhaseAndProgressToNext();
```

**File: `BwayHeroSelectionPhaseComponent.cpp`**

Replace the BeginPlay function (around line 19):

```cpp
void UBwayHeroSelectionPhaseComponent::BeginPlay()
{
	Super::BeginPlay();

	// Only server manages the phase
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// NEW: Listen for when Lyra's phase system starts HeroSelection phase
	if (UWorld* World = GetWorld())
	{
		if (ULyraGamePhaseSubsystem* PhaseSubsystem = World->GetSubsystem<ULyraGamePhaseSubsystem>())
		{
			// Bind to the HeroSelection phase tag
			FGameplayTag HeroSelectionTag = PhaseTag; // Uses the PhaseTag property
			if (!HeroSelectionTag.IsValid())
			{
				HeroSelectionTag = FGameplayTag::RequestGameplayTag(FName("GamePhase.HeroSelection"));
			}

			PhaseSubsystem->WhenPhaseStartsOrIsActive(
				HeroSelectionTag,
				EPhaseTagMatchType::ExactMatch,
				FLyraGamePhaseTagDelegate::CreateUObject(this, &UBwayHeroSelectionPhaseComponent::OnHeroSelectionPhaseStarted)
			);
			
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Listening for phase: %s"), *HeroSelectionTag.ToString());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Initialized"));
}
```

Add this new function after BeginPlay:

```cpp
void UBwayHeroSelectionPhaseComponent::OnHeroSelectionPhaseStarted(const FGameplayTag& PhaseTag)
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: HeroSelection phase started by Lyra system!"));
	
	// Now start our hero selection logic
	StartHeroSelectionPhase();
}
```

Update HandleAllPlayersReady (around line 186):

```cpp
void UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: All players ready - ending phase"));

	// End the phase
	EndHeroSelectionPhase();
	
	// NEW: Tell Lyra's phase system to progress to next phase
	EndPhaseAndProgressToNext();
}
```

Add this new function at the end of the file:

```cpp
void UBwayHeroSelectionPhaseComponent::EndPhaseAndProgressToNext()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// The Lyra phase system will automatically end the HeroSelection phase
	// and progress to the next phase when this phase's ability ends.
	// This happens automatically through the phase ability lifecycle.
	
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Phase complete, Lyra will progress to next phase"));
	
	// Optional: You can manually trigger the next phase if needed
	// For now, let the Experience definition handle the phase flow
}
```

---

## Part 2: Configure Your Experience (In Unreal Editor)

Since .uasset files can't be edited in text, you need to do this in the Unreal Editor.

### Step 1: Open Your Experience

1. **Open:** `Plugins/GameFeatures/BreakawayCore/Content/Experiences/B_BW_Experience_Dev.uasset`
   (or whichever experience you're using for testing)

2. **Look for:** "Actions" array or "Game Feature Actions" list

### Step 2: Check Phase Configuration

Look for actions of type **"Game Feature Action Add Abilities"** or similar that reference phases.

You should see references to:
- `BW_Phase_HeroSelection`
- `BW_Phase_Warmup`
- `BW_Phase_Playing`

### Step 3: Ensure Phases are in Correct Order

The phases need to be configured so that:

**HeroSelection starts FIRST**, before Warmup.

If you see an "Add Abilities" action or "Start Phase" action, check:
- Does it automatically start a phase on BeginPlay?
- Is there a "Default Phase" or "Initial Phase" setting?
- Change it to `BW_Phase_HeroSelection` instead of `BW_Phase_Warmup`

### Common Configuration Patterns:

#### Pattern A: Experience starts default phase automatically
```
Experience Actions:
├─ Add Abilities (includes phase abilities)
└─ Default Starting Phase: BW_Phase_HeroSelection  ← Make sure this is first!
```

#### Pattern B: Game Mode starts the phase
```
In your Game Mode or Experience:
├─ On Experience Loaded
└─ Start Phase: BW_Phase_HeroSelection
```

---

## Part 3: Update Phase Assets (If Needed)

### BW_Phase_HeroSelection Configuration

1. **Open:** `Plugins/GameFeatures/BreakawayCore/Content/Experiences/Phases/BW_Phase_HeroSelection.uasset`

2. **Verify:**
   - **Game Phase Tag:** `GamePhase.HeroSelection`
   - **Phase Ability Class:** Can be the base `LyraGamePhaseAbility` or a custom BP

3. **Phase Behavior Options:**

   **Option A: Simple Phase (Recommended for now)**
   - Use base `LyraGamePhaseAbility`
   - Let your component handle all logic
   - Phase just acts as a "marker" that this stage is active

   **Option B: Custom Phase Ability**
   - Create `BP_BW_PhaseAbility_HeroSelection` (Blueprint child of LyraGamePhaseAbility)
   - Override `ActivateAbility` to start your UI
   - Override `EndAbility` for cleanup

### BW_Phase_Warmup Configuration

1. **Open:** `Plugins/GameFeatures/BreakawayCore/Content/Experiences/Phases/BW_Phase_Warmup.uasset`

2. **Verify:**
   - **Game Phase Tag:** `GamePhase.Warmup` or `GamePhase.Playing.Warmup`
   - This should NOT auto-start - it should wait for HeroSelection to end

---

## Part 4: Game Mode Integration

### Option A: Let Experience Handle Everything (Recommended)

Don't manually start the hero selection phase from your Game Mode's BeginPlay.

**Remove this from BP_BWayGameMode BeginPlay:**
```
❌ Get Game State.HeroSelectionPhaseComponent.StartHeroSelectionPhase()
```

**Instead:** Let the Experience start the phase automatically.

### Option B: Manually Control Phase Flow

If you want the Game Mode to control phases:

**In BP_BWayGameMode:**

```blueprint
Event BeginPlay
├─ Parent: BeginPlay
│
├─ Bind Event: On Experience Loaded
│  └─ When experience finishes loading:
│     ├─ Get World → Get Subsystem: Lyra Game Phase Subsystem
│     │
│     └─ Start Phase
│        └─ Phase: BW_Phase_HeroSelection (the ability class)
│
└─ (Phase system handles the rest)
```

---

## Part 5: Phase Transition Flow

Once configured, the flow will be:

```
1. Experience Loads
   ↓
2. Experience starts BW_Phase_HeroSelection (via phase subsystem)
   ↓
3. Phase subsystem activates the HeroSelection phase ability
   ↓
4. Your HeroSelectionPhaseComponent receives "OnHeroSelectionPhaseStarted" callback
   ↓
5. Component shows UI, registers players, waits for ready
   ↓
6. When all players ready:
   ├─ Component hides UI
   ├─ Component spawns heroes
   └─ Component ends the phase
   ↓
7. Phase subsystem automatically ends HeroSelection phase ability
   ↓
8. Experience (or phase ability) starts next phase: BW_Phase_Warmup
   ↓
9. Game continues normally
```

---

## Debugging Tips

### Check Phase Status

Add this to your Game Mode or HUD for debugging:

```blueprint
Event Tick
├─ Get World → Get Subsystem: Lyra Game Phase Subsystem
│
├─ Is Phase Active?
│  └─ Phase Tag: GamePhase.HeroSelection
│  └─ Result: (show on screen)
│
└─ Print String: "HeroSelection Active: [result]"
```

### Verify Phase Tag Matches

Make sure these all match:
1. `BW_Phase_HeroSelection.uasset` → GamePhaseTag property
2. `BwayHeroSelectionPhaseComponent` → PhaseTag property
3. Your listening code uses the same tag

Common tag: `GamePhase.HeroSelection`

### Log Phase Transitions

The phase subsystem logs all transitions. Check your Output Log for:
```
LogLyraGamePhase: Starting phase: GamePhase.HeroSelection
LogLyraGamePhase: Ending phase: GamePhase.HeroSelection
LogLyraGamePhase: Starting phase: GamePhase.Warmup
```

---

## Summary Checklist

- [ ] Update `BwayHeroSelectionPhaseComponent.cpp` to listen for phase start
- [ ] Add `OnHeroSelectionPhaseStarted` callback function
- [ ] Add `EndPhaseAndProgressToNext` function
- [ ] Recompile C++ code
- [ ] Open your Experience in Unreal Editor
- [ ] Verify HeroSelection phase is configured to start first
- [ ] Remove manual phase start from Game Mode BeginPlay
- [ ] Test: UI shows, phases progress correctly
- [ ] Verify: Warmup doesn't start until hero selection completes

---

## Next Steps After Integration

1. **Polish phase transitions**
   - Add fade in/out between phases
   - Loading screens

2. **Add phase UI indicators**
   - "Hero Selection" title card
   - "Match Starting" transition

3. **Handle late joiners**
   - What happens if player joins during hero selection?
   - Auto-assign default hero?

4. **Add phase timer**
   - Auto-lock selections after X seconds
   - Show countdown in UI

---

## Need Help?

If phases still aren't working correctly:
1. Share your Experience configuration
2. Check Output Log for phase transition messages
3. Verify phase tags match exactly
4. Test with a simple phase (empty ability) first

---

**Created:** $(date)  
**Status:** Ready for Implementation



