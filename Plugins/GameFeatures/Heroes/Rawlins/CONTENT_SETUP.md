# Rawlins — Content Setup

**Codename:** `Gunslinger` (folder paths). **DisplayName:** Rawlins.  
See [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md) · [Breakaway_Hero_Stats_Sheet.md](../../../AI_Planning/Breakaway_Hero_Stats_Sheet.md).

Plugin: `Hero_Rawlins`. Runtime: `HeroRawlinsRuntime`.

## Asset paths

| Item | Path |
|------|------|
| Hero DA | `/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger` |
| Ability set | `/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_AbilitySet_Gunslinger` |
| Kit config | `/Hero_Rawlins/Kit/DA_BW_RawlinsKitConfig` |
| GFD | `/Hero_Rawlins/Hero_Rawlins` |
| Buildable DA | `/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_BuildableData_Gunslinger` |
| Jail floor trap BP | `/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable/BP_BW_Gunslinger_Buildable` |
| Jail cage BP | `/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable/BP_BW_Gunslinger_JailCage` |
| Place GA | `/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_BuildablePlacement` |

## Kit (Section 3)

| Slot | Ability | Input | Notes |
|------|---------|-------|-------|
| LMB | Primary Attack | `InputTag.Ability.Primary` | Two pistol shots, 1 dmg / 0.3 scale each |
| F | Double Down | `InputTag.Ability.Ability4` | Sheet "RMB" dodge → in-game **F**; 14s invuln roll |
| Q | Power Shot | `InputTag.Ability.Ability1` | 8s knockback blast |
| E | Slide Shot | `InputTag.Ability.Ability2` | 18s slide + launch |
| R | Blazing Barrage | `InputTag.Ability.Ability3` | 25s stationary 12-shot channel |
| 1 | Jail | `InputTag.Ability.Buildable` | One-shot floor trap → per-victim 450 HP cages (**21b**) |

**Common (humanoid set):** Left Shift slide · RMB Request Relic.

## Base stats

HP **400** · Armor **0** · Atk **60** · Speed **10.5** (→ 630 uu/s). Role display: **Shooter**.

## Setup

1. Close Editor → compile `LyraEditor` (builds `HeroRawlinsRuntime`) → cold restart.
2. Run `node Scripts/setup-rawlins-21a-functional.mjs` (combat kit).
3. Run `node Scripts/setup-rawlins-21b-jail.mjs` (Jail trap + PlaceBuildable).
4. Run `node Scripts/setup-rawlins-21c-parity.mjs` (sheet parity enforce + probe).
5. Follow [Rawlins_21a_Editor_Setup.md](../../BreakawayCore/Docs/Rawlins_21a_Editor_Setup.md) → [Rawlins_21b_Editor_Setup.md](../../BreakawayCore/Docs/Rawlins_21b_Editor_Setup.md) → [Rawlins_21c_Editor_Setup.md](../../BreakawayCore/Docs/Rawlins_21c_Editor_Setup.md).

## Parity notes (21c)

Sheet-authoritative: CDs (**14 / 8 / 18 / 25**), damage/scaling (Primary `1/0.3`, Power Shot `30/0.65`, Slide Shot `10/0.6`, Barrage `4×12/0.3`), Jail HP **450**.

Feel knobs (Barrage shot interval, knockback, slide distance) and Jail radii (**300 / 450 / 120**) are implementation defaults — the stats sheet does not define replacements.
