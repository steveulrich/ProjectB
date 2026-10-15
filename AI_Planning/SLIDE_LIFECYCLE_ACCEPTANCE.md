# Slide Movement & Lifecycle Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED — VERTICAL SLICE 1.0 SIGNED OFF** (Dorado ramp-to-flat slide transition tuned with `0` `boundedSeparatedTraversal` air-state pops across all `SlideGroundAirDiagnostic` cases, `11/11` diagnostic controls passed, `4/4` 4-world PIE `SlideTrajectoryPIE` sustained trajectory cases passed, `20/20` `Breakaway.CombatPolish.Slide` automation tests passed, and 4-process / 1080p packaged `LAN-20261015-01` 4-hero slide lifecycle verified).
- **Frozen Candidates & Compile Provenance:**
  - `LAN-20261015-01`: Exe SHA-256 `A0D887766ED66CC9A0C63F434302D272EBCE28AF3B3BB029B3899DE304D3AEB6` (`Saved/Logs/goal4-20261015/compile-goal4-editor05.json`, `compile-goal4-game02.json`, `package-verify01.json`; packages all Goal 4 Dorado ramp-to-flat slide physics & jump-cancel transitions).
  - `LAN-20260930-50`: `Saved/StagedBuilds/Windows/candidate-lan20260930-50.json` (4-process packaged `AuthoredLowSpeed`, `LowSpeed`, `Release`, `Jump` at 100ms lag).

## 2. Production Architecture & Invariants

- **Custom Movement Mode (`UBwayCharacterMovementComponent` — `MOVE_Custom` / `CMOVE_Slide`):**
  - Authored `MinSlideSpeed = 0.0f` allows standing/low-speed slide entry using `SlideInitialBoost`, decaying smoothly via `SlideFriction` while applying slope-parallel gravity acceleration on downhill ramps.
  - **Single-Timestep Gravity Invariant:** `PhysSliding` integrates airborne downward displacement once per substep without double-integrating `Velocity.Z` across `SlideAlongSurface`, matching `MOVE_Falling` descent (`240.32 cm` vs `245.25 cm` over `0.5s`).
  - **Dorado Ramp-to-Flat Step-Down & Crest Ground Snap (`UBwayCharacterMovementComponent::PhysSliding`):**
    - Projects the horizontal step and calls `FindFloor` with `FMath::Max(MaxStepHeight, 24.0f)` when transitioning from a walkable floor (`bHadWalkableFloor`) across a downhill ramp-to-flat transition or crest drop (`<= 24.0cm`), snapping cleanly to the walkable floor within `MAX_FLOOR_DIST` (`1.9f–2.4f cm`) without air-state pops or horizontal velocity loss (`boundedSeparatedTraversal = 0`).
  - **Slide-Jump Immediate Airborne Clearance (`UBwayCharacterMovementComponent::DoJump` & `UBwayGameplayAbility_Slide::OnMovementModeChanged`):**
    - When jumping out of a slide (`bWasSliding`), applies a `4.0cm` vertical clearance step before switching to `MOVE_Falling` and immediately ends `UBwayGameplayAbility_Slide` on `MOVE_Falling` transition so `State.Movement.Sliding` clears on the jump frame (`slideJumpAirborne = 183`, `invalidState = 0`).
  - **MoveSpeed Replication Invariant:** `ABwayCharacterWithAbilities::UpdateMovementSpeedFromAttributes` binds `GetGameplayAttributeValueChangeDelegate(MoveSpeedRating)` and `MoveSpeedMultiplier` on all net roles so simulated proxies compute identical `MaxWalkSpeed`.
- **Camera Shake Pool Exclusion (`ABwayCharacterWithAbilities::ActiveSlideCameraShake`):**
  - Stores a transient `TObjectPtr<UCameraShakeBase>` started when `State.Movement.Sliding` is added and calls `StopShake(..., true)` without `ReleaseToPool` when removed, preventing pooled shake reuse assertions.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| 4-Process Packaged Slide Lifecycle (100ms Lag) | `Candidate50` (`LAN-20260930-50`, `AuthoredLowSpeed`, `LowSpeed`, `Release`, `Jump`) | **PASS** — 4/4 packaged subcases across 4 peers (`Saved/Logs/slide-20260930/package50-summary.json`) |
| 20-Test Slide Physics & Camera Shake Automation | `Breakaway.CombatPolish.Slide` (`automation-goal4-slide02.json`) | **PASS** — `20/20` tests (gravity parity, wall brake, ramp support, camera shake, proxy speed) (`Saved/Logs/goal4-20261015/automation-goal4-slide02.json`) |
| 4-World PIE Dorado Ground/Air Diagnostic & Controls | `bway.Test.SlideGroundAirDiagnostic` (`ground-air-diag01.json`, `ground-air-controls01.json`) | **PASS** — `contactNearGround: 1480`, **`boundedSeparatedTraversal: 0`**, `slideJumpAirborne: 183`, `invalidState: 0`, `cleanup.proved: true`, and `11/11` negative/positive controls passed (`Saved/Logs/goal4-20261015/ground-air-diag01.json`) |
| 4-World PIE Dorado Sustained Slide Trajectory | `bway.Test.SlideTrajectoryPIE` (`trajectory-pie01.json`) | **PASS** — `4/4` cases (`DoradoFlat`, `DoradoDownhill`, `Release`, `Jump`) across 4 peers, `firstCausalError: null` (`Saved/Logs/goal4-20261015/trajectory-pie01.json`) |
| Packaged 1080p All-4-Hero Shift Slide Verification | `LAN-20261015-01` (`bway.Test.Goal4FullFlowAudit`) | **PASS** — `Argus`, `Alona`, `Korryn`, and `Rawlins` Shift Slide activation and clean release verified (`Saved/Logs/goal4-20261015/goal4-fullflow-1080p-summary.json`) |

## 4. Remaining Open Proof Gaps

- None for Vertical Slice 1.0 C++, PIE, automation, and packaged 1080p acceptance; optional human keyboard/gamepad feel sign-off checklist provided in Goal 4 artifact.

## 5. Archive Pointer

- Full chronological gate diary: [SLIDE_LIFECYCLE_ACCEPTANCE.md](./Archive/Full_History_20261013/SLIDE_LIFECYCLE_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
