# Understanding Events vs Callbacks in Hero Selection Component

## The Two Different "Phase Started" Functions

Your component has two related but **distinct** concepts with similar names. Here's the difference:

---

## 1. `OnHeroSelectionPhaseStarted` (Public Blueprint Event)

**Location:** Header file, line 87
```cpp
UPROPERTY(BlueprintAssignable, Category = "Hero Selection Phase")
FOnHeroSelectionPhaseEvent OnHeroSelectionPhaseStarted;
```

### What It Is:
- A **Blueprint-assignable delegate** (event dispatcher)
- A **public notification** that external systems can listen to
- An **outbound broadcast** - "Hey everyone, hero selection started!"

### Purpose:
- Allows **other Blueprints/systems** to react when hero selection begins
- Examples of listeners:
  - UI systems might show a title card
  - Audio system might play music
  - Camera system might change camera angle
  - Analytics might log the event

### Direction:
**OUTBOUND** - Your component broadcasts TO others

### Code Flow:
```cpp
void UBwayHeroSelectionPhaseComponent::StartHeroSelectionPhase()
{
    // ... do stuff ...
    
    // Then broadcast to anyone listening:
    OnHeroSelectionPhaseStarted.Broadcast();  ← Tells the world!
}
```

### Blueprint Usage:
```
Other Blueprint:
├─ Get Hero Selection Phase Component
└─ Bind Event to On Hero Selection Phase Started
   └─ When event fires:
      └─ Play Title Card Animation
      └─ Start Music
      └─ etc.
```

---

## 2. `HandleLyraPhaseActivated()` (Internal Callback Function)

**Location:** Header file, line 112 (renamed from previous conflict)
```cpp
/** Internal callback when Lyra's Game Phase system activates the HeroSelection phase */
UFUNCTION()
void HandleLyraPhaseActivated(const FGameplayTag& InPhaseTag);
```

### What It Is:
- A **private callback function**
- An **internal handler** that responds to Lyra's phase system
- An **inbound receiver** - "Lyra told me the phase started!"

### Purpose:
- **Receives notification** from Lyra's phase subsystem
- Triggers the component to start its hero selection logic
- Part of the **integration with Lyra's phase system**

### Direction:
**INBOUND** - Lyra's phase system calls INTO your component

### Code Flow:
```cpp
void UBwayHeroSelectionPhaseComponent::BeginPlay()
{
    // Register to listen for Lyra's phase activation:
    PhaseSubsystem->WhenPhaseStartsOrIsActive(
        HeroSelectionTag,
        EPhaseTagMatchType::ExactMatch,
        FLyraGamePhaseTagDelegate::CreateUObject(
            this, 
            &UBwayHeroSelectionPhaseComponent::HandleLyraPhaseActivated  ← Register callback
        )
    );
}

// Later, when Lyra starts the phase:
void UBwayHeroSelectionPhaseComponent::HandleLyraPhaseActivated(const FGameplayTag& InPhaseTag)
{
    // Lyra just told us the phase is active!
    StartHeroSelectionPhase();  ← Start our logic
}
```

### Not Used Directly:
This function is **never called by you** - only by Lyra's phase system internally.

---

## Complete Flow Diagram

```
┌─────────────────────────────────────────────────────┐
│  Lyra Game Phase Subsystem                          │
│  (Activates GamePhase.HeroSelection)                │
└────────────────┬────────────────────────────────────┘
                 │
                 │ Calls callback delegate
                 ▼
┌─────────────────────────────────────────────────────┐
│  HandleLyraPhaseActivated()                         │  ← INBOUND (private)
│  (Internal callback - responds to Lyra)            │
└────────────────┬────────────────────────────────────┘
                 │
                 │ Triggers component logic
                 ▼
┌─────────────────────────────────────────────────────┐
│  StartHeroSelectionPhase()                          │
│  (Main function that starts hero selection)         │
└────────────────┬────────────────────────────────────┘
                 │
                 │ Does work, then broadcasts
                 ▼
┌─────────────────────────────────────────────────────┐
│  OnHeroSelectionPhaseStarted.Broadcast()            │  ← OUTBOUND (public)
│  (Public event - notifies external systems)         │
└────────────────┬────────────────────────────────────┘
                 │
                 │ Broadcasts to all listeners
                 ▼
┌─────────────────────────────────────────────────────┐
│  External Blueprints/Systems                        │
│  - UI widgets                                       │
│  - Audio systems                                    │
│  - Camera controllers                               │
│  - Analytics                                        │
└─────────────────────────────────────────────────────┘
```

---

## Key Differences Summary

