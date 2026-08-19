# Breakaway core HUD visual layout

Visual UMG layout notes from a reference screenshot. This is not the live C++ HUD contract.

**Last reviewed:** August 19, 2026 (banner only)

For widget classes, slots, and Step 19.5 behavior, see [UI system](../Plugins/GameFeatures/BreakawayCore/Docs/UI_System.md) and [Core HUD layout setup](../Plugins/GameFeatures/BreakawayCore/Docs/CoreHUD_Layout_Setup.md). Minimap, buff timers, and some chrome in this guide are still post-slice.

---

## Overview: UI sections

The HUD consists of these main sections:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ [Team1 Portraits]        [TIMER]           [Team2 Portraits]    [Minimap]   │
│      ████                 03:59                 ████              ┌───┐     │
│                                                                   │   │     │
│                         [Relic Icon]                              └───┘     │
│                            ◉ 0                                              │
│   [HEALTH BUFF]                                    [DAMAGE BUFF]            │
│       ◈ 0:43                                           ◈ 0:28              │
│                                                                             │
│                                                                             │
│                                                                             │
│                                                                             │
│  ALPHA                                                                      │
│            [475]═══════════════════════════════    [002⚡]                  │
│                  [1] [F] [Q] [E] [R]                          Build #xxxxx  │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Step 1: Create the Root Widget

### 1.1 Create Widget Blueprint
1. In Content Browser, navigate to `Plugins/GameFeatures/BreakawayCore/Content/UI/`
2. Right-click → **User Interface** → **Widget Blueprint**
3. Parent class: `BwayCoreHUDWidget` (or `CommonActivatableWidget` if base class not ready)
4. Name: `WBP_BW_CoreHUD`

### 1.2 Set Up Root Canvas
1. Open `WBP_BW_CoreHUD`
2. In the Hierarchy panel, you should see a **Canvas Panel** as root
3. If not, drag a **Canvas Panel** from the Palette and make it the root

### 1.3 Canvas Panel Settings
| Property | Value |
|----------|-------|
| Is Variable | ✓ (checked) |
| Visibility | Visible |
| Render Opacity | 1.0 |

---

## Step 2: Top Section - Timer

### 2.1 Create Timer Container
1. Drag a **Canvas Panel** onto the root Canvas Panel
2. Rename to `TimerContainer`

**Anchor Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Top Center (click the anchor grid, select top-center dot) |
| Anchor Min | X: 0.5, Y: 0.0 |
| Anchor Max | X: 0.5, Y: 0.0 |
| Alignment | X: 0.5, Y: 0.0 |
| Position X | 0 |
| Position Y | 20 |
| Size X | 200 |
| Size Y | 80 |

### 2.2 Add Timer Background (Optional)
1. Inside `TimerContainer`, add an **Image** widget
2. Rename to `TimerBackground`

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Fill (stretch to fill parent) |
| Offset Left/Right/Top/Bottom | 0, 0, 0, 0 |
| Brush → Image | Use a dark semi-transparent texture or None |
| Color and Opacity | R: 0, G: 0, B: 0, A: 0.5 |

### 2.3 Add Timer Text
1. Inside `TimerContainer`, add a **Text Block**
2. Rename to `TimerText`
3. Set **Is Variable** = ✓

**Text Settings:**
| Property | Value |
|----------|-------|
| Anchor | Center (anchor to center of parent) |
| Anchor Min | X: 0.5, Y: 0.5 |
| Anchor Max | X: 0.5, Y: 0.5 |
| Alignment | X: 0.5, Y: 0.5 |
| Position X | 0 |
| Position Y | 0 |
| Text | "03:59" (placeholder) |
| Font → Font Family | Your game font (bold, impactful) |
| Font → Size | 48 |
| Color | R: 1.0, G: 1.0, B: 1.0, A: 1.0 (White) |
| Justification | Center |
| Auto Wrap Text | ✗ (unchecked) |

