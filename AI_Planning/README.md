# Breakaway Planning & Architecture Hub

**Last Reviewed:** October 15, 2026** October 13, 2026
**Engine:** Unreal Engine `5.8.1` Source Build (`LyraEditor` / `LyraGame`)

Use this index to navigate the active `AI_Planning/` working set. Historical narratives, superseded gate docs, and reference Lua files are isolated in `AI_Planning/Archive/` and must **never** be read during routine gates.

---

## 1. Executive Status Snapshot (October 2026)

| Track | Current Status | Primary Evidence / Reference |
|---|---|---|
| **Core 4v4 Relic Loop & 240s Demo Rules** | **PASSED** (Bo5 / First-to-3, Goal, Elimination, Territory Tiebreaker, Ownership at 200ms) | [DEMO_MATCH_RULES.md](./DEMO_MATCH_RULES.md) · [RELIC_PICKUP_OWNERSHIP.md](./RELIC_PICKUP_OWNERSHIP.md) |
| **Packaged LAN Transport & Rematch/Return** | **PASSED** (4-process Host/Join, Late-Arrival, Rejected-Travel Retry, Rematch, Connected Return at 200ms) | [PACKAGED_LAN_ACCEPTANCE.md](./PACKAGED_LAN_ACCEPTANCE.md) · [LATE_ARRIVAL_SELECTION_ACCEPTANCE.md](./LATE_ARRIVAL_SELECTION_ACCEPTANCE.md) |
| **Match Economy & 6-Item Upgrade Shop** | **PASSED** (50g Kill / 25g Assist credit, 4-slot cap, Attack, Armor, MoveSpeed, CDR, LifeLeech, RespawnReduction at 200ms) | [COMBAT_CREDIT_ACCEPTANCE.md](./COMBAT_CREDIT_ACCEPTANCE.md) · [SHOP_INPUT_ACCEPTANCE.md](./SHOP_INPUT_ACCEPTANCE.md) · [COOLDOWN_UPGRADE_ACCEPTANCE.md](./COOLDOWN_UPGRADE_ACCEPTANCE.md) · [LIFE_LEECH_ACCEPTANCE.md](./LIFE_LEECH_ACCEPTANCE.md) · [RESPAWN_UPGRADE_ACCEPTANCE.md](./RESPAWN_UPGRADE_ACCEPTANCE.md) |
| **Persistent Buildables (4 Heroes)** | **PASSED** (Shared `FBwayPlacementQuery`, obstruction recovery, 3-round persistence, active caps, Jail trap immunity at 200ms) | [BUILDABLE_AUTHORITY_ACCEPTANCE.md](./BUILDABLE_AUTHORITY_ACCEPTANCE.md) |
| **Slide Physics & Lifecycle** | **PASSED** (4-process packaged at 100ms + 20/20 Oct 1–9 gravity/wall/ramp/camera-shake repairs) | [SLIDE_LIFECYCLE_ACCEPTANCE.md](./SLIDE_LIFECYCLE_ACCEPTANCE.md) |
| **4-Hero Primary & Barrage Animations** | Recorded packaged checks passed; review corrections and scope limits are tracked in the ledger | [RAWLINS_BARRAGE_ACCEPTANCE.md](./RAWLINS_BARRAGE_ACCEPTANCE.md) · [SLICE_STATUS.md](./SLICE_STATUS.md) |
| **Current Goal Queue (Goals 1–11)** | Goals 1–3 have scoped evidence with review gaps; expanded polish Goals 4–9 precede roster Goals 10–11. Implementation is paused. | [MASTER_GOAL_QUEUE.md](./MASTER_GOAL_QUEUE.md) · [SLICE_STATUS.md](./SLICE_STATUS.md) · [PROJECT_ROADMAP.md](./PROJECT_ROADMAP.md) |

---

## 2. Which Document to Open

| Need | Authoritative Document |
|---|---|
| **Active gate state & current candidate** | [SLICE_STATUS.md](./SLICE_STATUS.md) |
| **Copy-pasteable `/goal` execution prompts** | [MASTER_GOAL_QUEUE.md](./MASTER_GOAL_QUEUE.md) |
| **Remaining milestones & priorities** | [PROJECT_ROADMAP.md](./PROJECT_ROADMAP.md) |
| **Agent operating rules & circuit breakers** | [WORKFLOW_EFFICIENCY.md](./WORKFLOW_EFFICIENCY.md) |
| **Locked vertical-slice scope** | [Vertical Slice Definition](./VERTICAL_SLICE_DEFINITION.md) |
| **Game design rules & formulas (1:1 Alpha)** | [Breakaway Reborn Design Spec](./Breakaway_Reborn_Design_Spec.md) |
| **Dated September 2017 reference evidence** | [Final Public Alpha Parity](./FINAL_ALPHA_PARITY.md) |
| **UE 5.8 C++ & plugin architecture** | [Project Architecture Overview](./project_architecture_overview.md) |
| **UE 5.8 C++/GAS/Net best practices** | [Unreal Engine Best Practices Bible](./UNREAL_ENGINE_BEST_PRACTICES_BIBLE.md) |
| **All 11 heroes' stats, scaling & buildables** | [Breakaway Hero Stats Sheet](./Breakaway_Hero_Stats_Sheet.md) |
| **Hero display names vs plugin folders** | [Hero Codename Map](./HERO_CODENAME_MAP.md) |
| **Lyra base classes vs Breakaway overrides** | [Lyra Base Reference](./Lyra_Base_Reference.md) |
| **Demo & DevMap match rules** | [Demo Match Rules](./DEMO_MATCH_RULES.md) |
| **Staging-to-Dorado NetGUID investigation** | [Travel GUID Investigation](./TRAVEL_GUID_INVESTIGATION.md) |

---

## 3. Directory Organization & Archive Policy

- **`AI_Planning/` (Active Working Set):**
  - Core specifications, roadmap, `SLICE_STATUS.md`, `WORKFLOW_EFFICIENCY.md` (`<= 120` lines), `MASTER_GOAL_QUEUE.md`, and condensed `*_ACCEPTANCE.md` / `*_ANIMATION.md` gate summaries (`<= 100` lines each).
- **`AI_Planning/Archive/` (Cold Storage — Do NOT Read During Routine Gates):**
  - `Archive/Full_History_20261013/`: Unabridged pre-condensation gate diaries and the 565-line historical workflow narrative.
  - `Archive/Superseded_Gates/`: Early milestone gates superseded by later consolidated multi-upgrade/transport gates.
  - `Archive/Workflow_Artifacts/`: Historical token audits and one-off step logs.
  - `Archive/Lumberyard_Lua_Reference/`: Reference Lumberyard Lua character state scripts (`basecharacter_state*.lua`).
- **`Plugins/GameFeatures/BreakawayCore/Docs/`:**
  - Deep-dive C++ class tables ([SYSTEMS_INDEX.md](../Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md)), onboarding ([BreakawayCore_Onboarding.md](../Plugins/GameFeatures/BreakawayCore/Docs/BreakawayCore_Onboarding.md)), and LAN frontend verification notes.
