# Character Selection UI - Blueprint Setup Guide

This guide explains how to create the Blueprint widget assets that extend the C++ base classes.

## Prerequisites

1. Compile the project to ensure all C++ classes are available
2. Import required font assets (see Font section below)
3. Create UI texture assets (see Assets section below)

---

## Widget Creation Steps

### 1. WBP_HeroSlot

**Location:** `/Game/UI/CharacterSelect/WBP_HeroSlot`

**Parent Class:** `UBwayHeroSlotWidget`

**Widget Hierarchy:**
```
[Root - Size Box] (200x200)
└── [Overlay]
    ├── [Border] - BackgroundBorder
    │   └── Brush: T_UI_Frame_Default (9-slice)
    ├── [Image] - PortraitImage
    │   └── Size: Fill
    ├── [Border] - SelectionFrame (Hidden by default)
    │   └── Brush: T_UI_Frame_Gold (9-slice)
    │   └── Material: M_UI_GoldShimmer (animated)
    ├── [Border] - LockedOverlay (Hidden by default)
    │   └── Color: Black 50% opacity
    │   └── Child: Lock Icon
    └── [Text Block] - PlayerIndicator (Hidden by default)
        └── Position: Top-Left corner
        └── Text: "P1"
```

**Blueprint Implementation Events:**

```
OnHeroDataSet(Portrait, DisplayName):
    - Set PortraitImage.Brush to Portrait texture
    - Store DisplayName for tooltip

OnSelectionStateChanged(bSelected):
    - Set SelectionFrame visibility (Visible if selected)
    - Play selection animation if desired

OnAvailabilityChanged(bAvailable):
    - Set widget opacity (1.0 if available, 0.5 if not)

OnLockedStateChanged(bLocked):
    - Set LockedOverlay visibility
    - Show lock icon if locked

OnPlayerIndicatorChanged(PlayerIndex, PlayerLabel):
    - Set PlayerIndicator visibility (PlayerIndex >= 0)
    - Set PlayerIndicator text to PlayerLabel
```

---

### 2. WBP_AbilityIcon

**Location:** `/Game/UI/CharacterSelect/WBP_AbilityIcon`

**Parent Class:** `UBwayAbilityIconWidget`

**Widget Hierarchy:**
```
[Root - Size Box] (80x80)
└── [Overlay]
    ├── [Border] - BackgroundBorder
    │   └── Brush: T_UI_AbilityFrame (9-slice)
    ├── [Image] - AbilityIcon
    │   └── Size: 64x64, centered
    ├── [Border] - SelectionBorder (Hidden by default)
    │   └── Brush: T_UI_Frame_Gold
    │   └── Padding: -2 (extends outside)
    ├── [Image] - LockedIcon (Hidden by default)
    │   └── Icon: Lock icon
    └── [Image] - UltimateIndicator (Hidden by default)
        └── Icon: Star/special indicator
```

**Blueprint Implementation Events:**

```
OnAbilityDataSet(Icon, AbilityName, bIsUltimate):
    - Set AbilityIcon.Brush to Icon texture
    - Set UltimateIndicator visibility if bIsUltimate

OnSelectionStateChanged(bSelected):
    - Set SelectionBorder visibility
    - Scale animation (1.0 -> 1.1 on select)

OnLockedStateChanged(bLocked):
    - Set LockedIcon visibility
    - Desaturate AbilityIcon if locked

OnCancelSlotStateChanged(bIsCancelSlot):
    - If true: Hide AbilityIcon, Show X icon instead
```

---

### 3. WBP_LevelProgressBar

**Location:** `/Game/UI/CharacterSelect/WBP_LevelProgressBar`

**Parent Class:** `UBwayProgressBarWidget`

**Widget Hierarchy:**
```
[Root - Size Box] (Width: 350, Height: 24)
└── [Overlay]
    ├── [Border] - FrameBorder
    │   └── Brush: T_UI_LevelBar_Frame (9-slice)
    ├── [Progress Bar] - ProgressFill
    │   └── Fill Color: Cyan (#00D4FF)
    │   └── Background: Transparent
    └── [Horizontal Box] - TextOverlay
        ├── [Text Block] - LevelText
        │   └── Font: Rajdhani Medium, 14pt
        │   └── Color: White
        ├── [Spacer]
        └── [Button] - InfoButton (optional)
            └── Icon: "i" info icon
```

**Blueprint Implementation Events:**

```
OnProgressChanged(NewProgress):
    - Set ProgressFill.Percent to NewProgress

OnLevelChanged(CurrentLevel, MaxLevel, LevelText):
    - Set LevelText.Text to LevelText

OnMaxLevelStateChanged(bIsMaxLevel):
    - If max: Change fill color to Gold
    - If max: Add glow effect
```

---

### 4. WBP_CurrencyDisplay

**Location:** `/Game/UI/CharacterSelect/WBP_CurrencyDisplay`

**Parent Class:** `UBwayCurrencyDisplayWidget`