---

## Step 3: Team 1 Portraits (Left Side)

### 3.1 Create Team 1 Container
1. Drag a **Horizontal Box** onto the root Canvas Panel
2. Rename to `Team1PortraitsContainer`

**Anchor Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Top Left |
| Anchor Min | X: 0.0, Y: 0.0 |
| Anchor Max | X: 0.0, Y: 0.0 |
| Alignment | X: 0.0, Y: 0.0 |
| Position X | 30 |
| Position Y | 20 |
| Size X | 280 |
| Size Y | 70 |

### 3.2 Create Portrait Template (Repeat 4x)
For each of the 4 team slots, create:

#### 3.2.1 Single Portrait Container
1. Inside `Team1PortraitsContainer`, add a **Canvas Panel**
2. Rename to `Team1_Portrait_1` (then _2, _3, _4)

**Slot Settings (Horizontal Box Slot):**
| Property | Value |
|----------|-------|
| Padding | Left: 2, Right: 2, Top: 0, Bottom: 0 |
| Size | Fill |
| Horizontal Alignment | Fill |
| Vertical Alignment | Fill |

**Canvas Panel Settings:**
| Property | Value |
|----------|-------|
| Size X | 65 |
| Size Y | 65 |

#### 3.2.2 Portrait Border (Team Color)
1. Inside the portrait Canvas Panel, add an **Image**
2. Rename to `PortraitBorder`

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Fill |
| Offset Left/Right/Top/Bottom | 0, 0, 0, 0 |
| Brush → Image | White square or border texture |
| Color and Opacity | R: 0.0, G: 0.8, B: 0.9, A: 1.0 (Cyan/Teal for Team 1) |

#### 3.2.3 Portrait Image
1. Inside the portrait Canvas Panel, add an **Image**
2. Rename to `HeroPortrait`
3. Set **Is Variable** = ✓

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Fill |
| Offset Left | 3 |
| Offset Right | 3 |
| Offset Top | 3 |
| Offset Bottom | 3 |
| Brush → Image | Hero portrait texture (bound in Blueprint) |
| Brush → Tiling | No Tile |
| Brush → Draw As | Image |

#### 3.2.4 Death Overlay (Optional)
1. Add another **Image** for death state
2. Rename to `DeathOverlay`

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Fill |
| Offset Left/Right/Top/Bottom | 3, 3, 3, 3 |
| Color and Opacity | R: 0.2, G: 0.2, B: 0.2, A: 0.7 |
| Visibility | Hidden (toggle when player dies) |

### 3.3 Repeat for All 4 Portraits
Copy the portrait structure 4 times inside `Team1PortraitsContainer`.

---

## Step 4: Team 2 Portraits (Right Side)

### 4.1 Create Team 2 Container
1. Drag a **Horizontal Box** onto the root Canvas Panel
2. Rename to `Team2PortraitsContainer`

**Anchor Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Top Right |
| Anchor Min | X: 1.0, Y: 0.0 |
| Anchor Max | X: 1.0, Y: 0.0 |
| Alignment | X: 1.0, Y: 0.0 |
| Position X | -30 |
| Position Y | 20 |
| Size X | 280 |
| Size Y | 70 |

### 4.2 Create 4 Portraits (Same as Team 1)
Repeat the exact same structure as Step 3.2, but change:

**Portrait Border Color (Team 2 - Red):**
| Property | Value |
|----------|-------|
| Color and Opacity | R: 0.9, G: 0.2, B: 0.2, A: 1.0 (Red for Team 2) |

---

## Step 5: Relic Indicator (Top Center)

### 5.1 Create Relic Container
1. Drag a **Canvas Panel** onto the root Canvas Panel
2. Rename to `RelicIndicatorContainer`

**Anchor Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Top Center |
| Anchor Min | X: 0.5, Y: 0.0 |
| Anchor Max | X: 0.5, Y: 0.0 |
| Alignment | X: 0.5, Y: 0.0 |
| Position X | 0 |
| Position Y | 100 |
| Size X | 100 |
| Size Y | 100 |

