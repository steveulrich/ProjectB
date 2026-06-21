# Match UI — Score & Relic Status (Steps 12–13)

Editor setup for Lyra HUD **slot widgets** injected via **`UGameFeatureAction_AddWidgets`**. Logic lives in C++ (extracted from `UBwayCoreHUDWidget`); Blueprint subclasses supply UMG layout and styling only.

Full step checklists: [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) Section 2.

**Prerequisites:** Section 1 Steps 0–11 complete (3/3 each). PIE uses **Listen Server** from Step 12 onward. **Recompile BreakawayCoreRuntime** after pulling C++ widget classes.

---

## C++ types (reference)

| Type | Role |
|------|------|
| `UBwayMatchHUDWidgetBase` | Shared match data access + scoring/timer delegate binding |
| `UBwayCaptureTheRelicScoreWidget` | Step 12 slot widget — scores (+ optional timer); parent for `W_BW_CaptureTheRelic_ScoreWidget` |
| `UBwayRelicStatusWidget` | Step 13 slot widget — possession poll + carrier name; parent for `W_BW_RelicStatusWidget` |
| `UBwayHealthHUDWidget` | Step 14 health slot — `ULyraHealthComponent` bind |
| `UBwayTeamPortraitsHUDWidget` | Step 14 portrait slot — display-slot team rows |
| `UBwayCoreHUDWidget` | **Deprecated** — monolithic shell; not used in Steps 12–14 |
| `UBwayScoringComponent` | Replicated team scores; `OnTeamScoreChanged` (also fired from `OnRep_Scores` on clients) |
| `UBwayRelicManagerComponent` | `GetRelicPossessingTeam()` (-1 = neutral) |
| `UGameFeatureAction_AddWidgets` | Lyra GF action — registers widgets into HUD layout extension slots |
| `ABwayGameState` | `GetRoundTimeRemaining()`, `GetPlayerTeam()` |

**ShooterCore slot tags** (from `Plugins/GameFeatures/ShooterCore/Config/Tags/ShooterCoreTags.ini`):

| Tag | Widget |
|-----|--------|
| `HUD.Slot.TeamScore` | `W_BW_CaptureTheRelic_ScoreWidget` |
| `HUD.Slot.ModeStatus` | `W_BW_RelicStatusWidget` |
| `HUD.Slot.Health` | `W_BW_HealthWidget` (Step 14) |
| `HUD.Slot.TeamPortraits` | `W_BW_TeamPortraitsWidget` (Step 14) |

**Reference widgets to inspect (do not modify):**

| Asset | Path |
|-------|------|
| `W_ScoreWidget_Elimination` | `/ShooterCore/Elimination/UI/W_ScoreWidget_Elimination` |
| `W_CPScoreWidget` | `/ShooterCore/ControlPoint/UI/W_CPScoreWidget` |
| `LAS_ShooterGame_StandardHUD` | `/ShooterCore/Experiences/LAS_ShooterGame_StandardHUD` |

---

## 1. Action set — `EAS_BW_CaptureTheRelic`

**Path:** `/BreakawayCore/Experiences/EAS_BW_CaptureTheRelic`

**Asset type:** `LyraExperienceActionSet` (Capture-the-Relic mode features + HUD widget injection)

> **Note:** `B_BW_CaptureTheRelic_Scoring` is a **GameStateComponent Blueprint**, not an action set. HUD **Add Widgets** rows live on **`EAS_BW_CaptureTheRelic`**.

### Step 12 — score widget only

1. Content Browser → `/BreakawayCore/Experiences/`
2. Open **`EAS_BW_CaptureTheRelic`** (`LyraExperienceActionSet`).
3. **Actions** → **Add** → **`Add Widgets`**.
4. Under **Widgets**, add one row:

| Property | Value |
|----------|--------|
| **Widget Class** | `W_BW_CaptureTheRelic_ScoreWidget` (Blueprint child of `BwayCaptureTheRelicScoreWidget` — section 2) |
| **Slot ID** | `HUD.Slot.TeamScore` |

