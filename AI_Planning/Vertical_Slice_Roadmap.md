# Breakaway: Reborn - Vertical Slice Roadmap

This roadmap outlines the path from the current prototype state to a production-quality Vertical Slice. It incorporates current project progress (C++ movement systems, Lyra integration) and focuses on "Slide feel and fluidity" as a core priority.

## 1. Current State Assessment
- **Movement Foundation:** Solid C++ implementation in `BreakawayCore` plugin. Sliding uses a custom physics mode (`PhysSliding`) with slope-based acceleration and steering.
- **Architecture:** Lyra-based GAS integration is established. Hero data assets and base classes (`BwayCharacterWithAbilities`) are ready.
- **UI:** Functional HUD and ability widgets exist but lack production polish.
- **Missing Elements:** No dedicated SaveGame/Persistence logic for campaign or stats. Lack of high-fidelity sensory feedback (Audio/VFX) for core mechanics.

## 2. Gap Analysis: The "Slice" Requirements
To reach Vertical Slice status, the following "Gaps" must be bridged:
| Feature | Current State | Vertical Slice Requirement |
| :--- | :--- | :--- |
| **Slide Feel** | Functional physics | "Elastic" feel with camera juice, sparks, and wind audio. |
| **Persistence** | Session-only | Persistent Gold/Upgrades and Campaign progress (`SaveGame`). |
| **VFX / Audio** | Placeholder / Standard | Impactful soundscapes and Niagara-based particle effects. |
| **Env. Polish** | Basic Arena | One "Production Ready" arena (Mythological theme). |

---

## 3. The Roadmap

### ## Core Gameplay (Movement & Combat)
- [ ] **Slide Juice Phase 1 (Camera):** Implement dynamic FOV scaling and camera shake based on slide velocity.
- [ ] **Slide Juice Phase 2 (Audio):** Add looping "friction" audio with real-time pitching based on surface material and speed.
- [ ] **Slide Juice Phase 3 (VFX):** Implement Niagara dust/sparks trail that scales with slide intensity.
- [ ] **Movement Tuning:** Refine `SlideSteerSpeed` and `SlideFrictionPower` constants in C++ to remove jitter during high-speed turns.
- [ ] **Relic Interaction:** Polish `GA_Throw` and `GA_Pass` animations with predictive client-side trajectories.

### ## Meta Systems (Persistence)
- [ ] **SaveGame Implementation:** Create `UBwaySaveGame` class to track Gold, Hero Upgrades, and "Ascension Rites" progress.
- [ ] **Campaign Flow:** Implement the "Liberation" logic where a hero is removed from the roster upon victory.
- [ ] **Attribute Persistence:** Ensure GAS `AttributeSet` values (Health, Gold) correctly load/save between matches.

### ## UI / UX
- [ ] **HUD Polish:** Replace placeholder bars with stylized "Mythological" containers.
- [ ] **Feedback Loop:** Add screen-space damage indicators and Relic possession highlights.
- [ ] **Menu Flow:** Create a production-ready Character Select screen using the existing `HeroSelect` content.

### ## Art & Tech Art (Environment & Character)
- [ ] **Hero Polish:** Finalize the "Spartacus" model with high-fidelity materials and physics-driven cloth/armor.
- [ ] **Arena "The Rite":** Texturing pass using Polycam-derived tileables for the arena floor.
- [ ] **Lighting Pass:** Set up Lumen-optimized lighting with dynamic GI for the main battle arena.

---

## 4. Technical Debt Warning
- **[IMPORTANT] Networking & Prediction:** The current `ApplySlideSteering` implementation directly rotates the `Velocity` vector. While fine for local testing, this can cause "teleporting" or jitter under latency. Recommend moving steering logic into `CalcVelocity` or using a predictive movement proxy.
- **[CAUTION] TObjectPtr Usage:** Ensure all raw pointers in newer headers are converted to `TObjectPtr` to follow UE5 best practices and avoid potential GC issues.
- **[NOTE] Asset Manager:** The project needs a configured `AssetManager` to handle asynchronous loading of Heroes/Buildables to prevent hitching during match start.

## 5. High-Risk Areas
- **Buildable Persistence:** Storing structure locations across rounds is technically complex for networking. This requires a robust `Server-Authoritative` serialization system.
- **AI StateTree Complexity:** Transitioning between "Combat" and "Sports" modes in StateTree may require significant debugging to prevent AI "indecision" at high speeds.
