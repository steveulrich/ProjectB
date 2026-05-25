# UI System

CommonUI activatable widgets — C++ bases, Blueprint visuals.

## Pattern

Lyra CommonUI → `UCommonUIExtensions::PushContentToLayer_ForPlayer` from PlayerController.

## Widget Bases

| C++ class | Purpose |
|-----------|---------|
| `UBwayCoreHUDWidget` | In-match HUD — health, score, timer, relic |
| `UBwayScoreboardWidget` | Tab scoreboard — K/D/A/objective from PlayerState |
| `UBwayResultsScreenWidget` | Post-match — MVP score formula in C++ |
| `UBwayPauseMenuWidget` | Pause menu |
| `UBwayHeroSelectWidget` | Hero pick — 6 BP implementable events |
| `UBwayHeroSlotWidget` | Hero slot button |
| `UBwayAbilityIconWidget` | Ability bar icon |
| `UBwayCurrencyDisplayWidget` | Gold display |
| `UBwayHUDHelpers` | Portrait/class/color BP library |

## Expected Blueprint Assets

| Asset | Path |
|-------|------|
| `WBP_BW_HeroSelect` | `/BreakawayCore/UI/HeroSelect/` |
| `WBP_BW_CoreHUD` | `/BreakawayCore/UI/` |
| `WBP_BW_Scoreboard` | `/BreakawayCore/UI/` |
| `WBP_BW_ResultsScreen` | `/BreakawayCore/UI/` |
| `WBP_BW_PauseMenu` | `/BreakawayCore/UI/` |

## Hero Select Events (BP implementable)

- `OnHeroListPopulated`, `OnHeroSelected`, `OnHeroLocked`, `OnSelectionTimerUpdated`, etc.

## Scoreboard Data

**`FScoreboardPlayerData`** populated from **`ABwayPlayerState`**: Kills, Deaths, Assists, ObjectiveScore, hero name, ping.

## Results MVP

Weighted score: `K*2 + A*1 + Objective*3 - D*0.5` — highest across all players.

## Between-Round UI

Bind to `UBwayRoundManagementComponent::OnBetweenRoundPlanningStarted` for buildable shop overlay.

## Setup Guide

- [CharacterSelect_BlueprintSetup.md](./CharacterSelect_BlueprintSetup.md)
- [BLUEPRINT_INTEGRATION_GUIDE.md](../../../AI_Planning/BLUEPRINT_INTEGRATION_GUIDE.md)