5. Leave **Layout** empty (ShooterCore layout comes from `LAS_ShooterGame_StandardHUD` on the experience).

### Step 13 — add relic status row

On the same **Add Widgets** action, add a second **Widgets** row:

| Property | Value |
|----------|--------|
| **Widget Class** | `W_BW_RelicStatusWidget` |
| **Slot ID** | `HUD.Slot.ModeStatus` |

Save the action set.

### Step 14 — add health + portrait rows

On the same **Add Widgets** action, add third and fourth **Widgets** rows:

| Property | Value |
|----------|--------|
| **Widget Class** | `W_BW_HealthWidget` |
| **Slot ID** | `HUD.Slot.Health` |

| Property | Value |
|----------|--------|
| **Widget Class** | `W_BW_TeamPortraitsWidget` |
| **Slot ID** | `HUD.Slot.TeamPortraits` |

Requires **`WBP_BW_MatchHUDLayout`** extension points — see [CoreHUD_Layout_Setup.md](./CoreHUD_Layout_Setup.md) or run `Scripts/pivot-hud-to-slots.mjs`.

---

## 2. Score widget — `W_BW_CaptureTheRelic_ScoreWidget`

**Path:** `/BreakawayCore/UI/Match/W_BW_CaptureTheRelic_ScoreWidget`

**Parent class:** **`BwayCaptureTheRelicScoreWidget`** (`UBwayCaptureTheRelicScoreWidget`)

### What C++ owns (no Event Graph required)

| Behavior | C++ |
|----------|-----|
| Bind `OnTeamScoreChanged` + initial poll | `NativeConstruct` |
| Update `Text_Team1Score` / `Text_Team2Score` when named with **BindWidget** | `RefreshScoreDisplay` |
| Optional round timer on `Text_Timer` | 1s world timer (`bShowRoundTimer`, `TimerPollInterval`) |
| BP hook for pips / custom visuals | `OnScoreChanged(T1, T2)`, `OnRoundTimeUpdated` |

**BindWidget names** (must match exactly if auto-updated):

- `Text_RoundLabel` (optional)
- `Text_Team1Score`
- `Text_Team2Score`
- `Text_Timer` (optional)

### Widget hierarchy (reference layout)

```
[Root - Size Box] (600 x 120)
└── [Vertical Box]
    ├── [Text Block] Text_RoundLabel     ← optional; "ROUND N" from C++
    ├── [Text Block] Text_Timer          ← optional; bShowRoundTimer=true (default)
    └── [Horizontal Box] ScoreRow
        ├── [Vertical Box] Team1Column
        │   ├── [Text Block] Text_Team1Score
        │   └── [Horizontal Box] Team1Pips   ← wire in OnScoreChanged (optional polish)
        ├── [Image] Image_CenterRelicIcon
        └── [Vertical Box] Team2Column
            ├── [Text Block] Text_Team2Score
            └── [Horizontal Box] Team2Pips
```

**Minimum Step 12 pass:** `Text_Team1Score` + `Text_Team2Score` at 0–0, updating on round win. No Event Graph needed if BindWidget names match.

### Designer defaults (Class Defaults on BP)

| Property | Step 12–14 |
|----------|------------|
| `bShowRoundTimer` | `true` — score slot owns timer (slot composition pivot) |
| `TimerPollInterval` | `1.0` |

### Optional Blueprint polish

Implement **`OnScoreChanged`** only if you add round-win pips or tint images — read `Team1Score` / `Team2Score` inputs and toggle pip visibility vs `PointsToWin`.

### Styling (Text_Team1Score / Text_Team2Score)

| Property | Value |
|----------|--------|
| Font Size | 48–56 |
| Justification | Center |
| Color | Team 1: blue `(0.2, 0.5, 1.0)`; Team 2: orange `(1.0, 0.4, 0.1)` |

**Display perspective:** C++ remaps authoritative game teams so **display Team 1 (left)** is always the **local player's team** (friendly/enemy columns). Spectators (`LocalTeam == -1`) see authoritative order (game team 0 left, team 1 right). No UMG changes required — `Text_Team1Score` / `Text_Team2Score` are display columns, not game team indices.

