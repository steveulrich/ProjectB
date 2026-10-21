# Slide Movement & Lifecycle Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** The reviewed held-jump and single-frame concave fixes have headless proof and a scoped four-world PIE05 reanalysis pass at 100ms. Compatible cooked validation remains pending. Historical slide automation, trajectory, and packaged diagnostics retain their original scopes; no physical-input or final slice sign-off is implied.
- **Frozen Candidates & Compile Provenance:**
  - `LAN-20261015-01`: Exe SHA-256 `A0D887766ED66CC9A0C63F434302D272EBCE28AF3B3BB029B3899DE304D3AEB6` (`Saved/Logs/goal4-20261015/compile-goal4-editor05.json`, `compile-goal4-game02.json`, `package-verify01.json`; packages all Goal 4 Dorado ramp-to-flat slide physics & jump-cancel transitions).
  - `LAN-20260930-50`: `Saved/StagedBuilds/Windows/candidate-lan20260930-50.json` (4-process packaged `AuthoredLowSpeed`, `LowSpeed`, `Release`, `Jump` at 100ms lag).

## 2. Production Architecture & Invariants

- **Custom Movement Mode (`UBwayCharacterMovementComponent` — `MOVE_Custom` / `CMOVE_Slide`):**
  - Authored `MinSlideSpeed = 0.0f` allows standing/low-speed slide entry using `SlideInitialBoost`, decaying smoothly via `SlideFriction` while applying slope-parallel gravity acceleration on downhill ramps.
  - **Single-Timestep Gravity Invariant:** `PhysSliding` integrates airborne downward displacement once per substep without double-integrating `Velocity.Z` across `SlideAlongSurface`, matching `MOVE_Falling` descent (`240.32 cm` vs `245.25 cm` over `0.5s`).
  - **Dorado Ramp-to-Flat Step-Down & Crest Ground Snap (`UBwayCharacterMovementComponent::PhysSliding`):**
    - Uses the movement component's `FindFloor` result after movement. With prior ground contact and a floor normal Z of at least `0.80`, a gap above `MAX_FLOOR_DIST` and at most `MaxStepHeight` triggers a downward sweep toward `MIN_FLOOR_DIST`. Floor transitions preserve surface speed; additional movement uses only the time left after all `SlideAlongSurface` sweeps.
  - **Slide-jump transition (`UBwayCharacterMovementComponent::DoJump` and `UBwayGameplayAbility_Slide::OnMovementModeChanged`):**
    - The inherited jump applies vertical velocity and switches to `MOVE_Falling`; the movement callback ends the local slide ability. The horizontal momentum boost applies on the first successful jump frame, including a recovered slide-jump state, but does not compound while jump remains held. There is no separate `4.0cm` clearance step.
  - **MoveSpeed Replication Invariant:** `UBwayCharacterMovementComponent::BindAbilitySystem` binds attribute-change delegates for `MoveSpeedRating` and `MoveSpeedMultiplier`. It updates `MaxWalkSpeed` from the rating; `GetMaxSpeed` applies the multiplier.
- **Camera Shake Pool Exclusion (`ABwayCharacterWithAbilities::ActiveSlideCameraShake`):**
  - Tracks the local owner's shake while `IsSliding()` is true. An initializer excludes the shake from the engine's expired-instance pool. Normal slide exit blends it out; teardown and owner changes stop it immediately through `APlayerCameraManager::StopCameraShake`.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| 4-Process Packaged Slide Lifecycle (100ms Lag) | `Candidate50` (`LAN-20260930-50`, `AuthoredLowSpeed`, `LowSpeed`, `Release`, `Jump`) | **PASS** — 4/4 packaged subcases across 4 peers (`Saved/Logs/slide-20260930/package50-summary.json`) |
