# Rawlins — Content Setup (Gate 4)

**Codename:** `Gunslinger` (folder paths). **DisplayName:** Rawlins.  
See [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md).

Greenfield in `Hero_Rawlins` plugin.

## Target (F3 + K2 + O2)

| Item | Path pattern |
|------|----------------|
| Hero DA | `/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger` |
| Ability set | K2 MVP — Trick Shot, Scatter Shot, Sticky Bomb, Ricochet ult |
| Buildables | **Cage**, **Boom Box** — O2 placement + persist |
| GFD | `GFD_Hero_Rawlins` |

Use Lyra/ShooterCore projectile patterns for hitscan/projectile GAs.

## Gate 4 done when

- [ ] Plugin enabled + scan path `/Hero_Rawlins/Characters/Heroes`
- [ ] All **four** heroes in hero select (E2 satisfied)
- [ ] ue-mcp hero search returns 4 `HeroData` assets from plugin paths

Then run **A1c playtest** — [DEVMAP_PLAYTEST_GUIDE.md](../../../AI_Planning/DEVMAP_PLAYTEST_GUIDE.md) (human = Alona).
