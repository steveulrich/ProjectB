# Networking

Listen-server 4v4 target; server-authoritative gameplay.

## Authority Model

| System | Authority |
|--------|-----------|
| Round/score/relic/buildable placement | Server (`BlueprintAuthorityOnly`) |
| Hero selection RPCs | Server on PlayerState |
| Relic throw/pass | Server RPC + client prediction |
| Gold changes | Server-only GE application |
| UI show/hide | Client RPCs on PlayerController |

## Listen Server 4v4

- **`UBwayBotCreationComponent`**: `bScaleBotsToTargetPlayerCount` fills to `TargetPlayerCount` (8)
- Team balance: **`ABreakawayGameMode::AssignPlayerToTeam`**

## Relic Replication

- State machine replicated (`CurrentState`, `CurrentCarrier`, `LastPossessingTeam`)
- Physics: **`URelicMovementReplicationComponent`** — server snapshots, client interpolation
- Do not enable `AActor::bReplicateMovement` on relic (conflicts with custom component)

**Test checklist:** 200ms latency (`Net PktLag=200`), rapid pickup/drop/throw/fumble, simultaneous goal edge cases.

`URelicMovementReplicationComponent` snap threshold is **600 cm** (covers ~200 ms × throw speed plus physics divergence). Velocity prediction scale is 0.7.

## Buildable Replication

- Actors replicate; registry tracks server-side list
- Persisting buildables remain after round reset — must replicate to all clients including late joiners (actors in world)

## Slide / Movement

**Risk:** direct velocity steering under latency (see Vertical Slice Roadmap). Tune in `UBwayCharacterMovementComponent` if jitter observed.

## Player Stats

`ABwayPlayerState` K/D/A/ObjectiveScore replicated — scoreboard/results read on all clients.

## Deferred

- Dedicated server hardening
- Full client-side prediction for all abilities
- Buildable structural serialization beyond actor replication

## Debug

`UBwayCheatManager` — `ShowHeroSelect`, `SelectHero`, etc. (authority commands)
