# Match HUD — Slot Composition (Step 14 pivot)

**Pivot (Jun 2026):** Follow Lyra/ShooterCore **layout + slot injection** — not a monolithic `WBP_BW_CoreHUD`. Step 14 adds **health** and **team portrait** slot widgets plus a Breakaway HUD layout shell. Steps 12–13 slot widgets stay registered on **`EAS_BW_CaptureTheRelic`**.

Step checklist: [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) Step 14.

**Also see:** [MatchUI_Setup.md](./MatchUI_Setup.md) (Steps 12–13), [HUD_LAYOUT_DETAILED_GUIDE.md](../../../AI_Planning/HUD_LAYOUT_DETAILED_GUIDE.md) (visual reference — slot widgets compose the same regions).

---

## Target architecture

```
LAS_BW_MatchHUD_Dev  (duplicate of LAS_ShooterGame_StandardHUD)
  └── Layout: WBP_BW_MatchHUDLayout @ UI.Layer.Game
        └── UIExtensionPoint widgets for HUD.Slot.*

EAS_BW_CaptureTheRelic → UGameFeatureAction_AddWidgets rows:
  W_BW_CaptureTheRelic_ScoreWidget  @ HUD.Slot.TeamScore     (scores + timer + round label)
  W_BW_RelicStatusWidget            @ HUD.Slot.ModeStatus
  W_BW_HealthWidget                 @ HUD.Slot.Health        (new)
  W_BW_TeamPortraitsWidget          @ HUD.Slot.TeamPortraits (new)

B_BW_Experience_Dev ActionSets:
  LAS_BW_MatchHUD_Dev (or LAS_ShooterGame_StandardHUD until layout dup runs)
  EAS_BW_CaptureTheRelic
  (NO B_BW_CoreHUD_Layout, NO monolithic WBP_BW_CoreHUD layout push)
```

**Deprecated:** `UBwayCoreHUDWidget` / `WBP_BW_CoreHUD` — kept in repo for reference only.

---

## C++ slot bases (reference)

| C++ class | Slot tag | Blueprint asset |
|-----------|----------|-----------------|
| `UBwayCaptureTheRelicScoreWidget` | `HUD.Slot.TeamScore` | `W_BW_CaptureTheRelic_ScoreWidget` |
| `UBwayRelicStatusWidget` | `HUD.Slot.ModeStatus` | `W_BW_RelicStatusWidget` |
| `UBwayHealthHUDWidget` | `HUD.Slot.Health` | `W_BW_HealthWidget` |
| `UBwayTeamPortraitsHUDWidget` | `HUD.Slot.TeamPortraits` | `W_BW_TeamPortraitsWidget` |

Shared base: **`UBwayMatchHUDWidgetBase`** — scoring/timer/round-state delegates, display-team remap.

### New Step 14 BindWidget names

**Score widget** (`W_BW_CaptureTheRelic_ScoreWidget`):

| Name | C++ behavior |
|------|----------------|
| `Text_RoundLabel` | `"ROUND N"` from `GetCurrentRoundNumberFromWorld`; refreshes on `OnRoundStateChanged` |
| `Text_Timer` | MM:SS; 1s client poll when `bShowRoundTimer=true` |
| `Text_Team1Score` / `Text_Team2Score` | Display-column scores (local team left) |

**Health widget** (`W_BW_HealthWidget`):

| Name | C++ behavior |
|------|----------------|
| `Progress_Health` | Percent from `ULyraHealthComponent` |
| `Text_HealthValues` | `"{health} / {max}"`; re-binds on respawn via `NativeTick` |

**Portraits widget** (`W_BW_TeamPortraitsWidget`):

| Name | C++ behavior |
|------|----------------|
| `Team1_Portrait_1` … `Team1_Portrait_4` | Display slot 0 (friendly left) |
| `Team2_Portrait_1` … `Team2_Portrait_4` | Display slot 1 (enemy right) |
| | `UBwayHUDHelpers::UpdateTeamPortraitsForDisplaySlot`; 2s poll + refresh on round state |

---

## Gameplay tags