| 20-Test Slide Physics & Camera Shake Automation | `Breakaway.CombatPolish.Slide` (`automation-goal4-slide02.json`) | **PASS** — `20/20` tests (gravity parity, wall brake, ramp support, camera shake, proxy speed) (`Saved/Logs/goal4-20261015/automation-goal4-slide02.json`) |
| 4-World PIE Dorado Ground/Air Diagnostic & Controls | `bway.Test.SlideGroundAirDiagnostic` (`ground-air-diag01.json`, `ground-air-controls01.json`) | **PASS** — `contactNearGround: 1480`, **`boundedSeparatedTraversal: 0`**, `slideJumpAirborne: 183`, `invalidState: 0`, `cleanup.proved: true`, and `11/11` negative/positive controls passed (`Saved/Logs/goal4-20261015/ground-air-diag01.json`) |
| 4-World PIE Dorado Sustained Slide Trajectory | `bway.Test.SlideTrajectoryPIE` (`trajectory-pie01.json`) | **PASS** — `4/4` cases (`DoradoFlat`, `DoradoDownhill`, `Release`, `Jump`) across 4 peers, `firstCausalError: null` (`Saved/Logs/goal4-20261015/trajectory-pie01.json`) |
| Packaged 1080p All-4-Hero Shift Slide Verification | `LAN-20261015-01` (`bway.Test.Goal4FullFlowAudit`) | **PASS** — `Argus`, `Alona`, `Korryn`, and `Rawlins` Shift Slide activation and clean release verified (`Saved/Logs/goal4-20261015/goal4-fullflow-1080p-summary.json`) |

## 4. Remaining Open Proof Gaps

- The first October 16 concave fixture reached zero required branches; its [failed result](../Saved/Logs/goal1-review-20261016/slide-headless01-brief.json) remains preserved. The corrected fixture passed actual branches at 30/60/120 Hz on Editor6, and Editor8's [independent 10-test run](../Saved/Logs/goal1-review-20261016/automation-editor08-independent.json) passed both `HeldJumpMomentum` and `ConcaveTimeBudget`.
- Slide PIE03 reached held jump but failed because legacy `DoJump` callbacks report zero delta time. The [failed verification](../Saved/Logs/goal1-review-20261016/slide-pie03-check.json) also rejected setup teleport corrections without bounded relocation evidence. Cleanup acknowledged all four peers and restored the authority transform; concave PIE was not reached. These observations do not establish a multiplayer pass.
- Reviewed [fixture repairs](../Saved/Logs/goal1-review-20261016/integrated-fixtures-editor09.json) compiled successfully in Editor9. Nine of nine approved Editor builds are used; see the [contract](../Saved/Logs/goal1-review-20261016/gate-contract.json).
- [Slide PIE05 reanalysis](../Saved/Logs/goal1-review-20261016/drafts-slide-pairing-editor09/pie05-reanalysis.json) passes both cases at native-observed 100ms: 16 held-jump and 14 concave timestamp pairs, with settled proxies and cleanup across all four worlds. All 28 correction records remain counted; 20 match bounded intentional setup relocations. The later cleanup capture found zero probes in all four worlds.
- Preserve the [original PIE05 verifier rejection](../Saved/Logs/goal1-review-20261016/slide-pie05-check.json): `Insufficient timestamp-paired moving-state evidence`. The [reviewed correction](../Saved/Logs/goal1-review-20261016/drafts-slide-pairing-editor09/manifest-final.json) admits same-case phase5/6 concave samples strictly between each peer's seed and proof. Every matched pair, including stationary samples, must satisfy the unchanged position/velocity limits; only moving pairs count toward the existing minimum count/span. All 96 verifier controls pass. Reanalysis uses the same immutable runtime bytes and adds no PIE attempt.
- This synthetic, seeded collision fixture does not establish an authored concave run-up, physical input, visual feel, separate-PC LAN, or historical trajectory parity. A compatible packaged regression remains required.
- Physical KBM/gamepad feel and final integrated presentation remain part of current Goals 4–9; the old Goal 4 observation probe cannot close them.

## 5. Archive Pointer

- Full chronological gate diary: [SLIDE_LIFECYCLE_ACCEPTANCE.md](./Archive/Full_History_20261013/SLIDE_LIFECYCLE_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