### 5.2 Add Relic Icon
1. Inside `RelicIndicatorContainer`, add an **Image**
2. Rename to `RelicIcon`

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Top Center |
| Anchor Min | X: 0.5, Y: 0.0 |
| Anchor Max | X: 0.5, Y: 0.0 |
| Alignment | X: 0.5, Y: 0.0 |
| Position X | 0 |
| Position Y | 0 |
| Size X | 60 |
| Size Y | 60 |
| Brush → Image | Your relic icon texture (golden circular icon) |
| Color and Opacity | R: 1.0, G: 0.85, B: 0.2, A: 1.0 (Gold) |

### 5.3 Add Possession Counter
1. Inside `RelicIndicatorContainer`, add a **Text Block**
2. Rename to `RelicPossessionText`
3. Set **Is Variable** = ✓

**Text Settings:**
| Property | Value |
|----------|-------|
| Anchor | Bottom Center |
| Anchor Min | X: 0.5, Y: 1.0 |
| Anchor Max | X: 0.5, Y: 1.0 |
| Alignment | X: 0.5, Y: 1.0 |
| Position X | 0 |
| Position Y | -10 |
| Text | "0" |
| Font → Size | 24 |
| Color | White |
| Justification | Center |

---

## Step 6: Buff Pickup Indicators

### 6.1 Create Health Buff Indicator (Left Side)

#### 6.1.1 Container
1. Drag a **Canvas Panel** onto root Canvas Panel
2. Rename to `HealthBuffContainer`

**Anchor Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Center Left |
| Anchor Min | X: 0.0, Y: 0.5 |
| Anchor Max | X: 0.0, Y: 0.5 |
| Alignment | X: 0.0, Y: 0.5 |
| Position X | 100 |
| Position Y | -50 |
| Size X | 120 |
| Size Y | 80 |

#### 6.1.2 Label Text
1. Add a **Text Block** inside `HealthBuffContainer`
2. Rename to `HealthBuffLabel`

**Text Settings:**
| Property | Value |
|----------|-------|
| Anchor | Top Center |
| Anchor Min | X: 0.5, Y: 0.0 |
| Anchor Max | X: 0.5, Y: 0.0 |
| Alignment | X: 0.5, Y: 0.0 |
| Position X | 0 |
| Position Y | 0 |
| Text | "HEALTH BUFF" |
| Font → Size | 14 |
| Color | White |

#### 6.1.3 Buff Icon
1. Add an **Image** inside `HealthBuffContainer`
2. Rename to `HealthBuffIcon`

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Center |
| Anchor Min | X: 0.5, Y: 0.5 |
| Anchor Max | X: 0.5, Y: 0.5 |
| Alignment | X: 0.5, Y: 0.5 |
| Position X | 0 |
| Position Y | 5 |
| Size X | 40 |
| Size Y | 40 |
| Brush → Image | Health buff icon (green crystal/heart) |
| Color | R: 0.2, G: 0.9, B: 0.5, A: 1.0 (Green) |

#### 6.1.4 Respawn Timer
1. Add a **Text Block** inside `HealthBuffContainer`
2. Rename to `HealthBuffTimer`
3. Set **Is Variable** = ✓

**Text Settings:**
| Property | Value |
|----------|-------|
| Anchor | Bottom Center |
| Anchor Min | X: 0.5, Y: 1.0 |
| Anchor Max | X: 0.5, Y: 1.0 |
| Alignment | X: 0.5, Y: 1.0 |
| Position X | 0 |
| Position Y | -5 |
| Text | "0:43" |
| Font → Size | 16 |
| Color | White |

### 6.2 Create Damage Buff Indicator (Right Side)
1. Duplicate `HealthBuffContainer`
2. Rename to `DamageBuffContainer`