| Aspect | `OnHeroSelectionPhaseStarted` (Event) | `HandleLyraPhaseActivated()` (Callback) |
|--------|--------------------------------------|----------------------------------------|
| **Type** | Blueprint Delegate (UPROPERTY) | Function (UFUNCTION) |
| **Visibility** | Public | Protected |
| **Direction** | Outbound (broadcasts) | Inbound (receives) |
| **Purpose** | Notify external systems | Respond to Lyra phase system |
| **Called By** | Your component (manually) | Lyra phase subsystem (automatically) |
| **Listeners** | Any Blueprint/C++ that binds | Only Lyra phase subsystem |
| **When** | After logic starts | Before logic starts |
| **Blueprint Usage** | Bind event in other Blueprints | Not used in Blueprint (internal) |

---

## Naming Convention Explanation

### Why "Handle" Prefix?

Using `Handle` prefix (instead of `On`) indicates:
- This is an **internal handler/callback**
- Not meant to be called externally
- Responds to an event from another system

### Common Unreal Naming Patterns:

```cpp
// Events/Delegates (OUTBOUND):
OnSomethingHappened     // Broadcast when something happens
OnXChanged              // Broadcast when X changes
OnYCompleted            // Broadcast when Y completes

// Callbacks (INBOUND):
HandleSomethingHappened // Handle when external system notifies
OnRep_Variable          // Replication notification callback
ReceiveXEvent           // Receive event from external source
```

---

## Analogy

Think of it like a phone system:

### `HandleLyraPhaseActivated()` (Callback)
- **Your phone ringing** - Someone (Lyra) is calling YOU
- **Inbound call** - You answer and receive information
- **Private** - Only specific callers (Lyra) have this number

### `OnHeroSelectionPhaseStarted` (Event)
- **You calling others** - You're announcing news to everyone
- **Outbound broadcast** - You're on a conference call with all listeners
- **Public** - Anyone can join the call and listen

---

## Code Example in Context

```cpp
// In BeginPlay:
void UBwayHeroSelectionPhaseComponent::BeginPlay()
{
    // Subscribe to Lyra's "phone calls"
    PhaseSubsystem->WhenPhaseStartsOrIsActive(
        HeroSelectionTag,
        EPhaseTagMatchType::ExactMatch,
        FLyraGamePhaseTagDelegate::CreateUObject(
            this, 
            &UBwayHeroSelectionPhaseComponent::HandleLyraPhaseActivated
        )
    );
}

// Lyra "calls" us:
void UBwayHeroSelectionPhaseComponent::HandleLyraPhaseActivated(const FGameplayTag& InPhaseTag)
{
    // Answer the call: "Ok, phase is active!"
    StartHeroSelectionPhase();
}

// Start our logic:
void UBwayHeroSelectionPhaseComponent::StartHeroSelectionPhase()
{
    // Do all the hero selection work...
    ShowHeroSelectionUI();
    Manager->StartHeroSelection();
    
    // Now "call" everyone else to tell them:
    OnHeroSelectionPhaseStarted.Broadcast();  // Announce to the world!
}

// Somewhere else in Blueprint:
// Player's HUD Blueprint:
// Event BeginPlay
//     Get Hero Selection Phase Component
//     Bind Event to OnHeroSelectionPhaseStarted
//         When fired: Show Title Card "HERO SELECTION"
```

---

## Why This Matters

**Without understanding the difference:**
- You might try to call `HandleLyraPhaseActivated()` directly ❌
- You might wonder why there are two similar-sounding things ❓
- You might not know which one to bind to in Blueprint ❓

**With understanding:**
- Bind to `OnHeroSelectionPhaseStarted` in your Blueprints ✅
- Let `HandleLyraPhaseActivated()` be called by Lyra automatically ✅
- Use the right tool for the right job ✅

---

## Practical Usage

### If You Want to React When Hero Selection Starts:

**In Any Blueprint:**
```
Event BeginPlay
├─ Get Game State
├─ Get Component: Hero Selection Phase Component
└─ Bind Event to On Hero Selection Phase Started  ← Use this!
   └─ Custom Event: On Hero Selection Began
      ├─ Play Sound
      ├─ Show Widget
      └─ etc.
```

### You Should NOT:
```
❌ Try to call HandleLyraPhaseActivated() manually
❌ Override HandleLyraPhaseActivated() in Blueprint
❌ Bind to HandleLyraPhaseActivated (it's internal)
```

---

## Summary

- **`HandleLyraPhaseActivated()`** = **RECEIVES** from Lyra (inbound, private, automatic)
- **`OnHeroSelectionPhaseStarted`** = **BROADCASTS** to everyone (outbound, public, manual)

Both relate to the hero selection phase starting, but serve completely different purposes in the system architecture!

---

**Now you know why they both exist and what each one does!** 🎓


