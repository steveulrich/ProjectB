# Breakaway: Reborn — Master Game Design & Technical Specification

**Engine Target:** Unreal Engine 5.8 (`5.8.1` Source Build on Lyra Starter Game)
**Parity Target:** 1:1 Amazon Game Studios *Breakaway* (September 2017 Final Public Alpha)
**Last Reviewed:** October 13, 2026
**Companion References:** [Hero Stats Sheet](./Breakaway_Hero_Stats_Sheet.md) · [Final Alpha Parity](./FINAL_ALPHA_PARITY.md) · [Architecture Overview](./project_architecture_overview.md)

---

## 1. Core Identity & Scope

*Breakaway: Reborn* (`ProjectB`) is a faithful 1:1 multiplayer recreation of Amazon Game Studios' mythological 4v4 team sport brawler *Breakaway* at its September 2017 Final Public Alpha bar, built on Unreal Engine 5.8, Epic's Lyra framework, `BreakawayCore`, and modular per-hero Game Feature plugins (`Hero_Spartacus`, `Hero_Alona`, `Hero_Morgan`, `Hero_Rawlins`, plus expansion plugins for the remaining 7 heroes).

> **Scope Note:** Earlier fictional prototype proposals (such as a *Pyre*-inspired "Ascension Rites" single-player campaign or `StateTree` campaign AI) are **retired and excluded**. All design and engineering work targets authentic 4v4 Capture-the-Relic multiplayer and LAN/bot parity.

---

## 2. Capture-the-Relic Match & Round Rules

Managed authoritatively by `UBwayRoundManagementComponent`, `UBwayScoringComponent`, and `UBwayMidfieldDividerComponent` on `ABwayGameState`, configured via `UBwayMatchConfigDataAsset` (`DA_BW_DemoMatchConfig` / `DA_BW_DevMatchConfig`):

- **Team Format:** 4v4 (`Team 0` Blue vs. `Team 1` Red; humans + `ABwayRelicBotController` fill to 4 per team).
- **Match Series:** Best of 5 Rounds (**First to 3 Round Wins**).
- **Round Timers:**
  - **Prematch Phase (`EGamePhase::Prematch`):** `10s` countdown before round start (players locked at spawn points; shop open).
  - **Playing Phase (`EGamePhase::Playing`):** `240s` (`4:00`) in Demo/Production (`DA_BW_DemoMatchConfig`); `90s` in explicit dev smoke configs (`DA_BW_DevMatchConfig`).
  - **PostRound Phase (`EGamePhase::PostRound`):** `5s` celebration/reset window before next round or `PostMatch` results.
- **Three Round Win Conditions (`ERoundWinCondition`):**
  1. **Goal Scored (`GoalScored`):** A player carries, passes, or throws the Relic into the opposing team's `ABwayGoalVolume` (`OnRelicEnteredGoal`), immediately awarding 1 round point to the attacking team.
  2. **Simultaneous Team Elimination (`TeamEliminated`):** All 4 players on one team are simultaneously dead (`Team1AliveCount == 0` or `Team2AliveCount == 0`) while the round is `Playing`.
  3. **Midfield Territory Tiebreaker (`TimeExpired`):** When the `240s` round clock expires, `UBwayMidfieldDividerComponent::GetTerritorySide(RelicLocation)` determines which half of the arena contains the Relic (`ERelicTerritorySide::Team1Territory`, `Team2Territory`, or `Neutral`). The team controlling the Relic on the opponent's half wins the round (`TiebreakerResolution`).
- **Death & Escalating Respawn Policy:**
  - Slain players drop the Relic immediately (`OnCarrierDied`) and enter a server-scheduled respawn queue (`ABwayPlayerState::RespawnSchedule` / `FBwayRespawnSchedule`).
  - September 2017 footage (`FINAL_ALPHA_PARITY.md`) establishes a `10s` base respawn delay that escalates with successive deaths within a round (`15s`, `17s` observed) and resets to `10s` at the start of the next round, modified by the player's `RespawnTimeReduction` attribute.

---

## 3. Relic Lifecycle, Ballistics & Turnover Mechanics

Implemented in `ARelicActor`, `URelicMovementReplicationComponent`, `UBwayRelicManagerComponent`, and `URelicSettings` (`DA_BW_RelicSettings_Default`):

### 3.1 Eight-State `ERelicState` Machine
1. `Neutral` — Spawned at center pedestal at round start (`ResetToNeutral`).
2. `Carried` — Attached to `CurrentCarrier`'s `RelicAttachSocket`; grants `AS_BW_BallAbilities` exactly once on authority.
3. `Dropped` — Physics-enabled after a combat fumble, carrier death, or manual drop (`PickupCooldownAfterDrop = 0.5s`).
4. `Passed` — Directed pass flight toward a teammate or pass trajectory (`PassSpeed = 1500 cm/s`, `bIsPassInFlight = true`).
5. `Thrown` — Charged ballistic throw toward the arena or enemy goal (`MinThrowSpeed = 800`, `MaxThrowSpeed = 2000`, `MaxChargeTime = 1.5s`).
6. `PendingRequest` — Reserved for pass-request coordination.
7. `Scoring` — Transient goal-entry state suppressing duplicate pickups/scores.
8. `Scored` — Terminal round-point state before `ResetToNeutral`.

