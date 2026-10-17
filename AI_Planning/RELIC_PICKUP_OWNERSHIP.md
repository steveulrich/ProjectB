# Relic Pickup Ownership, Ballistics & Interception Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Current corrective gate:** `relic-flight-20261016` **passes its scoped charged-throw and landed-pass checks**: Editor06/Game01 builds, 7/7 headless tests, reviewed four-world PIE, and four-process packaged transport at 100 ms lag. The strict packaged check has zero errors or critical lines. PIE retains two narrowly classified Pixel Streaming errors. This is not physical-input, rendered/UI, or second-PC LAN sign-off.
- **Historical verdict:** **PASSED** for the recorded direct-call ownership/ballistics fixtures, 4-world PIE, and packaged LAN verification at `100ms` / `200ms` lag.
- **Frozen candidate:** `LAN-20261016-01` (`Saved/StagedBuilds/LAN-20261016-01`, `Saved/Logs/relic-flight-20261016/package-result.json`). Its source archive, immutable package snapshot, and 131-file payload manifest bind the tested artifact; later Goal 1 changes are outside this candidate.
- **Executable identity:** `LyraGame.exe` SHA-256 `2BF06E652338A41275CB1CAF92382922A2F57DEA8E8B6E4CA7DCE6AF6EE7B62C` (`Saved/Logs/relic-flight-20261016/compile-game01-bound.json`).

## 2. Production Architecture & Invariants

- **Single-Carrier Authority Commit & Drop Cooldown (`ARelicActor::OnPickedUp`, `CanBePickedUp`, `PickupCooldownAfterDrop`):**
  - Authority commits `CurrentCarrier` and `ERelicState::Carried` before attachment or notification, preventing synchronous attachment overlap callbacks from triggering reentrant or competing pickups.
  - `DropFromCarrierInternal` records `LastDroppedCarrier` and `LastDropWorldTime` with a weak-lambda expiry timer so a fumbling/dropping carrier is blocked from immediate self-reacquisition during `URelicSettings::PickupCooldownAfterDrop` (`0.5s`), while teammates and defenders can acquire immediately.
- **Single Ability-Set Grant & Clean Release (`AS_BW_BallAbilities`):**
  - Authority grants `AS_BW_BallAbilities` (3 ability-set grants, including native `UBwayGameplayAbility_RelicThrow`, + 1 native `GA_BW_PassRelic` = `4` total specs) exactly once per pickup via `AttachToCarrier`; `DetachFromCarrier` revokes all 4 specs and removes `State.Relic.Carrying` across all peers on pass, throw, drop, fumble, or goal score.
- **Charged Ballistic Throws, Directed Passes & Prepared Catch (`ARelicActor`, `URelicSettings`, `State.Relic.PreparingCatch`):**
  - Charged throws interpolate speed across `[MinThrowSpeed=800, MaxThrowSpeed=2000]` over `MaxChargeTime=1.5s`; directed passes launch at `PassSpeed=1500` toward the selected teammate with `CurrentState = ERelicState::BeingPassed`, `bIsPassInFlight = true`, and `PassingTeamIndex`.
  - When an overlapping receiver holds `State.Relic.PreparingCatch` (or receives a pass/throw while eligible), `OnCollisionSphereBeginOverlap` zeroes linear/angular velocity (`StopMovementImmediately`) before `OnPickedUp` so high-velocity passes/throws are cleanly secured without physics bounce-off or duplicate ability grants.
- **Mid-Air Pass Interception (`ABwayPlayerState::Interceptions`):**
  - When an opposing player (`CatcherTeam != PassingTeamIndex`) catches/overlaps the Relic while `bIsPassInFlight` (`ERelicState::BeingPassed` / `ERelicState::Passed`), authority awards possession to the defender, increments `ABwayPlayerState::Interceptions` by `+1`, and populates `FBwayPlayerMatchSummary::Interceptions` in `FBwayPostMatchSummaryData`.
- **Combat Fumble Thresholds & Hard-CC Forced Fumbles (`ABwayPlayerState::ForcedFumbles`):**
  - Single hits or cumulative carrier damage meeting `URelicSettings::FumbleDamageThreshold` (`50.0`), as well as hard-CC/knockback/silence/jail tags (`State.Status.Stunned`, `State.Status.KnockedBack`, `State.Status.Rooted`, `State.Status.Silenced`, `State.Status.Jailed`), force an immediate `DropRelic` with `FumbleImpulseStrength` (`600.0`) and increment the attacker's `ABwayPlayerState::ForcedFumbles` by `+1`.

