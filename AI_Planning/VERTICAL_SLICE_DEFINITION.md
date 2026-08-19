# Vertical slice definition

Locked scope for the Capture-the-Relic vertical slice.

**Last reviewed:** August 19, 2026  
**Planning hub:** [Planning docs](./README.md)  
**Status and next work:** [Project roadmap](./PROJECT_ROADMAP.md)

Decisions in this file are the slice bar. A larger project plan may replace them later. Until then, prefer this document over older “2 buildables per hero” or “gold spends on buildables” wording.

## Slice bar

Design-spec Capture-the-Relic: **4v4**, **four heroes**, **one persistable buildable per hero**, **match-only gold (earn, no spend)**.

## Scope table

| Area | In scope | Out of scope |
|------|----------|--------------|
| Mode | 4v4 listen server; bots fill to 8 | Dedicated server hardening |
| Heroes | Argus, Alona, Korryn, Rawlins | Full roster; campaign liberation |
| Buildables | **One** per hero; persist across rounds in a Best-of-5 match | Second buildable; SaveGame / meta |
| Gold | Earn in-match; persist between rounds; reset on match end | Spend on buildables or items; campaign SaveGame |
| Maps | `L_BW_DevMap` (CI / integration) and `L_BW_Dorado` (demo) at parity | New arena |
| Campaign | — | Ascension Rites |
| Art polish | Functional slice; placeholder or kit art OK | Production mythological theming |

Hero plugin names and folders: [Hero codename map](./HERO_CODENAME_MAP.md). Ability numbers: [Breakaway hero stats sheet](./Breakaway_Hero_Stats_Sheet.md).

## Definition of done

Status reflects the August 19, 2026, doc refresh. Editor 3/3 PIE is still required where noted.

### DevMap / Dorado gates

| Gate | Status |
|------|--------|
| 8 players (human + bot) listen server | C++ ready; prove on Step 22 mini-match |
| 4 distinct heroes selectable (duplicate-hero rule per team) | Kits landed; prove on Step 22 |
| Best-of-5 rounds, 3 win conditions | Implemented |
| Relic pass / throw / score | Implemented |
| Relic fumble-on-damage | **Open** (Step 22) |
| Gold earned in-match; persists round-to-round | Implemented (no spend in slice) |
| One buildable per hero; persist Round 1 → Round 2 | C++ ready; prove on Step 22 |
| Results → Lyra front-end | Widgets exist; `11-8` E2E still deferred |
| Dorado parity with DevMap | **Open** |

### Documentation gate

| Gate | Status |
|------|--------|
| [BreakawayCore systems index](../Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md) | Living |
| [Blueprint asset audit](./BLUEPRINT_ASSET_AUDIT.md) Editor-verified | **Stale** (May 25, 2026). Re-run after Step 22. |

## Related

- [Core loop implementation plan](../Plugins/GameFeatures/BreakawayCore/Docs/CoreLoop_Implementation_Plan.md)
- [Project architecture overview](./project_architecture_overview.md)
- [Breakaway Reborn design spec](./Breakaway_Reborn_Design_Spec.md)
