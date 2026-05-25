# Breakaway Vertical Slice - Project Roadmap

> **Scope:** [VERTICAL_SLICE_DEFINITION.md](./VERTICAL_SLICE_DEFINITION.md)  
> **Playtest:** [DEVMAP_PLAYTEST_GUIDE.md](./DEVMAP_PLAYTEST_GUIDE.md)

## ✅ PHASE 1: Core Game Mode (COMPLETED)
**Status:** Ready for integration  
**Priority:** CRITICAL

### Deliverables
- [x] Round-based game mode with state management
- [x] All 3 win conditions (Goal, Elimination, Timer)
- [x] Team management and auto-balancing
- [x] Score tracking with replication
- [x] Goal trigger volumes
- [x] Player respawning

### Files Created
- BwayGameState.h/cpp
- BreakawayGameMode.h/cpp  
- BwayGoalVolume.h/cpp
- Integration documentation

---

## 🔄 PHASE 2: Relic System Polish (IN PROGRESS)
**Priority:** HIGH

### C++ Done (2026-05)
- [x] `URelicMovementReplicationComponent` smoothing
- [x] `LastPossessingTeam` replicated tracking
- [x] Goal volume scoring cooldown + settle delay
- [x] `bHasScoredThisRound` guard
- [x] Client throw prediction RPC

### Remaining
- [ ] 200ms latency playtest checklist
- [ ] Relic VFX/audio content
- [ ] Fumble-on-damage (design spec)

See [Relic_System.md](../Plugins/GameFeatures/BreakawayCore/Docs/Relic_System.md).

---

## 🦸 PHASE 3: Spartacus (C++ DONE — CONTENT NEXT)
**Priority:** HIGH

### C++ Done
- [x] Shield Bash, War Cry, Defensive Stance, Gladiator's Leap
- [x] Relic carrier blocks combat via `Gameplay.State.RelicCarrier`

### Editor Tasks
- [ ] Hero data asset, mesh, ability BPs — [CONTENT_SETUP.md](../Plugins/GameFeatures/Heroes/Spartacus/CONTENT_SETUP.md)
- [ ] Fire Catapult + Dragon Spire buildables

---

## 🦸 PHASE 4: Heroes Morgan, Alona, Rawlins
**Priority:** HIGH

- [x] Game Feature plugin templates + CONTENT_SETUP per hero
- [ ] Ability BPs/C++ per design spec
- [ ] 2 buildables each (6 remaining after Spartacus)

See [Plugins/GameFeatures/Heroes/README.md](../Plugins/GameFeatures/Heroes/README.md).

---

## 🏗️ PHASE 5: Buildables & Persistence (C++ DONE — CONTENT NEXT)
**Priority:** HIGH

### C++ Done
- [x] `bPersistsBetweenRounds` + round reset logic
- [x] `UBwayBuildableRegistryComponent`
- [x] `OnBetweenRoundPlanningStarted` delegate
- [x] `BuildableDataAssets[]` on hero data (2 per hero)

### Remaining
- [ ] 8 buildable BPs + data assets
- [ ] Between-round planning UI (BP)

See [Buildable_System.md](../Plugins/GameFeatures/BreakawayCore/Docs/Buildable_System.md).

---

## 🎮 PHASE 6: 4v4 Listen Server + Dorado + UI (IN PROGRESS)

### C++ Done
- [x] Bot backfill to 8 (`UBwayBotCreationComponent`)
- [x] Scoreboard K/D/A/objective from PlayerState
- [x] Results MVP weighted formula

### Remaining
- [ ] Dorado map parity with DevMap — [BLUEPRINT_ASSET_AUDIT.md](./BLUEPRINT_ASSET_AUDIT.md)
- [ ] HUD/scoreboard/results BP widgets
- [ ] 200ms relic latency test

---

## 🎨 PHASE 7: UI/UX Polish (POST-SLICE)
**Priority:** MEDIUM

### Required UI Elements

