# Character Selection UI - Asset Creation Guide

This guide provides specifications for creating the UI texture and material assets.

---

## 9-Slice Texture Specifications

All 9-slice textures should be created with appropriate margins for scaling without distortion.

### T_UI_Panel_Dark

**Purpose:** Dark translucent panel backgrounds

**Specifications:**
- Size: 512x512 pixels
- Format: PNG with alpha
- 9-Slice Margins: 16px all sides

**Design:**
```
┌────────────────────────────────────────┐
│  Corner (16px)   Edge   Corner (16px)  │
│ ┌──────────────────────────────────┐   │
│ │                                  │   │
│ │       Fill Area                  │   │
│ │       (Black 70% opacity)        │   │
│ │                                  │   │
│ └──────────────────────────────────┘   │
│  Edge                         Edge     │
└────────────────────────────────────────┘

Colors:
- Border: #1A1A1A (subtle dark outline)
- Fill: #000000 at 70% opacity
- Corner radius: 8px
```

---

### T_UI_Frame_Gold

**Purpose:** Selected hero slot border (animated with material)

**Specifications:**
- Size: 256x256 pixels
- Format: PNG with alpha
- 9-Slice Margins: 8px all sides

**Design:**
```
Border only, no fill (transparent center)
- Border width: 4px
- Border color: Gold gradient (#FFD700 to #FFA500)
- Outer glow: 2px soft gold
- Corner radius: 4px
```

---

### T_UI_Frame_Default

**Purpose:** Default hero slot frame

**Specifications:**
- Size: 256x256 pixels
- Format: PNG with alpha
- 9-Slice Margins: 8px all sides

**Design:**
```
Border only, no fill (transparent center)
- Border width: 2px
- Border color: #444444 (dark gray)
- No glow
- Corner radius: 4px
```

---

### T_UI_Button_Orange

**Purpose:** Primary button background (TRY button)

**Specifications:**
- Size: 256x64 pixels
- Format: PNG with alpha
- 9-Slice Margins: 12px horizontal, 8px vertical

**Design:**
```
- Fill: Linear gradient #FF8C00 (top) to #FF6600 (bottom)
- Border: 1px #FFB347 (highlight on top edge)
- Corner radius: 6px
- Subtle bevel effect (3D appearance)
```

---

### T_UI_Button_Red

**Purpose:** Currency display background

**Specifications:**
- Size: 256x64 pixels
- Format: PNG with alpha
- 9-Slice Margins: 12px horizontal, 8px vertical

**Design:**
```
- Fill: Linear gradient #CC0000 (top) to #990000 (bottom)
- Border: 1px #FF3333 (highlight on top edge)
- Corner radius: 6px
- Subtle bevel effect
```

---

### T_UI_LevelBar_Frame

**Purpose:** Level/XP progress bar frame

**Specifications:**
- Size: 512x32 pixels
- Format: PNG with alpha
- 9-Slice Margins: 4px all sides

**Design:**
```
- Outer frame: 2px metallic silver (#C0C0C0)
- Inner bevel: 1px shadow (#333333)
- Fill area: Transparent (for progress bar)
- Corner radius: 4px
```

---

### T_UI_AbilityFrame

**Purpose:** Ability icon frame in ability bar

**Specifications:**
- Size: 128x128 pixels
- Format: PNG with alpha
- 9-Slice Margins: 6px all sides

**Design:**
```
- Border: 2px dark (#222222)
- Inner highlight: 1px (#555555) at top
- Fill: Dark gradient #1A1A1A to #0D0D0D
- Corner radius: 4px
- Slight inner shadow effect
```

---

### T_UI_CoinIcon

**Purpose:** Currency coin icon

**Specifications:**
- Size: 64x64 pixels
- Format: PNG with alpha
- No 9-slice needed

**Design:**
```
- Circular gold coin
- Colors: #FFD700 (gold), #FFA500 (shadow)
- Center emblem or design
- Subtle shine/highlight
```

---

## Material Creation

### M_UI_GoldShimmer

**Purpose:** Animated gold border effect for selected hero

**Material Type:** Surface, Unlit, Transparent Blend

**HLSL Concept (for Material nodes):**

```hlsl
// Parameters
float Time;                    // Time node
float ShimmerSpeed = 2.0;      // Shimmer animation speed
float ShimmerWidth = 0.3;      // Width of shimmer band
float2 UV;                     // Texture coordinates

// Base gold color
float3 BaseGold = float3(1.0, 0.84, 0.0);      // #FFD700
float3 BrightGold = float3(1.0, 0.95, 0.6);    // Highlight

// Calculate shimmer position (diagonal sweep)
float shimmerPos = frac((UV.x + UV.y) * 0.5 - Time * ShimmerSpeed);

// Soft shimmer band
float shimmer = smoothstep(0.0, ShimmerWidth, shimmerPos) 
              * smoothstep(ShimmerWidth * 2.0, ShimmerWidth, shimmerPos);

// Mix base and bright gold
float3 FinalColor = lerp(BaseGold, BrightGold, shimmer * 0.5);

// Add subtle pulse
float pulse = sin(Time * 3.0) * 0.1 + 0.9;
FinalColor *= pulse;

// Output
EmissiveColor = FinalColor * 2.0;  // Boost for glow
Opacity = TextureSample.a;          // From base texture
```

