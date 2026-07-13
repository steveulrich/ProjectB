# Hero Codename Map

> **Paths / plugin names:** locked policy **I2** — codenames in folders/asset names.  
> **DisplayName (UI):** Section 3 / [Breakaway_Hero_Stats_Sheet.md](./Breakaway_Hero_Stats_Sheet.md) / [CoreLoop_Implementation_Plan.md](../Plugins/GameFeatures/BreakawayCore/Docs/CoreLoop_Implementation_Plan.md) — **not** older “Spartacus / Morgan Le Fay” marketing names for the slice roster.

## Roster

| DisplayName (UI / Section 3) | Codename (folder) | GF plugin | Hero data asset | Legacy BreakawayCore path |
|------------------------------|-------------------|-----------|-----------------|---------------------------|
| **Argus** | **Argus** | `Hero_Spartacus` | `DA_BW_HeroData_Argus` | `/BreakawayCore/Characters/Heroes/Argus/` |
| Alona | **Alona** | `Hero_Alona` | `DA_BW_HeroData_Alona` | `/BreakawayCore/Characters/Heroes/Alona/` |
| **Korryn** | **Hexweaver** | `Hero_Morgan` | `DA_BW_HeroData_Hexweaver` (or existing Hexweaver DA) | — |
| Rawlins | **Gunslinger** | `Hero_Rawlins` | `DA_BW_HeroData_Gunslinger` (or existing) | — |

**Argus note:** Do not rename `Argus` assets or the `Hero_Spartacus` plugin for path parity. Set **DisplayName = Argus**. Plugin FriendlyName may still say Spartacus.

**Historical note:** Older docs used DisplayName “Spartacus” for this hero. Section 3 superseded that — UI and hero select show **Argus**.

## Plugin content layout

```
Hero_Spartacus/Content/Characters/Heroes/Argus/     … Argus (plugin Hero_Spartacus)
Hero_Alona/Content/Characters/Heroes/Alona/
Hero_Morgan/Content/Characters/Heroes/Hexweaver/
Hero_Rawlins/Content/Characters/Heroes/Gunslinger/
```

Each plugin includes:

- `GFD_Hero_<PluginName>` (`UBwayGameFeatureData`) → `HeroDataAssets[]`
- Hero DA, ability set(s), **one** buildable DA + actor BP
- Per-hero PlaceBuildable BP; Confirm/Cancel stay on BreakawayCore humanoid set

## Config cutover

Per hero, after verify:

1. Add plugin to `B_BW_Experience_Dev` → `GameFeaturesToEnable` (Section 3: one at a time)
2. Add scan directory to `Config/DefaultGame.ini` → `HeroDataAsset`:

```ini
(Path="/Hero_Spartacus/Characters/Heroes")
```

3. Delete legacy folder under `/BreakawayCore/Characters/Heroes/<Codename>/` when duplicated

## Implementation order (Section 3)

1. Argus (`Hero_Spartacus`) — **in progress** (Step 18)
2. Alona
3. Korryn (`Hero_Morgan` / Hexweaver)
4. Rawlins (`Hero_Rawlins` / Gunslinger)

## Content bars (Section 3)

| Area | Bar |
|------|-----|
| Abilities | Stats sheet kit; 18a may use Spartacus C++ stand-ins until 18c |
| Buildables | **One per hero**; once/round; free; persist; sheet behavior |
| Gold | Earn in-match; **not** spent on buildables |

## Cheats / registry IDs

Hero registry uses **primary asset IDs** from hero DAs (e.g. `HeroDataAsset:Argus`), not DisplayName. Prefer `Hero=Argus` URL / `SelectHero` by primary asset name.

## Related

- [Spartacus CONTENT_SETUP.md](../Plugins/GameFeatures/Heroes/Spartacus/CONTENT_SETUP.md)
- [Argus_18b_Editor_Setup.md](../Plugins/GameFeatures/BreakawayCore/Docs/Argus_18b_Editor_Setup.md)
- [BLUEPRINT_ASSET_AUDIT.md](./BLUEPRINT_ASSET_AUDIT.md)
- [VERTICAL_SLICE_DEFINITION.md](./VERTICAL_SLICE_DEFINITION.md) — may still say Spartacus; prefer this map + Core Loop for DisplayName