**Widget Hierarchy:**
```
[Root - Size Box] (Width: 120, Height: 48)
└── [Border] - BackgroundBorder
    └── Brush: T_UI_Button_Red (9-slice)
    └── Padding: (16, 8)
    └── Child:
        [Horizontal Box]
        ├── [Image] - CoinIcon
        │   └── Size: 24x24
        │   └── Brush: T_UI_CoinIcon
        ├── [Spacer] - Width: 8
        └── [Text Block] - AmountText
            └── Font: Industry Bold, 18pt
            └── Color: White
```

**Blueprint Implementation Events:**

```
OnAmountChanged(NewAmount, FormattedText):
    - Set AmountText.Text to FormattedText

OnCurrencyIconChanged(NewIcon):
    - Set CoinIcon.Brush to NewIcon

OnAffordabilityChanged(bCanAfford):
    - If not affordable: Change AmountText color to Red
```

---

### 5. WBP_StyledButton

**Location:** `/Game/UI/CharacterSelect/WBP_StyledButton`

**Parent Class:** `UBwayStyledButtonWidget`

**Widget Hierarchy:**
```
[Root - Size Box] (MinWidth: 120, Height: 48)
└── [Border] - ButtonBorder
    └── Brush: Varies by style
    └── Padding: (24, 12)
    └── Child:
        [Overlay]
        ├── [Horizontal Box] - ContentBox
        │   ├── [Image] - ButtonIcon (Hidden by default)
        │   ├── [Spacer] - Width: 8 (if icon visible)
        │   └── [Text Block] - ButtonText
        │       └── Font: Industry Bold, 16pt
        │       └── Color: White
        └── [Circular Throbber] - LoadingSpinner (Hidden)
```

**Style Configurations:**