**Change Anchor Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Center Right |
| Anchor Min | X: 1.0, Y: 0.5 |
| Anchor Max | X: 1.0, Y: 0.5 |
| Alignment | X: 1.0, Y: 0.5 |
| Position X | -100 |
| Position Y | -50 |

**Change Label Text:** "DAMAGE BUFF"
**Change Icon Color:** R: 0.9, G: 0.3, B: 0.3, A: 1.0 (Red)

---

## Step 7: Minimap (Top Right)

### 7.1 Create Minimap Container
1. Drag a **Canvas Panel** onto root Canvas Panel
2. Rename to `MinimapContainer`

**Anchor Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Top Right |
| Anchor Min | X: 1.0, Y: 0.0 |
| Anchor Max | X: 1.0, Y: 0.0 |
| Alignment | X: 1.0, Y: 0.0 |
| Position X | -20 |
| Position Y | 100 |
| Size X | 200 |
| Size Y | 200 |

### 7.2 Add Minimap Border
1. Add an **Image** inside `MinimapContainer`
2. Rename to `MinimapBorder`

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Fill |
| Offset Left/Right/Top/Bottom | 0, 0, 0, 0 |
| Brush → Image | Border texture or solid |
| Color and Opacity | R: 0.3, G: 0.2, B: 0.15, A: 0.9 (Brown/Bronze) |

### 7.3 Add Minimap Render Target
1. Add an **Image** inside `MinimapContainer`
2. Rename to `MinimapImage`
3. Set **Is Variable** = ✓

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Fill |
| Offset Left/Right/Top/Bottom | 5, 5, 5, 5 |
| Brush → Image | Render Target (set via Blueprint) |

---

## Step 8: Bottom Section - Health Bar

### 8.1 Create Health Container
1. Drag a **Canvas Panel** onto root Canvas Panel
2. Rename to `HealthContainer`

**Anchor Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Bottom Center |
| Anchor Min | X: 0.5, Y: 1.0 |
| Anchor Max | X: 0.5, Y: 1.0 |
| Alignment | X: 0.5, Y: 0.0 |
| Position X | -100 |
| Position Y | -120 |
| Size X | 450 |
| Size Y | 50 |

### 8.2 Add Health Value Text
1. Add a **Text Block** inside `HealthContainer`
2. Rename to `HealthValueText`
3. Set **Is Variable** = ✓

**Text Settings:**
| Property | Value |
|----------|-------|
| Anchor | Left Center |
| Anchor Min | X: 0.0, Y: 0.5 |
| Anchor Max | X: 0.0, Y: 0.5 |
| Alignment | X: 0.0, Y: 0.5 |
| Position X | 0 |
| Position Y | 0 |
| Text | "475" |
| Font → Font Family | Bold/Impact font |
| Font → Size | 36 |
| Color | R: 0.2, G: 0.9, B: 0.95, A: 1.0 (Cyan) |
| Justification | Right |
| Min Desired Width | 80 |

### 8.3 Add Health Bar Background
1. Add an **Image** inside `HealthContainer`
2. Rename to `HealthBarBackground`

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Left Center |
| Anchor Min | X: 0.0, Y: 0.5 |
| Anchor Max | X: 0.0, Y: 0.5 |
| Alignment | X: 0.0, Y: 0.5 |
| Position X | 90 |
| Position Y | 0 |
| Size X | 350 |
| Size Y | 25 |
| Brush → Image | White or dark texture |
| Color and Opacity | R: 0.1, G: 0.1, B: 0.1, A: 0.8 (Dark) |

### 8.4 Add Health Bar Progress
1. Add a **Progress Bar** inside `HealthContainer`
2. Rename to `HealthProgressBar`
3. Set **Is Variable** = ✓

