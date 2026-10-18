# Relic Pickup Ownership, Ballistics & Interception Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Current Goal 2 gate:** `goal2-relic-base-20261018` **PASSED** across `LyraEditor` (`6/6`), `LyraGame` (`1/2`), 6/6 headless automation suites (`Saved/Logs/goal2-relic-base-20261018/headless-goal2-2.json`), 4-world PIE at `100ms` lag (`Saved/Logs/goal2-relic-base-20261018/pie-goal2-5.log`: `BwayRelicFlight: COMPLETE pass=1` + `BwayGoal2GateTest: PIE PASS worlds=4`), and consolidated 4-process packaged transport at `100ms` lag (`2 D3D12 + 2 NullRHI`, `Saved/Logs/goal2-relic-base-20261018/packaged-summary.json`). Physical human UI/input observation and separate-machine LAN validation remain distinct from same-PC synthetic-input/process transport.
- **Prior corrective gate:** `relic-flight-20261016` passed its scoped charged-throw and landed-pass checks (`LAN-20261016-01`, `Saved/Logs/relic-flight-20261016/package-result.json`).
- **Frozen candidate:** `LAN-20261018-01` (`Saved/StagedBuilds/LAN-20261018-01`, `Saved/Logs/goal2-relic-base-20261018/package-result.json`, 131 staged artifacts, `sourceIdentity` `0E68E6EF04C8A48EE935841FEE20033C2FDC727F8A69B84F93BEC5D3B901CD91`).
- **Executable identity:** `LyraGame.exe` SHA-256 `BC5B279D6AE7B066F23DCE9EADE16737E4EAB09F7CE2F4B88FE8C053EBC8C707` (`Saved/Logs/goal2-relic-base-20261018/compile-game-1.json`).

## 2. Production Architecture & Invariants

- **Single-Carrier Authority Commit & Drop Cooldown (`ARelicActor::OnPickedUp`, `CanBePickedUp`, `PickupCooldownAfterDrop`):**
  - Authority commits `CurrentCarrier` and `ERelicState::Carried` before attachment or notification, preventing synchronous attachment overlap callbacks from triggering reentrant or competing pickups.
  - `DropFromCarrierInternal` records `LastDroppedCarrier` and `LastDropWorldTime` with a weak-lambda expiry timer so a fumbling/dropping carrier is blocked from immediate self-reacquisition during `URelicSettings::PickupCooldownAfterDrop` (`0.5s`), while teammates and defenders can acquire immediately.
- **Single Ability-Set Grant & Clean Release (`AS_BW_BallAbilities`):**
  - Authority grants `AS_BW_BallAbilities` (3 ability-set grants, including native `UBwayGameplayAbility_RelicThrow`, + 1 native `GA_BW_PassRelic` = `4` total specs) exactly once per pickup via `AttachToCarrier`; `DetachFromCarrier` revokes all 4 specs and removes `State.Relic.Carrying` across all peers on pass, throw, drop, fumble, or goal score.
- **Charged Ballistic Throws, Directed Passes & Prepared Catch (`ARelicActor`, `URelicSettings`, `State.Relic.PreparingCatch`):**
  - Charged throws interpolate speed across `[MinThrowSpeed=800, MaxThrowSpeed=2000]` over `MaxChargeTime=1.5s`; directed passes launch at `PassSpeed=1500` toward the selected teammate with `CurrentState = ERelicState::BeingPassed`, `bIsPassInFlight = true`, and `PassingTeamIndex`.
  - When an overlapping receiver holds `State.Relic.PreparingCatch` (or receives a pass/throw while eligible), `OnInteractionSphereOverlap` calls `OnPickedUp`, and `AttachToCarrier` clears physics velocity so high-velocity passes/throws are cleanly secured without physics bounce-off or duplicate ability grants.
- **Mid-Air Pass Interception (`ABwayPlayerState::Interceptions`):**
  - When an opposing player (`CatcherTeam != PassingTeamIndex`) catches/overlaps the Relic while `bIsPassInFlight` (`ERelicState::BeingPassed`), authority awards possession to the defender, increments `ABwayPlayerState::Interceptions` by `+1`, and populates `FBwayPlayerMatchSummary::Interceptions` in `FBwayPostMatchSummaryData`.
- **Combat Fumble Thresholds & Hard-CC Forced Fumbles (`ABwayPlayerState::ForcedFumbles`):**
  - Single hits or cumulative carrier damage meeting `URelicSettings::FumbleDamageThreshold` (`50.0`), as well as watched CC tags (`State.Stunned`, `State.Status.Silenced`, `State.Status.Jailed`), force an immediate `DropRelic` with `FumbleImpulseStrength` (`600.0`) and increment the attacker's `ABwayPlayerState::ForcedFumbles` by `+1`.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Headless Ballistics, Prepared Catch, Pass Impact & Base Healing Automation | `Breakaway.Relic.BallisticsAndInterception`, `Breakaway.Relic.PassImpactLifecycle`, `Breakaway.Relic.RequestLifecycle`, `Breakaway.Relic.ChargedThrow.InputLifecycle`, `Breakaway.Relic.ChargedThrow.AuthoredInput`, `Breakaway.Economy.BaseHealingShopWindow` | **PASS** — 6/6 suites (`Saved/Logs/goal2-relic-base-20261018/headless-goal2-2.json`) |