### 3.2 Carrier Restrictions, Prepared Catch, Call-for-Relic & Turnovers
- **Carrier Restrictions (`State.Relic.Carrying`):** Carrying the Relic applies a movement speed multiplier (`0.85x` default) and blocks offensive hero combat abilities while enabling `GA_BW_PassRelic`, `GA_BW_Relic_Throw`, and `GA_BW_DropRelic`.
- **Call-for-Relic Indicator (`UBwayRelicRequestIndicatorComponent`):** Non-carriers can activate Call-for-Relic (`Gameplay.Relic.Requesting`, `3.0s`), projecting a 2D screen-space `72x72` hand marker (`W_BW_CaptureTheRelic_RelicRequestMarker`) visible exclusively to living teammates.
- **Prepared Catch (`State.Relic.PreparingCatch`):** Holding Right Mouse Button while unarmed prepares the hero to cleanly receive high-velocity incoming passes/throws on overlap.
- **Pass Interception (`Interceptions` Stat):** When an opposing player overlaps or catches a Relic while `CurrentState == ERelicState::Passed` and `PassingTeamIndex != CatcherTeamIndex`, authority transitions possession to the interceptor and increments `ABwayPlayerState::Interceptions`.
- **Forced Fumbles (`ForcedFumbles` Stat):** Heavy attacks, knockbacks/launches, stuns/imprisonments (`State.Status.Jailed`), silences, death, or damage exceeding `URelicSettings::FumbleDamageThreshold` (`50.0` default) call `ARelicActor::OnCarrierTakeDamage` / `DropRelic`, ejecting the Relic with `FumbleImpulseStrength` (`500.0`) and crediting the attacker's `ABwayPlayerState::ForcedFumbles`.

---

## 4. Persistent Buildable System

Implemented in `ABuildableActor`, `ATurretBase`, `ATrapBase`, `UBwayGameplayAbility_PlaceBuildable`, `FBwayPlacementQuery`, and `UBwayBuildableRegistryComponent`:

- **1-Per-Round Free Placement:** Every hero has one signature buildable (`UBwayHeroDataAsset::SignatureBuildable`). Each player may place **1 buildable per round for free** (`0` gold cost), tracked via `ABwayPlayerState::bHasPlacedBuildableThisRound` (`COND_OwnerOnly`, reset each round).
- **Cross-Round Persistence & Active Caps:** Surviving buildables **persist across rounds** until destroyed by enemy damage or evicted by the owner's per-type active cap in `UBwayBuildableRegistryComponent` (`Siege Engine = 1`, `Sun Shrine = 3`, `Cursed Ward = 3`, `Jail = 3`). Rawlins' spawned `ABwayRawlinsJailCage` (`225 HP`, `3.5s` duration) is an ownerless runtime prison exempt from the 3-trap cap.
- **Shared Placement Validation (`FBwayPlacementQuery`):** Both client preview and server authority execute the exact same ground trace, slope check (`<= MaxPlacementSlopeDegrees`), multi-corner support footprint check, bottom-pivot lift (`CalculateActorBottomOffset`), and oriented bounding-box (`PlacementQueryBox`) overlap check. Rejected placements never consume the player's round slot.
- **Collision Separation Invariant:** `ReplicatedVisualMesh` is strictly non-colliding (`NoCollision`), while `PlacementQueryBox` handles placement overlap queries and `AttackCollision` handles combat targeting (with `ABwayRawlinsJailBuildable` keeping `AttackCollision` disabled on both server and clients while armed).

---

## 5. Match Economy, Combat Credit & 6-Item Upgrade Shop

Implemented in `UBwayGoldAttributeSet`, `UBwayUpgradeComponent`, `UBwayUpgradeCatalogDataAsset` (`/BreakawayCore/Data/DA_BW_DefaultUpgradeCatalog`), `UBwayUpgradeShopWidget`, and `UBwayLifeLeechComponent`:

### 5.1 Gold Economy & Combat Attribution
- **Gold Balance (`UBwayGoldAttributeSet`):** `CurrentGold` starts at `0`, caps at `MaxGold = 500`, and accrues passive trickling income (`GoldPerSecond`) during `Playing`.
- **Kill & Assist Credit (`ABwayCharacterWithAbilities::ConsumeCombatCredit`):**
  - **Kill Award:** `50 Gold` (`GoldAwardForKill`) + `+1 Kills` to the final-blow opposing player.
  - **Assist Award:** `25 Gold` (`GoldAwardForAssist`) + `+1 Assists` to every other opposing player who dealt health damage within `AssistCreditWindowSeconds = 10.0s` prior to death.