**Progress Bar Settings:**
| Property | Value |
|----------|-------|
| Anchor | Left Center |
| Anchor Min | X: 0.0, Y: 0.5 |
| Anchor Max | X: 0.0, Y: 0.5 |
| Alignment | X: 0.0, Y: 0.5 |
| Position X | 92 |
| Position Y | 0 |
| Size X | 346 |
| Size Y | 21 |
| Percent | 0.95 (placeholder, bind in BP) |
| Fill Color | R: 0.2, G: 0.9, B: 0.95, A: 1.0 (Cyan) |
| Bar Fill Type | Left to Right |
| Background Color | Transparent (A: 0) |

---

## Step 9: Stamina/Resource Indicator

### 9.1 Create Stamina Container
1. Drag a **Canvas Panel** onto root Canvas Panel
2. Rename to `StaminaContainer`

**Anchor Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Bottom Center |
| Anchor Min | X: 0.5, Y: 1.0 |
| Anchor Max | X: 0.5, Y: 1.0 |
| Alignment | X: 0.5, Y: 0.0 |
| Position X | 300 |
| Position Y | -120 |
| Size X | 100 |
| Size Y | 50 |

### 9.2 Add Stamina Value Text
1. Add a **Text Block** inside `StaminaContainer`
2. Rename to `StaminaValueText`
3. Set **Is Variable** = ✓

**Text Settings:**
| Property | Value |
|----------|-------|
| Anchor | Left Center |
| Anchor Min | X: 0.0, Y: 0.5 |
| Anchor Max | X: 0.0, Y: 0.5 |
| Alignment | X: 0.0, Y: 0.5 |
| Position X | 0 |
| Position Y | 0 |
| Text | "002" |
| Font → Size | 28 |
| Color | R: 1.0, G: 0.85, B: 0.2, A: 1.0 (Gold/Yellow) |

### 9.3 Add Lightning Bolt Icon
1. Add an **Image** inside `StaminaContainer`
2. Rename to `StaminaIcon`

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Right Center |
| Anchor Min | X: 1.0, Y: 0.5 |
| Anchor Max | X: 1.0, Y: 0.5 |
| Alignment | X: 1.0, Y: 0.5 |
| Position X | 0 |
| Position Y | 0 |
| Size X | 30 |
| Size Y | 30 |
| Brush → Image | Lightning bolt icon |
| Color | R: 1.0, G: 0.85, B: 0.2, A: 1.0 (Gold) |

---

## Step 10: Ability Bar

### 10.1 Create Ability Bar Container
1. Drag a **Horizontal Box** onto root Canvas Panel
2. Rename to `AbilityBarContainer`

**Anchor Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Bottom Center |
| Anchor Min | X: 0.5, Y: 1.0 |
| Anchor Max | X: 0.5, Y: 1.0 |
| Alignment | X: 0.5, Y: 1.0 |
| Position X | 0 |
| Position Y | -20 |
| Size X | 400 |
| Size Y | 80 |

### 10.2 Create Single Ability Slot (Repeat 5x)
For each ability (1, F, Q, E, R), create:

#### 10.2.1 Ability Slot Container
1. Inside `AbilityBarContainer`, add a **Canvas Panel**
2. Rename to `AbilitySlot_1` (then _F, _Q, _E, _R)

**Horizontal Box Slot Settings:**
| Property | Value |
|----------|-------|
| Padding | Left: 4, Right: 4, Top: 0, Bottom: 0 |
| Size | Auto |

**Canvas Panel Settings:**
| Property | Value |
|----------|-------|
| Size X | 65 |
| Size Y | 80 |

#### 10.2.2 Ability Icon Background
1. Inside the ability slot, add an **Image**
2. Rename to `AbilityBackground`

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Top Center |
| Anchor Min | X: 0.5, Y: 0.0 |
| Anchor Max | X: 0.5, Y: 0.0 |
| Alignment | X: 0.5, Y: 0.0 |
| Position X | 0 |
| Position Y | 0 |
| Size X | 60 |
| Size Y | 60 |
| Brush → Image | Rounded square or ability frame texture |
| Color and Opacity | R: 0.15, G: 0.12, B: 0.1, A: 0.9 (Dark brown) |

