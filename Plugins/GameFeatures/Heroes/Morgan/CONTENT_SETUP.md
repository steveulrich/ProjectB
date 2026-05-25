# Morgan Le Fay — Content Setup (Gate 3)

**Codename:** `Hexweaver` (folder paths). **DisplayName:** Morgan Le Fay.  
See [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md).

Greenfield in `Hero_Morgan` plugin — no BreakawayCore legacy folder.

## Target (F3 + K2 + O2)

| Item | Path pattern |
|------|----------------|
| Hero DA | `/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver` |
| Ability set | K2 MVP — Burden of Sin, Circle of Spite, Aura of Silence (+ passive/slide as needed) |
| Buildables | **Elder Stone**, **Tome of Frailty** — O2 placement + persist |
| GFD | `GFD_Hero_Morgan` |

Implement abilities as BP `ULyraGameplayAbility` or new C++ subclasses. See [Breakaway_Reborn_Design_Spec.md](../../../AI_Planning/Breakaway_Reborn_Design_Spec.md).

## Gate 3 done when

- [ ] Plugin enabled + scan path `/Hero_Morgan/Characters/Heroes`
- [ ] Morgan Le Fay appears in hero select (4-hero E2 progress)
- [ ] ue-mcp confirms DA under Hexweaver folder

Then **Gate 4 — Gunslinger** ([Rawlins CONTENT_SETUP](../Rawlins/CONTENT_SETUP.md)).
