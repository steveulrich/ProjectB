# Breakaway Vertical Slice - Asset Map & Interactive Visualization

> **Scope:** [VERTICAL_SLICE_DEFINITION.md](./VERTICAL_SLICE_DEFINITION.md) · **Audit:** [BLUEPRINT_ASSET_AUDIT.md](./BLUEPRINT_ASSET_AUDIT.md)

## Overview

This document explains the comprehensive asset mapping system created for the Breakaway vertical slice project. The interactive visualization provides a complete breakdown of every asset (existing and planned) needed to complete the vertical slice, along with their dependencies and integration into the game flow.

## Files

### `VerticalSlice_AssetMap.html`
A self-contained, interactive HTML website that visualizes:
- **Complete asset inventory** (100+ assets mapped)
- **Game flow from startup to match end** (6 stages)
- **Asset dependencies** (what each asset requires and what requires it)
- **Development roadmap** (10 phases mapped to roadmap)
- **Project statistics** and progress tracking

## How to Use

### 1. Opening the Visualization

Simply open `VerticalSlice_AssetMap.html` in any modern web browser:
- **Windows**: Double-click the file or right-click → Open with → Your browser
- **Command Line**: `start VerticalSlice_AssetMap.html` (Windows) or `open VerticalSlice_AssetMap.html` (Mac/Linux)

### 2. Navigation

The interface has 4 main tabs:

#### 📊 Overview Tab
- **Project Statistics**: See total assets, completion status breakdown
- **Key Systems Overview**: Quick view of each system's progress
- **Legend**: Understanding status indicators
  - 🟢 **Complete**: Fully implemented and tested
  - 🟡 **Partial**: Exists but needs work or polish
  - 🔴 **Planned**: Not yet implemented

#### 🎯 Game Flow Tab
Shows the complete player journey through 6 stages:

1. **🚀 Game Startup** - Initial load and system initialization
2. **🔍 Matchmaking** - Finding and joining matches
3. **⚙️ Match Initialization** - Map load and game mode setup
4. **🦸 Hero Selection** - Player hero selection phase
5. **⚔️ Match Gameplay** - Main gameplay loop
6. **🏆 Match End** - Match conclusion and results

**How to use:**
- Click on any stage to expand it
- See all assets involved in that stage
- Click on individual assets to see details

#### 📦 All Assets Tab
Complete searchable asset inventory:

**Search**: Type in the search box to filter assets by:
- Name
- Type (C++ class, Blueprint, Widget, Data Asset)
- Category
- Description

**Filters**: Quick filter buttons for:
- Status (All, Complete, Partial, Planned)
- Type (C++ Classes, Blueprints, Widgets, Data Assets)

**Categories**: Assets organized into:
- Core Systems
- Hero System
- Heroes (4 planned heroes)
- Abilities
- Relic System
- Scoring System
- Spawn System
- Buildable System
- Buildables (4 planned buildables)
- UI System
- Map
- Lyra Core (base engine systems)

#### 🗺️ Roadmap Tab
10-phase development roadmap showing:
- Phase name and status
- Task breakdown with checkboxes
- Progress tracking

**Phases:**
1. ✅ Core Game Mode (COMPLETE)
2. 🔄 Relic System Polish (IN PROGRESS)
3. 📋 First Hero - Spartacus (PLANNED)
4. 📋 Buildable System (PLANNED)
5. 📋 UI/UX Polish (PLANNED)
6. 📋 Second Hero - Morgan (PLANNED)
7. 📋 Heroes 3 & 4 (PLANNED)
8. 📋 Multiplayer Testing (PLANNED)
9. 📋 Map Design (PLANNED)
10. 📋 Final Integration (PLANNED)

### 3. Asset Details Modal

Click on any asset card to open a detailed modal showing:

#### Asset Information
- **Name** and **Type**
- **Status** (Complete/Partial/Planned)
- **Description** of functionality
- **File Path** (if applicable)

#### Dependencies
**"📦 Dependencies"** - Assets that this asset REQUIRES
- Example: `BreakawayGameMode` depends on `BwayGameState`, `RelicActor`, etc.
- Click on any dependency to navigate to its details

