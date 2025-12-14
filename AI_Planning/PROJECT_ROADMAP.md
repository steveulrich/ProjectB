# Breakaway Vertical Slice - Project Roadmap

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

## 🔄 PHASE 2: Relic System Polish (NEXT)
**Estimated Time:** 2-3 days  
**Priority:** HIGH

### Current Issues to Fix
1. **Replication Choppiness**
   - Smooth interpolation for thrown/passed relic
   - Proper physics replication settings
   - Network prediction for carrier

2. **Goal Scoring Reliability**
   - Ensure LastPossessingTeam always tracks correctly
   - Handle edge cases (simultaneous pickup/drop)
   - Add scoring cooldown to prevent double-scores

3. **Visual Feedback**
   - Relic trail effect
   - Pickup/drop animations
   - Team color tinting on relic

### Implementation Tasks
- [ ] Add network smoothing to relic movement
- [ ] Implement client-side prediction for relic throws
- [ ] Add LastPossessingTeam tracking (see INTEGRATION_QUICK_REF.md)
- [ ] Create VFX for relic states (carried, thrown, scored)
- [ ] Add audio cues for relic events

### Testing Checklist
- [ ] Test with 200ms latency simulation
- [ ] Verify scoring works in all scenarios
- [ ] Test rapid pickup/drop/throw sequences
- [ ] Multiplayer test with 8 clients

---

## 🦸 PHASE 3: First Hero Implementation (Spartacus)
**Estimated Time:** 5-7 days  
**Priority:** HIGH

### Hero Choice: Spartacus (Tank)
**Why first?** Simpler abilities, good for testing core systems

### Design Specs
**Role:** Tank/Initiator  
**Health:** 300  
**Movement Speed:** 550 (slightly slower than base)

### Abilities to Implement

#### Q - Shield Bash (Ability 1)
- Dash forward, stunning first enemy hit
- Cooldown: 8 seconds
- Implementation: Dash movement + sphere trace for collision

#### E - War Cry (Ability 2)  
- AOE buff for nearby allies (speed + damage)
- Cooldown: 12 seconds
- Implementation: Sphere overlap + temporary gameplay effects

#### F - Defensive Stance (Ability 3)
- Reduce incoming damage, slow movement
- Toggle ability, drains resource over time
- Implementation: Gameplay effect with tags

#### R - Gladiator's Leap (Ultimate)
- Leap to target location, AOE damage on landing
- Cooldown: 45 seconds
- Implementation: Launch character + target actor + AOE damage

### Implementation Tasks
- [ ] Create HeroDataAsset for Spartacus
- [ ] Set up skeletal mesh and animations
- [ ] Implement all 4 abilities as ULyraGameplayAbility subclasses
- [ ] Create gameplay effects (damage, buffs, debuffs)
- [ ] Set up input bindings
- [ ] Create ability UI icons and cooldown displays
- [ ] Polish VFX and SFX for each ability

### Testing Checklist
- [ ] All abilities work in singleplayer
- [ ] All abilities replicate correctly in multiplayer
- [ ] Cooldowns work and display correctly
- [ ] Can pick up/throw relic while using abilities
- [ ] Abilities interact correctly with buildables

---

## 🏗️ PHASE 4: Buildable System Implementation
**Estimated Time:** 4-5 days  
**Priority:** MEDIUM

### Spartacus Buildable: Barricade Wall
**Function:** Blocks movement and projectiles  
**Health:** 200  
**Build Time:** 2 seconds

### Implementation Tasks
- [ ] Polish buildable placement system (already prototyped)
- [ ] Implement build timer and invulnerability
- [ ] Create Barricade Wall actor with collision
- [ ] Add health system to buildables (extends BwayActorWithAbilitiesAndHealth)
- [ ] Implement destruction VFX
- [ ] Add placement preview with valid/invalid zones
- [ ] Limit to 1 buildable per player at a time
- [ ] Add buildable destruction on round reset

### Testing Checklist
- [ ] Can place buildable in valid locations
- [ ] Cannot place in invalid locations (out of bounds, too close to goal)
- [ ] Buildable blocks movement correctly
- [ ] Buildable takes damage and can be destroyed
- [ ] Buildable persists between rounds (if not destroyed)
- [ ] Replicates correctly in multiplayer

---

## 🎨 PHASE 5: UI/UX Polish
**Estimated Time:** 3-4 days  
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
