# Breakaway planning docs

Hub for slice scope, current status, and where detailed docs live.

**Last reviewed:** August 19, 2026

Use this page to pick the right document. Do not treat older roadmaps, the HTML asset map, or May 2026 editor audits as live status.

A larger project plan is expected later. Until that lands, treat the following documents as the working set.

## Status snapshot

Breakaway is a Capture-the-Relic vertical slice on Lyra (Unreal Engine 5.5). Gameplay lives in the `BreakawayCore` Game Feature plugin plus four hero plugins.

| Track | Status |
|-------|--------|
| Section 1 — Humanoid relic loop (Steps 0–10) | Complete |
| Section 2 — Match HUD (Steps 12–16) | Complete |
| Step 11 — In-match flow orchestrator | Complete (`11-8` front-end E2E deferred) |
| Section 3 — Four heroes (Steps 17–21) | C++ kits landed; Step 21 passed; some editor 3/3 PIE gates still open |
| Step 22 — Section 3 capstone | **Next feature gate** |
| Dorado map parity and packaged demo | Open |

**Next feature work:** Step 22 (fumble-on-damage, staging end-to-end, four-hero match, HUD regression). For the step-by-step checklists, see [Core loop implementation plan](../Plugins/GameFeatures/BreakawayCore/Docs/CoreLoop_Implementation_Plan.md).

## Which document to use

| If you need | Open |
|-------------|------|
| Current priorities and remaining work | [Project roadmap](./PROJECT_ROADMAP.md) |
| Locked slice scope (in / out) | [Vertical slice definition](./VERTICAL_SLICE_DEFINITION.md) |
| Ordered implementation steps and PIE checklists | [Core loop implementation plan](../Plugins/GameFeatures/BreakawayCore/Docs/CoreLoop_Implementation_Plan.md) |
| How systems fit together | [Project architecture overview](./project_architecture_overview.md) |
| C++ class tables, tags, Blueprint hooks | [BreakawayCore systems index](../Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md) |
| Hero display names vs plugin folders | [Hero codename map](./HERO_CODENAME_MAP.md) |
| Ability numbers, cooldowns, buildables | [Breakaway hero stats sheet](./Breakaway_Hero_Stats_Sheet.md) |
| Design intent (rules, fantasy) | [Breakaway Reborn design spec](./Breakaway_Reborn_Design_Spec.md) |
| Lyra versus Breakaway ownership | [Lyra base reference](./Lyra_Base_Reference.md) |
| DevMap smoke test | [DevMap playtest guide](./DEVMAP_PLAYTEST_GUIDE.md) |
| Optional interactive flow graph | [Gameplay flow map](../Docs/GameplayFlowMap/README.md) |

## Living vs historical

**Living** (update when status changes):

- This hub
- [Project roadmap](./PROJECT_ROADMAP.md)
- [Vertical slice definition](./VERTICAL_SLICE_DEFINITION.md)
- [Vertical slice roadmap](./Vertical_Slice_Roadmap.md)
- [Core loop implementation plan](../Plugins/GameFeatures/BreakawayCore/Docs/CoreLoop_Implementation_Plan.md)
- [Project architecture overview](./project_architecture_overview.md)
- [BreakawayCore systems index](../Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md)
- [Heroes README](../Plugins/GameFeatures/Heroes/README.md)

**Historical or editor- lagging** (read with a date check):

| Document | Why it lags |
|----------|-------------|
| [Blueprint asset audit](./BLUEPRINT_ASSET_AUDIT.md) | Last Editor-verified May 25, 2026. Hero plugins and HUD assets landed after that audit. |
| [Asset map README](./ASSET_MAP_README.md) and `VerticalSlice_AssetMap.html` | November 2025 visualization. Roster, phases, and buildable counts are obsolete. |
| Per-hero `*_Editor_Setup.md` files under BreakawayCore/Docs | Checklists for a single step. Status belongs in the core loop plan, not in each setup file. |

## Roster (Section 3)

Official UI names differ from plugin and folder names. For paths, see [Hero codename map](./HERO_CODENAME_MAP.md).

| Display name | Plugin | Folder | Buildable (one per hero) |
|--------------|--------|--------|--------------------------|
| Argus | `Hero_Spartacus` | Argus | Siege Engine |
| Alona | `Hero_Alona` | Alona | Sun Shrine |
| Korryn | `Hero_Morgan` | Hexweaver | Cursed Ward |
| Rawlins | `Hero_Rawlins` | Gunslinger | Jail |

## Doc locations

| Purpose | Path |
|---------|------|
| Planning, scope, and this hub | `AI_Planning/` |
| Runtime system reference and editor checklists | `Plugins/GameFeatures/BreakawayCore/Docs/` |
| Per-hero plugin setup | `Plugins/GameFeatures/Heroes/<Name>/CONTENT_SETUP.md` |

Keep facts in one place: system behavior in BreakawayCore docs; priority and schedule in `AI_Planning/`. Cross-link; do not copy long checklists into both trees.
