# Match Flow and Phases

End-to-end flow from menu to match and back.

## High-Level Flow

```
FrontEnd → Server travel → Experience load → Warmup → Hero Selection → Playing (rounds) → PostGame → FrontEnd
```

## Round States (`ERoundState`)

| State | Meaning |
|-------|---------|
| `WaitingToStart` | Between rounds / pre-first-round |
| `RoundActive` | Timer running, relic in play |
| `RoundEnding` | Win condition resolved |
| `RoundComplete` | Between-round planning window |

Managed by **`UBwayRoundManagementComponent`**.

## Win Conditions

1. **Goal scored** — relic enters enemy `ABwayGoalVolume` → `OnRelicScored`
2. **Team eliminated** — all members dead → `CheckTeamElimination`
3. **Timer expired** — possession team wins → `OnRoundTimerExpired`

Best-of-N: default **3 points** to win match (`PointsToWin` on RoundManagement).

## Round Reset (`ResetRoundState`)

- Destroys buildables where `bPersistsBetweenRounds == false`
- Resets relic via `UBwayRelicManagerComponent`
- Respawns players via `GameMode::RestartPlayer`
- **Does not reset gold** on PlayerState ASC (match-only persistence)

## Between-Round Planning

`OnBetweenRoundPlanningStarted` fires after `RoundComplete` with `(CompletedRoundNumber, RoundEndDelay)`.

Hook UI here for buildable spend / loadout (minimal C++ hook; BP widget optional).

## Match End

1. `UBwayRoundManagementComponent::CheckMatchEnd` → `OnMatchEnded`
2. `ABwayGameState::TransitionToPostGame` → results widget
3. `ReturnToFrontEnd` → `/Game/System/FrontEnd/Maps/L_LyraFrontEnd`

## Key Classes

| Class | Responsibility |
|-------|----------------|
| `UBwayRoundManagementComponent` | Start/end round, timer, gold awards, win checks |
| `UBwayScoringComponent` | Replicated team scores |
| `UBwayHeroSelectionPhaseComponent` | Phase integration, spawn gate, hero UI |
| `UBwayHeroSelectionManager` | Pick/lock rules, duplicate-hero enforcement |
| `ABwayGameState` | Teams, delegates, PostGame transition |

## Delegates (BlueprintAssignable)

- `OnRoundStarted`, `OnRoundEnded`, `OnMatchEnded`
- `OnRoundStateChanged`, `OnRoundTimeChanged`
- `OnBetweenRoundPlanningStarted`

## Gameplay Tags (phases)

Defined in `BreakawayCore.ini`:

- `Breakaway.GamePhase.*`
- `Breakaway.WinCondition.*`

## Known Issues

- Playing phase tag on RoundManagement may still reference `ShooterGame.GamePhase.Playing`
- Some round logic duplicated on GameMode (legacy); components are authoritative path
