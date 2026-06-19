# Core HUD Layout — Editor Setup (Step 14)

Create **`WBP_BW_CoreHUD`** — the full in-match shell parented to **`UBwayCoreHUDWidget`**. Replaces or composes standalone Step 12–13 slot widgets. Step checklist: [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) Step 14.

**Also see:** [HUD_LAYOUT_DETAILED_GUIDE.md](../../../AI_Planning/HUD_LAYOUT_DETAILED_GUIDE.md) for pixel-level UMG anchor tables.

---

## C++ base (reference)

`UBwayCoreHUDWidget` ([`BwayCoreHUDWidget.h`](../Source/BreakawayCoreRuntime/Public/UI/BwayCoreHUDWidget.h)) provides:

| Getter / event | Use in BP |
|----------------|-----------|
| `GetTeam1Score()` / `GetTeam2Score()` | Initial poll |
| `GetRoundTimeFormatted()` | Timer text |
| `GetCurrentRoundNumber()` | Round label |
| `GetHealthPercent()` / `GetCurrentHealth()` / `GetMaxHealth()` | Health bar |
| `GetRelicPossessingTeam()` / `IsRelicCarried()` | Relic slot — `OnRelicPossessionChanged` fires from C++ poll (0.25s) |
| `GetLocalPlayerTeam()` | Highlight friendly team |
| `OnScoreChanged(T1, T2)` | Update score texts |
| `OnRoundTimeUpdated(Seconds, FormattedTime)` | Timer + sudden-death color at ≤60s |
| `OnRoundStateChanged(NewState)` | Optional round FSM label |
| `OnHealthChanged(Health, Max, Percent)` | Health bar |
| `OnRelicPossessionChanged(Team, bCarried)` | Relic slot (C++ poll — implement in BP for visuals) |

**Team portraits:** `UBwayHUDHelpers::GetTeamPlayerHUDData` / `UpdateTeamPortraits` — 4 slots per team, humanoid placeholder OK for Section 2.

---

## 1. Create widget

**Path:** `/BreakawayCore/UI/Match/WBP_BW_CoreHUD`

1. Content Browser → `/BreakawayCore/UI/Match/`
2. **User Interface → Widget Blueprint**
3. Parent: **`BwayCoreHUDWidget`**
4. Name: `WBP_BW_CoreHUD`

---

## 2. Layout hierarchy (reference screenshot)

```
[Root - Canvas Panel]
├── [Horizontal Box] Team1PortraitsContainer          ← top-left, 4 portraits + health pips
├── [Horizontal Box] Team2PortraitsContainer          ← top-right
├── [Vertical Box] TopCenterCluster                   ← anchor top-center
│   ├── [Text Block] Text_RoundLabel                  ← "ROUND 1"
│   ├── [Text Block] Text_Timer                       ← MM:SS from OnRoundTimeUpdated
│   └── [Horizontal Box] ScoreRow
│       ├── [Text Block] Text_Team1Score
│       ├── [Image] Image_RelicIcon
│       └── [Text Block] Text_Team2Score
├── [Vertical Box] RelicStatusCluster                 ← center (optional; may embed W_BW_RelicStatusWidget)
│   ├── [Text Block] Text_RelicStatus
│   └── [Text Block] Text_CarrierName
├── [Overlay] BottomCenterCluster                     ← anchor bottom-center
│   ├── [Progress Bar] Progress_Health
│   ├── [Text Block] Text_HealthValues                ← "1000 / 1000"
│   └── [Horizontal Box] AbilityBarStub               ← hidden Section 2; visible Section 3
└── [Border] Border_SuddenDeathBanner                  ← hidden; Step 16 wires visibility
    └── [Text Block] Text_SuddenDeathWarning
```

**ASCII overview:**

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ [T1 portraits ×4]        [ROUND N]  [5:00]        [T2 portraits ×4]       │
│                          [ 0  ◉  0 ]                                        │
│                         [Relic status text]                                 │
│                                                                             │
│                      [████████ health ████████]                             │
│                      [ ability stubs hidden ]                               │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Key widget properties

### TopCenterCluster (Canvas slot)

| Property | Value |
|----------|--------|
| Anchors | Top center (0.5, 0) |
| Position Y | 20 |
| Size | ~500 × 100 |

### Text_Timer

| Property | Value |
|----------|--------|
| Font Size | 48 |
| Justification | Center |

### Text_Team1Score / Text_Team2Score

| Property | Value |
|----------|--------|
| Font Size | 56 |
| Color | Team colors (blue / orange) |

### Progress_Health

| Property | Value |
|----------|--------|
| Percent | 1.0 (placeholder) |
| Fill Color | Green |

### Team portrait slots (×4 per side)

Use **`UBwayHUDHelpers → Update Team Portraits`** with an array of 4 `Image` widgets per team, or manually bind from `GetTeamPlayerHUDData` in Event Graph.

| Portrait Image | Size |
|----------------|------|
| Each slot | 65 × 65 |
| Team 1 border tint | Blue |
| Team 2 border tint | Orange |

Placeholder texture: `/BreakawayCore/UI/HeroSelect/T_HeroPortrait_Placeholder` (if present).

