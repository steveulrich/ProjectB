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
| `UBwayPostRoundSummaryWidget` | Step 15 PostRound team-aggregate interstitial |
| `UBwayMatchStatsLibrary` | Per-player / team stat helpers |
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

## 6. PostRound summary — `WBP_BW_PostRoundSummary` (Step 15)

**Path:** `/BreakawayCore/UI/Match/WBP_BW_PostRoundSummary`

**Parent class:** **`BwayPostRoundSummaryWidget`**

**Setup script:** `node Scripts/setup-post-round-summary.mjs` (requires Unreal Editor open with MCP bridge connected)

### What C++ owns

| Behavior | C++ |
|----------|-----|
| Show/dismiss on PostRound phase | `ABwayPlayerController` binds `OnPostRoundSummaryStarted` / `OnMatchPhaseChanged` |
| Team round stat values | `ApplySummaryData` → `RefreshBoundWidgets` |
| Header text | **ROUND WIN** / **ROUND LOSS** from local team vs round winner |
| BP layout hook | `OnSummaryReady(SummaryData)` — round pips, styling |
| **Display perspective** | **Team0 / left = local (blue), Team1 / right = enemy (red)** — C++ remaps in `ApplySummaryData` via `RemapPostRoundSummaryForDisplay` (same pattern as CTR score widget `OnScoreChanged`) |

**BindWidget names** (team value columns — labels are static in UMG):

- `Text_Header`
- `Text_Team0_KDA`, `Text_Team1_KDA`
- `Text_Team0_Gold`, `Text_Team1_Gold`
- `Text_Team0_Damage`, `Text_Team1_Damage`
- `Text_Team0_Healing`, `Text_Team1_Healing`
- `Text_Team0_Buildables`, `Text_Team1_Buildables`

### Widget hierarchy (reference — END OF MATCH interstitial layout)

```
[CanvasPanel] RootCanvas (full viewport)
├── [Image] DimOverlay (anchors 0,0,1,1 — semi-transparent black)
└── [SizeBox] CenterPanelSize (anchors center 0.5,0.5 — 920×540)
    └── [Border] PanelBorder
        └── [VerticalBox] MainVBox
            ├── [TextBlock] Text_Header
            ├── [HorizontalBox] RoundPipsRow  ← wire in OnSummaryReady
            ├── [SizeBox] SpacerAfterPips
            └── [VerticalBox] StatsVBox
                ├── Row_KDA: Text_Team0_KDA | Text_Label_KDA | Text_Team1_KDA
                ├── Row_Gold: ...
                ├── Row_Damage: ...
                ├── Row_Healing: ...
                └── Row_Buildables: ...
```

**Anchors:** center panel uses canvas anchors `0.5,0.5,0.5,0.5` + alignment `0.5,0.5` + fixed size (not position offsets). Dim overlay uses full-screen anchors `0,0,1,1`.

### GameState wiring

`BP_BW_GameState` → **Post Round Summary Widget Class** = `WBP_BW_PostRoundSummary` (script sets CDO automatically).

### PIE URL (PostRound loop)

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&PostRoundDuration=3&WarmupDuration=5
```

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
| PostRound overlay missing | `PostRoundSummaryWidgetClass` on `BP_BW_GameState`; re-run setup script; verify PostRound in log |
| PostRound stats all zero | Expected for Damage/Healing/Buildables stubs; score a goal for Gold delta |
| Widget off-screen | Re-run script — center panel must use canvas anchors, not default top-left offsets |
| Final round skips PostRound | Recompile — match-winning round must enter PostRound before PostMatch (`bPendingMatchEndAfterPostRound`) |
| PostMatch shows immediately | Expected only after PostRound timer; check log for `PostRound complete — match ended, advancing to PostMatch` |

---

## 7. PostMatch summary — Step 16

**Flow:** final round → `WBP_BW_PostRoundSummary` (PostRound) → `WBP_BW_MatchSummaryInterstitial` (timed) → `WBP_BW_MatchBreakdown` (buttons).

| Widget | Parent C++ | Path |
|--------|------------|------|
| `WBP_BW_MatchSummaryInterstitial` | `BwayPostMatchInterstitialWidget` | `/BreakawayCore/UI/Match/` |
| `WBP_BW_MatchBreakdown` | `BwayMatchBreakdownWidget` | `/BreakawayCore/UI/Match/` |
| `WBP_BW_ResultsWidget` | `BwayResultsScreenWidget` (orchestrator) | existing |

**Config:** `DA_BW_MatchFlow_Dev` → `PostMatchSummaryDuration` (default 4s); URL `PostMatchSummaryDuration=5`.

**Interstitial BindWidget names** (same stat columns as PostRound): `Text_Header`, `Text_Team0_*`, `Text_Team1_*` — header **VICTORY** / **DEFEAT**; team columns remapped local-left.

**Breakdown:** implement `OnBreakdownReady` — iterate `GetPlayerColumns()`; highlight `bIsMVP`; wire buttons to `RequestReturnToLobby` / `RequestPlayAgain`.

**Sudden death timer:** on score widget BP, implement `OnSuddenDeathTimerStateChanged` — tint `Text_Timer` red when true.

**PIE URL (full match end):**

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&PostRoundDuration=3&PostMatchSummaryDuration=4&WarmupDuration=5
```

### Editor setup script

With Unreal Editor + MCP bridge open:

```bash
node Scripts/setup-step16-postmatch-ui.mjs
```

Creates/refreshes all three widget trees, wires `BP_BW_GameState.ResultsScreenWidgetClass`, and sets `InterstitialWidgetClass` / `BreakdownWidgetClass` on the results orchestrator CDO. Log: `AI_Planning/setup_step16_postmatch_ui_log.json`.

| Widget | Layout |
|--------|--------|
| **Interstitial** | Same centered 920×540 panel as PostRound — `END OF MATCH` subtitle, `VICTORY`/`DEFEAT` header, `RoundPipsRow`, 5 team stat rows |
| **Breakdown** | Centered 1200×720 panel — stat labels column (220px) + horizontal `PlayerColumnsHost` scroll area + footer buttons |
| **Results orchestrator** | Reparent + CDO wiring only (no tree rebuild) — C++ spawns interstitial/breakdown at runtime; remove legacy on-screen results UI from old `WBP_BW_ResultsWidget` graph if still visible |

**Blueprint follow-up after script:**

- Interstitial: `OnInterstitialReady` → spawn/configure pips (reuse `WBP_BW_RoundPip` pattern)
- Breakdown: `OnBreakdownReady` → spawn player columns into `PlayerColumnsHost`; wire `Btn_ReturnToLobby` / `Btn_PlayAgain` → `RequestReturnToLobby` / `RequestPlayAgain`
- Score widget: `OnSuddenDeathTimerStateChanged` → red timer tint

---

## Related

- [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) — Steps 12–14 checklists
- [CoreHUD_Layout_Setup.md](./CoreHUD_Layout_Setup.md) — Step 14 slot composition
- [UI_System.md](./UI_System.md) — C++ widget bases
- [MatchFlow_and_Phases.md](./MatchFlow_and_Phases.md) — PostRound / phase timing
