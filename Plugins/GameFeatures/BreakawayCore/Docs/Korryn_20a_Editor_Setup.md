# Step 20a — Korryn Functional (Editor Setup)

**Status:** C++ landed — awaiting rebuild, asset wiring, PIE, and 3/3 cold starts.

C++ lives in `HeroMorganRuntime` plus shared BreakawayCore status/damage/movement support. Complete these Editor steps after recompiling with the Editor **closed**, then restart the Editor.

**Related:** [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) · [CONTENT_SETUP.md](../../Heroes/Morgan/CONTENT_SETUP.md) · [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md)

## Ability ownership (canonical)

| Scope | Ability set | Grants |
|-------|-------------|--------|
| **Common (all heroes)** | `DA_BW_AbilitySet_Humanoid` | Left Shift movement slide · RMB Request Relic · Confirm/Cancel placement |
| **Per-hero** | `DA_BW_AbilitySet_Hexweaver` | LMB Primary · F Flock · Q Burden · E Circle · R Aura |
| **Hero DA** | `DA_BW_HeroData_Hexweaver` | Leave buildable empty until **20b** (Cursed Ward). Script clears stale Elder Stone but preserves a future Cursed Ward path |

**Do not** put Request Relic or movement slide on the per-hero set. **Do not** grant PlaceBuildable in 20a (that is **20b** — see [Korryn_20b_20c_Editor_Setup.md](./Korryn_20b_20c_Editor_Setup.md)). After 20b is wired, 20a script reruns **preserve** the Hexweaver / Cursed Ward buildable.

## Input layout

| Key | Input tag | Ability |
|-----|-----------|---------|
| **LMB** | `InputTag.Ability.Primary` | Primary Attack (orb + armor shred) |
| **RMB** | `InputTag.Ability.RelicRequest` | Request Relic (**common**) |
| **F** | `InputTag.Ability.Ability4` | **Flock** (sheet "RMB" dodge column → in-game **F**) |
| **Q** | `InputTag.Ability.Ability1` | Burden of Sin |
| **E** | `InputTag.Ability.Ability2` | Circle of Spite |
| **R** | `InputTag.Ability.Ability3` | Aura of Silence |
| **Left Shift** | `InputTag.Ability.Slide` | Common movement slide |

## 1. Recompile + enable plugin

1. Close Unreal Editor.
2. Compile `LyraEditor` / ProjectB so `HeroMorganRuntime` and updated `BreakawayCoreRuntime` build.
3. Restart Editor (ini scan for `/Hero_Morgan/Kit` needs a cold start).
4. Confirm **Edit → Plugins → Hero Morgan** is available.

## 2. Automated wiring (preferred)

With Editor + MCP bridge running:

```
node Scripts/setup-korryn-20a-functional.mjs
```

Probe only:

```
node Scripts/setup-korryn-20a-functional.mjs --probe-only
```

The script:

- Ensures Hexweaver hero DA / ability set + `DA_BW_KorrynKitConfig`
- Sets DisplayName **Korryn**, Support, HP **335** / Armor **0** / Atk **40** / Speed **10**
- Sets `AttributeSetClass` = `BwayHeroAttributeSet`
- Grants five native abilities (Primary / Ability4 / Ability1 / Ability2 / Ability3)
- Clears stale Elder Stone buildable; preserves future Cursed Ward
- Wires `Hero_Morgan` GFD
- Adds **`Hero_Morgan`** to `B_BW_Experience_Dev` **GameFeaturesToEnable**

## 3. Manual fallback — hero DA

| Field | Value |
|-------|--------|
| **DisplayName** | `Korryn` |
| **HeroClass** | Support |
| **HeroStats → MaxHealth** | `335` |
| **HeroStats → BaseDamage** | `40` |
| **HeroStats → Armor** | `0` |
| **HeroStats → MoveSpeed** | `10` |
| **AttributeSetClass** | `BwayHeroAttributeSet` |
| **AbilitySets** | `[DA_BW_AbilitySet_Hexweaver]` |
| **BuildableDataAsset** | empty (20b) |

## 4. Manual fallback — ability set grants

| Slot | Input tag | Native class |
|------|-----------|--------------|
| LMB | `InputTag.Ability.Primary` | `BwayGameplayAbility_KorrynPrimary` |
| F | `InputTag.Ability.Ability4` | `BwayGameplayAbility_KorrynFlock` |
| Q | `InputTag.Ability.Ability1` | `BwayGameplayAbility_KorrynBurdenOfSin` |
| E | `InputTag.Ability.Ability2` | `BwayGameplayAbility_KorrynCircleOfSpite` |
| R | `InputTag.Ability.Ability3` | `BwayGameplayAbility_KorrynAuraOfSilence` |

## 5. PIE pass checklist

URL (listen server):

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Korryn&NumBots=7
```

- [ ] Native classes load; five grants present; DisplayName Korryn; stats 335/0/40/10
- [ ] GFD registration + `Hero_Morgan` on experience; no buildable grant
- [ ] Cold-start Asset Manager resolves `Hero=Korryn`
- [ ] Primary: damage then armor stacks (−2 / hit, cap −10, 2s refresh); damage increases vs shredded armor
- [ ] Flock: 1.5s immunity, pawn pass-through, mid-air hover; cancels restore movement
- [ ] Burden: cone damage + 50% slow (~2s)
- [ ] Circle: enter/exit slow + 35% incoming damage amp; cleans on destroy
- [ ] Aura: radial damage + silence blocks combat abilities (second player if bots do not fire abilities)
- [ ] HUD slot order Primary / F / Q / E / R; relic carrier blocks LMB/F/Q/E/R
- [ ] Argus / Alona regression + relic pickup / request / score
- [ ] **3/3 consecutive cold starts** with no unexpected errors

Only after the checklist passes, mark Step 20a complete in `CoreLoop_Implementation_Plan.md`.