---

## 4. Event graph — C++ events

### OnScoreChanged (Team1Score, Team2Score)

```
OnScoreChanged
  → Set Text_Team1Score = Team1Score (as text)
  → Set Text_Team2Score = Team2Score (as text)
```

### OnRoundTimeUpdated (SecondsRemaining, FormattedTime)

```
OnRoundTimeUpdated
  → Set Text_Timer = FormattedTime
  → If SecondsRemaining <= 60:
        Set Text_Timer Color = Red
    Else:
        Set Text_Timer Color = White
```

### OnHealthChanged (NewHealth, MaxHealth, HealthPercent)

```
OnHealthChanged
  → Set Progress_Health Percent = HealthPercent
  → Set Text_HealthValues = Format "{0} / {1}" (NewHealth, MaxHealth)
```

### OnRoundStateChanged (NewState)

```
OnRoundStateChanged
  → (Optional) Set debug text from NewState
```

### Relic status (poll fallback until 14-0 C++ lands)

```
Event Construct
  → Set Timer 0.25s looping → PollRelicForHUD

PollRelicForHUD
  → GetRelicPossessingTeam → update Text_RelicStatus / Text_CarrierName
  → IsRelicCarried → branch carrier name visibility
```

### Client timer fallback

`OnRoundTimeUpdated` fires from server tick only. For dedicated clients, add:

```
Event Tick
  → Accumulate Delta → every 1.0s:
        GetRoundTimeFormatted → Set Text_Timer
        GetRoundTimeRemaining → sudden-death color branch
```

Listen-server PIE pass may work without Tick fallback; include it before claiming Step 14 bulletproof on multi-client.

### Team portraits refresh

```
Event Construct
  → Set Timer 2.0s looping → RefreshPortraits

RefreshPortraits
  → Get Local Player Team For HUD
  → Bway HUD Helpers → Update Team Portraits For Display Slot (DisplaySlot=0, LocalTeam, Team1PortraitImages)
  → Bway HUD Helpers → Update Team Portraits For Display Slot (DisplaySlot=1, LocalTeam, Team2PortraitImages)
```

Also call once on `OnRoundStateChanged` when state returns to active round.

---

## 5. Experience migration (Step 14-4)

### Option A — Embed slot widgets (fastest)

1. Keep `LAS_ShooterGame_StandardHUD` on experience.
2. Remove score/relic rows from `B_BW_CaptureTheRelic_Scoring` **Widgets** list (avoid duplicates).
3. Add new action set **`B_BW_CoreHUD_Layout`** (or extend scoring set) with **Add Widgets → Layout**:

| Layout property | Value |
|-----------------|--------|
| **Layout Class** | `WBP_BW_CoreHUD` |
| **Layer ID** | `Lyra.HUD.PlayerHUD` |

If Lyra layout stacking conflicts, use **Option B**.

### Option B — Full layout replace

1. Duplicate `LAS_ShooterGame_StandardHUD` → `LAS_BW_MatchHUD_Dev`.
2. Change its **Layout** entry to `WBP_BW_CoreHUD` at `Lyra.HUD.PlayerHUD`.
3. Swap experience ActionSet from ShooterCore HUD to `LAS_BW_MatchHUD_Dev`.
4. Remove standalone widget rows from scoring set.

### Experience table (Step 14 pass)

| Add | Purpose |
|-----|---------|
| `B_BW_CoreHUD_Layout` (or modified scoring/HUD set) | Injects `WBP_BW_CoreHUD` as layout |

| Remove | Purpose |
|--------|---------|
| `W_BW_CaptureTheRelic_ScoreWidget` slot row | Superseded by CoreHUD |
| `W_BW_RelicStatusWidget` slot row | Superseded by CoreHUD relic cluster |

---

## 6. Optional C++ — substep 14-0

`OnRelicPossessionChanged` is declared on `UBwayCoreHUDWidget` but not fired in C++ today. Optional fix in `BwayCoreHUDWidget.cpp`:

- Bind `RelicManager` on Construct
- Fire `OnRelicPossessionChanged` from poll or when possession team replicates

After C++ change: run Tier 1 — `Scripts/Test/Run-Tier.ps1 -Tier 1`.

---

## 7. PIE URL

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&RoundDuration=90
```

**Listen Server**, 1 player.

---

## Troubleshooting

| Symptom | Check |
|---------|--------|
| Double score/timer | Both slot widgets and CoreHUD active — remove Step 12–13 slot rows |
| Timer frozen | Add Tick fallback; verify round active (`Playing` phase) |
| Health bar empty | Pawn spawned; `ULyraHealthComponent` on humanoid; respawn re-bind (C++ handles in NativeTick) |
| Portraits empty | `GetTeamPlayerHUDData` returns bots + player; placeholder texture assigned |
| No HUD | Layout Layer ID = `Lyra.HUD.PlayerHUD`; experience action set loaded |
| Relic text stale | 0.25s poll or complete 14-0 C++ bind |

---

## Related

- [MatchUI_Setup.md](./MatchUI_Setup.md) — Steps 12–13 standalone widgets
- [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) — Step 14 checklist
- [UI_System.md](./UI_System.md)
