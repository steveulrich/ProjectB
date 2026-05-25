# Character Selection UI - Integration Guide

> **See also:** [SYSTEMS_INDEX.md](./SYSTEMS_INDEX.md) · [MatchFlow_and_Phases.md](./MatchFlow_and_Phases.md) · [UI_System.md](./UI_System.md)

This guide explains how to integrate the character selection UI with your game systems.

---

## Quick Start

### 1. Compile the Project

After adding all the new C++ classes, compile the project to make them available in Blueprints.

### 2. Create Blueprint Widgets

Follow the [Blueprint Setup Guide](CharacterSelect_BlueprintSetup.md) to create the Widget Blueprints.

### 3. Create Assets

Follow the [Asset Guide](CharacterSelect_AssetGuide.md) to create textures, materials, and import fonts.

### 4. Wire Up the Main Screen

The main screen `WBP_CharacterSelectScreen` extends `UBwayHeroSelectWidget` which provides all the data binding functions.

---

## Data Flow

```
┌─────────────────────┐     ┌─────────────────────┐
│  UBwayHeroRegistry  │────>│  UBwayHeroDataAsset │
│  (GameInstance)     │     │  (Per Hero)         │
└─────────────────────┘     └─────────────────────┘
         │                           │
         │ GetAvailableHeroes()      │ Portrait, Class, Abilities
         ▼                           ▼
┌─────────────────────────────────────────────────┐
│           UBwayHeroSelectWidget                 │
│  - FHeroDisplayInfo array                       │
│  - Selection state                              │
│  - Availability checking                        │
└─────────────────────────────────────────────────┘
         │                           │
         │ Events                    │ Data
         ▼                           ▼
┌─────────────────────────────────────────────────┐
│         WBP_CharacterSelectScreen (BP)          │
│  - Hero Grid (WBP_HeroSlot instances)           │
│  - Ability Preview                              │
│  - Level Progress                               │
└─────────────────────────────────────────────────┘
```

---

## Blueprint Event Graph - Main Screen

### Event: Construct

```
Event Construct
  │
  ├─> Call "GetAvailableHeroes" (inherited)
  │     Returns: TArray<FHeroDisplayInfo>
  │
  ├─> For Each hero in array:
  │     ├─> Create Widget: WBP_HeroSlot
  │     ├─> Call SetHeroData(hero display info)
  │     ├─> Bind OnHeroSlotClicked delegate
  │     └─> Add to HeroGrid (Uniform Grid Panel)
  │
  └─> If has valid selection:
        └─> Update preview panel
```

### Event: OnHeroListChanged (Blueprint Implementable)

```
Event OnHeroListChanged
  │
  ├─> Clear existing hero grid children
  │
  ├─> Call "GetAvailableHeroes"
  │
  └─> Rebuild grid (same as Construct)
```

### Event: OnLocalSelectionChanged (Blueprint Implementable)

```
Event OnLocalSelectionChanged (NewHeroId)
  │
  ├─> Call "GetSelectedHeroDisplayInfo"
  │     Returns: FHeroDisplayInfo (with full data)
  │
  ├─> Update HeroNameText with DisplayName
  │
  ├─> Update HeroClassText with ClassName
  │
  ├─> Update LevelProgressBar (if level data available)
  │
  ├─> Update AbilityPreviewPanel:
  │     ├─> Clear ability icons
  │     ├─> For Each ability in Abilities array:
  │     │     ├─> Create WBP_AbilityIcon
  │     │     ├─> Call SetAbilityData
  │     │     └─> Add to AbilityIconRow
  │     └─> Select first ability for preview
  │
  ├─> Update 3D Preview (Scene Capture):
  │     └─> Spawn/update preview actor with HeroMesh
  │
  └─> Update selection highlight in grid:
        └─> For Each slot: SetSelectionState(slot.HeroId == NewHeroId)
```

### Function: HandleHeroSlotClicked

```
Custom Event HandleHeroSlotClicked (HeroId)
  │
  ├─> Call "SelectHero" (inherited)
  │     Returns: bool (success)
  │
  └─> If failed:
        └─> Play error sound/feedback
```

### Function: HandleAbilityIconClicked

```
Custom Event HandleAbilityIconClicked (AbilityIndex)
  │
  ├─> Get current hero display info
  │
  ├─> Get ability at index from Abilities array
  │
  ├─> Update AbilityPreviewPanel:
  │     ├─> Set preview image
  │     ├─> Set ability name
  │     └─> Set description
  │
  └─> Update icon selection states
```

---

## Hero Data Asset Setup

For each hero, populate the `UBwayHeroDataAsset`:

1. **Basic Info:**
   - Display Name
   - Hero Class (enum)
   - Portrait texture

2. **Ability Display Info (Array):**
   
   For each ability slot (typically 6):
   ```
   [0] Primary Attack
   [1] Secondary Ability
   [2] Ability 1
   [3] Ability 2
   [4] Ultimate
   [5] Passive (or Cancel slot in UI)
   ```

   Each entry needs:
   - Ability Name
   - Description
   - Icon texture
   - Preview image (optional)
   - Preview video (optional)
   - bIsUltimate flag

