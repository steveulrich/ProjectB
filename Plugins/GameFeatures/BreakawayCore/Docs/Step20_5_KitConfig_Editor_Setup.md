# Step 20.5 — Argus + Alona Kit Config Retrofit

**Status:** Assets wired — awaiting manual PIE (Argus / Alona / Korryn) and **3/3** cold starts.

Gives Argus and Alona the same one-DA-per-hero kit-tuning pattern as Korryn (`UBwayKorrynKitConfig`). This is **not** a parity retune — 18c / 19c remain authority for feel.

**Related:** [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) · [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)

## What changed (C++)

| Hero | Kit class | Ability base | Soft path default |
|------|-----------|--------------|-------------------|
| Argus | `UBwayArgusKitConfig` (`BreakawayCoreRuntime`) | `UBwayGameplayAbility_ArgusBase` | `/Hero_Spartacus/Kit/DA_BW_ArgusKitConfig` |
| Alona | `UBwayAlonaKitConfig` (`HeroAlonaRuntime`) | `UBwayGameplayAbility_AlonaBase` | `/Hero_Alona/Kit/DA_BW_AlonaKitConfig` |
| Korryn | `UBwayKorrynKitConfig` (unchanged) | `UBwayGameplayAbility_KorrynBase` | `/Hero_Morgan/Kit/DA_BW_KorrynKitConfig` |

Combat abilities resolve the kit DA on activate and overlay damage / movement / heal / knockback values. Cooldown durations can be overridden from the kit via `ApplyCooldownWithOptionalDuration` (native GE remains fallback when the DA is missing).

**Out of scope:** ability-set grants, inputs, buildables (Siege Engine / Sun Shrine), hero base stats.

## 1. Recompile + cold start

1. Close Unreal Editor.
2. Compile `LyraEditor` / ProjectB so `BreakawayCoreRuntime` and `HeroAlonaRuntime` rebuild.
3. Restart Editor (ini scans for `/Hero_Spartacus/Kit` and `/Hero_Alona/Kit` need a cold start).

## 2. Automated wiring

With Editor + MCP bridge running:

```
node Scripts/setup-step20-5-kit-configs.mjs
```

Probe only:

```
node Scripts/setup-step20-5-kit-configs.mjs --probe-only
```

The script:

- Creates `/Hero_Spartacus/Kit/DA_BW_ArgusKitConfig` and `/Hero_Alona/Kit/DA_BW_AlonaKitConfig` if missing
- Seeds sheet / existing C++ defaults (idempotent refresh)
- Soft-binds `KitConfig` on native combat CDOs and existing ability Blueprint CDOs
- Probes all **three** canonical kit assets (Argus + Alona + Korryn)
- Deletes the duplicate `/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_KorrynKitConfig` only after the canonical `/Hero_Morgan/Kit` asset is verified

**Does not** rewrite ability sets, grants, inputs, experience plugins, or buildables.

## 3. Asset Manager scans (`DefaultGame.ini`)

| PrimaryAssetType | Directory | AssetBaseClass |
|------------------|-----------|----------------|
| `BwayArgusKitConfig` | `/Hero_Spartacus/Kit` | `/Script/Engine.PrimaryDataAsset` |
| `BwayAlonaKitConfig` | `/Hero_Alona/Kit` | `/Script/Engine.PrimaryDataAsset` |
| `BwayKorrynKitConfig` | `/Hero_Morgan/Kit` | `/Script/Engine.PrimaryDataAsset` (existing) |

## 4. PIE pass checklist

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Argus&NumBots=7
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Alona&NumBots=7
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Korryn&NumBots=7
```

Listen server, 1 player.

- [x] Probe shows three kit assets; native classes load; soft refs bound
- [ ] Argus: log `Resolved kit config` on combat activate; Primary / F / Q / E / R match pre-20.5 feel (18c numbers)
- [ ] Alona: same for Primary / F / Q / E / R (19c numbers)
- [ ] Korryn unchanged
- [ ] Grants / inputs / buildables unchanged
- [ ] No `AssetBaseClassLoaded` / primary-asset ensures on cold start
- [ ] **3/3** consecutive cold starts

Only after the checklist passes, mark Step 20.5 complete in `CoreLoop_Implementation_Plan.md` and commit with:

`core-loop: step 20.5 passed (argus+alona kit configs)`
