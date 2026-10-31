# Respawn Countdown & HUD Lifecycle Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (Headless automation + 4-world 1080p PIE + 4-process packaged server-synchronized escalating respawn progression, deadline replication, local respawn HUD & 4v4 top-bar portrait respawn countdowns, broadcast scorebug round-win pips/territory possession, menu/modal occlusion, replacement-pawn rebind, round-reset cancellation, and direct late-join / browser-return lifecycle at 200ms lag).
- **Frozen Candidates:**
  - `LAN-20261030-01`: Exe SHA-256 `5CCCB439FE397CA31D07967C95D0E199AA3A5F3C54B4A3C0F3616C640ED98147` (Goal 6 broadcast scorebug `PointsToWin` pips, `240s` Demo server-clock round timer, arena territory possession, and 4v4 Friendly Blue / Enemy Red top-bar portrait live respawn countdown timers, gold Relic carrier frame/badge, death overlay, and immediate respawn/round-start reset at `200ms` lag across `2 D3D12 + 2 NullRHI`).
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
- **4v4 Top-Bar Team Portrait Respawn Timers & Broadcast Scorebug (`UBwayTeamPortraitsHUDWidget`, `UBwayTeamPortraitSlotWidget`, `UBwayCaptureTheRelicScoreWidget`, `UBwayRelicStatusWidget`):**
  - `FBwayTeamPortraitSlotViewModel` binds `bIsWaitingToRespawn` and clamped `RespawnRemainingSeconds` (`FMath::Max(0.0f, ServerEndTime - ServerNow)`) from `ABwayPlayerState::GetRespawnSchedule()`, refreshed every `0.10s` and immediately on `ABwayPlayerState::OnRespawnScheduleChanged` / round-state transitions.
  - `UBwayTeamPortraitSlotWidget` displays Friendly (`Blue`) vs Enemy (`Red`) team framing, hero portrait, gold Relic carrier frame + `RELIC` badge (`bHasRelic`), dark `Image_DeathOverlay`, and centered `Text_RespawnTimer` whole-second countdown strictly while `!bIsAlive && bIsWaitingToRespawn && RespawnRemainingSeconds > 0`, resetting immediately on respawn or round start with zero stale or negative timers.
  - `UBwayCaptureTheRelicScoreWidget` binds authoritative `PointsToWin` (`3` pips in Demo) and clamped `240s` server-clock round countdown; `UBwayRelicStatusWidget` combines carrier possession with `UBwayMidfieldRulesLibrary::GetRelicHalfFromRelic` arena territory readout.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Goal 6 Headless HUD, Portrait Respawn & Economy Suite | `Breakaway.UI.MatchHUDAndPortraits` + `FrontEnd` + `Respawn` + `Economy` | **PASS** — `17/17` tests (`0` errors, `0` warnings) + `4/4` post-broadcast-parity HUD tests verifying portrait respawn timers, `5px` mini health bars, expiry/reset clamping, zero-padded `04:00` scorebug pips/timer, hexagonal `{1, Mouse, F, R, Q, E}` ability overlays, KBM/gamepad label rebuilds, health/armor/gold/shop pulse, broadcast overlay (`UPGRADES [B]`, crosshair, `INTERCEPTION!` toast), and 8-player MVP/tie/disconnect reconciliation (`Saved/Logs/hud-parity-20261030/automation-hud-parity-03.json`, `Saved/Logs/goal6-hud-20261029/automation-goal6-hud05.json`) |
| Goal 6 4-World 1080p PIE Match HUD & Broadcast Parity Audit | `Run-Goal6PIEAudit.ps1` (`bway.Test.Goal6RuntimeAudit`) | **PASS** — `4` PIE worlds at `1920x1080`, Stages `1..6` across live broadcast HUD & dead portrait respawn timers, stadium minimap, event toast, crosshair, near-goal `UPGRADES [B]` prompt, hexagonal ability pedestal, 8-player Tab Scoreboard, all 3 PostRound win banners, frozen PostMatch Results, Host Rematch reset, and `20` Host/Client 1080p PNGs (`Saved/Logs/goal6-hud-20261029/pie-hud-parity-02-summary.json`, `pie-goal6-03-summary.json`) |
| Goal 6 4-Process Packaged Match HUD & Portrait Respawn (200ms) | `Run-Goal6PackagedTransport.ps1` (`LAN-20261030-01`, `hud04`) | **PASS** — `2 D3D12 (640x360 30 FPS) + 2 NullRHI (30 FPS)` at `-PktLag=100` (`200ms` RTT), Stages `1..6`, real mid-match client disconnect reconciliation, frozen PostMatch stats, Host Rematch reset, `0` errors/ensures, and `10` D3D12 Host/Client PNGs (`Saved/Logs/goal6-hud-20261029/goal6-LAN-20261030-01-hud04/result.json`) |
| Headless Escalating Progression & Asset/Experience Audit | `Breakaway.Respawn.EscalatingProgression` | **PASS** — Dev (`3/0/3`) & Demo (`10/5/25`, `reset=true`) real assets, all 4 Breakaway experiences, `10s -> 15s -> 20s -> 25s` cap, Round 2 reset to `10s`, and `RespawnTimeReduction` composition (`Saved/Logs/goal3-respawn-travel-20261019/headless-goal3-2.json`) |
| 4-World PIE Escalating Progression & Seamless Travel (200ms) | `bway.Test.RespawnUpgrade` | **PASS** — `L_BW_HeroSelect_Staging` -> `L_BW_Dorado` with 8 PlayerStates & 4 locked humans/world, Round 1 `10s/15s` escalation + Round 2 `10s` reset (`eff=6s` at `40%`), 0 NetGUID/VSM warnings (`Saved/Logs/goal3-respawn-travel-20261019/pie-goal3-summary-2.json`) |
| 4-Process Packaged Escalating Progression & Rematch (200ms) | `LAN-20261019-01` | **PASS** — 2 D3D12 + 2 NullRHI (client3 at 12 FPS) at 200ms; cooked `DA_BW_MatchFlow_Demo 10.0/5.0/25.0 reset=true`, stage=8, 1283–1402 client samples, 0 errors/warnings (`Saved/Logs/goal3-respawn-travel-20261019/packaged/packaged-result.json`) |
| 4-World PIE Respawn Deadline & Countdown | `bway.Test.RespawnCountdown` | **PASS** — 73 checks, 984 HUD samples (`Saved/Logs/respawn-countdown-20260926/pie-countdown.json`) |
| 4-Process Packaged Countdown & Controls (200ms) | `Candidate10` (`LAN-20260926-10`) | **PASS** — 76 checks, 13 controls, 674 HUD samples (`candidate10-countdown-summary.json`) |
| 4-Process Packaged Late-Join & Browser Travel Lifecycle (200ms) | `Candidate12` (`LAN-20260926-12`) | **PASS** — 483 late-join samples + 2,653 travel/rehost samples (`candidate12-lifecycle-summary.json`) |

## 4. Remaining Open Proof Gaps

- Physical KBM/gamepad input, four-human play, 1080p packaged performance sign-off, and second-PC LAN hardware verification (Goal 9).

## 5. Archive Pointer

- Full chronological gate diary: [RESPAWN_COUNTDOWN_ACCEPTANCE.md](./Archive/Full_History_20261013/RESPAWN_COUNTDOWN_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
