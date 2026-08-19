# Breakaway project roadmap

Current priorities for the Capture-the-Relic vertical slice.

**Last reviewed:** August 19, 2026  
**Scope:** [Vertical slice definition](./VERTICAL_SLICE_DEFINITION.md)  
**Step-by-step checklists:** [Core loop implementation plan](../Plugins/GameFeatures/BreakawayCore/Docs/CoreLoop_Implementation_Plan.md)  
**Doc hub:** [Planning docs](./README.md)

This file is the status and priority list. The core loop plan is the implementation checklist. A larger project plan is expected later; until then, use this page for “what next.”

## Current status

The C++ match loop, match HUD, and four hero kits are in the repo. The next feature gate is **Section 3 Step 22** (capstone). Several editor 3/3 PIE checklists from Steps 17–20.5 have no matching `core-loop: step N passed` commit.

| Area | Status | Evidence |
|------|--------|----------|
| Humanoid relic loop | Complete | Core loop Steps 0–10 |
| Match HUD (scores, relic, health, portraits, post-round, post-match) | Complete | Steps 12–16 plus Step 19.5 |
| In-match phase FSM | Complete | Steps 11-1–11-7 and 11b |
| Front-end queue E2E | Deferred | Step `11-8`; matchmaking C++ (`11-MM-1`–`11-MM-4`) landed |
| Argus, Alona, Korryn, Rawlins kits + one buildable each | C++ landed | Steps 18–21; Step 21 has a formal pass commit |
| Fumble-on-damage | Not implemented | `ForcedFumbles` exists on `ABwayPlayerState`; no drop-on-damage gameplay |
| Hero-select staging E2E | Open | Map and routing exist; Step 22 checklist |
| Dorado parity | Open | `L_BW_Dorado` exists; not gated against DevMap |
| 200 ms relic latency test | Open | Relic polish remaining |

## Completed

### Match loop

- Round-based Capture-the-Relic with goal, elimination, and timer win conditions
- Sudden death at 0:00 with midfield divider
- Relic pickup, throw, pass, walk-in score, and throw-in score
- Relic bot AI (pickup and walk-in scoring)
- Bot backfill to eight players
- Match-flow config (`UBwayMatchFlowConfig`) driving `UBwayRoundManagementComponent`

### HUD and results

- Score, relic status, health, and team portrait slots
- Ability bar, gold counter, and relic-mode slot swap (Step 19.5)
- Post-round summary and post-match stats / MVP

### Heroes and buildables

- Four Game Feature plugins with kit configs
- One buildable per hero: Siege Engine, Sun Shrine, Cursed Ward, Jail
- Once-per-round free placement; persist between rounds
- Relic carrier blocks combat abilities; slide, Request Relic, and PlaceBuildable stay allowed

### Entry and routing (C++)

- `ForceHumanoid`, `Hero=` URL, skip-select-without-skip-apply
- Direct PIE hero UI and round-management hero-lock gate
- Hero-select staging map and playlist routing
- Matchmaking goal definitions and mock queue travel (`11-MM-1`–`11-MM-4`)

## Next work

Tackle in this order unless a new project plan says otherwise.

### 1. Section 3 capstone (Step 22)

Highest-priority feature work.

| ID | Task |
|----|------|
| 22-1 | Fumble-on-damage: carrier drops relic on damage; increment `ForcedFumbles` |
| 22-2 | Enable all four hero plugins on `B_BW_Experience_Dev` |
| 22-3 | Staging E2E: front-end tile → `L_BW_HeroSelect_Staging` → lock → match |
| 22-4 | Step 19.5 HUD regression on all four heroes |
| 22-5 | Four-hero 4v4 listen-server mini-match |
| 22-6 | `ForceHumanoid=1` Section 1 relic regression |
| 22-7 | Align design-spec wording for one buildable per hero and match-only gold |

Pass criteria live in the [core loop plan, Step 22](../Plugins/GameFeatures/BreakawayCore/Docs/CoreLoop_Implementation_Plan.md#step-22--section-3-capstone).

### 2. Editor verification (optional before 22)

Run if you need confidence that Steps 17–20.5 are actually green in PIE:

- Step 17: `ForceHumanoid`, `Hero=`, staging travel, once-per-round buildable
- Alona 19c parity vs the stats sheet
- Korryn 20c parity vs the stats sheet
- Step 20.5 kit DAs resolve for Argus and Alona

### 3. Match entry

- Finish editor checklists for `11-MM-1`–`11-MM-4`
- Step `11-8`: front-end → match → results → menu (3/3 cold starts)

### 4. Slice polish (after Step 22)

- `L_BW_Dorado` spawn, goal, and relic parity with DevMap
- Relic latency playtest at 200 ms
- Ability cooldown presentation and remaining HUD art
- Between-round planning UI (C++ delegate exists)
- Assist tracking (TODO on `ABwayCharacterWithAbilities`)
- Packaged Windows build

## Explicitly deferred

Do not pull these into the current slice unless the upcoming project plan says so:

- Ascension Rites campaign
- SaveGame / meta gold
- Gold spend on stat enhancers or items (earn-only in slice)
- Second buildable per hero (slice is **one** per hero)
- Dedicated-server hardening
- Production mythological art pass
- Full 11-hero roster

## Tech debt (non-blocking)

- Duplicate team / score / relic state on `ABwayGameState` versus GameState components
- `PlayingPhaseTag` still defaults to `ShooterGame.GamePhase.Playing`
- ShooterCTF spawn-tag coupling
- Slide net steering under latency

## Success bar (slice)

From the [vertical slice definition](./VERTICAL_SLICE_DEFINITION.md):

- 4v4 listen server with bots filling to eight
- Four distinct heroes, duplicate-hero rule per team
- Best-of-5, three win conditions
- Relic pass, throw, score, and fumble
- Gold earned in-match (no spend)
- One persistable buildable per hero
- Results return to the Lyra front-end