---

## 3. Relic status widget — `W_BW_RelicStatusWidget`

**Path:** `/BreakawayCore/UI/Match/W_BW_RelicStatusWidget`

**Parent class:** **`BwayRelicStatusWidget`** (`UBwayRelicStatusWidget`)

### What C++ owns (no Event Graph required)

| Behavior | C++ |
|----------|-----|
| Poll replicated possession every 0.25s | World timer in `NativeConstruct` (`RelicPollInterval`) |
| Update `Text_Status` / `Text_Carrier` when named with **BindWidget** | `RefreshRelicStatus` |
| Carrier name from `ARelicActor::CurrentCarrier` | `GetRelicCarrierNameFromWorld` |
| BP hook for team color / icons | `OnRelicStatusUpdated` |

**BindWidget names:**

- `Text_Status`
- `Text_Carrier`

Default status strings are editable on the C++ class (`NeutralStatusText`, `Team1StatusText`, `Team2StatusText`) or override in Class Defaults.

### Widget hierarchy

```
[Root - Overlay]
├── [Border] Background (optional)
└── [Vertical Box] (center)
    ├── [Text Block] Text_Status
    └── [Text Block] Text_Carrier    ← collapsed by C++ when neutral
```

### Optional Blueprint polish

Implement **`OnRelicStatusUpdated`** to set status text color by `PossessingTeamIndex` (blue / orange / white).

---

## 4. Experience wiring — `B_BW_Experience_Dev`

Open `/BreakawayCore/Experiences/B_BW_Experience_Dev` (parent **`LyraExperienceDefinition`** — do not reparent).

### Step 12

| Add to ActionSets | Purpose |
|-------------------|---------|
| `LAS_ShooterGame_StandardHUD` or `LAS_BW_MatchHUD_Dev` | Lyra HUD layout + extension slots |
| `EAS_BW_CaptureTheRelic` | Injects slot widgets (`HUD.Slot.*`) |

| Remove / omit | |
|---------------|--|
| Relic status row on EAS | Step 13 |
| Health / portrait rows on EAS | Step 14 |
| `B_BW_CoreHUD_Layout` / monolithic CoreHUD | Deprecated |

### Step 13

Add second widget row to **`EAS_BW_CaptureTheRelic`** (section 1). No other experience changes.

### Step 14

Run `Scripts/pivot-hud-to-slots.mjs` or manually add health/portrait rows + swap to `LAS_BW_MatchHUD_Dev`. See [CoreHUD_Layout_Setup.md](./CoreHUD_Layout_Setup.md).

---

## 5. PIE URLs

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&RoundDuration=90
```

**PIE:** 1 player, **Listen Server**.

---

## Troubleshooting

| Symptom | Check |
|---------|--------|
| No HUD at all | `LAS_ShooterGame_StandardHUD` on experience; cold-start PIE |
| Score widget missing | `EAS_BW_CaptureTheRelic` on experience; Slot ID = `HUD.Slot.TeamScore`; BP parent = `BwayCaptureTheRelicScoreWidget` |
| EAS widget list empty | Run `Scripts/pivot-hud-to-slots.mjs` to restore rows |
| Scores stuck at 0 | Recompile C++; `UBwayScoringComponent` on `BP_BW_GameState`; BindWidget names match |
| Text blocks never update | Widget names must be exact: `Text_Team1Score`, `Text_Team2Score` |
| ShooterCore elimination score still showing | Only one widget per slot |
| Relic status always NEUTRAL | BP parent = `BwayRelicStatusWidget`; Listen Server PIE; `RelicManager` on GameState |
| Carrier name blank | `CurrentCarrier` replicated; wait one poll interval after pickup |

---

## Related

- [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) — Steps 12–14 checklists
- [CoreHUD_Layout_Setup.md](./CoreHUD_Layout_Setup.md) — Step 14 slot composition
- [UI_System.md](./UI_System.md) — C++ widget bases
- [MatchFlow_and_Phases.md](./MatchFlow_and_Phases.md) — PostRound / phase timing