#### 10.2.3 Ability Icon
1. Add an **Image** inside the ability slot
2. Rename to `AbilityIcon`
3. Set **Is Variable** = ✓

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Top Center |
| Anchor Min | X: 0.5, Y: 0.0 |
| Anchor Max | X: 0.5, Y: 0.0 |
| Alignment | X: 0.5, Y: 0.0 |
| Position X | 0 |
| Position Y | 5 |
| Size X | 50 |
| Size Y | 50 |
| Brush → Image | Ability icon texture |
| Color | R: 1.0, G: 0.85, B: 0.4, A: 1.0 (Gold/Yellow) |

#### 10.2.4 Cooldown Overlay
1. Add an **Image** for cooldown
2. Rename to `CooldownOverlay`
3. Set **Is Variable** = ✓

**Image Settings:**
| Property | Value |
|----------|-------|
| Anchor | Top Center |
| Position X | 0 |
| Position Y | 5 |
| Size X | 50 |
| Size Y | 50 |
| Color and Opacity | R: 0.0, G: 0.0, B: 0.0, A: 0.7 |
| Visibility | Hidden (show during cooldown) |

#### 10.2.5 Keybind Text
1. Add a **Text Block** inside the ability slot
2. Rename to `KeybindText`

**Text Settings:**
| Property | Value |
|----------|-------|
| Anchor | Bottom Center |
| Anchor Min | X: 0.5, Y: 1.0 |
| Anchor Max | X: 0.5, Y: 1.0 |
| Alignment | X: 0.5, Y: 1.0 |
| Position X | 0 |
| Position Y | 0 |
| Text | "1" (or "F", "Q", "E", "R") |
| Font → Size | 16 |
| Font → Font Family | Bold |
| Color | White |

### 10.3 Repeat for All 5 Abilities
Create 5 slots with keybinds: **1**, **F**, **Q**, **E**, **R**

---

## Step 11: Alpha Watermark (Bottom Left)

### 11.1 Create Alpha Text
1. Drag a **Text Block** onto root Canvas Panel
2. Rename to `AlphaWatermark`

**Text Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Bottom Left |
| Anchor Min | X: 0.0, Y: 1.0 |
| Anchor Max | X: 0.0, Y: 1.0 |
| Alignment | X: 0.0, Y: 1.0 |
| Position X | 20 |
| Position Y | -80 |
| Text | "ALPHA" |
| Font → Size | 24 |
| Font → Font Family | Bold/Italic |
| Color | R: 1.0, G: 1.0, B: 1.0, A: 0.3 (Semi-transparent white) |

---

## Step 12: Build Number (Bottom Right)

### 12.1 Create Build Text
1. Drag a **Text Block** onto root Canvas Panel
2. Rename to `BuildNumberText`

**Text Settings:**
| Property | Value |
|----------|-------|
| Anchor Preset | Bottom Right |
| Anchor Min | X: 1.0, Y: 1.0 |
| Anchor Max | X: 1.0, Y: 1.0 |
| Alignment | X: 1.0, Y: 1.0 |
| Position X | -20 |
| Position Y | -20 |
| Text | "Breakaway Build # 0.0.0.0: 481022" |
| Font → Size | 12 |
| Color | R: 1.0, G: 0.8, B: 0.4, A: 0.6 (Gold, semi-transparent) |

---

## Final Hierarchy Structure

Your widget hierarchy should look like this:

