# Project: Breakaway Reborn (Resurrection)
**Document Type:** Master Game Design & Technical Specification  
**Engine:** Unreal Engine 5.7  
**Context:** This document serves as the "Source of Truth" for an AI Agent assisting in the development of Breakaway: Reborn, a recreation of Amazon Game Studios' cancelled "Mythological Sport Brawler."

## 1. Executive Summary & Core Identity
Breakaway: Reborn is a 4v4 team battle game that blends Action Brawler combat with Sports objectives.
- **The Hook:** Players must fight to control the "Relic" (Ball) and transport it to the enemy's "Relay" (Goal) to score.
- **The Twist:** "Buildables." Players can construct persistent structures (Walls, Turrets, Jump Pads) on the field, permanently altering the arena layout for the duration of the match.
- **New Addition:** A Single-Player Campaign ("Ascension Rites") inspired by Pyre, designed to onboard players and explore the lore.

## 2. Game Design Document (GDD)

### 2.1 The "Relic" Game Mode (Multiplayer)
- **Format:** 4v4. Best of 5 Rounds.
- **Objective:** Carry the Relic into the enemy Relay to score 1 point. First to 3 points wins the round.
- **Relic Mechanics:**
    - **Possession:** The carrier cannot use combat abilities or primary attacks. They can only Move, Pass, Throw, or Dodge.
    - **Physics:** The Relic behaves like a physical object. Throws can be charged for distance and can bounce off walls/floors.
    - **Passing:** A soft-lock system allows passing to teammates (Default Key: E).
    - **Turnovers:** Taking damage while holding the Relic causes a "Fumble," dropping the ball immediately.

### 2.2 The Buildable System
- Each Hero has 2 unique buildables.
- **Persistence:** Structures stay on the map between rounds. A wall placed in Round 1 is still there in Round 2 unless destroyed.
- **Economy:** Players earn Gold from kills/objectives to purchase/upgrade Buildables between rounds.

### 2.3 Hero Roster (Reconstructed)
*Data sourced from Alpha footage and press releases.*

#### Spartacus (The Gladiator)
- **Role:** Frontline Brawler
- **Playstyle:** Aggressive melee, great for clearing space.
- **Abilities:**
    - **Primary:** Gladius combo.
    - **Dash:** Shoulder bash to close gaps.
    - **Ultimate:** Dragon's Breath (High damage AOE).
- **Buildables:**
    - **Fire Catapult:** Rains fire on a target area.
    - **Dragon Spire:** A hidden trap that erupts a spear from the ground when enemies approach.

#### Morgan Le Fay (The Sorceress)
- **Role:** Controller / Debuffer
- **Playstyle:** Area denial and weakening enemies.
- **Abilities:**
    - **Passive:** Flock (Becomes intangible and flies forward).
    - **Q:** Burden of Sin (Projectile that passes through enemies and slows).
    - **E:** Circle of Spite (Hex zone: Slows enemies + Increases damage taken).
    - **Ultimate:** Aura of Silence (AOE Silence and Damage).
- **Buildables:**
    - **Elder Stone:** Charges energy when enemies take damage near it, then explodes.
    - **Tome of Frailty:** Turret that reduces enemy attack power.

#### Alona (The Sun Priestess)
- **Role:** Support / Healer
- **Playstyle:** Sustains the team and controls zones.
- **Abilities:**
    - **Combat:** Sun-based projectiles.
    - **Crowd Control:** Knockback burst to push enemies away ("Up and Away").
- **Buildables:**
    - **Sun Shrine:** Pulses healing to nearby allies.
    - **Starlight:** Focuses a laser beam that deals increasing damage over time.

#### Rawlins (The Gunslinger)
- **Role:** Ranged DPS
- **Playstyle:** High damage output from a distance.
- **Abilities:**
    - **Passive:** Trick Shot (Bullets ricochet once).
    - **Q:** Scatter Shot (Cone AOE).
    - **E:** Sticky Bomb (Explosive attachable).
    - **Ultimate:** Ricochet (Bouncing high-damage shot).
- **Buildables:**
    - **Cage:** Traps an enemy in a localized prison.
    - **Boom Box:** [Pending details]

#### The Black Knight
- **Role:** Heavy Tank
- **Playstyle:** Slow but immovable; controls physics.
- **Abilities:**
    - **E:** Gravity Well (Sucks players toward a center point).
    - **R:** Death Grip (Hooks/Drags an enemy).
