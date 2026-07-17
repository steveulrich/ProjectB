# Match HUD — Slot Composition (Steps 14 + 19.5)

**Pivot (Jun 2026):** Follow Lyra/ShooterCore **layout + slot injection** — not a monolithic `WBP_BW_CoreHUD`. Step 14 adds **health** and **team portrait** slots. Step 19.5 adds **ability bar** and **gold** slots and upgrades the portrait widget. Steps 12–13 slot widgets stay registered on **`EAS_BW_CaptureTheRelic`**.

Step checklist: [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) Steps 14 and 19.5.

**Also see:** [MatchUI_Setup.md](./MatchUI_Setup.md) (Steps 12–13), [HUD_LAYOUT_DETAILED_GUIDE.md](../../../AI_Planning/HUD_LAYOUT_DETAILED_GUIDE.md) (visual reference — slot widgets compose the same regions).

---

## Target architecture

```
LAS_BW_MatchHUD  (duplicate of LAS_ShooterGame_StandardHUD)
  └── Layout: WBP_BW_MatchHUDLayout @ UI.Layer.Game
        └── UIExtensionPoint widgets for HUD.Slot.*

EAS_BW_CaptureTheRelic → UGameFeatureAction_AddWidgets rows:
  W_BW_CaptureTheRelic_ScoreWidget  @ HUD.Slot.TeamScore     (scores + timer + round label)
  W_BW_RelicStatusWidget            @ HUD.Slot.ModeStatus
  W_BW_HealthWidget                 @ HUD.Slot.Health
  W_BW_TeamPortraitsWidget          @ HUD.Slot.TeamPortraits.Friendly
  W_BW_TeamPortraitsWidget_Enemy    @ HUD.Slot.TeamPortraits.Enemy
  W_BW_AbilityBar                   @ HUD.Slot.AbilityBar
  W_BW_GoldWidget                   @ HUD.Slot.Gold

B_BW_Experience_Dev ActionSets:
  LAS_BW_MatchHUD
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
| `UBwayTeamPortraitsHUDWidget` | `HUD.Slot.TeamPortraits.Friendly` / `.Enemy` | `W_BW_TeamPortraitsWidget` (+ `_Enemy`) |
| `UBwayTeamPortraitSlotWidget` | (spawned by row) | `W_BW_TeamPortraitSlot` |
| `UBwayAbilityBarHUDWidget` | `HUD.Slot.AbilityBar` | `W_BW_AbilityBar` |
| `UBwayCurrencyDisplayWidget` | `HUD.Slot.Gold` | `W_BW_GoldWidget` |

Shared base: **`UBwayMatchHUDWidgetBase`** — scoring/timer/round-state delegates, display-team remap.

### Bound widget contracts

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

**Portraits** (`W_BW_TeamPortraitsWidget` / `_Enemy`):

| Name | C++ behavior |
|------|----------------|
| `TeamPortraitsRow` | HorizontalBox; C++ spawns four `W_BW_TeamPortraitSlot` children |
| `Text_TeamLabel` | Optional; Friendly vs Enemy label from `DisplaySlotIndex` |
| `PortraitSlotWidgetClass` | Class Defaults → `W_BW_TeamPortraitSlot` |
| `DisplaySlotIndex` | `0` on Friendly BP, `1` on Enemy BP |

Per-slot bind names on `W_BW_TeamPortraitSlot`: `Frame`, `Image_Portrait`, `Image_DeathOverlay`, `Text_RelicBadge`.

**Do not** put one dual-team portraits widget on both left and right extension points — that duplicates FRIENDLY+ENEMY on each side.

| Layout extension point | Tag | Widget |
|------------------------|-----|--------|
| `ExtensionPoint_Team1Portraits` | `HUD.Slot.TeamPortraits.Friendly` | `W_BW_TeamPortraitsWidget` (`DisplaySlotIndex=0`) |
| `ExtensionPoint_Team2Portraits` | `HUD.Slot.TeamPortraits.Enemy` | `W_BW_TeamPortraitsWidget_Enemy` (`DisplaySlotIndex=1`) |

**Ability bar** (`W_BW_AbilityBar`):

| Position | Input tag | Display key |
|----------|-----------|-------------|
| 0 | `InputTag.Ability.Buildable` | current binding for 1 |
| 1 | `InputTag.Ability.Primary` | current binding for LMB |
| 2 | `InputTag.Ability.Ability4` | current binding for F |
| 3 | `InputTag.Ability.Ability1` | current binding for Q |
| 4 | `InputTag.Ability.Ability2` | current binding for E |
| 5 | `InputTag.Ability.Ability3` | current binding for R |

The six `W_BW_MatchAbilitySlot` instances are created by C++. Relic carrying preserves position 0 and replaces positions 1–5 with the explicit real relic mapping; missing grants remain disabled without shifting.

**Gold widget** (`W_BW_GoldWidget`): `Text_Amount` is updated from `UBwayGoldAttributeSet::CurrentGold`; `Image_CurrencyIcon` is optional.

---

## Gameplay tags

Registered in `Plugins/GameFeatures/BreakawayCore/Config/Tags/BreakawayCore.ini`:

| Tag | Purpose |
|-----|---------|
| `HUD.Slot.Health` | Bottom-center health bar |
| `HUD.Slot.TeamPortraits.Friendly` | Top-left local-team portrait row |
| `HUD.Slot.TeamPortraits.Enemy` | Top-right enemy portrait row |
| `HUD.Slot.TeamPortraits` | **Deprecated** — caused dual injection when both layout points shared it |
| `HUD.Slot.AbilityBar` | Bottom-center six-position ability bar |
| `HUD.Slot.Gold` | Compact local-player gold counter |

ShooterCore tags (`HUD.Slot.TeamScore`, `HUD.Slot.ModeStatus`) unchanged.

---

## Editor / script setup

Run with Unreal Editor open (ue-mcp). **Recompile BreakawayCoreRuntime first** after pulling C++ slot classes.

```powershell
cd "E:\Unreal Projects\ProjectB\Scripts"
node setup-step19-5-match-hud.mjs
```

Optional — refresh score widget with round label:

```powershell
node setup-ctr-score-widget.mjs
```

### What each script does

| Script | Action |
|--------|--------|
| `setup-step19-5-match-hud.mjs` | Audit carrier grants/IMC; create/reparent ability-slot, ability-bar, and gold widgets; rebuild 4v4 portraits; add AbilityBar/Gold extension points and EAS rows; ensure relic tags in the Lyra input config |

---

## Manual editor checklist (if scripts fail)

1. **`EAS_BW_CaptureTheRelic`** (`/BreakawayCore/Experiences/`) → **Add Widgets** → all six rows (table above).
2. **`LAS_BW_MatchHUD`** points **Layout** at **`WBP_BW_MatchHUDLayout`** @ **`UI.Layer.Game`**.
3. On **`B_BW_Experience_Dev`**: use **`LAS_BW_MatchHUD`**; remove **`B_BW_CoreHUD_Layout`** if present.
4. On **`W_BW_CaptureTheRelic_ScoreWidget`**: Class Defaults → **`bShowRoundTimer = true`**.

---

## PIE URL

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&RoundDuration=90
```

