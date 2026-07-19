# Step 21a — Rawlins Functional (Editor Setup)

**Status:** **Passed** (with 21b/21c).

C++ lives in `HeroRawlinsRuntime`. Complete these Editor steps after recompiling with the Editor **closed**, then restart the Editor.

**Related:** [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) · [CONTENT_SETUP.md](../../Heroes/Rawlins/CONTENT_SETUP.md) · [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)

## Ability ownership (canonical)

| Scope | Ability set | Grants |
|-------|-------------|--------|
| **Common (all heroes)** | `DA_BW_AbilitySet_Humanoid` | Left Shift movement slide · RMB Request Relic · Confirm/Cancel placement |
| **Per-hero** | `DA_BW_AbilitySet_Gunslinger` | LMB Primary · F Double Down · Q Power Shot · E Slide Shot · R Blazing Barrage |
| **Hero DA** | `DA_BW_HeroData_Gunslinger` | After **21b**: `BuildableDataAsset` → Jail DA. Script preserves Jail on 21a reruns. |

**Do not** put Request Relic or movement slide on the per-hero set. **Do not** grant PlaceBuildable in 21a (that is **21b**). After 21b is wired, 21a script reruns **preserve** the Gunslinger / Jail buildable.

## Input layout

| Key | Input tag | Ability |
|-----|-----------|---------|
| **LMB** | `InputTag.Ability.Primary` | Primary Attack (two-shot) |
| **RMB** | `InputTag.Ability.RelicRequest` | Request Relic (**common**) |
| **F** | `InputTag.Ability.Ability4` | **Double Down** (sheet "RMB" dodge column → in-game **F**) |
| **Q** | `InputTag.Ability.Ability1` | Power Shot |
| **E** | `InputTag.Ability.Ability2` | Slide Shot |
| **R** | `InputTag.Ability.Ability3` | Blazing Barrage |
| **Left Shift** | `InputTag.Ability.Slide` | Common movement slide |

## 1. Recompile + enable plugin

1. Close Unreal Editor.
2. Compile `LyraEditor` / ProjectB so `HeroRawlinsRuntime` builds.
3. Restart Editor (ini scan for `/Hero_Rawlins/Kit` needs a cold start).
4. Confirm **Edit → Plugins → Hero Rawlins** is available.

## 2. Automated wiring (preferred)

With Editor + MCP bridge running:

```
node Scripts/setup-rawlins-21a-functional.mjs
```

Probe only:

```
node Scripts/setup-rawlins-21a-functional.mjs --probe-only
```

The script:

- Ensures Gunslinger hero DA / ability set + `DA_BW_RawlinsKitConfig`
- Sets DisplayName **Rawlins**, custom class **Shooter**, HP **400** / Armor **0** / Atk **60** / Speed **10.5**
- Sets `AttributeSetClass` = `BwayHeroAttributeSet`
- Grants five native abilities (Primary / Ability4 / Ability1 / Ability2 / Ability3)
- Clears stale buildable; preserves future Jail
- Wires `Hero_Rawlins` GFD
- Adds **`Hero_Rawlins`** to `B_BW_Experience_Dev` **GameFeaturesToEnable**

## 3. Manual fallback — hero DA

| Field | Value |
|-------|--------|
| **DisplayName** | `Rawlins` |
| **HeroClass** | Fighter (or any) + **bUseCustomClassName** = true, **CustomClassName** = `Shooter` |
| **HeroStats → MaxHealth** | `400` |
| **HeroStats → BaseDamage** | `60` |
| **HeroStats → Armor** | `0` |
| **HeroStats → MoveSpeed** | `10.5` |
| **AttributeSetClass** | `BwayHeroAttributeSet` |
| **AbilitySets** | `[DA_BW_AbilitySet_Gunslinger]` |
| **BuildableDataAsset** | empty (21b) |

## 4. Manual fallback — ability set grants

| Slot | Input tag | Native class |
|------|-----------|--------------|
| LMB | `InputTag.Ability.Primary` | `BwayGameplayAbility_RawlinsPrimary` |
| F | `InputTag.Ability.Ability4` | `BwayGameplayAbility_RawlinsDoubleDown` |
| Q | `InputTag.Ability.Ability1` | `BwayGameplayAbility_RawlinsPowerShot` |
| E | `InputTag.Ability.Ability2` | `BwayGameplayAbility_RawlinsSlideShot` |
| R | `InputTag.Ability.Ability3` | `BwayGameplayAbility_RawlinsBlazingBarrage` |

## 5. PIE pass checklist

URL (listen server):

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Rawlins&NumBots=7
```

- [ ] Native classes load; five grants present; DisplayName Rawlins; stats 400/0/60/10.5
- [ ] GFD registration + `Hero_Rawlins` on experience; no buildable grant
- [ ] Cold-start Asset Manager resolves `Hero=Rawlins`
- [ ] Primary: two consecutive pistol shots each dealing scaled damage
- [ ] Double Down (F): invuln roll forward (~14s CD)
- [ ] Power Shot (Q): knockback blast (~8s CD)
- [ ] Slide Shot (E): forward slide + launch (~18s CD)
- [ ] Blazing Barrage (R): stationary 12-shot channel; aimable; cancels on damage taken (~25s CD)
- [ ] HUD slot order Primary / F / Q / E / R; relic carrier blocks LMB/F/Q/E/R; Shift + RMB still work
- [ ] Argus / Alona / Korryn regression + relic pickup / request / score
- [ ] **3/3 consecutive cold starts** with no unexpected errors (no `AssetBaseClassLoaded` ensure)

Only after the checklist passes, mark Step 21a complete in `CoreLoop_Implementation_Plan.md`.
