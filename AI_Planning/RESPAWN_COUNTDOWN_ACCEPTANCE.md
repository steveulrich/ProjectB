# Respawn Countdown & HUD Lifecycle Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (Headless automation + 4-world PIE + 4-process packaged server-synchronized escalating respawn progression, deadline replication, HUD countdown display, menu/modal occlusion, round-reset cancellation, and direct late-join / browser-return lifecycle at 200ms lag).
- **Frozen Candidates:**
  - `LAN-20261019-01`: Exe SHA-256 `64FAB3BD1EC1F0C7FCA61CFFE0EC8F7DCC0D45F531F5F8AE2672E50E9064C338` (Goal 3 revalidated Dev `3/0/3` and Demo `10/5/25` asset/experience/cooked runtime progression, per-round reset, and `RespawnTimeReduction` composition at 200ms lag with delayed client3 at 12 FPS).
  - `LAN-20261014-02`: Exe SHA-256 `996E8F7DEBC4B7D1D9F333E423EF5D9844F4FF60C27E2BF3A72CCFF2DB0C347E` (earlier Goal 3 escalating progression run at 200ms lag).
  - `LAN-20260926-10`: Exe SHA-256 `6FD8A2E5CC6C1919DAD7B5532F96C4686B90B390294B9EB02219A0528393E670` (76 checks, 13 controls, 674 HUD samples at 200ms).
  - `LAN-20260926-12`: Exe SHA-256 `03D81F86EB36116C82F290DA19CD93242500A6F4A68A46177504BF70D673441C` (direct UI late-join 483 samples & travel/rehost lifecycle 2,653 samples).

## 2. Production Architecture & Invariants

- **Data-Driven Escalating Respawn Progression (`UBwayMatchFlowConfig` & `UBwayRoundManagementComponent`):**
  - Provisional Demo match rule (`DA_BW_MatchFlow_Demo`, used by `B_BW_Experience_CaptureTheRelic`, `B_BW_Experience_HeroSelectStaging`, and `B_BW_Experience_BallMode`): `BaseRespawnDelaySeconds = 10.0f`, `PerDeathRespawnIncrementSeconds = 5.0f`, `MaxRespawnDelaySeconds = 25.0f`, `bResetRespawnEscalationEachRound = true` (`10s` 1st death, `15s` 2nd, `20s` 3rd, `25s` 4th+ within the same round). (`FINAL_ALPHA_PARITY.md` does not establish the exact original progression, cap, or reset boundary.)
  - Dev match rule (`DA_BW_MatchFlow_Dev`, used by `B_BW_Experience_Dev`): fixed `3.0f / 0.0f / 3.0f` (`3s` constant respawn delay).
  - `UBwayRoundManagementComponent::ComputeEscalatedRespawnBaseDelay(int32 PriorDeathsInRound)` computes the escalated base delay; `PlayerRoundDeathCounts` and `ABwayPlayerState::DeathsInCurrentRound` reset to `0` on `StartNewRound`, `StartRound`, `ResetRoundState`, and `ResetForNextMatch`.
- **Replicated Respawn Schedule (`ABwayPlayerState::RespawnSchedule`):**
  - `FBwayRespawnSchedule` replicates `ServerEndTime` (in `GameState->GetServerWorldTimeSeconds()` space) and `RoundNumber`.
  - Set by `UBwayRoundManagementComponent::RefreshRespawnSchedule` on authority when a player dies during `Playing`, and cleared on respawn completion, round reset, or match end.
- **Respawn HUD Widget (`UBwayRespawnHUDWidget` & `ABwayPlayerController`):**
  - Created and owned by `ABwayPlayerController` for local players; polls `ServerEndTime - GetServerWorldTimeSeconds()` every `0.10s`.
  - Hit-test-invisible (`ESlateVisibility::HitTestInvisible`) so it never blocks mouse/gamepad input, and automatically hides whenever a `Menu` or `Modal` CommonUI layer (such as the Upgrade Shop or Hero Select) is active.
- **Late-Join Hero Select Self-Dismissal (`WBP_BW_HeroSelect`):**
  - Automatically deactivates once the local player has `bHeroLocked == true` and a valid possessed pawn during `Playing` or `PostRound`.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Headless Escalating Progression & Asset/Experience Audit | `Breakaway.Respawn.EscalatingProgression` | **PASS** — Dev (`3/0/3`) & Demo (`10/5/25`, `reset=true`) real assets, all 4 Breakaway experiences, `10s -> 15s -> 20s -> 25s` cap, Round 2 reset to `10s`, and `RespawnTimeReduction` composition (`Saved/Logs/goal3-respawn-travel-20261019/headless-goal3-2.json`) |
| 4-World PIE Escalating Progression & Seamless Travel (200ms) | `bway.Test.RespawnUpgrade` | **PASS** — `L_BW_HeroSelect_Staging` -> `L_BW_Dorado` with 8 PlayerStates & 4 locked humans/world, Round 1 `10s/15s` escalation + Round 2 `10s` reset (`eff=6s` at `40%`), 0 NetGUID/VSM warnings (`Saved/Logs/goal3-respawn-travel-20261019/pie-goal3-summary-2.json`) |
| 4-Process Packaged Escalating Progression & Rematch (200ms) | `LAN-20261019-01` | **PASS** — 2 D3D12 + 2 NullRHI (client3 at 12 FPS) at 200ms; cooked `DA_BW_MatchFlow_Demo 10.0/5.0/25.0 reset=true`, stage=8, 1283–1402 client samples, 0 errors/warnings (`Saved/Logs/goal3-respawn-travel-20261019/packaged/packaged-result.json`) |
| 4-World PIE Respawn Deadline & Countdown | `bway.Test.RespawnCountdown` | **PASS** — 73 checks, 984 HUD samples (`Saved/Logs/respawn-countdown-20260926/pie-countdown.json`) |
| 4-Process Packaged Countdown & Controls (200ms) | `Candidate10` (`LAN-20260926-10`) | **PASS** — 76 checks, 13 controls, 674 HUD samples (`candidate10-countdown-summary.json`) |
| 4-Process Packaged Late-Join & Browser Travel Lifecycle (200ms) | `Candidate12` (`LAN-20260926-12`) | **PASS** — 483 late-join samples + 2,653 travel/rehost samples (`candidate12-lifecycle-summary.json`) |

## 4. Remaining Open Proof Gaps

- Physical visual layout inspection at 1080p and second-PC LAN hardware verification.

## 5. Archive Pointer

- Full chronological gate diary: [RESPAWN_COUNTDOWN_ACCEPTANCE.md](./Archive/Full_History_20261013/RESPAWN_COUNTDOWN_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
