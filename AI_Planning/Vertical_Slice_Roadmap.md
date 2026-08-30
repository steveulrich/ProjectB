# Breakaway vertical slice roadmap

Short priority list for the design-spec slice. For status tables and remaining tasks, see [Project roadmap](./PROJECT_ROADMAP.md). For PIE checklists, see [Core loop implementation plan](../Plugins/GameFeatures/BreakawayCore/Docs/CoreLoop_Implementation_Plan.md).

**Last reviewed:** August 27, 2026  
**Scope:** [Vertical slice definition](./VERTICAL_SLICE_DEFINITION.md)

## Current state

- **Match loop:** Rounds, scoring, relic, teams, bots, and sudden death run on `ABwayGameState` components.
- **HUD:** Slot-injected match HUD (Steps 12–16 and 19.5).
- **Heroes:** Argus, Alona, Korryn, and Rawlins each have a C++ kit, kit config, and one buildable.
- **Capstone:** Step 22 C++ (fumble-on-damage) landed; editor PIE still open.
- **Content in git:** `.uasset` files are Editor-local. Treat [Blueprint asset audit](./BLUEPRINT_ASSET_AUDIT.md) as stale until re-run.

## Priority order

1. **Step 22 PIE** — fumble, staging E2E, four-hero match, HUD regression, leftover 17/19c/20c/20.5 3/3.
2. **Front-end E2E (`11-8`)** — menu → match → results → menu.
3. **Dorado parity** — same spawn, goal, and relic wiring as DevMap.
4. **Net polish** — 200 ms relic latency playtest (`Net PktLag=200`); slide steering if jitter shows up.

## Explicitly deferred

- Ascension Rites campaign
- SaveGame / meta gold
- Second buildable per hero
- Production art pass

## Technical debt

- Consolidate duplicate GameState versus component state
- Migrate off ShooterCTF spawn tag coupling
- Slide net steering under latency
