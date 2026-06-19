# UI System

CommonUI activatable widgets — C++ bases, Blueprint visuals.

## Pattern

Lyra CommonUI → `UCommonUIExtensions::PushContentToLayer_ForPlayer` from PlayerController.

In-match HUD (Section 2): Lyra **`UGameFeatureAction_AddWidgets`** injects C++-backed slot widgets into ShooterCore layout slots (`HUD.Slot.TeamScore`, `HUD.Slot.ModeStatus`) or a full **`UBwayCoreHUDWidget`** layout at `Lyra.HUD.PlayerHUD` (Step 14).

## Widget Bases

| C++ class | Purpose |
|-----------|---------|
| `UBwayMatchHUDWidgetBase` | Shared match data access + delegate binding; display-team remap helpers |
| `UBwayCaptureTheRelicScoreWidget` | Step 12 score slot — `OnTeamScoreChanged`, optional timer poll |
| `UBwayRelicStatusWidget` | Step 13 relic status slot — 0.25s possession poll |
| `UBwayCoreHUDWidget` | Step 14 full in-match HUD — health, score, timer, relic |
| `UBwayScoreboardWidget` | Tab scoreboard — K/D/A/objective from PlayerState |
| `UBwayResultsScreenWidget` | Post-match — MVP score formula in C++ |
| `UBwayPauseMenuWidget` | Pause menu |
| `UBwayHeroSelectWidget` | Hero pick — 6 BP implementable events |
| `UBwayHeroSlotWidget` | Hero slot button |
| `UBwayAbilityIconWidget` | Ability bar icon |
| `UBwayCurrencyDisplayWidget` | Gold display |
| `UBwayHUDHelpers` | Portrait/class/color BP library; `*ForDisplaySlot` portrait helpers |

## Expected Blueprint Assets

| Asset | Path |
|-------|------|
| `WBP_BW_HeroSelect` | `/BreakawayCore/UI/HeroSelect/` |
| `WBP_BW_CoreHUD` | `/BreakawayCore/UI/Match/` |
| `W_BW_CaptureTheRelic_ScoreWidget` | `/BreakawayCore/UI/Match/` — parent **`BwayCaptureTheRelicScoreWidget`** |
| `W_BW_RelicStatusWidget` | `/BreakawayCore/UI/Match/` — parent **`BwayRelicStatusWidget`** |
| `WBP_BW_BetweenRoundPlanning` | `/BreakawayCore/UI/Match/` |
| `WBP_BW_ResultsWidget` | `/BreakawayCore/UI/` |
| `WBP_BW_Scoreboard` | `/BreakawayCore/UI/` (not created yet) |
| `WBP_BW_PauseMenu` | `/BreakawayCore/UI/` (not created yet) |

## Hero Select Events (BP implementable)

- `OnHeroListPopulated`, `OnHeroSelected`, `OnHeroLocked`, `OnSelectionTimerUpdated`, etc.

## Scoreboard Data

**`FScoreboardPlayerData`** populated from **`ABwayPlayerState`**: Kills, Deaths, Assists, ObjectiveScore, hero name, ping.

## Results MVP

Weighted score: `K*2 + A*1 + Objective*3 - D*0.5` — highest across all players.

## Between-Round UI

Bind to `UBwayRoundManagementComponent::OnBetweenRoundPlanningStarted` for buildable shop overlay (stub in Section 2 Step 15).

## Setup Guides

- [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) — Section 2 Steps 12–16
- [MatchUI_Setup.md](./MatchUI_Setup.md) — score + relic status widgets
- [CoreHUD_Layout_Setup.md](./CoreHUD_Layout_Setup.md) — Core HUD shell
- [CharacterSelect_BlueprintSetup.md](./CharacterSelect_BlueprintSetup.md)
- [BLUEPRINT_INTEGRATION_GUIDE.md](../../../AI_Planning/BLUEPRINT_INTEGRATION_GUIDE.md)