#### Referenced By
**"🔗 Referenced By"** - Assets that REQUIRE this asset
- Example: `LyraGameMode` is referenced by `BreakawayGameMode`
- Shows how removing/changing this asset would affect others

#### Game Flow Stages
**"🎯 Game Flow Stages"** - Which game flow stages use this asset
- Helps understand when and where the asset is active

### 4. Interaction Features

- **Click asset cards** to see full details and dependencies
- **Click dependencies** in the modal to navigate between related assets
- **Click flow stages** to expand and see involved assets
- **Click category headers** in Overview to filter by that category
- **Use search** to quickly find specific assets
- **Use filters** to view assets by status or type

## Asset Inventory Breakdown

### Total Assets Mapped: 100+

#### By Category:
- **Core Systems**: 5 assets (Game Mode, Game State, Player State, Experience, etc.)
- **Hero System**: 6 assets (Character class, Movement, Selection UI, etc.)
- **Heroes**: 4 planned heroes (Spartacus, Morgan, Valkyrie, Robin)
- **Abilities**: 16+ abilities (4 per hero)
- **Relic System**: 7 assets (Actor, Settings, Abilities, Replication)
- **Scoring System**: 1 asset (Goal Volume)
- **Spawn System**: 3 assets (Manager, Spawn Points, Data)
- **Buildable System**: 7 assets (Base classes, Turret, Trap, Placement)
- **Buildables**: 4 planned (Barricade, Mana Shrine, Healing Totem, Arrow Trap)
- **UI System**: 13 widgets (HUD, Hero Select, Score, Timer, etc.)
- **Map**: 1 arena map
- **Lyra Core**: 9 referenced base engine systems

#### By Type:
- **C++ Classes**: ~45 classes
- **Blueprints**: ~5 blueprints
- **Widget Blueprints**: ~13 widgets
- **Data Assets**: ~30 data assets
- **Maps**: 1 map

#### By Status:
- **Complete**: ~25 assets (Phase 1 deliverables)
- **Partial**: ~20 assets (Exist but need polish)
- **Planned**: ~60 assets (To be implemented)

## Key Asset Relationships

### Critical Dependencies

1. **BreakawayGameMode** (Hub)
   - Depends on: BwayGameState, RelicActor, BwayGoalVolume, Spawn System
   - Referenced by: B_BWayGameMode, BreakawayExperience, All UI Widgets

2. **BwayCharacterWithAbilities** (Player Character)
   - Depends on: LyraCharacter, BwayHeroDataAsset, Movement Component
   - Referenced by: All Abilities, RelicActor, BuildableBase

3. **BwayHeroDataAsset** (Hero Definition)
   - Depends on: LyraAbilitySet, BuildableBase
   - Referenced by: All 4 Heroes, Hero Selection UI

4. **RelicActor** (Core Gameplay Object)
   - Depends on: RelicSettings, MovementReplication, AbilitySystem
   - Referenced by: GameMode, GoalVolume, All Relic Abilities

5. **W_HUD_Layout** (Main UI Container)
   - Depends on: LyraHUDLayout, All HUD widgets
   - Referenced by: BreakawayExperience

### Dependency Chains

**Complete Startup Chain:**
```
Game Launch
  → LyraGameInstance
    → BreakawayExperience
      → BreakawayGameMode
        → BwayGameState
        → BwaySpawnPointManagerComponent
        → Map Load (L_BreakawayArena)
```

**Hero Selection Chain:**
```
Hero Selection Phase
  → W_HeroSelectWidget
    → BwayHeroSelectionPhaseComponent
      → BwayHeroSelectionManager
        → BwayPlayerState
          → BwayHeroDataAsset (DA_Hero_Spartacus, etc.)
```

**Match Gameplay Chain:**
```
Match Start
  → BreakawayGameMode spawns players
    → BwayCharacterWithAbilities
      → BwayHeroDataAsset grants abilities
        → LyraAbilitySystemComponent
          → Individual Abilities (GA_ShieldBash, etc.)
```

## Use Cases

### For Development Planning
1. **Identify Dependencies**: Before implementing an asset, check what it depends on
2. **Impact Analysis**: See what will be affected by changes to an asset
3. **Work Sequencing**: Use dependency chains to determine implementation order

