# Hero Codename Map

> **Source of truth** for slice display names vs internal asset paths (locked 2026-05-25).  
> Policy: **I2** — codenames in folders/asset names; **DisplayName** uses slice roster names.

## Roster

| DisplayName (UI / design spec) | Codename (folder) | GF plugin | Hero data asset (target) | Legacy BreakawayCore path |
|--------------------------------|-------------------|-----------|--------------------------|---------------------------|
| Spartacus | **Argus** | `Hero_Spartacus` | `DA_BW_HeroData_Argus` → keep name, set DisplayName | `/BreakawayCore/Characters/Heroes/Argus/` |
| Alona | **Alona** | `Hero_Alona` | `DA_BW_HeroData_Alona` | `/BreakawayCore/Characters/Heroes/Alona/` |
| Morgan Le Fay | **Hexweaver** | `Hero_Morgan` | `DA_BW_HeroData_Hexweaver` (new) | — |
| Rawlins | **Gunslinger** | `Hero_Rawlins` | `DA_BW_HeroData_Gunslinger` (new) | — |

**Spartacus note (H3):** Do not rename `Argus` assets for path parity; only fix **DisplayName**, portrait, and **HeroMesh** in the hero DA.

## Plugin content layout (L2 + M1)

After migration, each hero lives under its GF plugin mount (typically `/Hero_<PluginName>/`):

```
Hero_Spartacus/Content/Characters/Heroes/Argus/     … Spartacus (codename Argus)
Hero_Alona/Content/Characters/Heroes/Alona/
Hero_Morgan/Content/Characters/Heroes/Hexweaver/
Hero_Rawlins/Content/Characters/Heroes/Gunslinger/
```

Each plugin includes:

- `GFD_Hero_<PluginName>` (`UBwayGameFeatureData`) → `HeroDataAssets[]`
- Hero DA, ability set(s), two buildable DAs (F3), placement BPs (O2)

## Config cutover (P1 + N1)

Per hero, **after** ue-mcp verifies hero select:

1. Add plugin to `B_BW_Experience_CaptureTheRelic` → `GameFeaturesToEnable`
2. Add scan directory to `Config/DefaultGame.ini` → `PrimaryAssetTypesToScan` / `HeroDataAsset`:

```ini
(Path="/Hero_Spartacus/Characters/Heroes")
```

3. **Delete** legacy folder under `/BreakawayCore/Characters/Heroes/<Codename>/`

Do **not** add the next hero’s scan path until the previous hero passes its gate.

## Implementation order (G1)

1. Spartacus (`Argus`) — **in progress**
2. Alona
3. Morgan (`Hexweaver`)
4. Rawlins (`Gunslinger`)

## Content bars (locked)

| Area | Bar |
|------|-----|
| Abilities | **K2** — design-spec MVP behavior |
| Buildables | **O2** — placement + `bPersistsBetweenRounds`; no combat tuning until after A1c |
| A1c pass | **E2 + F3** — all four heroes in select before playtest gate |
| A1c human player | **Alona** (D1) |

## Cheats / registry IDs

Hero registry uses **primary asset IDs** from hero DAs, not DisplayName. After migration, update dev cheats/docs if they reference old paths. Prefer `SelectHero` by primary asset name once stable.

## Related

- [BLUEPRINT_ASSET_AUDIT.md](./BLUEPRINT_ASSET_AUDIT.md)
- [VERTICAL_SLICE_DEFINITION.md](./VERTICAL_SLICE_DEFINITION.md)
- [Plugins/GameFeatures/Heroes/Spartacus/CONTENT_SETUP.md](../Plugins/GameFeatures/Heroes/Spartacus/CONTENT_SETUP.md) — Gate 1 checklist