### 5.2 Match Upgrade Shop (6 Catalog Items, 4 Equipment Slots, 4 Ranks Each)
Players can equip up to **4 distinct upgrade items** per match (`MaxEquippedUpgrades = 4`), upgrading each selected item from Rank 1 to Rank 4 (`75 / 125 / 175 / 225` provisional gold per rank). Upgrades persist across deaths and rounds within a match and reset on `ResetForNextMatch` (Rematch / New Match):

| Upgrade ID | Target Attribute (`UBwayHeroAttributeSet`) | Rank 1–4 Effect Values | Gameplay Impact |
|---|---|---|---|
| `AttackStrength` | `AttackStrength` | `+5 / +10 / +15 / +20` | Increases ability/primary damage via kit `AttackStr` scaling |
| `Armor` | `Armor` | `+5 / +10 / +15 / +20` | Reduces incoming damage by `Armor %` |
| `MoveSpeed` | `MoveSpeedMultiplier` | `+5% / +10% / +15% / +20%` | Scales live `MaxWalkSpeed` on server and proxies |
| `CooldownReduction` | `CooldownReduction` (`[0, 1]`) | `+5% / +10% / +15% / +20%` | Multiplies ability cooldown duration by `(1 - CDR)` |
| `LifeLeech` | `LifeLeech` (`[0, 1]`) | `+5% / +10% / +15% / +20%` | Heals attacker for `ActualEnemyPlayerHealthLoss * LifeLeech` |
| `RespawnTimeReduction` | `RespawnTimeReduction` (`[0, 1]`) | `+10% / +20% / +30% / +40%` | Multiplies respawn delay by `(1 - Reduction)`, updating live pending deaths |

### 5.3 Authoritative Shop Purchase Windows (`UBwayUpgradeComponent::CanPurchaseUpgrades`)
Purchases are validated on the server (`ServerTryPurchaseUpgrade`) and permitted only when the player is in one of three valid states:
1. **Prematch (`EGamePhase::Prematch`):** Before the round gates open.
2. **While Dead:** While the player's pawn is dead or awaiting respawn (`RespawnSchedule.bIsWaitingToRespawn`).
3. **Team Base Healing Zone:** While the living player is inside their team's base healing volume (`IsPlayerInHealingZone`).

---

## 6. Combat Damage & Armor Formula

Implemented in `UBwayDamageExecution` (`BwayDamageExecution.h`) and hero ability damage calculators:

$$\text{EffectiveDamage} = \left(\text{BaseDamage} + \text{AttackStrength} \times \text{Scaling}\right) \times \left(1.0 - \frac{\text{Clamp}(\text{TargetArmor}, 0, 100)}{100.0}\right) \times \text{IncomingDamageMultiplier}$$

- **`BaseDamage` & `Scaling`:** Authored per ability in the hero's kit config (`UBwayArgusKitConfig`, `UBwayAlonaKitConfig`, `UBwayKorrynKitConfig`, `UBwayRawlinsKitConfig`) and [Breakaway Hero Stats Sheet](./Breakaway_Hero_Stats_Sheet.md).
- **`TargetArmor`:** Victim's current `UBwayHeroAttributeSet::Armor` (base hero armor + `Armor` shop upgrades + Alona `Sun Shrine` `+15` aura minus Argus `No Retreat` / Korryn armor-shred debuffs).
- **`IncomingDamageMultiplier`:** Defaults to `1.0`; increased by debuffs such as Korryn's *Circle of Spite* / *Cursed Ward* (`1.10x` / `1.25x` via `State.Status.DamageAmplified`) or reduced by defensive stances such as Argus's *Retribution* shield channel (`0.50x`).

---

## 7. Bot AI Architecture (`AModularAIController` Behavior Tree)

Implemented in `ABwayRelicBotController` (`inherits AModularAIController`), Behavior Tree `BT_BW_RelicBot`, Blackboard `BB_BW_RelicBot`, and custom C++ nodes (`UBTService_UpdateRelicBotBlackboard`, `UBTTask_UseHeroAbility`, `UBTTask_InteractWithRelic`):

- **Blackboard Synchronization (`UBTService_UpdateRelicBotBlackboard`):** Samples `ARelicActor` state, `CarrierActor`, `bSelfHasRelic`, `bTeamHasRelic`, `NearestEnemy`, `EnemyGoalLocation`, and `RelicLocation`.
- **Priority Branches (`BT_BW_RelicBot`):**
  1. **Carrier Branch (`bSelfHasRelic == true`):** Pathfinds toward `EnemyGoalLocation` and executes `UBTTask_InteractWithRelic` (throws into goal within scoring range or passes to an open teammate under pressure).
  2. **Loose Relic Branch (`RelicState != Carried`):** Moves to `RelicLocation` to secure possession.
  3. **Escort / Intercept Branch:** Escorts friendly carrier or engages `CarrierActor` / `NearestEnemy` using `UBTTask_UseHeroAbility` (evaluating ability range, cooldown tags, and `State.Relic.Carrying` restrictions).
- **Teardown Safety Invariant:** `ABwayRelicBotController::OnUnPossess` explicitly stops `UBehaviorTreeComponent` before calling `Super::OnUnPossess` so round resets and seamless travel never access a null pawn.
