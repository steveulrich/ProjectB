# Economy — Gold

Match-only gold on PlayerState ASC.

## Attribute Set

**`UBwayGoldAttributeSet`** (on `ABwayPlayerState` ASC):

| Attribute | Purpose |
|-----------|---------|
| `CurrentGold` | Spendable balance |
| `MaxGold` | Clamp upper bound |
| `GoldPerSecond` | Passive income metadata |

Modified via **instant GameplayEffects** (created at runtime in RoundManagement).

## Income Sources

| Source | Where |
|--------|-------|
| Passive per second | `UBwayRoundManagementComponent` during `RoundActive` |
| Kill | `NotifyPlayerKilled` → `GoldAwardForKill` |
| Goal scored | Team-wide award on `OnRelicScored` |

## Spending (vertical slice)

**Buildables do not spend gold.** Placement is once-per-round and free (`UBwayBuildablePlacementLibrary::CanPlayerPlaceBuildable` — no gold check). `UBwayBuildableDataAsset::Cost` may remain `0` for slice content.

**Deferred (post–vertical slice):** gold spend on **stat enhancers / items**, not buildable purchase. See Core Loop Section 3 principles.

## Boundaries

| Scope | Behavior |
|-------|----------|
| Within match | Gold persists round-to-round on PlayerState |
| Match end | No SaveGame — gold discarded on new match |
| Campaign | Out of vertical slice scope |

## UI

**`UBwayCurrencyDisplayWidget`** binds to `CurrentGold` attribute change delegate.

## Blueprint

Display-only; no direct gold mutation from BP without server authority GE.

## Future (Deferred)

- Stat enhancer / item shop spend
- `UBwaySaveGame` for campaign/meta
- Structured GE assets (`GE_AwardGold_Kill`) instead of transient effects