---

## Showing the Widget

### From Game Mode / Experience

```cpp
// In your game mode or experience component
void ShowHeroSelection(APlayerController* PC)
{
    if (UPrimaryGameLayout* Layout = UPrimaryGameLayout::GetPrimaryGameLayout(PC))
    {
        Layout->PushWidgetToLayerStackAsync<UBwayHeroSelectWidget>(
            FGameplayTag::RequestGameplayTag("UI.Layer.Menu"),
            true, // bSuspendInputUntilComplete
            HeroSelectWidgetClass,
            [](EAsyncWidgetLayerState State, UBwayHeroSelectWidget* Widget)
            {
                if (State == EAsyncWidgetLayerState::AfterPush && Widget)
                {
                    // Widget is now active
                }
            }
        );
    }
}
```

### From Blueprint

```
Push Widget to Layer Stack Async
  - Layer: UI.Layer.Menu (GameplayTag)
  - Widget Class: WBP_CharacterSelectScreen
  - Suspend Input: True
```

---

## 3D Character Preview Setup

### Scene Capture Component 2D

1. Create a Blueprint Actor: `BP_CharacterPreviewCapture`
2. Add components:
   - Scene Capture Component 2D
   - Skeletal Mesh Component (for preview character)
   - Point Lights (for lighting)

3. In your GameMode or Level Blueprint:
   - Spawn preview actor at fixed location (out of player view)
   - Get Scene Capture Render Target
   - Assign to Character3DPreview Image widget

### Updating Preview

```
Function: UpdateCharacterPreview(HeroDataAsset)
  │
  ├─> Get preview actor reference
  │
  ├─> Set Skeletal Mesh to HeroDataAsset.HeroMesh
  │
  ├─> Set Animation Blueprint to HeroDataAsset.AnimationBP
  │
  └─> Capture scene (or let continuous capture update)
```

---

## Testing Checklist

### Basic Functionality
- [ ] Widget opens when triggered
- [ ] Hero grid populates with all heroes
- [ ] Clicking hero slot fires OnHeroSlotClicked
- [ ] SelectHero() successfully updates selection
- [ ] Selection persists across widget close/reopen

### Visual Feedback
- [ ] Selected hero shows gold border
- [ ] Unavailable heroes are grayed/locked
- [ ] Hover state works on hero slots
- [ ] Ability icons show selection state

### Data Binding
- [ ] Hero name displays correctly
- [ ] Hero class displays correctly
- [ ] Ability icons show correct textures
- [ ] Ability preview updates on icon click
- [ ] Level bar shows correct progress (if implemented)

### Network (Multiplayer)
- [ ] Selection replicates to server
- [ ] Other players see selection indicators (P1, P2)
- [ ] Hero availability updates when teammate selects

---

## Troubleshooting

### "No hero registry available"
- Ensure `UBwayHeroRegistry` is properly registered as a Game Instance Subsystem
- Check that hero assets are registered with Asset Manager (Primary Asset Type: "HeroDataAsset")

### Hero grid is empty
- Verify heroes exist in `/BreakawayCore/Characters/Heroes/`
- Check Asset Manager configuration in Project Settings
- Ensure hero data assets have Primary Asset ID set

### Selection not working
- Check that `ABwayPlayerState` has `ServerSetSelectedHeroId` implemented
- Verify PlayerState is properly replicated
- Check network authority

### Abilities not showing
- Populate `AbilityDisplayInfos` array in hero data assets
- Ensure icon textures are assigned
- Check that array is not empty

---

## File Summary

### New C++ Classes

| File | Purpose |
|------|---------|
| `UI/BwayHeroSlotWidget.h/cpp` | Hero grid slot widget |
| `UI/BwayAbilityIconWidget.h/cpp` | Ability bar icon widget |
| `UI/BwayProgressBarWidget.h/cpp` | Level progress bar widget |
| `UI/BwayCurrencyDisplayWidget.h/cpp` | Currency display widget |
| `UI/BwayStyledButtonWidget.h/cpp` | Styled button widget |
| `UI/BwayAbilityPreviewWidget.h/cpp` | Ability preview panel widget |

### Modified Files

| File | Changes |
|------|---------|
| `HeroSystems/BwayHeroDataAsset.h` | Added FAbilityDisplayInfo, EHeroClass, AbilityDisplayInfos |
| `HeroSystems/BwayHeroSelectWidget.h/cpp` | Extended FHeroDisplayInfo, added helper functions |

### Documentation

| File | Purpose |
|------|---------|
| `CharacterSelect_BlueprintSetup.md` | Blueprint widget creation guide |
| `CharacterSelect_AssetGuide.md` | Texture, material, font specifications |
| `CharacterSelect_Integration.md` | This integration guide |

