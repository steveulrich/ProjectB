# UI System

CommonUI activatable widgets — C++ bases, Blueprint visuals.

## Pattern

Lyra CommonUI → `UCommonUIExtensions::PushContentToLayer_ForPlayer` from PlayerController.

In-match HUD (Section 2): Lyra **`UGameFeatureAction_AddWidgets`** on **`EAS_BW_CaptureTheRelic`** injects C++-backed slot widgets into ShooterCore-style layout extension slots. Step 14 adds health + portrait slots on a duplicated layout (`WBP_BW_MatchHUDLayout` / `LAS_BW_MatchHUD_Dev`).

## Widget Bases

| C++ class | Purpose |
|-----------|---------|
| `UBwayMatchHUDWidgetBase` | Shared match data access + delegate binding; display-team remap helpers |
| `UBwayCaptureTheRelicScoreWidget` | Step 12 score slot — display-column scores, timer poll, optional `Text_RoundLabel` (**Complete**) |
| `UBwayRelicStatusWidget` | Step 13 relic status slot — 0.25s possession poll |
| `UBwayHealthHUDWidget` | Step 14 health slot — `ULyraHealthComponent` bind + respawn re-bind |
| `UBwayTeamPortraitsHUDWidget` | Step 14 portrait slot — display-slot-aware team rows |
| `UBwayCoreHUDWidget` | **Deprecated** — monolithic shell; use slot widgets instead |
| `UBwayScoreboardWidget` | Tab scoreboard — K/D/A/objective from PlayerState |
| `UBwayPostRoundSummaryWidget` | Step 15 PostRound team-aggregate interstitial |
| `UBwayMatchStatsLibrary` | Per-player / team stat helpers + PostRound summary builder |
| `UBwayResultsScreenWidget` | Post-match flow — MVP formula; Step 16 orchestrates interstitial + breakdown |
| `UBwayPauseMenuWidget` | Pause menu |
| `UBwayHeroSelectWidget` | Hero pick — 6 BP implementable events |
| `UBwayHeroSlotWidget` | Hero slot button |
| `UBwayHeroAbilityUILibrary` | Resolve ability bar display from ability sets / ASC + CDO DisplayData |
| `UBwayAbilityIconWidget` | Ability bar icon |
| `UBwayCurrencyDisplayWidget` | Gold display |
| `UBwayHUDHelpers` | Portrait/class/color BP library; `*ForDisplaySlot` portrait helpers |

## Expected Blueprint Assets

| Asset | Path |
|-------|------|
| `WBP_BW_HeroSelect` | `/BreakawayCore/UI/HeroSelect/` |
| `W_BW_CaptureTheRelic_ScoreWidget` | `/BreakawayCore/UI/Match/` — parent **`BwayCaptureTheRelicScoreWidget`** (**Step 12**) |
| `W_BW_RelicStatusWidget` | `/BreakawayCore/UI/Match/` — parent **`BwayRelicStatusWidget`** (**Step 13**) |
| `W_BW_HealthWidget` | `/BreakawayCore/UI/Match/` — parent **`BwayHealthHUDWidget`** (**Step 14**) |
| `W_BW_TeamPortraitsWidget` | `/BreakawayCore/UI/Match/` — parent **`BwayTeamPortraitsHUDWidget`** (**Step 14**) |
| `WBP_BW_MatchHUDLayout` | `/BreakawayCore/UI/Match/` — duplicated Shooter layout + Breakaway extension points |
| `WBP_BW_PostRoundSummary` | `/BreakawayCore/UI/Match/` — parent **`BwayPostRoundSummaryWidget`** (**Step 15**) |
| `WBP_BW_ResultsWidget` | `/BreakawayCore/UI/` |
| `WBP_BW_Scoreboard` | `/BreakawayCore/UI/` (not created yet) |
| `WBP_BW_PauseMenu` | `/BreakawayCore/UI/` (not created yet) |
| `WBP_BW_CoreHUD` | `/BreakawayCore/UI/Match/` — **deprecated**; do not wire to experience |

## HUD slot tags

| Tag | Widget |
|-----|--------|
| `HUD.Slot.TeamScore` | `W_BW_CaptureTheRelic_ScoreWidget` |
| `HUD.Slot.ModeStatus` | `W_BW_RelicStatusWidget` |
| `HUD.Slot.Health` | `W_BW_HealthWidget` |
| `HUD.Slot.TeamPortraits` | `W_BW_TeamPortraitsWidget` |

Injection owner: **`EAS_BW_CaptureTheRelic`**. Layout owner: **`LAS_BW_MatchHUD_Dev`** (or `LAS_ShooterGame_StandardHUD` until layout dup exists) @ **`UI.Layer.Game`**.

## Display perspective (Section 2+)

Authoritative game teams (`0` / `1`) are unchanged in gameplay code. HUD widgets remap at the UI boundary:

| API | Role |
|-----|------|
| `UBwayMatchHUDWidgetBase::GetDisplayTeamScore` | Left/right score columns |
| `UBwayMatchHUDWidgetBase::GetDisplayRelicPossessingTeam` | Relic status |
| `UBwayHUDHelpers::UpdateTeamPortraitsForDisplaySlot` | Left/right portrait rows |

**Local team left:** when `GetLocalPlayerTeam` is `0` or `1`, display slot 0 = friendly. **Spectators** (`LocalTeam == -1`) see game team 0 left, team 1 right.

## Hero Select Events (BP implementable)

- `OnHeroListPopulated`, `OnHeroSelected`, `OnHeroLocked`, `OnSelectionTimerUpdated`, etc.

## Scoreboard Data

**`FScoreboardPlayerData`** populated from **`ABwayPlayerState`**: Kills, Deaths, Assists, ObjectiveScore, hero name, ping.

## Results MVP

Weighted score: `K*2 + A*1 + Objective*3 - D*0.5` — highest across all players.

## PostRound / PostMatch UI

- **PostRound (Step 15):** bind to `UBwayRoundManagementComponent::OnPostRoundSummaryStarted` or rely on `ABwayPlayerController` auto-show. Dismiss via `OnMatchPhaseChanged` when phase ≠ `PostRound`.
- **PostMatch (Step 16):** END OF MATCH interstitial (team aggregate) → Match Breakdown (horizontal per-player columns, MVP highlight) → Return to Lobby / Play Again.

Stat helpers: `UBwayMatchStatsLibrary::GetPlayerMatchStats`, `GetPlayerLastRoundStats`, `AggregateTeamStats`.

## Setup Guides

- [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) — Section 2 Steps 12–16
- [MatchUI_Setup.md](./MatchUI_Setup.md) — score + relic status widgets
- [CoreHUD_Layout_Setup.md](./CoreHUD_Layout_Setup.md) — Step 14 slot composition (health + portraits)
- [CharacterSelect_BlueprintSetup.md](./CharacterSelect_BlueprintSetup.md)
- [BLUEPRINT_INTEGRATION_GUIDE.md](../../../AI_Planning/BLUEPRINT_INTEGRATION_GUIDE.md)
