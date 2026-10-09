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
└── UBwayGameplayAbility              (UI DisplayData on CDO; default cancels PlacementSession)
    ├── UBwayGameplayAbility_Base     (combat — blocked when relic carrier)
    │   ├── UBwayGameplayAbility_WarCry          (18a Argus stand-in → 18c For Glory)
    │   ├── UBwayGameplayAbility_ShieldBash      (18a stand-in → 18c No Retreat)
    │   ├── UBwayGameplayAbility_GladiatorsLeap  (18a stand-in → 18c Argus defense Slide)
    │   ├── UBwayGameplayAbility_DefensiveStance (18a stand-in → 18c Retribution)
    │   └── UBwayGameplayAbility_MeleePrimary    (Argus LMB — done)
    ├── UBwayGameplayAbility_Slide    (common movement — Left Shift)
    ├── UBwayGameplayAbility_PlaceBuildable (abstract — one BP child per hero on hero ability set)
    ├── UBwayGameplayAbility_ConfirmBuildablePlacement (LMB — humanoid set)
    ├── UBwayGameplayAbility_CancelBuildablePlacement (RMB — humanoid set)
    └── UBwayGameplayAbility_RelicRequest (Request Relic — blocked during placement)
```

Relic GAs (content) should inherit **`ULyraGameplayAbility`** directly or use tags that are not blocked by `Gameplay.State.RelicCarrier`. Common slide and Request Relic use **`UBwayGameplayAbility`**. Hero combat kit (including F / defense dodge) uses **`UBwayGameplayAbility_Base`**.

**During buildable placement:** Primary and Request Relic use `ActivationBlockedTags` → `State.BuildablePlacement`. Confirm/Cancel/start use `Ability.Buildable.PlacementExempt` and clear `CancelAbilitiesWithTag`. All other **`UBwayGameplayAbility`** subclasses inherit default `CancelAbilitiesWithTag` → `Ability.Buildable.PlacementSession`.

**Ownership:** PlaceBuildable on the **per-hero** ability set; Confirm/Cancel/Slide/RelicRequest on the **humanoid** set. See [Buildable_System.md](./Buildable_System.md) · [Argus_18b_Editor_Setup.md](./Argus_18b_Editor_Setup.md).

## Hero Data Contract

**`UBwayHeroDataAsset`**:

- `AbilitySets[]` — `ULyraAbilitySet` granted in `ABwayCharacterWithAbilities::InitializeHeroData` (includes per-hero PlaceBuildable)
- `HeroStats`, mesh, anim BP
- `BuildableDataAsset` — single placement definition for key **1**
- Ability bar UI via **`UBwayHeroAbilityUILibrary::ResolveAbilityBarForHero`** (`UBwayAbilityUISettings` slot order + ability CDO **`DisplayData`**)

Discovery: **`UBwayHeroRegistry`** + AssetManager scan `/BreakawayCore/Characters/Heroes/` and hero plugin paths.

## Per-Hero Game Feature Plugins

Expected under `/GameFeatures/Heroes/<Name>/`:

- `.uplugin` with `UBwayGameFeatureData` listing `HeroDataAssets`
- Content: mesh, anims, ability BPs, buildable BPs
- Loaded via experience `GameFeaturesToEnable`

Templates: [`Plugins/GameFeatures/Heroes/README.md`](../../Heroes/README.md)

## Argus / Spartacus

- **Current path:** Core Loop Step 18 complete in C++. DisplayName **Argus**; plugin `Hero_Spartacus`.

## Slide

**`UBwayGameplayAbility_Slide`** + **`UBwayCharacterMovementComponent`** custom movement mode `MOVE_Custom` / slide physics. Common on humanoid set (Left Shift). Distinct from Argus sheet "Slide" defense dodge on **F**.

## Gameplay Cues

Tags: `GameplayCue.Breakaway.*` in `BreakawayCore.ini`

Base: **`UGCN_BwayCombatFeedback`**

## Input

Lyra Enhanced Input routes actions through gameplay input tags (`InputTag.Ability.*`,
`InputTag.Relic.*`) in `DA_BW_InputData_Humanoid`.

`IMC_BW_HeroAbilities` contains the common hero, placement, and relic mappings at priority 1.
Both gameplay experiences use this context. They must not also install
`IMC_BW_RelicAbilities`: an independently applied context consumes overlapping keys before
the hero mappings reach the input component. Equal context priorities do not prevent this.
The older relic context remains as an uninstalled reference asset.

Shared keys intentionally coexist inside the gameplay context. LMB routes primary fire,
relic throw, and placement confirm; RMB routes relic request/drop and placement cancel.
E routes the second hero ability and relic pass; F routes defense and relic dash.
GAS grant lifetime, carrier restrictions, and placement tags decide which action may run.
Keep these restrictions when changing mappings; context availability is not gameplay authority.

The gameplay context now also provides controller routes for the same intents: right trigger
for primary fire, D-pad Down to start placement, D-pad Right/Left to confirm/cancel placement,
and the right face button for relic dash. Primary/right-trigger and relic throw, plus the
right-face-button hero slot and relic dash, intentionally share keys because the active GAS
grant and carrier/placement tags decide which action is eligible. D-pad directions avoid the
existing ability slots and the D-pad Up shop toggle.

The consolidation preserves all valid existing keys and the relic press triggers. The authored
mapping gap is closed, but this does not establish physical-input acceptance. Verify resolved
mappings and carrier/placement transitions after changing either the experience, the context,
the input config, or the relevant GAS restrictions.

## TODO

- Optional: Alona 19c / Korryn 20c editor 3/3 PIE confirmation (folded into Step 22 playtest)
