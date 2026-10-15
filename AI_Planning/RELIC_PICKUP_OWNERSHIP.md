# Relic Pickup Ownership, Ballistics & Interception Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (Headless `Breakaway.Relic.OwnershipLifecycle` + `Breakaway.Relic.BallisticsAndInterception` automation, 4-world PIE, and 4-process packaged LAN verification at `100ms` / `200ms` lag).
- **Frozen Candidate:** `LAN-20261014-01` (`Saved/StagedBuilds/LAN-20261014-01`, `Saved/Logs/codex-packaged-LAN-20261014-01/package-result.json`).
- **Executable Identity:** `LyraGame.exe` SHA-256 `328B5BD779CFFFD95194773FF0A0BF12F47A78888CA78A6B2EC4AD091DE83B19` (`Saved/Logs/compile-goal2-game.json`).

## 2. Production Architecture & Invariants

- **Single-Carrier Authority Commit & Drop Cooldown (`ARelicActor::OnPickedUp`, `CanBePickedUp`, `PickupCooldownAfterDrop`):**
  - Authority commits `CurrentCarrier` and `ERelicState::Carried` before attachment or notification, preventing synchronous attachment overlap callbacks from triggering reentrant or competing pickups.
  - `DropFromCarrierInternal` records `LastDroppedCarrier` and `LastDropWorldTime` with a weak-lambda expiry timer so a fumbling/dropping carrier is blocked from immediate self-reacquisition during `URelicSettings::PickupCooldownAfterDrop` (`0.5s`), while teammates and defenders can acquire immediately.
- **Single Ability-Set Grant & Clean Release (`AS_BW_BallAbilities`):**
  - Authority grants `AS_BW_BallAbilities` (3 authored abilities + 1 native `GA_BW_PassRelic` = `4` total specs) exactly once per pickup via `AttachToCarrier`; `DetachFromCarrier` revokes all 4 specs and removes `State.Relic.Carrying` across all peers on pass, throw, drop, fumble, or goal score.
- **Charged Ballistic Throws, Directed Passes & Prepared Catch (`ARelicActor`, `URelicSettings`, `State.Relic.PreparingCatch`):**
  - Charged throws interpolate speed across `[MinThrowSpeed=800, MaxThrowSpeed=2000]` over `MaxChargeTime=1.5s`; directed passes launch at `PassSpeed=1500` toward the selected teammate with `CurrentState = ERelicState::BeingPassed`, `bIsPassInFlight = true`, and `PassingTeamIndex`.
  - When an overlapping receiver holds `State.Relic.PreparingCatch` (or receives a pass/throw while eligible), `OnCollisionSphereBeginOverlap` zeroes linear/angular velocity (`StopMovementImmediately`) before `OnPickedUp` so high-velocity passes/throws are cleanly secured without physics bounce-off or duplicate ability grants.
- **Mid-Air Pass Interception (`ABwayPlayerState::Interceptions`):**
  - When an opposing player (`CatcherTeam != PassingTeamIndex`) catches/overlaps the Relic while `bIsPassInFlight` (`ERelicState::BeingPassed` / `ERelicState::Passed`), authority awards possession to the defender, increments `ABwayPlayerState::Interceptions` by `+1`, and populates `FBwayPlayerMatchSummary::Interceptions` in `FBwayPostMatchSummaryData`.
- **Combat Fumble Thresholds & Hard-CC Forced Fumbles (`ABwayPlayerState::ForcedFumbles`):**
  - Single hits or cumulative carrier damage meeting `URelicSettings::FumbleDamageThreshold` (`50.0`), as well as hard-CC/knockback/silence/jail tags (`State.Status.Stunned`, `State.Status.KnockedBack`, `State.Status.Rooted`, `State.Status.Silenced`, `State.Status.Jailed`), force an immediate `DropRelic` with `FumbleImpulseStrength` (`600.0`) and increment the attacker's `ABwayPlayerState::ForcedFumbles` by `+1`.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Headless Ownership & Ballistics/Interception Automation | `Breakaway.Relic.OwnershipLifecycle` + `Breakaway.Relic.BallisticsAndInterception` | **PASS** — 11/11 suite (`Saved/Logs/automation-goal2-relic-base-run3.json`) |
| 4-World PIE Ballistics, Prepared Catch, Interception & Fumbles | `bway.Test.RunGoal2Gate` (4 PIE worlds) | **PASS** — `interceptsP1=1 summaryIntercepts=1 fumblesP0=1 fumblesP2=1 totalFumbles=4 summaryFumbles=4` (`Saved/Logs/pie-goal2/pie-goal2-result.json`) |
| 4-Process Packaged 4-Owner Pass/Throw/Goal Lifecycle (200ms) | `Candidate07` (`LAN-20260926-07`, 23 stages) | **PASS** — 37 checks, 48 controls, 56 peer observations (`packaged-ownership02-verification.json`) |
| 4-Process Packaged Ballistics, Interception & Forced Fumbles (100ms, 2 D3D12 + 2 NullRHI) | `Candidate01` (`LAN-20261014-01`, `bway.Test.RunGoal2Gate`) | **PASS** — Host + 3 clients converged on `interceptsP1=1`, `totalFumbles=4`, `0` error/critical lines (`Saved/Logs/goal2-packaged-LAN-20261014-01-100ms-r2-result.json`) |

## 4. Remaining Open Proof Gaps

- Physical human keyboard/mouse/gamepad playtest (Goal 4) and second-PC LAN hardware verification.

## 5. Archive Pointer

- Full chronological gate diary: [RELIC_PICKUP_OWNERSHIP.md](./Archive/Full_History_20261013/RELIC_PICKUP_OWNERSHIP.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