| 4-World PIE Charged Throw, Landed Pass, Prepared Catch, Interception, Fumbles & Base Shop (100ms) | `bway.Test.RelicFlight` + `bway.Test.Goal2RelicBasePIE` (4 PIE worlds, `Net PktLag=100`) | **PASS** — `BwayRelicFlight: COMPLETE pass=1 peers=4 scenarios=5 remoteOwner=257 groundedDelta=0 airborneDelta=1 cleanup=1` + `BwayGoal2GateTest: PIE PASS worlds=4 interceptsP1=1 summaryIntercepts=1 fumblesP0=1 fumblesP2=1 totalFumbles=2 summaryFumbles=2 p0HP=243.5 inBase0=1 canShop0=1` (`Saved/Logs/goal2-relic-base-20261018/pie-goal2-5.log`) |
| 4-Process Packaged Consolidated Goal 2 & Relic Flight (100ms, 2 D3D12 + 2 NullRHI) | `LAN-20261018-01` (`bway.Test.Goal2RelicBaseHost`/`Client` + `bway.Test.RelicFlight`) | **PASS** — Host (`p0HP=293.1`) + 3 clients (`p0HP=243.5`) converged on `interceptsP1=1`, `fumblesP0=1`, `fumblesP2=1`, `totalFumbles=2`, `inBase0=1`, `canShop0=1`, and `BwayRelicFlight: COMPLETE pass=1 peers=4 scenarios=5 remoteOwner=257 groundedDelta=0 airborneDelta=1 cleanup=1` (`Saved/Logs/goal2-relic-base-20261018/packaged-summary.json`) |
| Historical 4-Process Packaged 4-Owner Pass/Throw/Goal Lifecycle (200ms) | `Candidate07` (`LAN-20260926-07`, 23 stages) | **PASS** — 37 checks, 48 controls, 56 peer observations (`packaged-ownership02-verification.json`) |

## 4. Goal 2 Reconciliation & Gate Record (`goal2-relic-base-20261018`)

- **Reconciled project tuning (`URelicSettings`):** Retained `MinThrowSpeed=800.0`, `MaxThrowSpeed=2000.0`, `MaxChargeTime=1.5s`, `PassSpeed=1500.0`, `PickupCooldownAfterDrop=0.5s`, `FumbleDamageThreshold=50.0` (single hit or cumulative carrier damage during the current carry window), and `FumbleImpulseStrength=600.0` pending external reference reconciliation.
- **Decoupled RMB Prepared Catch (`State.Relic.PreparingCatch`) vs Call-For-Relic Indicator (`Gameplay.Relic.Requesting`):**
  - `UBwayGameplayAbility_RelicRequest` applies `State.Relic.PreparingCatch` while RMB input is held and removes it immediately on input release (`OnInputReleased` / `InputReleased`), while allowing the `GE_BW_Relic_Request` indicator (`Gameplay.Relic.Requesting`, `3.0s`) to run for its full three-second lifetime.
  - Becoming the Relic carrier (`Gameplay.State.RelicCarrier`), dying (`Status.Death` / `Status.Death.Dying`), or changing controller/possession cancels the ability and clears both `State.Relic.PreparingCatch` and the request effect without leaving stale state.
  - `IA_BW_Relic_Request`, `IMC_BW_HeroAbilities`, and `IMC_BW_RelicAbilities` use continuous held input without `InputTriggerPressed` so RMB hold Duration is preserved until physical key release.
- **Forced-Fumble Attribution:** Single hits or cumulative damage meeting `50.0`, as well as hard CC tags (`State.Status.Silenced`, `State.Status.Jailed`, `State.Stunned`), force an immediate `DropRelic` with `FumbleImpulseStrength` (`600.0`) and award `+1` `ForcedFumbles` to the forcing opponent once (never to the victim, a teammate, or a stale attacker), with active hard-CC instigator taking priority over earlier chip damage.
- **Remaining Open Proof Gaps:** Physical human keyboard/mouse/gamepad playtest (Goal 9) and second-PC LAN hardware verification.

## 5. Historical Commit Review & Corrective Gate (`relic-flight-20261016`)

- Contract and evidence: `Saved/Logs/relic-flight-20261016/gate-contract.json` (`LAN-20261016-01`). Passed Editor06/Game01 builds, 7/7 headless tests (`automation03.json`), 4-world PIE (`pie02-reviewed-result.json`), and 4-process packaged transport at 100 ms lag (`transport01/check.json`).

## 6. Archive Pointer

- Full chronological gate diary: [RELIC_PICKUP_OWNERSHIP.md](./Archive/Full_History_20261013/RELIC_PICKUP_OWNERSHIP.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