Registered in `Plugins/GameFeatures/BreakawayCore/Config/Tags/BreakawayCore.ini`:

| Tag | Purpose |
|-----|---------|
| `HUD.Slot.Health` | Bottom-center health bar |
| `HUD.Slot.TeamPortraits` | Top left/right portrait rows |

ShooterCore tags (`HUD.Slot.TeamScore`, `HUD.Slot.ModeStatus`) unchanged.

---

## Editor / script setup

Run with Unreal Editor open (ue-mcp). **Recompile BreakawayCoreRuntime** after pulling C++ slot classes.

```powershell
cd "E:\Unreal Projects\ProjectB\Scripts"
node setup-health-hud-widget.mjs
node setup-team-portraits-widget.mjs
node setup-match-hud-layout.mjs
node pivot-hud-to-slots.mjs
```

Optional — refresh score widget with round label:

```powershell
node setup-ctr-score-widget.mjs
```

### What each script does

| Script | Action |
|--------|--------|
| `setup-health-hud-widget.mjs` | Create/reparent `W_BW_HealthWidget` → `BwayHealthHUDWidget` |
| `setup-team-portraits-widget.mjs` | Create/reparent `W_BW_TeamPortraitsWidget` → `BwayTeamPortraitsHUDWidget` |
| `setup-match-hud-layout.mjs` | Duplicate Shooter layout → `WBP_BW_MatchHUDLayout`; duplicate LAS → `LAS_BW_MatchHUD_Dev`; add `UIExtensionPoint` for Health + TeamPortraits |
| `pivot-hud-to-slots.mjs` | Restore all four widget rows on `EAS_BW_CaptureTheRelic`; swap experience to `LAS_BW_MatchHUD_Dev`; remove `B_BW_CoreHUD_Layout`; set `bShowRoundTimer=true` on score widget |

---

## Manual editor checklist (if scripts fail)

1. **`EAS_BW_CaptureTheRelic`** (`/BreakawayCore/Experiences/`) → **Add Widgets** → four rows (table above).
2. Duplicate **`LAS_ShooterGame_StandardHUD`** → **`LAS_BW_MatchHUD_Dev`**; point **Layout** at **`WBP_BW_MatchHUDLayout`** @ **`UI.Layer.Game`**.
3. On **`B_BW_Experience_Dev`**: replace Shooter HUD action set with **`LAS_BW_MatchHUD_Dev`**; remove **`B_BW_CoreHUD_Layout`** if present.
4. On **`W_BW_CaptureTheRelic_ScoreWidget`**: Class Defaults → **`bShowRoundTimer = true`**.

---

## PIE URL

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&RoundDuration=90
```

**Listen Server**, 1 player.

---

## Pass checklist (Step 14)

- [ ] Four slot widgets visible (score, relic status, health, portraits)
- [ ] Timer + round label on score slot; no duplicate score/relic UI
- [ ] Health bar tracks damage / respawn
- [ ] Portrait rows populate (placeholder OK)
- [ ] **3/3** cold-start Listen Server runs
- [ ] Regression: Steps 12–13 behavior, Section 1 gameplay

---

## Troubleshooting

| Symptom | Check |
|---------|--------|
| No HUD at all | `LAS_BW_MatchHUD_Dev` or `LAS_ShooterGame_StandardHUD` on experience |
| Missing health/portraits only | Extension points on `WBP_BW_MatchHUDLayout`; tags `HUD.Slot.Health` / `HUD.Slot.TeamPortraits` |
| Score/relic missing | `EAS_BW_CaptureTheRelic` widget rows restored (not empty) |
| Double score/timer | Remove `B_BW_CoreHUD_Layout` / monolithic CoreHUD from experience |
| Timer frozen on client | Score widget `bShowRoundTimer=true`; C++ 1s poll active |
| Health empty | Humanoid pawn + `ULyraHealthComponent`; recompile C++ |

---

## Related

- [MatchUI_Setup.md](./MatchUI_Setup.md) — Steps 12–13
- [UI_System.md](./UI_System.md) — C++ widget bases
- [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) — Section 2
