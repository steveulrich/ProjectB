# Step 21c — Rawlins Parity (Editor Setup)

**Status:** **Passed** — sheet spot-check + 3/3 cold starts green. All four slice heroes on `B_BW_Experience_Dev`.

Formal sheet parity for Rawlins. CDs, damage/scaling, Barrage bullet count, and Jail HP must match [`Breakaway_Hero_Stats_Sheet.md`](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md). Feel knobs (shot interval, knockback, slide distance) and Jail radii (`300 / 450 / 120`) stay as 21a/21b implementation defaults — the sheet does not define replacements.

**Related:** [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) · [Rawlins_21a_Editor_Setup.md](./Rawlins_21a_Editor_Setup.md) · [Rawlins_21b_Editor_Setup.md](./Rawlins_21b_Editor_Setup.md) · [CONTENT_SETUP.md](../../Heroes/Rawlins/CONTENT_SETUP.md)

## Prerequisites

- Steps **21a** + **21b** wired (combat kit + Jail)
- `HeroRawlinsRuntime` compiled; Editor cold-started at least once after compile
- Editor open with ue-mcp bridge

## Automated wiring (preferred)

```
node Scripts/setup-rawlins-21c-parity.mjs
node Scripts/setup-rawlins-21c-parity.mjs --probe-only
```

Logs:

- `AI_Planning/setup_rawlins_21c_probe.json`
- `AI_Planning/setup_rawlins_21c_apply.json`
- `AI_Planning/setup_rawlins_21c_probe_after.json`
- `AI_Planning/setup_rawlins_21c_parity_log.json`

The script:

- Seeds **sheet-only** fields on `DA_BW_RawlinsKitConfig` (does not overwrite feel/radii)
- Syncs hero DA base stats (400 / 0 / 60 / 10.5) and DisplayName **Rawlins**
- Syncs Jail cage CDO `MaxHealth = 450`
- Appends all four slice heroes (+ `BreakawayCore`) to `B_BW_Experience_Dev` `GameFeaturesToEnable`
- Probes + asserts parity; exits non-zero on drift

### Probe expectations

| Check | Expected |
|-------|----------|
| Hero stats | HP **400**, Armor **0**, Atk **60**, Speed **10.5** |
| Kit Primary | base **1**, scale **0.3**, shots **2** |
| Kit Double Down CD | **14** |
| Kit Power Shot | base **30**, scale **0.65**, CD **8** |
| Kit Slide Shot | base **10**, scale **0.6**, CD **18** |
| Kit Barrage | base **4**/bullet, scale **0.3**, shots **12**, CD **25** |
| Kit Jail HP | **450** |
| Cage CDO | `MaxHealth=450` |
| Cooldown GE fallbacks | 14 / 8 / 18 / 25 |
| Ability set | **6** grants (5 combat + Buildable); native **or** BP subclasses of native Rawlins abilities |
| Experience plugins | `BreakawayCore`, `Hero_Spartacus`, `Hero_Alona`, `Hero_Morgan`, `Hero_Rawlins` |

## Sheet spot-check (Atk 60)

Damage formula: `Final = Base + Atk × Scale`.

| Ability | Key | Expected raw | CD |
|---------|-----|--------------|----|
| Primary (per shot) | LMB | `1 + 60×0.3` → **19** | — |
| Double Down | F | — (invuln roll) | **14s** |
| Power Shot | Q | `30 + 60×0.65` → **69** | **8s** |
| Slide Shot | E | `10 + 60×0.6` → **46** | **18s** |
| Blazing Barrage (per bullet) | R | `4 + 60×0.3` → **22** × **12** | **25s** |
| Jail cage | 1 | HP **450** | once/round placement |

Look for Output Log lines like:

```
ScaledDamage: base=30.0 atk=60.0 scale=0.65 -> 69.0
ScaledDamage: base=10.0 atk=60.0 scale=0.60 -> 46.0
ScaledDamage: base=4.0 atk=60.0 scale=0.30 -> 22.0
```

## PIE gate (listen server)

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Rawlins&NumBots=7
```

### Pass checklist (21c)

- [ ] Probe reports **PARITY PASS**
- [ ] Spawn log: stats HP **400** / Atk **60**
- [ ] Primary: two shots → scaled **19** each
- [ ] Double Down (F): invuln roll; cannot re-cast for **14s**
- [ ] Power Shot (Q): knockback; scaled **69**; CD **8s**
- [ ] Slide Shot (E): slide + launch; scaled **46**; CD **18s**
- [ ] Blazing Barrage (R): **12** shots; **22** per bullet; CD **25s**; aimable; interruptible on damage
- [ ] Jail (1): cage **450 HP**; trap / multi-capture still works (21b)
- [ ] `B_BW_Experience_Dev` has all four slice heroes enabled
- [ ] Argus / Alona / Korryn smoke + relic pickup / score still OK
- [ ] **3/3 consecutive Editor cold starts** — no unexpected `Error`, no `AssetBaseClassLoaded` ensure

## Non-goals

- VFX / animation polish
- Changing Jail radii or Barrage fire rate (no sheet authority)
- Step 22 capstone (fumble, staging E2E)

Only after the checklist + 3/3 pass, mark Step 21 (21a–21c) complete in `CoreLoop_Implementation_Plan.md` with:

`core-loop: step 21c passed (Rawlins parity)`