#### In-Match HUD
- [ ] Team scores display (Red vs Blue)
- [ ] Round timer countdown
- [ ] Round number indicator
- [ ] Ability cooldown indicators
- [ ] Health bar
- [ ] Relic carrier indicator (who has it)
- [ ] Kill feed
- [ ] Team roster (alive/dead status)

#### Game Flow Screens
- [ ] Round start countdown ("Round 1 - Fight!")
- [ ] Round end screen (win condition + scoring team)
- [ ] Match end screen (final scores, MVP)
- [ ] Respawn timer countdown

#### Hero Selection (Existing - Polish)
- [ ] Hero portrait and stats display
- [ ] Ability descriptions and icons
- [ ] Lock-in confirmation

### Implementation Tasks
- [ ] Create UCommonActivatableWidget for each screen
- [ ] Bind to GameState delegates for automatic updates
- [ ] Design team color scheme (Blue/Red or custom)
- [ ] Add smooth transitions between states
- [ ] Implement audio cues for UI events

---

## 🎭 PHASE 6: Second Hero - Morgan Le Fay (Ranged Mage)
**Estimated Time:** 5-7 days  
**Priority:** MEDIUM

### Design Specs
**Role:** Ranged DPS / Control  
**Health:** 200  
**Movement Speed:** 600

### Abilities

#### Q - Arcane Bolt
- Ranged projectile dealing damage
- Cooldown: 3 seconds

#### E - Mystic Bind
- Snare/root enemy in place for 2 seconds
- Cooldown: 10 seconds

#### F - Blink
- Short range teleport
- Cooldown: 12 seconds

#### R - Meteor Strike (Ultimate)
- AOE targeted damage after delay
- Cooldown: 60 seconds

### Buildable: Mana Shrine
**Function:** Regenerates ability cooldowns for nearby allies  
**Effect:** 20% faster cooldown recovery in radius

---

## 🎯 PHASE 7: Heroes 3 & 4
**Estimated Time:** 10-12 days total  
**Priority:** MEDIUM

### Hero 3: Valkyrie (Support)
- Healing abilities
- Movement speed buffs
- Resurrect ultimate
- **Buildable:** Healing Totem

### Hero 4: Robin Hood (Ranged Physical)
- High single-target damage
- Precision-based abilities
- Stealth/mobility
- **Buildable:** Arrow Trap

---

## 🎮 PHASE 8: Multiplayer Testing & Polish
**Estimated Time:** 4-5 days  
**Priority:** CRITICAL

### Testing Matrix
- [ ] 2v2 matches
- [ ] 3v3 matches  
- [ ] 4v4 matches (full)
- [ ] Test all hero combinations
- [ ] Test with varying latencies (50ms, 100ms, 200ms)
- [ ] Test dropped connections/reconnections

### Polish Tasks
- [ ] Optimize replication frequency
- [ ] Reduce bandwidth usage
- [ ] Add lag compensation for abilities
- [ ] Add network smoothing for movement
- [ ] Fix any remaining replication bugs
- [ ] Performance profiling and optimization

---

## 🗺️ PHASE 9: Map Design & Atmosphere
**Estimated Time:** 3-5 days  
**Priority:** LOW (Can be done in parallel)

### Map Requirements
- [ ] Symmetrical layout for fairness
- [ ] Clear visual distinction between team halves
- [ ] Goals clearly marked and obvious
- [ ] Relic spawn at exact center
- [ ] Multiple paths/routes for strategy
- [ ] High ground/low ground variation
- [ ] Clear boundaries and walls

### Atmosphere
- [ ] Mythological theme (El Dorado inspired)
- [ ] Team color lighting (Blue half vs Red half)
- [ ] Ambient audio (crowd cheering, arena sounds)
- [ ] Skybox and environmental details
- [ ] Goal area special effects

---

## 📊 PHASE 10: Final Integration & Vertical Slice
**Estimated Time:** 3-4 days  
**Priority:** CRITICAL

