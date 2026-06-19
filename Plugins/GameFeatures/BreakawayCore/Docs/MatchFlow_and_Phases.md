# Match Flow & Phases

How Breakaway matches start, progress through phases, and end. This document matches the **Step 11** implementation in `UBwayRoundManagementComponent` and `UBwayMatchFlowLibrary`.

---

## Overview

| Layer | Owner | Responsibility |
|-------|-------|----------------|
| **Experience wiring** | `B_BW_Experience_Dev` + action sets | Gameplay abilities, teams, bots, relic — **not** phase grants when orchestrating |
| **Config data** | `UBwayMatchFlowConfig` (`DA_BW_MatchFlow_Dev`) | Phase abilities, durations, `PointsToWin`, `DefaultNumBots`, `bOrchestrateMatchFlow` |
| **Config attachment** | `UBwayGameFeatureAction_MatchFlowConfig` | Data-only `UGameFeatureAction` on experience **Actions** (or action set) — **do not reparent** the experience BP |
| **Resolution** | `UBwayMatchFlowLibrary::ResolveMatchFlowSettings` | Config asset + URL overrides → `FBwayResolvedMatchFlowSettings` |
| **Orchestration** | `UBwayRoundManagementComponent` (RM) | **Sole `StartPhase` caller** when `bOrchestrateMatchFlow=true`; drives `EBwayMatchPhase` |
| **Presentation** | `ABwayGameState` | Results screen, `ReturnToFrontEnd` — **no** `StartPhase` |

When `bOrchestrateMatchFlow=true`, remove auto-granted `BW_Phase_*` actions from the experience. RM owns all GAS phase transitions.

---

## Config resolution order

`UBwayMatchFlowLibrary::ResolveMatchFlowSettings` builds `FBwayResolvedMatchFlowSettings` in this order:

1. **Base config asset** (first match wins):
   - URL `?MatchFlowConfig=<PrimaryAssetName>` (type `BwayMatchFlowConfig`)
   - `UBwayGameFeatureAction_MatchFlowConfig` on experience **Actions** or linked action-set **Actions**
   - `UBwayExperienceDefinition::MatchFlowConfig` soft ptr (only if experience is reparented — **avoid**)
   - Hard fallback: `/BreakawayCore/MatchFlow/DA_BW_MatchFlow_Dev`

2. **URL overrides** (applied on top of the resolved config; read via `UBwayGameplayUrlLibrary` after authority `OptionsString` merge):

   | Key | Type | Applied to |
   |-----|------|------------|
   | `PointsToWin` | int | RM `PointsToWin` |
   | `NumBots` | int | `UBwayBotCreationComponent` |
   | `MatchFlowConfig` | primary asset name | Base config (step 1) |
   | `PrematchDuration` | float (seconds) | RM phase timer |
   | `WarmupDuration` | float (seconds) | RM phase timer |
   | `PostRoundDuration` | float (seconds) | RM phase timer |
   | `RoundDuration` | float (seconds) | RM round FSM timer |

3. **Apply rules** — `ApplyMatchRulesOnly` sets `PointsToWin`, `RoundDuration` on RM and `NumBots` on BotCreation (BotCreation hook runs on experience load; RM high-priority hook runs first).

### URL option sources

`UBwayGameplayUrlLibrary::AugmentGameModeOptionsString` merges gameplay keys into `GameMode::OptionsString` from (highest priority first):

- Editor play URL / additional server options / launch parameters
- PIE `LastURL` (authority world preferred)
- `WorldContext.LastURL`, `World.URL`
- Command line
- Existing `GameMode::OptionsString` (lowest)

Called from:

- `ABreakawayGameMode::InitGame` (after `Super::InitGame`)
- `ABreakawayGameMode::RefreshGameplayUrlOptions` (`BeginPlay`, `PostLogin`)
- `UBwayRoundManagementComponent::HandleExperienceLoadedForMatchRules` (before resolve)

---

## Match phase loop (`EBwayMatchPhase`)

Top-level phases are replicated on RM (`CurrentMatchPhase`). Nested round FSM (`ERoundState`) runs **only inside Playing**.

```
Prematch → Warmup (once) → Playing ⇄ PostRound → PostMatch
                              ↑__________|
                         (until PointsToWin)
```

