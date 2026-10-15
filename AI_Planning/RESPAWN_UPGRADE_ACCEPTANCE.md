# Respawn Time Reduction Upgrade Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (Economy + escalating respawn automation, 4-world PIE, and 4-process packaged remote purchase, live pending-death timer shortening across escalating `10s -> 15s` within-round delays, Round 2 reset to `10s` base (`6s` at `40%` reduction), and rematch reset at 200ms lag).
- **Frozen Candidate:** `LAN-20261014-02` (`Saved/StagedBuilds/LAN-20261014-02`, 13 hashed pak/executable entries; earlier `LAN-20260927-06` retained).
- **Executable Identity:** `LyraGame.exe` SHA-256 `996E8F7DEBC4B7D1D9F333E423EF5D9844F4FF60C27E2BF3A72CCFF2DB0C347E`.

## 2. Production Architecture & Invariants

- **Attribute & Upgrade Effect (`UBwayHeroAttributeSet` & `UBwayUpgradeEffect_RespawnTimeReduction`):**
  - `RespawnTimeReduction` replicated attribute clamped to `[0.0, 1.0]` (`0.0` default).
  - 6th catalog item in `DA_BW_DefaultUpgradeCatalog` (`RespawnTimeReduction`): 4 ranks granting `+10% / +20% / +30% / +40%` (`0.10` per rank) at provisional prices `75 / 125 / 175 / 225` gold while enforcing the 4-slot match equipment cap.
- **Escalating Progression Composition & Live Pending-Death Recalculation (`UBwayRoundManagementComponent::RefreshRespawnSchedule`):**
  - Bound to `RespawnTimeReduction` attribute changes on authority.
  - Computes `EscalatedBaseDelay = GetBaseRespawnDelayForDeathCount(DeathsInCurrentRound)` (`10s` 1st death, `15s` 2nd, `20s` 3rd, `25s` 4th+ within the round; reset to `10s` each new round) and stores it in `FPendingRespawn::BaseDelaySeconds`.
  - When a dead player purchases `RespawnTimeReduction` while waiting to respawn, recalculates `EffectiveDelay = BaseDelaySeconds * (1.0f - ClampedReduction)` from `FPendingRespawn::StartTime`, reschedules `TimerHandle` (clamping already-elapsed remaining time to a `0.05s` non-reentrant timer), and updates replicated `ABwayPlayerState::RespawnSchedule`.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Escalating Progression + Upgrade Automation | `Breakaway.Respawn.EscalatingProgression+Breakaway.Economy.RespawnUpgrade` | **PASS** — 2/2 suites pass (`10s/15s/20s/25s` progression + `10%–40%` live shortening + Round 2 reset; `Saved/Logs/goal3-respawn-progression04.json`) |
| 4-World PIE Escalating Live Timer Shortening | `bway.Test.RespawnUpgrade` | **PASS** — Death 1 (`10s -> 9s -> 8s`), Death 2 (`15s -> 10.5s -> 9s`), Round 2 Death 1 (`10s -> 6s` at 40%); 0 NetGUID/VSM warnings (`Saved/Logs/pie-goal3/pie-goal3-result.json`) |
| 4-Process Packaged Escalating Upgrade & Rematch Reset (200ms) | `LAN-20261014-02` | **PASS** — 2 D3D12 + 2 NullRHI at 200ms; stage=8, 1530–1582 client samples, sawPostRound=1, sawResults=1, sawRematch=1, 0 errors/warnings (`Saved/Logs/goal3-packaged-20261014/packaged-LAN-20261014-02-200ms-result.json`) |
| Earlier 4-Process Packaged Remote Purchase & Rematch Reset (200ms) | `Candidate06` (`LAN-20260927-06`) | **PASS** — 133 checks, 17 controls, 19,933 client rows (`candidate06-respawn-upgrade-summary.json`) |

## 4. Remaining Open Proof Gaps

- Original September 2017 reference confirmation for exact rank percentages and gold costs.
- Physical shop input while dead and second-PC LAN hardware verification.

## 5. Archive Pointer

- Full chronological gate diary: [RESPAWN_UPGRADE_ACCEPTANCE.md](./Archive/Full_History_20261013/RESPAWN_UPGRADE_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
