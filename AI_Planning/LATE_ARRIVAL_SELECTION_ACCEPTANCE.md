# Late-Arrival Hero Selection & Travel Recovery Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (4-process packaged delayed newcomer arrival during staging 5s settle countdown, pre-travel `PlayerArray` revalidation, rejected-travel host recovery latch, and Connected/Rediscovery/SelectionEdges regression suites at 200ms lag).
- **Frozen Candidates:**
  - `LAN-20260930-51` (`Saved/StagedBuilds/Windows/candidate-lan20260930-51.json`, 131 staged artifacts): Passed delayed arrival during settle window + full browser regressions.
  - `LAN-20260930-53` (`Saved/StagedBuilds/Windows/candidate-lan20260930-53.json`, 131 staged artifacts): Passed rejected-travel host retry & client waiting state (36 controls).

## 2. Production Architecture & Invariants

- **Settle-Window Late Arrival Cancellation (`UBwayHeroSelectionPhaseComponent`):**
  - Keeps `OnPlayerStateAdded` registration open while `bTravelPending` is active during the 5-second post-lock settle countdown.
  - If a new human player joins before `ExecuteArenaTravel` fires, cancels `TravelTimerHandle`, clears `bTravelPending`, and requires the newcomer to select and lock a hero before restarting the travel countdown.
  - Revalidates `GameState->PlayerArray` inside `ExecuteArenaTravel` immediately before calling `World->ServerTravel`.
- **Rejected-Travel Host Recovery Latch:**
  - If `World->ServerTravel` returns `false`, latches `bTravelFailed` on `UBwayHeroSelectionPhaseComponent` and replicates the failure state so the host UI displays **Retry travel** (`ServerRetryArenaTravel`) while clients display **Waiting for host**.
- **In-Match Late-Join Selection Gate (`ABreakawayGameMode` & `UBwayRoundManagementComponent`):**
  - `ABreakawayGameMode::EnsurePlayerReadyForMatch` gates unlocked human newcomers in `PlayersAwaitingLateJoinHeroSelection` idempotently across `PostLogin` and `HandleStartingNewPlayer`, deferring pawn spawn until hero lock (`TryCompletePrematchAfterHeroLocks`).

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| 4-Process Packaged Settle-Window Late Arrival (200ms) | `Candidate51` (`LAN-20260930-51`, `SettleLateArrival`) | **PASS** — Newcomer cancels pending travel, locks hero, all 4 peers travel cleanly (`candidate51-late-arrival-summary.json`) |
| 4-Process Packaged Browser Regression Matrix (200ms) | `Candidate51` (`SelectionEdges`, `Connected`, `Rediscovery`) | **PASS** — Zero regressions across all 3 transport topologies (`candidate51-regressions-summary.json`) |
| 4-Process Packaged Rejected-Travel Recovery (200ms) | `Candidate53` (`LAN-20260930-53`, `RejectedTravelRetry`) | **PASS** — 36/36 controls; host **Retry travel** recovers & completes travel (`candidate53-travel-retry-summary.json`) |

## 4. Remaining Open Proof Gaps

- Physical mouse/gamepad UI button clicks and visual layout inspection at 1080p.
- Second-PC LAN hardware verification.

## 5. Archive Pointer

- Full chronological gate diary: [LATE_ARRIVAL_SELECTION_ACCEPTANCE.md](./Archive/Full_History_20261013/LATE_ARRIVAL_SELECTION_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
