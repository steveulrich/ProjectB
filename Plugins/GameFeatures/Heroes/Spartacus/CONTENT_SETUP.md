# Spartacus — Content Setup (Gate 1)

**Codename:** `Argus` (paths unchanged; DisplayName = Spartacus). See [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md).

C++ abilities exist under `BreakawayCoreRuntime`. Migrate legacy Editor content from `/BreakawayCore/Characters/Heroes/Argus/` into this plugin, then hard-cutover (N1).

## Current Editor state (ue-mcp 2026-05-25)

| Asset | Status |
|-------|--------|
| `DA_BW_HeroData_Argus` | Exists; DisplayName still "Argus"; **HeroMesh = Alona SKM** |
| `DA_BW_AbilitySet_Argus` | Exists; 4 abilities + display infos |
| `DA_BW_BuildableData_Argus` | Single legacy DA; pistol placeholder mesh |
| `BuildableDataAssets[]` | **Empty** on hero DA |

## Target (F3 + K2 + O2)

| Item | Target path (in plugin) |
|------|-------------------------|
| Hero DA | `/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus` |
| DisplayName | **Spartacus** |
| HeroClass | Tank / Fighter |
| HeroMesh | Spartacus mesh (fix from Alona placeholder) |
| Ability set | `DA_BW_AbilitySet_Argus` — K2 MVP wired to C++ GAs |
| BuildableDataAssets[0] | `DA_BW_Buildable_FireCatapult` — O2 placement only |
| BuildableDataAssets[1] | `DA_BW_Buildable_DragonSpire` — O2 placement only |
| GFD | `GFD_Hero_Spartacus` (`UBwayGameFeatureData`) |

### Ability BPs (K2 — subclass C++)

| C++ class | BP asset |
|-----------|----------|
| `UBwayGameplayAbility_ShieldBash` | `GA_Argus_ShieldBash` (or keep existing Argus names) |
| `UBwayGameplayAbility_WarCry` | `GA_Argus_WarCry` |
| `UBwayGameplayAbility_DefensiveStance` | `GA_Argus_DefensiveStance` |
| `UBwayGameplayAbility_GladiatorsLeap` | `GA_Argus_GladiatorsLeap` |
| `UBwayGameplayAbility_Slide` | `GA_Slide` (shared) |

Wire cooldowns, GEs, input tags per [Spartacus_Implementation_Summary.md](../../BreakawayCore/Docs/Spartacus_Implementation_Summary.md).

### Buildables (O2 — placement + persist only)

| Slice name | Data asset | BP actor |
|------------|------------|----------|
| Fire Catapult | `DA_BW_Buildable_FireCatapult` | `BP_Argus_FireCatapult` (subclass `ABuildableActor` / turret base) |
| Dragon Spire | `DA_BW_Buildable_DragonSpire` | `BP_Argus_DragonSpire` |

- Set **`bPersistsBetweenRounds = true`** on buildable BPs.
- **`BuildableActorClass`** + mesh required; combat behavior stub OK until after A1c.

## Migration steps (Editor + agent)

1. **Enable plugin** — Edit → Plugins → `Hero Spartacus` (Registered → Enabled). Restart if prompted.
2. **Create folder** — `Hero_Spartacus/Content/Characters/Heroes/Argus/`
3. **Move or duplicate** from BreakawayCore Argus folder: hero DA, ability set, abilities, UI icons, buildable assets.
4. **Fix hero DA** — DisplayName Spartacus; correct HeroMesh; fill **`BuildableDataAssets[]`** with both buildable DAs.
5. **Create** `GFD_Hero_Spartacus` in plugin Content root; set `HeroDataAssets` → hero DA.
6. **Agent: config** — Add to `Config/DefaultGame.ini`:

```ini
+PrimaryAssetTypesToScan=(PrimaryAssetType="HeroDataAsset",AssetBaseClass="/Script/Engine.PrimaryDataAsset",bHasBlueprintClasses=False,bIsEditorOnly=False,Directories=((Path="/BreakawayCore/Characters/Heroes"),(Path="/Hero_Spartacus/Characters/Heroes")),SpecificAssets=,Rules=(Priority=-1,ChunkId=-1,bApplyRecursively=True,CookRule=AlwaysCook))
```

(Replace the existing single-directory `HeroDataAsset` line.)

7. **Editor: experience** — Open `B_BW_Experience_CaptureTheRelic`; add **`Hero_Spartacus`** to **GameFeaturesToEnable**.
8. **Verify (ue-mcp)** — With Editor open:

```bash
node Scripts/audit-breakaway-assets.mjs
```

Expect hero registry / search to find Spartacus DisplayName and plugin path; hero select shows Spartacus (not duplicated with legacy Argus folder).

9. **N1 cutover** — Delete `/BreakawayCore/Characters/Heroes/Argus/` after verification.

## Gate 1 done when

- [ ] `Hero_Spartacus` in experience `GameFeaturesToEnable`
- [ ] Hero DA primary asset loads from `/Hero_Spartacus/...`
- [ ] DisplayName **Spartacus** in hero select
- [ ] Two buildable DAs in `BuildableDataAssets[]`
- [ ] K2 abilities activate in PIE without errors
- [ ] O2 buildable places and registers on `UBwayBuildableRegistryComponent`
- [ ] BreakawayCore `Argus/` folder removed
- [ ] `HERO_CODENAME_MAP.md` unchanged for Spartacus row (already correct)

Then proceed to **Gate 2 — Alona** ([CONTENT_SETUP](../Alona/CONTENT_SETUP.md)).
