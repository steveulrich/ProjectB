# Vertical slice definition

Locked scope for the Capture-the-Relic vertical slice.

**Last reviewed:** September 5, 2026
**Planning hub:** [Planning docs](./README.md)  
**Status and next work:** [Project roadmap](./PROJECT_ROADMAP.md)

Decisions in this file are the slice bar. The September 5 direction below takes precedence over the earlier scope table where they conflict. The final public alpha is the original-game reference; historical exclusions are not evidence that those systems satisfy the new bar.

## September 5 owner direction

- Prioritize missing and unfinished functionality as well as replication defects.
- All systems other than final art must reach original Breakaway final-public-alpha parity, excluding new features. Reconcile prior exclusions (including earn-only gold and buildable count) against evidence from that version; do not mix requirements from different alpha versions.
- Placeholder assets are acceptable only when their complete integration is finished. Compatible final assets must be replaceable through documented asset/configuration interfaces without rewriting gameplay or changing replicated behavior. Document required skeletons, sockets, animation events, dimensions, collision, and material/UI interfaces; arbitrary incompatible assets cannot be assumed interchangeable.
- Required delivery is LAN with a working front end: host/find/join, selection/staging, complete match, results, and return to the front end. Internet services are not a required delivery gate.
- Primary test setup: four human players on a listen server, one PIE player and three separate client instances, with bots filling the 4v4 match. Also verify the packaged LAN flow; successful direct PIE entry alone is insufficient.
- Keep Breakaway as the working name. Final setting, character identities, and kits are not locked. Existing four kits remain the implementation/test baseline until the parity scope is resolved.
- The owner identified the other task as “Vertical Slice Implementation” and authorized treating its existing buildable, HUD, match-flow, and editor-bridge changes as complete and committing them. Preserve that implementation as the working baseline; completion of the expanded slice still requires verification.

## Earlier implementation baseline (pending parity reconciliation)

Design-spec Capture-the-Relic: **4v4**, **four heroes**, **one persistable buildable per hero**, **match-only gold (earn, no spend)**.

## Earlier scope table (September 5 direction takes precedence)

| Area | In scope | Out of scope |
|------|----------|--------------|
| Mode | 4v4 listen server; bots fill to 8; real LAN front-end host/find/join | Dedicated server hardening; internet session service requirement |
| Heroes | Argus, Alona, Korryn, Rawlins | Full roster; campaign liberation |
| Buildables | **One** per hero; persist across rounds in a Best-of-5 match | Second buildable; SaveGame / meta |
| Gold | Earn in-match; persist between rounds; reset on match end | Spend on buildables or items; campaign SaveGame |
| Maps | `L_BW_DevMap` (CI / integration) and `L_BW_Dorado` (demo) at parity | New arena |
| Campaign | — | Ascension Rites |
| Art polish | Complete presentation integration using replaceable placeholder assets | Final production art and locked setting |

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
| Relic fumble-on-damage | **C++ landed** (Step 22 PIE) |
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