## 3. Historical Verified Scope

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Headless Ownership & Ballistics/Interception Automation | `Breakaway.Relic.OwnershipLifecycle` + `Breakaway.Relic.BallisticsAndInterception` | **PASS** — 11/11 suite (`Saved/Logs/automation-goal2-relic-base-run3.json`) |
| 4-World PIE Ballistics, Prepared Catch, Interception & Fumbles | `bway.Test.RunGoal2Gate` (4 PIE worlds) | **PASS** — `interceptsP1=1 summaryIntercepts=1 fumblesP0=1 fumblesP2=1 totalFumbles=4 summaryFumbles=4` (`Saved/Logs/pie-goal2/pie-goal2-result.json`) |
| 4-Process Packaged 4-Owner Pass/Throw/Goal Lifecycle (200ms) | `Candidate07` (`LAN-20260926-07`, 23 stages) | **PASS** — 37 checks, 48 controls, 56 peer observations (`packaged-ownership02-verification.json`) |
| 4-Process Packaged Ballistics, Interception & Forced Fumbles (100ms, 2 D3D12 + 2 NullRHI) | `Candidate01` (`LAN-20261014-01`, `bway.Test.RunGoal2Gate`) | **PASS** — Host + 3 clients converged on `interceptsP1=1`, `totalFumbles=4`, `0` error/critical lines (`Saved/Logs/goal2-packaged-LAN-20261014-01-100ms-r2-result.json`) |

## 4. Remaining Open Proof Gaps

- Physical human keyboard/mouse/gamepad playtest (Goal 4) and second-PC LAN hardware verification.

## Charged throw and landed-pass corrective gate

- Contract and evidence: `Saved/Logs/relic-flight-20261016/gate-contract.json`. Frozen candidate `LAN-20261016-01` passes this corrective gate; it does not establish unrelated Goal 1 completion. These results describe the frozen candidate; they do not assert full-source equivalence to this selective commit.
- Current compiled/headless evidence: `compile-editor06-bound.json` and `automation03.json` in the gate directory pass with empty compile failure signals, 7/7 tests, and framework/process exit 0. Coverage includes authored assets, preservation of the original throw HUD icon, measured Chaos velocity/displacement, cancellation, and real pass impacts.
- PIE02 gameplay evidence: five remote-owner scenarios, three authoritative launches, 64 settled peer samples, and four completion witnesses pass. Ground recovery adds zero interceptions; the airborne catch adds one. `pie02-check.json` retains the strict failed verdict for two exact `LogPixelStreaming2RTC` target-window errors. `pie02-reviewed-result.json` revision 2 passes the native synthetic-input/RPC/physics scope with zero unclassified errors or critical lines; no Pixel Streaming, physical/UI, or packaged error is waived. Its initial closure-check failure remains in `pie02-reviewed-result-initial-closure-check.json`: the reviewer required a nonexistent log footer. Actual shutdown/exit markers and no remaining owned processes establish normal closure. `pie02-cleanup.json` records that closure and restored temporary settings.
- Packaged evidence: `transport01/check.json` passes the same five scenarios, three launches, 64 settled samples, four witnesses, and zero/one interception controls at 100 ms lag. `transport01/transport.json` records a D3D12 listen host and remote owner plus two NullRHI witnesses. All three clients exited with code 0; the harness force-terminated the owned host only after client completion. No residual Unreal process remained. All 148 warnings are retained; errors and critical lines are zero. The package's 131 payload hashes matched before transport and in the authorized post-run retry (`post-transport-hash-audit01.json`): all hashes and sizes match, with zero mismatches. Seven additional `Saved` configuration files are inventoried separately, not treated as original payload.
- Independent `transport01/reviewed-result.json` confirms actual RHI metadata, 100 ms lag on each peer, and all five input scenarios on the rendered remote owner. It preserves warning groups, including staging-travel, duplicate-component, and movie-window warnings; none establish visual or physical-input acceptance.
- Retained failed attempt: `pie01-full-check.json` records missing synthetic-input device routing, rejected by platform-user filtering before throw activation. The fixture now validates the owning user's device for every press/release. The user approved one additional editor build; Editor06 and PIE02 validate that correction. `pie01-cleanup.json` preserves the failed attempt's cleanup rather than relabeling it as passed.
- Input acceptance: the authored `InputTag.Relic.Throw` grant and left-mouse mappings reach the native ability; short, full, and overlong holds release once with clamped speeds. Only throw opts into the `Started` press edge; other abilities retain their existing `Triggered` behavior. Cancellation and ownership loss must not restart a charge while the same input remains held or leave duplicate grants after reacquisition.
- Pass acceptance: actual floor/wall physics contact clears pass-flight metadata before an opponent's grounded recovery, which earns no interception. An opposing airborne catch still earns exactly one. Authority, remote owner, and both other clients must converge on state, grants, and interception totals.
- Required proof: affected headless regressions, four-world PIE, and a fresh four-process packaged run at 100 ms lag with a rendered host/active owner and two NullRHI witnesses. Native input injection proves the instrumented input path, not physical keyboard/gamepad feel. Second-PC LAN and human controls remain separate acceptance.
- This corrective gate does not advance the master goal queue. Independently authorized Goal 1 work retains its own contract and evidence.

## 5. Archive Pointer

- Full chronological gate diary: [RELIC_PICKUP_OWNERSHIP.md](./Archive/Full_History_20261013/RELIC_PICKUP_OWNERSHIP.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