| Style | Background | Text Color | Hover |
|-------|-----------|------------|-------|
| Primary | Orange (#FF8C00) | White | Lighten 10% |
| Secondary | Red (#CC0000) | White | Lighten 10% |
| Tertiary | Gray (#444444) | White | Lighten 10% |
| Positive | Green (#00CC00) | White | Lighten 10% |
| Negative | Red (#CC0000) | White | Lighten 10% |
| Ghost | Transparent | Cyan | White BG 10% |

**Blueprint Implementation Events:**

```
OnButtonTextChanged(NewText):
    - Set ButtonText.Text to NewText

OnButtonStyleChanged(NewStyle):
    - Switch on NewStyle enum
    - Apply corresponding background brush
    - Apply corresponding text color

OnLoadingStateChanged(bLoading):
    - Set ContentBox visibility (!bLoading)
    - Set LoadingSpinner visibility (bLoading)

OnButtonIconChanged(NewIcon):
    - Set ButtonIcon visibility (NewIcon != null)
    - Set ButtonIcon.Brush to NewIcon
```

---

### 6. WBP_AbilityPreviewPanel

**Location:** `/Game/UI/CharacterSelect/WBP_AbilityPreviewPanel`

**Parent Class:** `UBwayAbilityPreviewWidget`

**Widget Hierarchy:**
```
[Root - Vertical Box]
├── [Size Box] - PreviewContainer (400x225, 16:9 aspect)
│   └── [Overlay]
│       ├── [Image] - PreviewImage
│       │   └── Size: Fill
│       └── [Media Player] - PreviewVideo (optional)
├── [Spacer] - Height: 16
├── [Text Block] - AbilityNameText
│   └── Font: Bebas Neue Bold, 24pt
│   └── Color: Gold (#FFD700)
├── [Spacer] - Height: 8
├── [Rich Text Block] - DescriptionText
│   └── Font: Noto Sans Regular, 14pt
│   └── Color: White 80%
│   └── AutoWrap: True
├── [Spacer] - Height: 16
└── [Horizontal Box] - AbilityIconRow
    └── (Populated with WBP_AbilityIcon instances)
```

**Blueprint Implementation Events:**

```
OnAbilityPreviewSet(PreviewImage, AbilityName, Description, bIsUltimate):
    - Set PreviewImage.Brush to PreviewImage texture
    - Set AbilityNameText.Text to AbilityName (uppercase)
    - Set DescriptionText.Text to Description
    - If bIsUltimate: Add "ULTIMATE" prefix or special styling

OnPreviewVideoAvailable(VideoSource):
    - If video system is set up: Play video
    - Otherwise: Just show static image

OnPreviewCleared():
    - Hide preview image
    - Clear text fields
```

---

### 7. WBP_CharacterSelectScreen (Main Screen)

**Location:** `/Game/UI/CharacterSelect/WBP_CharacterSelectScreen`

**Parent Class:** `UBwayHeroSelectWidget`

**Widget Hierarchy:**
```
[Root - Canvas Panel]
├── [Overlay] - LeftPanel
│   └── Anchor: Top-Left
│   └── Position: (40, 80)
│   └── Size: (520, Auto)
│   └── Children:
│       ├── [Border] - BackgroundPanel
│       │   └── Brush: T_UI_Panel_Dark (9-slice)
│       └── [Vertical Box] - ContentBox
│           ├── [Text Block] - HeroNameText
│           │   └── Font: Bebas Neue Bold Italic, 64pt
│           │   └── Color: Gold (#FFD700)
│           ├── [Text Block] - HeroClassText
│           │   └── Font: Montserrat SemiBold, 18pt
│           │   └── Color: White 70%
│           ├── WBP_LevelProgressBar - LevelBar
│           ├── [Spacer] - Height: 16
│           ├── [Scroll Box] - HeroGridScroll
│           │   └── [Uniform Grid Panel] - HeroGrid
│           │       └── SlotPadding: 4
│           │       └── MinDesiredSlotWidth: 120
│           │       └── MinDesiredSlotHeight: 120
│           ├── [Spacer] - Height: 16
│           └── [Horizontal Box] - PurchaseSection
│               ├── WBP_CurrencyDisplay - CostDisplay
│               ├── [Spacer] - Width: 8
│               └── WBP_StyledButton - TryButton
│                   └── Text: "TRY"
│                   └── Style: Primary
│
├── [Overlay] - RightPanel
│   └── Anchor: Top-Right
│   └── Position: (-40, 80)
│   └── Size: (480, Auto)
│   └── Children:
│       ├── [Image] - Character3DPreview
│       │   └── Size: (480, 540)
│       │   └── Material: Render Target from Scene Capture
│       ├── WBP_AbilityPreviewPanel - AbilityPreview
│       └── [Horizontal Box] - CustomizationTabs
│           ├── WBP_StyledButton - SkinsTab
│           ├── WBP_StyledButton - TauntsTab
│           └── WBP_StyledButton - WeaponsTab
│
├── [Vertical Box] - LoadoutSlots
│   └── Anchor: Right
│   └── Position: (-20, Center)
│   └── Children: 4x Circle buttons with "+"
│
└── [Horizontal Box] - BottomBar
    └── Anchor: Bottom-Stretch
    └── Position: (40, -40)
    └── Children:
        ├── WBP_StyledButton - LeaveButton
        │   └── Text: "LEAVE"
        │   └── Style: Ghost
        │   └── Icon: Back arrow
        ├── [Spacer] - Expanding
        └── [Text Block] - BuildInfoText
            └── Font: Noto Sans, 12pt
            └── Color: White 50%
```

**Blueprint Event Graph Setup:**

```
Event NativeOnActivated:
    1. Call GetAvailableHeroes() to get hero list
    2. For each hero: Create WBP_HeroSlot, call SetHeroData
    3. Add slots to HeroGrid
    4. Bind OnHeroSlotClicked to HandleHeroSlotClicked

HandleHeroSlotClicked(HeroId):
    1. Call SelectHero(HeroId)
    2. Update preview panel with selected hero data

Event OnHeroListChanged:
    1. Refresh the hero grid
    2. Update selection states

Event OnLocalSelectionChanged(NewHeroId):
    1. Update HeroNameText with selected hero name
    2. Update HeroClassText with class name
    3. Update ability icons in preview panel
    4. Update 3D preview (scene capture)

TryButton OnClicked:
    1. Handle purchase/try logic

LeaveButton OnClicked:
    1. Call DeactivateWidget()
```

---

## Asset Checklist

### Textures (Create in `/Game/UI/Textures/`)

- [ ] `T_UI_Panel_Dark` - Dark panel background (512x512, 9-slice)
- [ ] `T_UI_Frame_Gold` - Gold selection frame (256x256, 9-slice)
- [ ] `T_UI_Frame_Default` - Default slot frame (256x256, 9-slice)
- [ ] `T_UI_Button_Orange` - Primary button bg (256x64, 9-slice)
- [ ] `T_UI_Button_Red` - Secondary button bg (256x64, 9-slice)
- [ ] `T_UI_LevelBar_Frame` - Level bar frame (512x32, 9-slice)
- [ ] `T_UI_CoinIcon` - Currency coin icon (64x64)
- [ ] `T_UI_AbilityFrame` - Ability icon frame (128x128, 9-slice)

### Materials (Create in `/Game/UI/Materials/`)

- [ ] `M_UI_GoldShimmer` - Animated gold border effect
- [ ] `M_UI_BackgroundBlur` - Backdrop blur material

### Fonts (Import to `/Game/UI/Fonts/`)

- [ ] Bebas Neue Bold (for hero names)
- [ ] Montserrat SemiBold (for class labels)
- [ ] Rajdhani Medium (for level text)
- [ ] Noto Sans Regular (for descriptions)
- [ ] Industry Bold (for buttons)

---

## Testing Checklist

1. [ ] Hero grid populates correctly with all heroes
2. [ ] Clicking a hero slot selects it
3. [ ] Selected hero shows gold border with animation
4. [ ] Unavailable heroes are grayed out
5. [ ] Player indicators (P1, P2) show correctly in multiplayer
6. [ ] Ability icons update when hero is selected
7. [ ] Clicking ability icon shows its preview
8. [ ] Level bar displays correct progress
9. [ ] Currency display shows correct amount
10. [ ] TRY button visibility based on ownership
11. [ ] LEAVE button closes the widget
12. [ ] UI scales correctly on different resolutions
13. [ ] Ultrawide (21:9) displays correctly

