# Vertical Slice Definition (Source of Truth)

Locked scope from planning session — **2026-05-25**.

## Slice Bar

Design-spec Capture-the-Relic: **4v4**, **4 heroes**, **buildables**, **match-only gold**.

## Scope Table

| Area | In scope | Out of scope |
|------|----------|--------------|
| Mode | 4v4 listen server, bots fill to 8 | Dedicated server hardening |
| Heroes | Spartacus, Morgan, Alona, Rawlins | Full roster, campaign liberation |
| Buildables | 2 per hero, persist across rounds in match | SaveGame / meta |
| Gold | Persists between rounds; resets on match end | Campaign SaveGame |
| Maps | L_BW_DevMap + L_BW_Dorado parity | New arena |
| Campaign | — | Ascension Rites entirely |
| Art polish | Functional slice | Production mythological theming |

## Definition of Done

### DevMap / Dorado gates

- [ ] 8 players (human + bot) listen server
- [ ] 4 distinct heroes selectable (duplicate-hero rule)
- [ ] Best-of-5 rounds, 3 win conditions
- [ ] Relic pass/throw/score/fumble (content-dependent)
- [ ] Gold earned/spent between rounds within match
- [ ] 2 buildables per hero; persist Round 1 → Round 2
- [ ] Results → Lyra front-end

### Documentation gate

- [ ] [BreakawayCore/Docs/SYSTEMS_INDEX.md](../Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md) complete
- [ ] [BLUEPRINT_ASSET_AUDIT.md](./BLUEPRINT_ASSET_AUDIT.md) Editor-verified

## Doc Locations

| Purpose | Path |
|---------|------|
| System reference | `Plugins/GameFeatures/BreakawayCore/Docs/` |
| Planning / schedule | `AI_Planning/` |

## Hero content gates (before A1c)

All four heroes (**E2 + F3**) must pass plugin migration gates. Order: Spartacus → Alona → Morgan → Rawlins.  
Codenames: [HERO_CODENAME_MAP.md](./HERO_CODENAME_MAP.md). Gate 1 checklist: [Spartacus CONTENT_SETUP.md](../Plugins/GameFeatures/Heroes/Spartacus/CONTENT_SETUP.md).

## Related

- [HERO_CODENAME_MAP.md](./HERO_CODENAME_MAP.md)
- [project_architecture_overview.md](./project_architecture_overview.md)
- [PROJECT_ROADMAP.md](./PROJECT_ROADMAP.md)
- [Breakaway_Reborn_Design_Spec.md](./Breakaway_Reborn_Design_Spec.md)
