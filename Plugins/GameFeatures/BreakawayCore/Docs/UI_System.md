# UI System

CommonUI activatable widgets — C++ bases, Blueprint visuals.

## Pattern

Lyra CommonUI → `UCommonUIExtensions::PushContentToLayer_ForPlayer` from PlayerController.

In-match HUD: Lyra **`UGameFeatureAction_AddWidgets`** on **`EAS_BW_CaptureTheRelic`** injects C++-backed slot widgets into ShooterCore-style layout extension slots. Step 14 adds health + portrait slots on `WBP_BW_MatchHUDLayout` / `LAS_BW_MatchHUD`; Step 19.5 adds the ability bar + gold slots and upgrades portrait state.

## Widget Bases

| C++ class | Purpose |
|-----------|---------|
| `UBwayMatchHUDWidgetBase` | Shared match data access + delegate binding; display-team remap helpers |
| `UBwayCaptureTheRelicScoreWidget` | Step 12 score slot — display-column scores, timer poll, optional `Text_RoundLabel` (**Complete**) |
| `UBwayRelicStatusWidget` | Step 13 relic status slot — 0.25s possession poll |
| `UBwayHealthHUDWidget` | Step 14 health slot — `ULyraHealthComponent` bind + respawn re-bind |
| `UBwayTeamPortraitsHUDWidget` | Single-team portrait row; `DisplaySlotIndex` 0/1; spawns four slot widgets |
| `UBwayTeamPortraitSlotWidget` | One roster portrait (frame, image, death overlay, relic badge) |
| `UBwayAbilityBarHUDWidget` | Step 19.5 fixed six-position match ability bar; live ASC/relic/input-map state |
| `UBwayMatchAbilitySlotWidget` | Non-interactive presentation for one ability-bar position |
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
| `UBwayCurrencyDisplayWidget` | Gold attribute binding with optional `Text_Amount` / `Image_CurrencyIcon` bindings |
| `UBwayHUDHelpers` | Portrait/class/color BP library; `*ForDisplaySlot` portrait helpers |

## Expected Blueprint Assets

| Asset | Path |
|-------|------|
| `WBP_BW_HeroSelect` | `/BreakawayCore/UI/HeroSelect/` |
| `W_BW_CaptureTheRelic_ScoreWidget` | `/BreakawayCore/UI/Match/` — parent **`BwayCaptureTheRelicScoreWidget`** (**Step 12**) |
| `W_BW_RelicStatusWidget` | `/BreakawayCore/UI/Match/` — parent **`BwayRelicStatusWidget`** (**Step 13**) |
| `W_BW_HealthWidget` | `/BreakawayCore/UI/Match/` — parent **`BwayHealthHUDWidget`** (**Step 14**) |
| `W_BW_TeamPortraitSlot` | `/BreakawayCore/UI/Match/` — parent **`BwayTeamPortraitSlotWidget`** |
| `W_BW_TeamPortraitsWidget` | `/BreakawayCore/UI/Match/` — parent **`BwayTeamPortraitsHUDWidget`**, `DisplaySlotIndex=0` (Friendly) |
| `W_BW_TeamPortraitsWidget_Enemy` | `/BreakawayCore/UI/Match/` — same parent, `DisplaySlotIndex=1` (Enemy) |
| `W_BW_MatchAbilitySlot` | `/BreakawayCore/UI/Match/` — parent **`BwayMatchAbilitySlotWidget`** (**Step 19.5**) |
| `W_BW_AbilityBar` | `/BreakawayCore/UI/Match/` — parent **`BwayAbilityBarHUDWidget`** (**Step 19.5**) |
| `W_BW_GoldWidget` | `/BreakawayCore/UI/Match/` — parent **`BwayCurrencyDisplayWidget`** (**Step 19.5**) |
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
| `HUD.Slot.TeamPortraits.Friendly` | `W_BW_TeamPortraitsWidget` |
| `HUD.Slot.TeamPortraits.Enemy` | `W_BW_TeamPortraitsWidget_Enemy` |
| `HUD.Slot.AbilityBar` | `W_BW_AbilityBar` |
| `HUD.Slot.Gold` | `W_BW_GoldWidget` |

Injection owner: **`EAS_BW_CaptureTheRelic`**. Layout owner: **`LAS_BW_MatchHUD`** @ **`UI.Layer.Game`**.

## Display perspective (Section 2+)

Authoritative game teams (`0` / `1`) are unchanged in gameplay code. HUD widgets remap at the UI boundary:

| API | Role |
|-----|------|
| `UBwayMatchHUDWidgetBase::GetDisplayTeamScore` | Left/right score columns |
| `UBwayMatchHUDWidgetBase::GetDisplayRelicPossessingTeam` | Relic status |
| `UBwayHUDHelpers::UpdateTeamPortraitsForDisplaySlot` | Left/right portrait rows |

**Local team left:** when `GetLocalPlayerTeam` is `0` or `1`, display slot 0 = friendly. **Spectators** (`LocalTeam == -1`) see game team 0 left, team 1 right.

Portrait players are sorted deterministically by replicated `PlayerNum`, `PlayerId`, then name. Alive/dead comes from the replicated PlayerState ASC `Status.Death` tag, not a remote owning controller. Relic and death changes refresh by delegate; a 0.5-second poll covers roster churn.

Each side is its own injected row widget (`Friendly` / `Enemy` tags). C++ spawns four `UBwayTeamPortraitSlotWidget` children into `TeamPortraitsRow` — same pattern as the ability bar.

## Step 19.5 match strip

- Ability positions never shift: **1 / LMB / F / Q / E / R**.
- Hero entries resolve exact input-tag grants from the owning PlayerState ASC and read `UBwayGameplayAbility::DisplayData`; unresolved entries remain disabled placeholders.
- While carrying, position 0 remains the buildable and positions 1–5 use the configured `InputTag.Relic.*` mapping. The current content has four real carrier actions; the fifth position is intentionally disabled.
- Key labels resolve `ULyraInputConfig` actions through active Enhanced Input mappings and refresh on `ControlMappingsRebuiltDelegate`.
- The owner-only replicated buildable-used flag greys only position 0 until round start resets it.
- `W_BW_GoldWidget` uses the existing PlayerState ASC `CurrentGold` delegate, so passive/kill/goal awards update the same display.

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
- `Scripts/setup-step19-5-match-hud.mjs` — idempotent Step 19.5 UMG/layout/input/EAS setup
- [CharacterSelect_BlueprintSetup.md](./CharacterSelect_BlueprintSetup.md)
- [BLUEPRINT_INTEGRATION_GUIDE.md](../../../AI_Planning/BLUEPRINT_INTEGRATION_GUIDE.md)
