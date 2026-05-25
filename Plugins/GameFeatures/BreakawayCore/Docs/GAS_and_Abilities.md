# GAS and Abilities

Gameplay Ability System layout for Breakaway heroes and relic.

## ASC Location

- **PlayerState** — primary ASC (Lyra pattern), gold attributes, hero + relic abilities
- **Pawn** — Lyra character extension for pawn-local abilities
- **Relic** — own ASC on `ARelicActor` (minimal)
- **Buildables** — `ABwayActorWithAbilitiesAndHealth`

## Ability Hierarchy

```
ULyraGameplayAbility
├── UBwayGameplayAbility_Base     (combat — blocked when relic carrier)
│   ├── UBwayGameplayAbility_WarCry
│   ├── UBwayGameplayAbility_ShieldBash
│   ├── UBwayGameplayAbility_GladiatorsLeap
│   └── UBwayGameplayAbility_DefensiveStance
└── UBwayGameplayAbility_Slide    (movement)
```

Relic GAs (content) should inherit **`ULyraGameplayAbility`** directly or use tags that are not blocked by `Gameplay.State.RelicCarrier`.

## Hero Data Contract

**`UBwayHeroDataAsset`**:

- `AbilitySets[]` — `ULyraAbilitySet` granted in `ABwayCharacterWithAbilities::InitializeHeroData`
- `HeroStats`, mesh, anim BP, UI display infos
- `BuildableDataAssets[]` — placement definitions

Discovery: **`UBwayHeroRegistry`** + AssetManager scan `/BreakawayCore/Characters/Heroes/`.

## Per-Hero Game Feature Plugins

Expected under `/GameFeatures/Heroes/<Name>/`:

- `.uplugin` with `UBwayGameFeatureData` listing `HeroDataAssets`
- Content: mesh, anims, ability BPs, buildable BPs
- Loaded via `UBwayHeroPluginLoader` or experience `GameFeaturesToEnable`

Templates: [`Plugins/GameFeatures/Heroes/README.md`](../../Heroes/README.md)

## Spartacus (Reference C++ Implementation)

See [Spartacus_Implementation_Summary.md](./Spartacus_Implementation_Summary.md).

## Slide

**`UBwayGameplayAbility_Slide`** + **`UBwayCharacterMovementComponent`** custom movement mode `MOVE_Custom` / slide physics.

## Gameplay Cues

Tags: `GameplayCue.Breakaway.*` in `BreakawayCore.ini`

Base: **`UGCN_BwayCombatFeedback`**

## Input

Lyra Enhanced Input + gameplay input tags (`InputTag.Ability.*`, `InputTag.Relic.*`). No custom IMC C++ in BreakawayCore — configured in experience ActionSets.

## TODO

- Morgan, Alona, Rawlins ability implementations (Phase 4 content)
- Fumble-on-damage gameplay effect for relic carrier
- Primary attack / combo (design spec) — not in C++ yet
