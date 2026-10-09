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

## Spending (final-alpha parity)

**Buildables do not spend gold.** Placement is once-per-round and free (`UBwayBuildablePlacementLibrary::CanPlayerPlaceBuildable` — no gold check). `UBwayBuildableDataAsset::Cost` may remain `0` for slice content.

**Match upgrades are required for the final-public-alpha parity bar.** The current provisional catalog
contains attack-strength and armor traits with four ranks each. Purchases are server-authoritative and
available during round preparation, while dead, and inside an eligible teammate base. The catalog,
prices, and effect tuning remain replaceable until the reference inventory is recovered; the shop UI
and multiplayer lifecycle still require packaged verification. See `Match_Upgrade_Economy.md`.

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

## Out of the current slice

- `UBwaySaveGame` for campaign/meta
- Additional reference equipment effects beyond the current attack/armor catalog
- Structured GE assets (`GE_AwardGold_Kill`) instead of the current server-side award path