**Material Node Setup:**
1. Create Material, set to Unlit, Blend Mode: Translucent
2. Add Time node → Multiply (speed) → Add to UV
3. Create shimmer band using Smoothstep nodes
4. Lerp between gold colors based on shimmer
5. Add sine wave for subtle pulse
6. Multiply by texture for border shape
7. Output to Emissive Color

---

### M_UI_HolographicText

**Purpose:** Glowing text effect for hero names (optional)

**Material Type:** Surface, Unlit, Additive Blend

**HLSL Concept:**

```hlsl
// Parameters
float Time;
float3 TextColor = float3(1.0, 0.84, 0.0);  // Gold
float GlowIntensity = 1.5;
float FlickerSpeed = 8.0;
float FlickerAmount = 0.1;

// Subtle flicker effect
float flicker = 1.0 - (sin(Time * FlickerSpeed) * FlickerAmount);

// Edge glow (requires font SDF or distance field)
float glowAmount = smoothstep(0.4, 0.5, TextAlpha);
float edgeGlow = smoothstep(0.3, 0.4, TextAlpha) - glowAmount;

// Final output
float3 FinalColor = TextColor * glowAmount * flicker;
FinalColor += TextColor * 0.5 * edgeGlow;  // Add edge glow

EmissiveColor = FinalColor * GlowIntensity;
Opacity = max(glowAmount, edgeGlow * 0.5);
```

---

### M_UI_AbilityCooldown (Optional - for future use)

**Purpose:** Radial cooldown sweep effect

**HLSL Concept:**

```hlsl
// Parameters
float CooldownProgress;  // 0.0 (full) to 1.0 (ready)
float2 UV;

// Convert UV to polar coordinates
float2 center = float2(0.5, 0.5);
float2 dir = UV - center;
float angle = atan2(dir.y, dir.x);

// Normalize angle to 0-1 range (starting from top)
float normalizedAngle = frac((angle + 3.14159) / 6.28318 + 0.25);

// Create cooldown mask
float cooldownMask = step(normalizedAngle, CooldownProgress);

// Darken the on-cooldown portion
float3 BaseColor = TextureSample.rgb;
float3 CooldownColor = BaseColor * 0.3;

float3 FinalColor = lerp(CooldownColor, BaseColor, cooldownMask);
```

---

### M_UI_BackgroundBlur

**Purpose:** Backdrop blur for panels

**Setup:**
1. Use the built-in `BackdropBlur` widget instead of a custom material
2. Or create a Post-Process material with Gaussian blur
3. Settings: Blur Strength ~5-10, apply to Widget only

---

## Font Import Guide

### Required Fonts

1. **Bebas Neue** - Hero name titles
   - Download from: Google Fonts or dafont.com
   - Import: Bold weight
   - Create Font Face asset in UE

2. **Montserrat** - Class labels
   - Download from: Google Fonts
   - Import: SemiBold weight

3. **Rajdhani** - Level/stats text
   - Download from: Google Fonts
   - Import: Medium weight

4. **Noto Sans** - Body text, descriptions
   - Download from: Google Fonts
   - Import: Regular weight

5. **Industry** - Button text (or substitute: Bebas Neue, Oswald)
   - Alternative: Use Bebas Neue or similar condensed font

### Font Import Steps in Unreal:

1. Copy .ttf or .otf files to `/Game/UI/Fonts/`
2. Right-click in Content Browser → Import
3. Create Font Face asset for each font file
4. Create Composite Font asset to combine weights
5. Create Text Style assets for consistent styling

---

## Color Palette Reference

| Name | Hex | Usage |
|------|-----|-------|
| Gold Primary | #FFD700 | Selected borders, hero names |
| Gold Secondary | #FFA500 | Gold shadows, accents |
| Orange Primary | #FF8C00 | Primary buttons |
| Red Primary | #CC0000 | Currency bg, negative actions |
| Cyan Accent | #00D4FF | Level bar fill, highlights |
| Dark Panel | #0D0D0D | Panel backgrounds |
| Dark Border | #1A1A1A | Panel borders |
| Text White | #FFFFFF | Primary text |
| Text Gray | #AAAAAA | Secondary text (70% opacity) |
| Disabled Gray | #666666 | Disabled elements |

---

## Asset Organization

```
/Game/UI/
├── CharacterSelect/
│   ├── WBP_CharacterSelectScreen.uasset
│   ├── WBP_HeroSlot.uasset
│   ├── WBP_AbilityIcon.uasset
│   ├── WBP_LevelProgressBar.uasset
│   ├── WBP_CurrencyDisplay.uasset
│   ├── WBP_StyledButton.uasset
│   └── WBP_AbilityPreviewPanel.uasset
├── Textures/
│   ├── T_UI_Panel_Dark.uasset
│   ├── T_UI_Frame_Gold.uasset
│   ├── T_UI_Frame_Default.uasset
│   ├── T_UI_Button_Orange.uasset
│   ├── T_UI_Button_Red.uasset
│   ├── T_UI_LevelBar_Frame.uasset
│   ├── T_UI_AbilityFrame.uasset
│   └── T_UI_CoinIcon.uasset
├── Materials/
│   ├── M_UI_GoldShimmer.uasset
│   ├── M_UI_HolographicText.uasset
│   └── M_UI_BackgroundBlur.uasset
└── Fonts/
    ├── FF_BebasNeue_Bold.uasset
    ├── FF_Montserrat_SemiBold.uasset
    ├── FF_Rajdhani_Medium.uasset
    ├── FF_NotoSans_Regular.uasset
    └── Font_UI_Composite.uasset
```