### Final Checklist
- [ ] All 4 heroes fully functional
- [ ] All 4 buildables working
- [ ] All 3 win conditions tested and working
- [ ] Complete UI flow from hero select to match end
- [ ] Multiplayer stable with 8 players
- [ ] No critical bugs
- [ ] Performance target met (60 FPS on target hardware)

### Deliverables
- [ ] Packaged build for Windows
- [ ] Video showcase of gameplay
- [ ] Updated GitHub README with setup instructions
- [ ] Known issues and future work documented

---

## Timeline Estimates

### Sprint Schedule (2-week sprints)

**Sprint 1 (Week 1-2):** ✅ Core Game Mode + Relic Polish  
**Sprint 2 (Week 3-4):** Hero 1 (Spartacus) + Buildable System  
**Sprint 3 (Week 5-6):** Hero 2 (Morgan) + UI Polish  
**Sprint 4 (Week 7-8):** Heroes 3 & 4  
**Sprint 5 (Week 9-10):** Multiplayer Testing + Map Polish  
**Sprint 6 (Week 11-12):** Final Integration & Bug Fixes  

**Total Estimated Time:** 12 weeks to completed vertical slice

### Accelerated Schedule (If needed)
- Reduce to 3 heroes initially
- Simplify hero abilities
- Use placeholder art/animations
- **Minimum viable:** 8-10 weeks

---

## Current Status Summary

### ✅ Completed
- Game Mode framework with round management
- All 3 win conditions implemented
- Team management and balancing
- Goal scoring system
- Documentation and setup guides

### 🔄 In Progress
- *Awaiting integration of Phase 1 deliverables*

### 📋 Next Immediate Steps
1. Integrate Game Mode system (follow INTEGRATION_QUICK_REF.md)
2. Test all win conditions in multiplayer
3. Fix any relic replication issues discovered
4. Begin Spartacus hero implementation

---

## Risk Assessment

### High Risk Items
- **Multiplayer Stability:** Requires extensive testing
- **Ability Balance:** Will need iteration after playtesting
- **Performance:** May need optimization with 8+ players

### Medium Risk Items
- **Buildable Placement:** Complex edge cases to handle
- **Animation Synchronization:** May require additional work
- **UI/UX Flow:** Needs user testing for clarity

### Low Risk Items
- **Core Game Mode:** Solid foundation implemented
- **Single Hero Implementation:** Well-documented patterns
- **Map Design:** Can use greybox initially

---

## Success Metrics

### Technical Metrics
- 60 FPS with 8 players
- < 100ms input latency
- < 5% packet loss acceptable
- No critical bugs
- Clean compile with zero warnings

### Gameplay Metrics
- All 3 win conditions can occur naturally
- Average match length: 10-15 minutes
- Hero balance: No hero > 60% win rate
- All heroes feel distinct and fun

### Project Metrics
- Vertical slice playable end-to-end
- 4 heroes, 4 buildables fully implemented
- Clean, documented codebase
- Setup instructions for new developers

---

## Resources Needed

### Assets
- 4 hero skeletal meshes with animations
- Ability VFX and SFX
- UI icons and textures
- Relic model and effects
- Buildable models
- Map props and materials

### Tools
- Unreal Engine 5.5
- Lyra Starter Project
- Version control (Git)
- Network testing tools

### Team Size
- 1-2 programmers
- 1 designer (optional, for ability tuning)
- 1 artist (optional, for placeholder art)

**Solo developer estimate:** ~12-16 weeks full-time

---

## Next Actions

### Immediate (This Week)
1. ✅ Integrate Phase 1 Game Mode files
2. ✅ Test all win conditions
3. ✅ Verify multiplayer replication
4. Start Phase 2: Relic polish

### Short Term (Next 2 Weeks)
1. Complete relic system improvements
2. Begin Spartacus hero implementation
3. Set up ability framework

### Medium Term (Next 4-6 Weeks)
1. Complete first 2 heroes
2. Polish buildable system
3. Implement core UI

**Stay focused on getting ONE hero fully working before scaling to four!**

---

**Document Version:** 1.0  
**Last Updated:** 2025-11-03  
**Next Review:** After Phase 2 completion