### For Progress Tracking
1. **Visual Progress**: See completion status at a glance
2. **Phase Tracking**: Monitor which roadmap phases are on track
3. **Priority Identification**: Focus on high-dependency assets first

### For Documentation
1. **Asset Reference**: Quick lookup of asset purpose and location
2. **System Understanding**: See how systems interconnect
3. **Onboarding**: New developers can visualize the project structure

### For Design Review
1. **Completeness Check**: Ensure all necessary assets are planned
2. **Dependency Validation**: Verify logical relationships
3. **Scope Management**: Understand full vertical slice requirements

## Updating the Asset Map

The asset database is contained within the HTML file as a JavaScript object. To add or modify assets:

1. Open `VerticalSlice_AssetMap.html` in a text editor
2. Find the `assetDatabase` object (starts around line 500)
3. Add/modify asset entries following this structure:

```javascript
"AssetID": {
    id: "AssetID",
    name: "Display Name",
    type: "cpp_class|blueprint|widget_blueprint|data_asset|map",
    category: "Category Name",
    status: "complete|partial|planned",
    description: "What this asset does",
    path: "File path (optional)",
    dependencies: ["Asset1", "Asset2"],
    referencedBy: ["Asset3", "Asset4"],
    gameFlowStages: ["startup", "match_gameplay"]
}
```

4. Save and refresh the browser

## Technical Details

### Technologies Used
- **HTML5**: Structure
- **CSS3**: Styling with custom properties
- **Vanilla JavaScript**: All interactivity (no frameworks)
- **Responsive Design**: Works on desktop and mobile

### Features
- ✅ Fully self-contained (no external dependencies)
- ✅ Works offline
- ✅ Fast and responsive
- ✅ Searchable and filterable
- ✅ Interactive dependency navigation
- ✅ Persistent state (tab selection)

### Browser Compatibility
- Chrome/Edge: ✅ Full support
- Firefox: ✅ Full support
- Safari: ✅ Full support
- Mobile browsers: ✅ Responsive design

## Project Insights

### Current State (as of mapping)
- **Phase 1 Complete**: Core game mode with all win conditions
- **Phase 2 In Progress**: Relic system polish needed
- **67% of assets planned**: Need implementation
- **Strong foundation**: Lyra framework provides robust base

### Critical Path to Vertical Slice
1. ✅ Core Game Mode (DONE)
2. 🔄 Relic Polish (IN PROGRESS)
3. 🎯 Spartacus Hero (NEXT - blocks other heroes)
4. 🎯 Buildable System (NEXT - required for heroes)
5. 🎯 UI/UX (PARALLEL - can develop alongside heroes)
6. 🎯 3 More Heroes (SEQUENTIAL - reuse patterns)
7. 🎯 Multiplayer Testing (FINAL)

### Estimated Completion
- **Remaining Assets**: ~60 to implement
- **12-week timeline** (as per roadmap)
- **Accelerated**: 8-10 weeks with scope reduction

## Recommendations

### For Immediate Use
1. **Review Game Flow tab** to understand player journey
2. **Check Phase 2 tasks** in Roadmap tab for current work
3. **Use asset details** to understand dependencies before coding
4. **Filter by "planned" status** to see remaining work

### For Planning
1. **Use dependency chains** to sequence work
2. **Identify reusable patterns** (e.g., hero abilities structure)
3. **Prioritize high-dependency assets** (e.g., BwayHeroDataAsset before heroes)
4. **Plan parallel work** (UI can be developed alongside heroes)

### For Team Communication
1. **Share the visualization** with team members
2. **Use as reference** in planning meetings
3. **Track progress** by updating status fields
4. **Document decisions** in asset descriptions

## Conclusion

This interactive asset map serves as the **single source of truth** for the Breakaway vertical slice development. It provides:

- ✅ Complete asset inventory
- ✅ Dependency mapping
- ✅ Game flow visualization
- ✅ Progress tracking
- ✅ Development roadmap integration

Use it as your guide to ensure nothing is missed and to understand how all the pieces fit together to create the complete vertical slice experience.

---

**Created**: November 29, 2025  
**Version**: 1.0  
**Scope**: Breakaway Vertical Slice (4v4 Match, 4 Heroes, Core Systems)