| Phase | RM entry | Pawns | GAS | Timer |
|-------|----------|-------|-----|-------|
| **Prematch** | `EnterPrematch()` on experience load | Frozen (`ShouldDeferPlayerRestartForMatchFlow`) | `PrematchPhaseAbility` | `PrematchDuration` (config/URL) |
| **Warmup** | After prematch completes | Spawn / restart deferred players + bots | `WarmupPhaseAbility` | `WarmupDuration` — **once per match** |
| **Playing** | After warmup, or after PostRound | Active | `PlayingPhaseAbility` (match-long) | Per-round: `RoundDuration` via round FSM |
| **PostRound** | After round win, match not over | Active | `PostRoundPhaseAbility` | `PostRoundDuration` |
| **PostMatch** | Match win threshold reached | Stopped / results | `PostMatchPhaseAbility` | Presentation-driven |

**Hybrid timing:** Config timers are **authoritative**. If the GAS phase ability fires `PhaseEnded` early, RM shortens the current phase (clears timer, advances).

**Legacy path:** When `bOrchestrateMatchFlow=false`, RM registers a `PlayingPhaseTag` listener instead of the orchestrator chain.

---

## Experience load sequence

1. Lyra loads `B_BW_Experience_Dev`.
2. RM **high-priority** `OnExperienceLoaded` hook:
   - `AugmentGameModeOptionsString`
   - `ResolveMatchFlowSettings` → cache on `ResolvedMatchFlowSettings`
   - `LogResolvedMatchFlowSettings`
   - If no BotCreation: `ApplyMatchRulesOnly`
   - If orchestrating: `EnterPrematch()` (disables legacy listener)
3. BotCreation **normal-priority** hook: resolve again, `ApplyMatchRulesOnly` + spawn bots (when phase allows).

RM phase timers read **`ResolvedMatchFlowSettings`** durations directly — URL overrides must be resolved before `EnterPrematch()` (they are).

---

## Locked decisions (grill summary)

| # | Decision |
|---|----------|
| 1 | One experience (`B_BW_Experience_Dev`); URL flags for entry point only — no fork for menu vs PIE |
| 2 | `UBwayGameFeatureAction_MatchFlowConfig` on experience **Actions** — not reparenting to `UBwayExperienceDefinition` |
| 3 | RM is the **only** `StartPhase` caller when orchestrating |
| 4 | `ABwayGameState` handles results / front-end travel only |
| 5 | Hero skip (`SkipHeroSelection=1`) must **not** call `EndPhaseAndProgressToNext()` — RM owns Warmup entry |
| 6 | `EBwayMatchPhase::HeroSelection` deferred to Section 3 |
| 7 | Queue/matchmaking picks experience; `UBwayMatchFlowConfig` stays on the experience |

---

## Dev PIE URLs

**Fast multi-round test (short phase timers):**

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&WarmupDuration=5&PostRoundDuration=3
```

**Single-round / results test:**

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=1
```

**Expected logs (11b):**

```
BwayMatchFlow: WarmupDuration=5.0s (URL override from Editor.UserEditedPlayWorldURL, DA default was 3.0s)
BwayMatchFlow: PostRoundDuration=3.0s (URL override from …)
BwayRoundManagement: Warmup config timer started (5.0s, authoritative)
BwayRoundManagement: PostRound config timer started (3.0s, authoritative)
```

Filter: `BwayMatchFlow`, `BwayRoundManagement`, `BwayGameplayUrl`.

---

## Key C++ files

| File | Role |
|------|------|
| `GameModes/BwayMatchFlowConfig.h` | Primary data asset |
| `GameModes/BwayGameFeatureAction_MatchFlowConfig.h` | Experience action |
| `GameModes/BwayMatchFlowLibrary.*` | Resolve + apply + log |
| `GameModes/BwayGameplayUrlLibrary.*` | URL merge + getters |
| `GameModes/BwayMatchPhaseTypes.h` | `EBwayMatchPhase` |
| `GameState/BwayRoundManagementComponent.*` | Orchestrator |
| `BreakawayGameMode.*` | InitGame URL augment, pawn defer during Prematch |

---

## Related docs

- [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) — Step 11 sub-steps
- [Lyra_Integration.md](./Lyra_Integration.md)
- [HeroSelect_Staging_Setup.md](./HeroSelect_Staging_Setup.md) — Section 3 hero phase
