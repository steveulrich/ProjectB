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

**`W_BW_GoldWidget`** is injected at **`HUD.Slot.Gold`** through `EAS_BW_CaptureTheRelic` and derives from **`UBwayCurrencyDisplayWidget`**.

`UBwayCurrencyDisplayWidget`:

- Retries until the owning PlayerState ASC is available, then reads the initial `CurrentGold`.
- Binds the `CurrentGold` attribute-change delegate, covering passive, kill, and goal awards.
- Updates optional `Text_Amount` and `Image_CurrencyIcon` widgets directly; Blueprint owns layout/style only.
- Unbinds safely on widget destruction. Gold remains on the PlayerState ASC through pawn death and round changes.

Create/wire the compact match widget with `Scripts/setup-step19-5-match-hud.mjs` after recompiling `BreakawayCoreRuntime`.

## Blueprint

Display-only; no direct gold mutation from BP without server authority GE.

## Future (Deferred)

- Stat enhancer / item shop spend
- `UBwaySaveGame` for campaign/meta
- Structured GE assets (`GE_AwardGold_Kill`) instead of transient effects
