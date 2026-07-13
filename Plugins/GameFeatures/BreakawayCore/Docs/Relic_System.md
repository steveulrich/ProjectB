# Relic System

Capture-the-Relic objective — physical ball with GAS-driven interactions.

## Core Types

| Type | File | Role |
|------|------|------|
| `ARelicActor` | `Public/Relic/RelicActor.h` | 8-state FSM, physics, replication |
| `URelicSettings` | `Public/Relic/RelicSettings.h` | Mesh, forces, carrier ability set, cooldowns |
| `URelicMovementReplicationComponent` | `Public/Relic/RelicMovementReplicationComponent.h` | Client interpolation for thrown/dropped relic |
| `UBwayRelicManagerComponent` | `Public/GameState/BwayRelicManagerComponent.h` | Spawn, reset, carrier team tracking |
| `ABwayGoalVolume` | `Public/BwayGoalVolume.h` | Goal trigger → scoring |

## Relic States (`ERelicState`)

`Neutral` → `Carried` → `Dropped` / `Thrown` / `BeingPassed` → `Scoring` → `Resetting`

## Pickup Flow

1. Character requests relic (gameplay tag on ASC — `State.RequestingRelic` or `RelicSettings::RequestingTag`).
2. Overlap on `InteractionSphere` → server sends `Event.Interaction.PickupRelic` to PlayerState ASC.
3. `GA_PickupRelic` (Blueprint/content) calls `ARelicActor::OnPickedUp`.
4. Server grants carrier `ULyraAbilitySet` from `URelicSettings`, applies `Gameplay.State.RelicCarrier` on the PlayerState ASC (replicated loose tag), sets `ABwayPlayerState::bHasRelic`.

## Carrier Restrictions

- Tag `Gameplay.State.RelicCarrier` blocks **`UBwayGameplayAbility_Base`** combat abilities (LMB, F, Q, E, R) via `ActivationBlockedTags`.
- Applied on the **PlayerState ASC** with `TagAndCountToAll` replication; clients also mirror the tag when `bHasRelic` replicates and when the local carrier attaches, so LocalPredicted abilities fail `CanActivateAbility` without rubberbanding.
- **Not blocked while carrying:** common movement slide (Left Shift), Request Relic (RMB), buildable placement (1), and relic throw/pass/drop GAs.
- Relic interaction abilities (throw/pass/drop) granted via separate ability set — must **not** inherit `UBwayGameplayAbility_Base` or must clear blocked tags.

## Throw / Pass

- `Server_ThrowRelic` / `Server_PassRelic` — validated RPCs
- `ClientPredictThrow` — client prediction
- `Multicast_PlayThrowPassFX` — cosmetics
- `URelicMovementReplicationComponent::EnableSmoothReplication` during physics states

## Scoring

1. `ABwayGoalVolume` overlap: **relic actor** (thrown/pass) or **carrier pawn** while `ERelicState::Carried` (walk-in — carried relic has no collision)
2. Validates attacking team vs carrier / `LastPossessingTeam`
3. 0.1s settle delay → `OnEnteredGoal` → `bHasScoredThisRound = true`
4. Goal volume scoring cooldown from `URelicSettings::ScoringCooldown`
4. `UBwayRoundManagementComponent::OnRelicScored` → team gold + carrier `AddObjectiveScore`

## Replication Notes

- `SetReplicatingMovement(false)` — movement via custom component only
- `LastPossessingTeam`, `bHasScoredThisRound` replicated
- Net update frequency 66/33 Hz on relic actor

## Blueprint / Content Requirements

- `URelicSettings` data asset
- GAs: Pickup, Drop, Throw, Pass (on carrier ability set)
- VFX/SFX referenced in settings or `ARelicActor` Niagara slots

## TODO / Polish

- Further latency testing at 200ms+
- Fumble-on-damage (design spec) — not yet in C++
- Pass soft-lock target selection (content)

See also [Networking.md](./Networking.md).