**Listen Server**, 1 player.

---

## Pass checklist (Steps 14 + 19.5)

- [ ] Six slot widgets visible (score, relic status, health, portraits, ability bar, gold)
- [ ] Timer + round label on score slot; no duplicate score/relic UI
- [ ] Health bar tracks damage / respawn
- [ ] Portrait rows populate (placeholder OK)
- [ ] Ability bar stays ordered 1/LMB/F/Q/E/R and live remaps update labels
- [ ] Relic pickup/loss swaps only the five combat entries
- [ ] Confirmed buildable use greys position 0 until next round
- [ ] Gold updates for passive, kill, and goal awards
- [ ] Portrait frames, death state, and relic badge replicate on clients
- [ ] **3/3** cold-start Listen Server runs
- [ ] Regression: Steps 12–13 behavior, Section 1 gameplay

---

## Troubleshooting

| Symptom | Check |
|---------|--------|
| No HUD at all | `LAS_BW_MatchHUD` on the experience and `WBP_BW_MatchHUDLayout` @ `UI.Layer.Game` |
| Missing health/portraits only | Extension points on `WBP_BW_MatchHUDLayout`; tags `HUD.Slot.Health` / `HUD.Slot.TeamPortraits.Friendly` / `.Enemy` |
| Doubled FRIENDLY+ENEMY labels | Legacy `HUD.Slot.TeamPortraits` on both left/right points; use Friendly/Enemy tags + single-team row widgets |
| Missing ability bar/gold only | Run `setup-step19-5-match-hud.mjs`; verify `HUD.Slot.AbilityBar` / `HUD.Slot.Gold` EAS rows |
| Score/relic missing | `EAS_BW_CaptureTheRelic` widget rows restored (not empty) |
| Double score/timer | Remove `B_BW_CoreHUD_Layout` / monolithic CoreHUD from experience |
| Timer frozen on client | Score widget `bShowRoundTimer=true`; C++ 1s poll active |
| Health empty | Humanoid pawn + `ULyraHealthComponent`; recompile C++ |
| Key shows `—` | Input tag is absent from `DA_BW_InputData_Humanoid`, action is not in the active IMC, or the live ASC grant is unresolved |

---

## Related

- [MatchUI_Setup.md](./MatchUI_Setup.md) — Steps 12–13
- [UI_System.md](./UI_System.md) — C++ widget bases
- [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) — Section 2
