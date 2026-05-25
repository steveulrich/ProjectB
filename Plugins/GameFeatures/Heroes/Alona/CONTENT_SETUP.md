# Alona — Content Setup (Gate 2)

**Codename:** `Alona`. See [HERO_CODENAME_MAP.md](../../../AI_Planning/HERO_CODENAME_MAP.md).  
**A1c human player** — finish this gate before final playtest (D1).

Migrate from `/BreakawayCore/Characters/Heroes/Alona/` → `Hero_Alona` plugin (M1/N1/P1).

## Target (F3 + K2 + O2)

| Item | Notes |
|------|-------|
| Hero DA | `/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona` |
| Ability set | `DA_BW_AbilitySet_Alona` — K2 MVP (sun projectiles, knockback, heal patterns) |
| Buildables | **Sun Shrine**, **Starlight** — two DAs in `BuildableDataAssets[]`; O2 placement + persist |
| GFD | `GFD_Hero_Alona` |

## Fix before gate (audit)

- Wire `DA_BW_BuildableData_Alona` → `BP_BW_Alona_Buildable`; set `BuildableActorClass` + mesh
- Fill `BuildableDataAssets[]` (legacy single field + second buildable)
- Populate `AbilityDisplayInfos` if hero select UI is empty

## Gate 2 done when

- [ ] Plugin enabled in experience + scan path `/Hero_Alona/Characters/Heroes`
- [ ] Two buildable DAs; O2 placement works
- [ ] BreakawayCore `Alona/` deleted
- [ ] ue-mcp hero search finds Alona from plugin path

Then **Gate 3 — Hexweaver** ([Morgan CONTENT_SETUP](../Morgan/CONTENT_SETUP.md)).