```
[Canvas Panel] (Root)
├── TimerContainer
│   ├── TimerBackground
│   └── TimerText
├── Team1PortraitsContainer (Horizontal Box)
│   ├── Team1_Portrait_1
│   │   ├── PortraitBorder
│   │   ├── HeroPortrait
│   │   └── DeathOverlay
│   ├── Team1_Portrait_2
│   ├── Team1_Portrait_3
│   └── Team1_Portrait_4
├── Team2PortraitsContainer (Horizontal Box)
│   ├── Team2_Portrait_1
│   ├── Team2_Portrait_2
│   ├── Team2_Portrait_3
│   └── Team2_Portrait_4
├── RelicIndicatorContainer
│   ├── RelicIcon
│   └── RelicPossessionText
├── HealthBuffContainer
│   ├── HealthBuffLabel
│   ├── HealthBuffIcon
│   └── HealthBuffTimer
├── DamageBuffContainer
│   ├── DamageBuffLabel
│   ├── DamageBuffIcon
│   └── DamageBuffTimer
├── MinimapContainer
│   ├── MinimapBorder
│   └── MinimapImage
├── HealthContainer
│   ├── HealthValueText
│   ├── HealthBarBackground
│   └── HealthProgressBar
├── StaminaContainer
│   ├── StaminaValueText
│   └── StaminaIcon
├── AbilityBarContainer (Horizontal Box)
│   ├── AbilitySlot_1
│   │   ├── AbilityBackground
│   │   ├── AbilityIcon
│   │   ├── CooldownOverlay
│   │   └── KeybindText
│   ├── AbilitySlot_F
│   ├── AbilitySlot_Q
│   ├── AbilitySlot_E
│   └── AbilitySlot_R
├── AlphaWatermark
└── BuildNumberText
```

---

## Color Reference Table

| Element | Hex | RGB |
|---------|-----|-----|
| Team 1 (Cyan/Teal) | #00CDE6 | R: 0.0, G: 0.8, B: 0.9 |
| Team 2 (Red) | #E63333 | R: 0.9, G: 0.2, B: 0.2 |
| Health Bar (Cyan) | #33E6F2 | R: 0.2, G: 0.9, B: 0.95 |
| Gold/Yellow | #FFD933 | R: 1.0, G: 0.85, B: 0.2 |
| Ability Icon Gold | #FFD966 | R: 1.0, G: 0.85, B: 0.4 |
| Dark Background | #1A1A1A | R: 0.1, G: 0.1, B: 0.1 |
| Brown/Bronze | #4D3326 | R: 0.3, G: 0.2, B: 0.15 |
| Health Buff (Green) | #33E680 | R: 0.2, G: 0.9, B: 0.5 |
| Damage Buff (Red) | #E64D4D | R: 0.9, G: 0.3, B: 0.3 |

---

## Blueprint Bindings Checklist

After completing the layout, bind these variables in Blueprint Event Graph:

- [ ] `TimerText` → Bind to `OnRoundTimeUpdated`
- [ ] `HealthValueText` → Bind to `OnHealthChanged`
- [ ] `HealthProgressBar` → Bind Percent to health percentage
- [ ] `StaminaValueText` → Bind to stamina/resource system
- [ ] `HeroPortrait` (x8) → Bind to team player data
- [ ] `AbilityIcon` (x5) → Bind to ability cooldowns
- [ ] `CooldownOverlay` (x5) → Show/hide based on cooldown state
- [ ] `HealthBuffTimer` → Bind to buff respawn timer
- [ ] `DamageBuffTimer` → Bind to buff respawn timer
- [ ] `RelicPossessionText` → Bind to relic system

---

## Tips for Fine-Tuning

1. **Preview at Different Resolutions**: Use the Designer's resolution dropdown to test 1920x1080, 2560x1440, etc.

2. **Adjust Position Values**: The exact Position X/Y values may need tweaking based on your art assets and preferences.

3. **Use DPI Scaling**: Set `DPI Scaling Rule` on the root widget to `Shortest Side` for consistent scaling.

4. **Test with Controller**: Ensure any interactive elements (if added) work with gamepad navigation.

5. **Animation Polish**: Add subtle animations for health changes, ability cooldowns, and score updates using Widget Animations.