- **Buildables:**
    - **Spike Trap:** Slows and damages enemies walking over it.

#### Anne Bonny (The Pirate)
- **Role:** Hybrid Zoner
- **Abilities:**
    - **Primary:** Cutlass & Pistol swap.
    - **Movement:** Grapple Hook (Pull self to wall or enemy to self).
    - **Ultimate:** Cannon Barrage (Global/Map artillery strike).
- **Buildables:**
    - **Accelerator:** Speed ramp to boost Relic carriers.

## 3. Technical Design Document (TDD)

### 3.1 Architecture: Gameplay Ability System (GAS)
We will strictly use UE5's GAS plugin for all character logic.
- **Ability System Component (ASC):** Attached to PlayerState for persistence (keeping upgrades/gold/stats across death/respawn).
- **Attributes:** Health, Stamina, Gold, Armor, MoveSpeed.
- **Tags:**
    - `State.Relic.Carrying`: Blocks Ability.Combat input. Enables Ability.Relic.Pass and Ability.Relic.Throw.
    - `State.Status.Silenced`: Used by Morgan Le Fay’s Aura of Silence.

### 3.2 Networking Strategy
- **Relic Physics:** Use a server-authoritative projectile with client-side prediction for the "Throw." For "Passing" (Homing), use a deterministic path to ensure the catcher sees the ball arrive correctly even with latency.
- **Melee Combat:** Use GAS Companion or similar logic for melee combos. Implement "Window-based" hit detection (Active Frames) verified by the server.

### 3.3 AI System: StateTree
- **Reasoning:** Behavior Trees are too rigid for the fluid switching between "Combat Mode" and "Sports Mode."
- **Implementation:** Use StateTree for Bots and Campaign Enemies.
    - **State A (Objective):** If Relic is free -> Go to Relic.
    - **State B (Carrier):** If holding Relic -> Pathfind to Goal -> If Enemy near -> Pass.
    - **State C (Combat):** If Enemy has Relic -> Attack Carrier.

## 4. Single Player Campaign: "Ascension Rites"
- **Reference:** Pyre Campaign Structure.
- **Premise:** The player leads a band of exiles. Winning matches ("Rites") liberates one team member, effectively removing them from your roster.
- **Mechanic:** This forces the player to master the entire roster rather than "maining" one hero.
- **Loop:**
    - **Overworld:** Visual Novel style dialogue & travel.
    - **Preparation:** Spend "Stardust" to upgrade Hero abilities (modifying GAS AttributeSets).
    - **The Rite:** 3v3 or 4v4 Match against AI.
    - **Liberation:** Winner chooses one hero to "Ascend" (leave the party).

## 5. AI-Assisted Development Workflow
Instructions for the Agent to assist the Human Developer.

### 5.1 Code Generation (Cursor / Copilot)
- **Rule:** Always generate C++ headers (.h) first, then implementation (.cpp).
- **Standard:** Use `TObjectPtr<>` for all UE5 pointers. Use `UPROPERTY()` macros correctly for replication.
- **Example Prompt:** "Create a UGameplayAbility for Morgan Le Fay's 'Circle of Spite'. It should spawn a AGameplayAbilityTargetActor_GroundTrace to select a location, then spawn a ABP_HexZone actor that applies a GE_Slow and GE_DamageAmp to overlapping enemies."

### 5.2 Blueprint Assistant (Ludus AI)
Use Ludus to explain complex Lyra-based interactions if the codebase becomes difficult to trace.

### 5.3 Asset Generation
- **Tripo3D/Meshy:** Use for generating environmental props (statues, pillars, rubble) to populate the "Mythological Arena" quickly.
- **Polycam:** Use for generating tileable textures for the arena floor (e.g., "Cracked Obsidian Tiles", "Golden Paving").

## 6. Implementation Task List (Phase 1)
1. **Project Setup:** Initialize UE 5.7 Project with "Lyra Starter Game" as a base (or clean Third Person if Lyra is too heavy).
2. **GAS Foundation:** Set up `AttributeSet_Base` and `Character_Base` with ASC.
3. **Relic Prototype:** Create `BP_Relic` (Physics Actor) and implement `GA_Pickup`, `GA_Throw`, `GA_Pass`.
4. **Buildable System:** Create `BP_Buildable_Base` with a Ghost placement mode and a Construct ability.
5. **Character Alpha:** Implement Spartacus (Melee) and Rawlins (Ranged) to test the Combat vs. Projectile balance.
