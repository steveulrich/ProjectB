# Breakaway Core Loop — Implementation & Testing Plan

Iterative, step-by-step plan for getting Capture-the-Relic playable on **`L_BW_DevMap`** using **`B_BW_Experience_Dev`**, humanoid pawns only, and Lyra bots. Each step builds on the last; **do not advance until a step passes its checklist 3 cold starts in a row**.

Decisions captured from design review (May 2026).

---

## Progress (Section 1)

| Step | Status | Notes |
|------|--------|-------|
| **0** | **Done** | Humanoid dev path (`SkipHeroSelection`), C++ in `BreakawayGameMode` / `BwayBotCreationComponent` / `BwayPlayerController` |
| **1** | **Done** | Movement sandbox — `B_BW_Experience_Dev`, teams, bots, humanoid pawn |
| **2** | **Done** | Relic + goals spawn on DevMap |
| **3** | **Done** | `EAS_BW_RelicPickup`, listen-server pickup |
| **4** | **Done** | `EAS_BW_RelicThrow` |
| **5** | **Done** | `EAS_BW_RelicPass` |
| **6** | **Done** | Walk-in score + scoring component |
| **7** | **Done** | Throw / pass score |
| **8** | **Done** | Rounds, reset, `EnsureBotsForRound`, listen-server respawn fix |
| **9** | **Done** | Sudden death at 0:00 (X-axis half), `OnSuddenDeathWarning`, auto-spawn `B_BW_MidfieldDivider` |
| **10** | **Complete** | Relic bot AI + `GE_BW_Relic_Request` pickup + carrier walk-in goal; see [RelicBot_AI_Setup.md](./RelicBot_AI_Setup.md) |
| **11** | **Complete** | Match-flow config + RM phase FSM (11-1–11-7), URL duration overrides (11b); front-end E2E (11-8) deferred |

---

## Principles

| Decision | Choice |
|----------|--------|
| Test harness | PIE on `L_BW_DevMap` (Section 1) |
| Experience | Modify **`B_BW_Experience_Dev`** in place |
| Pawn | **`DA_BW_PawnData_Humanoid`** — no heroes until Section 3 |
| Progress mechanism | **Action sets + ability sets + experience / GameState / map config** — no `CoreLoopStep` URL gates |
| Teams | **4v4** — you + 3 bots vs 4 bots |
| Hero select | **Paused** — `SkipHeroSelection=1` |
| Bulletproof bar | Checklist + **3 cold-start PIE runs**; **listen server** from Step 3; **`Net PktLag=100`** on throw / pass / score steps |
| UI | **Section 2** after Step 11 — no match HUD required in Section 1 |
| Heroes | **Section 3** — after Section 2 UI (possibly later) |

**Standard PIE URL (Section 1):**

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7
```

**PIE settings:** 1 player, **Listen Server** from Step 3 onward (Standalone OK for Steps 1–2).

**Git discipline:** Commit when a step passes — e.g. `core-loop: step 3 passed (relic pickup)`.

---

## Pass Criteria Template (every step)

- [ ] Documented experience action sets match the step table below
- [ ] 3/3 cold-start PIE runs pass the step checklist
- [ ] No unexpected `Error` in Output Log (documented warnings OK)
- [ ] No open TODOs blocking the *next* step
- [ ] Re-test **previous** step’s main behavior still works (no regression)

---

## Step 0 — Humanoid dev path (C++ prerequisite)

**Status:** **Complete** (May 2026).

**Goal:** `SkipHeroSelection=1` means “humanoid from experience only,” not random heroes.

**C++ changes (small, not step gating):**

When `SkipHeroSelection` is set:

1. `BwayBotCreationComponent` — **do not** call `AssignRandomHeroToPlayer`
2. `BreakawayGameMode::ApplyHeroDataToNewPawn` — **skip** entirely
3. `BreakawayGameMode::PostLogin` — **do not** call `Client_RequestPreSelectedHero`
4. `ABwayPlayerController::Server_SubmitPreSelectedHero` — **no-op** (blocks frontend hero apply)

**Files:** `BreakawayGameMode.cpp`, `BwayBotCreationComponent.cpp`, `BwayPlayerController.cpp`

Everyone spawns via Lyra experience **`DefaultPawnData = DA_BW_PawnData_Humanoid`**.

**Also do once:** Strip `B_BW_Experience_Dev` to Step 1 baseline (see Step 1).

**Pass:** Recompile; Step 1 checklist attempt succeeds without hero assets loading.

---

# Section 1 — Core Gameplay (Humanoid)

## Step 1 — Movement sandbox

**Status:** **Complete** (May 2026).

**Goal:** You + 7 bots spawn, 4v4, everyone moves; nothing else.

### Experience (`B_BW_Experience_Dev`)

| Add | Purpose |
|-----|---------|
| `LAS_BW_SharedInput` | Movement input |
| `B_BW_TeamSetup_TwoTeams` | 4v4 teams |
| `B_BW_BotSpawner_BallMode` | 7 Lyra bots |
| `DefaultPawnData` → `DA_BW_PawnData_Humanoid` | Humanoid pawn |

| Remove / omit | |
|---------------|--|
| `EAS_BW_CaptureTheRelic` | Relic input / abilities |
| `B_BW_CaptureTheRelic_Scoring` | Scoring |
| ShooterCore HUD / music action sets | Match UI |
| All `BW_Phase_*` actions | No phases yet |

### GameState / map

- `BP_BW_GameState` → **`RelicClass = None`**
- Goal spawn points on DevMap → **no goal class** (or `bAutoSpawnOnBeginPlay = false`)
- `RoundManagement` → **`bAutoStartFirstRound = false`**, no Playing phase

### Pass checklist

- [ ] You spawn as humanoid with WASD + camera
- [ ] 7 bots spawn; log shows 4 per team
- [ ] Bots navigate (not stuck at origin)
- [ ] No relic, goals, round timer, hero UI
- [ ] 3/3 cold starts

---

## Step 2 — Relic & goals spawn

**Status:** **Complete** (May 2026).

**Goal:** Match objects exist in the world; still no player interaction required.

### Changes

| Asset | Change |
|-------|--------|
| `BP_BW_GameState` | Set **`RelicClass`** → `BP_BW_RelicActor`, **`RelicSettings`** → `DA_BW_RelicSettings_Default` |
| DevMap `SP_GoalTeam1` / `SP_GoalTeam2` | Spawn data → **`ABwayGoalVolume`** BP |
| Experience | Still **no** `EAS_BW_CaptureTheRelic` |

Relic spawns via `BreakawayGameMode::SpawnInitialGameObjects` → `RelicManager`. Goals via spawn points.

### Pass checklist

- [ ] Relic visible at `SP_Relic`
- [ ] Two goal volumes at team goal points
- [ ] Relic is **neutral** (verify `bAutoPickup` in relic settings)
- [ ] Still no round timer / phases
- [ ] 3/3 cold starts

---

## Step 3 — Pickup

**Status:** **Complete** (May 2026).

**Goal:** Player can pick up the relic.

### Changes

- **Create** `EAS_BW_RelicPickup` (split from `EAS_BW_CaptureTheRelic`) — grants request-pickup input + `GA_PickupRelic` path only
- Add to experience ActionSets
- Carrier ability set (`AS_BW_BallAbilities` on `DA_BW_RelicSettings_Default`) grants on pickup — verify drop-only first if throw / pass would confuse testing

### Pass checklist

- [ ] **Listen server** PIE
- [ ] Hold / interact to request pickup → relic attaches to socket
- [ ] `Gameplay.State.RelicCarrier` on server; **replicates to client**
- [ ] 3/3 cold starts + 3/3 with listen server

---

## Step 4 — Throw

**Status:** **Complete** (May 2026).

**Goal:** Carrier can throw the relic.

### Changes

- **Create** `EAS_BW_RelicThrow` — throw input + `GA_ThrowRelic`
- Ensure carrier set includes throw GA (via `AS_BW_BallAbilities` or subset)

### Pass checklist

- [ ] Throw detaches relic with physics
- [ ] Server authoritative; client sees thrown relic
- [ ] **`Net PktLag=100`** — throw still works (may feel sloppy; must not break)
- [ ] 3/3 cold starts

---

## Step 5 — Pass

**Status:** **Complete** (May 2026).

**Goal:** Carrier can pass to a teammate.

### Changes

- **Create** `EAS_BW_RelicPass` — pass input + `GA_PassRelic`
- Pass state `BeingPassed` vs throw `Thrown` — verify in log

### Pass checklist

- [ ] Pass impulse toward aim / teammate
- [ ] Teammate can receive / re-pickup
- [ ] **`Net PktLag=100`** pass test
- [ ] 3/3 cold starts

---

## Step 6 — Walk-in score

**Status:** **Complete** (May 2026).

**Goal:** Carry relic into **enemy** goal → round point logic fires (scoring component can be added minimally here).

### Changes

- Confirm `ABwayGoalVolume` validates `LastPossessingTeam` vs defending team
- Wire **`B_BW_CaptureTheRelic_Scoring`** OR minimal score listener so you can **see** the point in log / delegate (HUD comes in Section 2)
- **Do not** start full round reset yet — only verify **`OnRelicScored`** / `ScoringComponent->AddScore` fires

### Pass checklist

- [ ] Team 1 carrier enters Team 2 goal → Team 1 scores
- [ ] Wrong goal (own goal side) does **not** score
- [ ] Scoring cooldown respected
- [ ] 3/3 cold starts + listen server

---

## Step 7 — Throw-in score

**Status:** **Complete** (May 2026).

**Goal:** Thrown / passed relic crossing enemy goal plane scores.

### Pass checklist

- [ ] Throw into enemy goal → score registers
- [ ] Pass into enemy goal → score registers
- [ ] **`Net PktLag=100`**
- [ ] 3/3 cold starts

---

## Step 8 — Rounds & reset

**Status:** **Complete** (May 2026).

**Goal:** Score ends round → everyone respawns → relic resets → new round starts.

### Changes

| Asset | Change |
|-------|--------|
| Experience Actions | Add **`BW_Phase_Warmup`** → **`BW_Phase_Playing`** (no HeroSelection) |
| `RoundManagement` on GameState | **`bAutoStartFirstRound = false`**; listens for Playing phase |
| Experience | Keep scoring action set |
| Dev tuning | **`PointsToWin = 3`**, **`RoundDuration = 90s`** for faster iteration (restore 180s before Step 11) |

### Pass checklist

- [ ] Score → `EndRound` → players restart at team spawns
- [ ] Relic → neutral at spawn; `bHasScoredThisRound` cleared
- [ ] Next round starts after `RoundEndDelay`
- [ ] Match ends at `PointsToWin`
- [ ] 3/3 full mini-matches (score 3 rounds)

---

## Step 9 — Sudden death (fail-safe at 0:00)

**Status:** **Complete** (May 2026).

Sudden death is **not** a mode change — goal and elimination work normally all round, including the final minute.

- **At 60s remaining:** awareness only (delegate / log; UI in Section 2)
- **At 0:00:** if round still active, team whose **half** the relic is in **loses** → other team wins
- **Replaces** current `TimeExpired` possession win in `OnRoundTimerExpired`
- The final minute does **not** turn on or off any win conditions — only awareness

### C++ work (real feature, not gating)

1. **`EBwayWinCondition::SuddenDeath`** (+ gameplay tag)
2. **Midfield rule (X-axis)** — `UBwayMidfieldRulesLibrary`; relic spawn at origin; Team 1 = −X, Team 2 = +X (no gameplay volume required)
3. **`OnRoundTimerExpired`** — uses `GetLosingTeamAtMidfieldFromRelic`, win condition `SuddenDeath`
4. **`OnSuddenDeathWarning`** + auto-spawned **`B_BW_MidfieldDivider`** at `(0,0,0)` (visible only during warning window)

**Editor assets:** `B_BW_MidfieldDivider` (optional mesh), `MidfieldDividerComponent` on `BP_BW_GameState`.

### Pass checklist

- [ ] Round reaches 0:00 with no goal → midfield rule picks winner
- [ ] Relic on Team 1 half at 0:00 → Team 2 wins
- [ ] Relic on Team 2 half at 0:00 → Team 1 wins
- [ ] Goal during final minute still ends round immediately (no regression)
- [ ] 3/3 cold starts + listen server

---

## Step 10 — Bot relic AI

**Status:** **In progress** — **C++ landed** (May 2026); **editor assets + checklist** remain.

**C++ (done):**

- `ABwayRelicBotController` — runs configured BT after possess / round restart
- `UBwayRelicBotLibrary` + `BwayRelicBotBlackboard` key names
- `UBwayBTService_UpdateRelicBlackboard`
- `UBwayBTTask_TryPickupRelic`, `UBwayBTTask_ThrowRelicAtGoal`, `UBwayBTTask_PassRelicForward`
- `UBwayBotCreationComponent` — default `ABwayRelicBotController`, soft BT/BB refs, `ApplyRelicAIToBot` on spawn + `RestartAllBots`

**Editor (you):** see [RelicBot_AI_Setup.md](./RelicBot_AI_Setup.md).

**Goal:** Bots play the ball — no combat, no buildables.

### Deliverables

| Layer | Asset / type |
|-------|----------------|
| C++ | `ABwayRelicBotController`, `UBwayRelicBotLibrary`, `UBwayBTService_UpdateRelicBlackboard`, BT tasks (pickup / throw / pass) |
| Content | **`BB_BW_RelicBot`**, **`BT_BW_RelicBot`**, **`BP_BW_RelicBotController`** |
| Config | `BP_BW_GameState` → `BotCreationComponent`: controller class + soft refs to BT/BB |

Experience still uses **`B_BW_BotSpawner_BallMode`** for bot count; AI assets are assigned on **`UBwayBotCreationComponent`**.

### Behaviors (BT)

- Seek pickup → walk-in score at enemy goal
- Throw at enemy goal (when not in walk-in range)
- Pass toward forward teammate
- Chase enemy carrier (move to `RelicCarrier` blackboard key)

### Pass checklist

- [ ] Bot team scores without human touching relic (watch 1 full round)
- [ ] Human can still play normally alongside bots
- [ ] 3/3 runs where bot-initiated score happens

---

## Step 11 — Match entry + in-match flow

**Status:** **Complete** — Part B **11-1–11-7 + 11b** done; Part A **11-MM-1–4** landed; **11-8** (front-end E2E) deferred.

| Track | Scope | Status |
|-------|-------|--------|
| **Part A — Matchmaking entry** (`11-MM-1` → `11-MM-4`) | Front-end queue → mock/backend → `UCommonSessionSubsystem` travel | **11-MM-1–4** C++ landed; editor checklists open |
| **Part B — In-match orchestration** (`11-1` → `11b`) | `UBwayMatchFlowConfig` + `UBwayRoundManagementComponent` phase FSM | **Complete** (11-1–11-7 + 11b) |

**Goal:** Lyra front-end → queue or custom lobby → dev match → results → front-end. Entry is **data-driven** via `UMatchmakingGoalDefinition` / `UCustomGameConfig`; in-match phases remain **`UBwayRoundManagementComponent`** (not experience action-set grants).

**Delivery:** One sub-step per commit; **PIE or front-end pass after every C++ sub-step**.

```
11-MM-1 → 11-MM-2 → 11-MM-3 → 11-MM-4 → 11-2 → 11-3 → … → 11-8 → 11b
              ↑ menu persistence          ↑ session travel   ↑ in-match FSM
```

**Dependency:** Finish **11-MM-4** (session travel from menu) before resuming **11-2** (hero skip de-conflict). Step 10 regression gates **11-2** as before.

---

### Part A — Matchmaking entry (`11-MM-1` → `11-MM-4`)

Replaces the old “playlist tile only” **11-8** path for ranked/unranked queues. **`UBwayUserFacingExperienceDefinition`** dev tiles remain valid for direct PIE; queue goals use **`UMatchmakingGoalDefinition`**.

| Piece | Role |
|-------|------|
| **`UMatchmakingGoalDefinition`** | Primary asset (`MatchmakingGoalDefinition`); `QueueTypeTag`, `MaxPartySize`, `PlayersPerTeam`, `MapID`, `AllowedExperiences` |
| **`UBwayMatchmakingGoalLibrary`** | Phase 1 Blueprint helpers — log / read goal fields |
| **`UCustomGameConfig`** | Lightweight `UObject`; mutable custom-lobby settings; survives menu navigation |
| **`ULyraGameInstance`** | Holds `CurrentCustomGameConfig`; `InitializeCustomGameSettings()` / `GetCustomGameSettings()` |
| **`ULyraMatchmakingSubsystem`** | `UGameInstanceSubsystem`; `StartMatchmakingQueue`, `CreateCustomLobby`, mock `OnMockMatchFound` |
| **`UCommonSessionSubsystem`** | Phase 4 travel — `HostSession` (mock/offline) with resolved map + experience from goal or custom config |

**Relationship to Part B:** `AllowedExperiences` → `LyraExperienceDefinition` (e.g. `B_BW_Experience_Dev`). Each experience still carries **`UBwayGameFeatureAction_MatchFlowConfig`** → `DA_BW_MatchFlow_Dev` for in-match rules. Matchmaking picks *which* experience to load; match flow config governs *how* the match runs.

#### `11-MM-1` — Core data assets + UI read (Phase 1)

**Status:** C++ **implemented**; editor assets + UI hook **open**.

| Task | Deliverable |
|------|-------------|
| C++ | `UMatchmakingGoalDefinition`, `UBwayMatchmakingGoalLibrary` |
| `DefaultGame.ini` | `MatchmakingGoalDefinition` scan under `/BreakawayCore/Matchmaking`, `AssetBaseClass=/Script/Engine.PrimaryDataAsset` |
| Editor | `GM_QuickPlay_5v5`, `GM_Brawl_6v6` data assets |
| UI | Menu button → `Log Matchmaking Goal` → output log |

**Sample asset values (editor):**

| Asset | QueueTypeTag | PlayersPerTeam | MapID | AllowedExperiences[0] |
|-------|--------------|----------------|-------|------------------------|
| `GM_QuickPlay_5v5` | `Matchmaking.Queue.QuickPlay` | 5 | `L_BW_DevMap` | `B_BW_Experience_Dev` |
| `GM_Brawl_6v6` | `Matchmaking.Queue.Brawl` | 6 | `L_BW_DevMap` | `B_BW_Experience_Dev` |

**Pass checklist:**

- [ ] Restart editor after `DefaultGame.ini` change (cold start)
- [ ] Create assets under `/BreakawayCore/Matchmaking/`
- [ ] Wire a Lyra front-end button: on click → `Log Matchmaking Goal` with selected DA
- [ ] Log shows exact `QueueTypeTag` and `PlayersPerTeam` from the asset
- [ ] No compilation errors; no session / travel side effects
- [ ] Tier 1 compile pass

#### `11-MM-2` — Custom game config persistence (Phase 2)

**Status:** C++ **implemented**; Blueprint persistence test **open**.

| Task | Deliverable |
|------|-------------|
| C++ | `UCustomGameConfig` (`bIsCustomGameDefinition`, `CustomMaxPlayersPerTeam`, `SelectedExperienceOverride`, `SelectedMapOverride`, `ExtraArgs`) |
| C++ | `ULyraGameInstance::CurrentCustomGameConfig`, `InitializeCustomGameSettings()`, `GetCustomGameSettings()` |

**Pass checklist:**

- [ ] Blueprint: set `CustomMaxPlayersPerTeam = 4` on menu A
- [ ] Navigate to menu B, call `GetCustomGameSettings()` → still **4**
- [ ] Tier 1 compile pass

#### `11-MM-3` — Matchmaking subsystem + mock queue (Phase 3)

| Task | Deliverable |
|------|-------------|
| C++ | `ULyraMatchmakingSubsystem` in `LyraGame` module |
| API | `StartMatchmakingQueue(Goal)`, `CreateCustomLobby()`, `SendMatchmakingPayloadToBackend` |
| Mock | Log JSON payload → 3s timer → `OnMockMatchFound` delegate + on-screen message |

**Pass checklist:**

- [ ] Menu button → `StartMatchmakingQueue` → JSON prints to log / screen
- [ ] After 3s: `OnMockMatchFound` → screen message *"Match Found! Simulating Server Handoff."*
- [ ] Tier 1 compile pass

#### `11-MM-4` — Session travel integration (Phase 4)

| Path | Behavior |
|------|----------|
| **Queue** | Bind `OnMockMatchFound` → first `AllowedExperiences` entry → build `UCommonSession_HostSessionRequest` → `UCommonSessionSubsystem::HostSession` |
| **Custom** | `ExecuteCustomSessionCreation` — if `bIsCustomGameDefinition`, skip mock timer → `SelectedExperienceOverride` → `HostSession` immediately |

**Pass checklist — matchmaking:**

- [ ] Select unranked goal → Play → 3s mock → loading screen → map from goal’s experience
- [ ] `SkipHeroSelection=1` in travel URL for dev goals

**Pass checklist — custom game:**

- [ ] Custom menu: override map/experience → Start → immediate travel (no 3s wait) to chosen override

**Pass checklist — regression:**

- [ ] Step 10 URL still works when bypassing front-end
- [ ] Tier 1 compile pass; front-end E2E **3/3** cold starts

---

### Part B — In-match orchestration (`11-1` → `11b`)

### Architecture (implemented — May 2026)

Do **not** reparent `B_BW_Experience_Dev` to a Breakaway experience class. Keep parent **`LyraExperienceDefinition`** so `DefaultPawnData`, action sets, and Lyra experience load stay intact. Reparenting to `UBwayExperienceDefinition` was tried and broke pawn wiring (cylinder fallback pawns).

| Piece | Role |
|-------|------|
| **`B_BW_Experience_Dev`** | Single dev experience; parent **`LyraExperienceDefinition`** |
| **`UBwayGameFeatureAction_MatchFlowConfig`** | **Preferred** way to attach `DA_BW_MatchFlow_Dev` — add to experience **Actions** (or an action set’s Actions) |
| **`UBwayExperienceDefinition`** | Optional C++ parent with embedded `MatchFlowConfig` soft ptr — **deprecated for content**; library still reads it if you reparent, but don’t |
| **`UBwayMatchFlowConfig`** | Primary data asset (`BwayMatchFlowConfig`); phase abilities, durations, `PointsToWin`, `DefaultNumBots`, `bOrchestrateMatchFlow` |
| **`UBwayMatchFlowLibrary`** | Resolve config + apply **`PointsToWin` / `NumBots`** only in **11-1** (`ApplyMatchRulesOnly`) |
| **`UBwayGameplayUrlLibrary`** | Merges URL options into `GameMode->OptionsString` at `InitGame` (World URL, PIE LastURL, editor launch params, command line) |
| **`UBwayExperienceLibrary`** | `RegisterPackagedBreakawayExperiences()` — registers GF experiences with asset manager in **standalone / packaged** builds |
| **`ALyraWorldSettings` fallback** | If path→ID lookup fails, construct `LyraExperienceDefinition:<AssetName>` (same as `Experience=` URL option) |
| **Hooks** | `UBwayBotCreationComponent::OnExperienceLoaded` applies full rules + spawns bots; `UBwayRoundManagementComponent` high-priority hook only applies rules if BotCreation is absent |

**Packaging / asset manager (`Config/DefaultGame.ini` + `ProjectB.uproject`):**

- `BreakawayCore` enabled in **`.uproject`** (same as hero GF plugins)
- `LyraExperienceDefinition` scan: `/BreakawayCore/Experiences` + `SpecificAssets` for dev experiences
- `BwayMatchFlowConfig` scan: `AssetBaseClass=/Script/Engine.PrimaryDataAsset` (not GF class path — asset manager init timing)
- `MapsToCook` + `DirectoriesToAlwaysCook` for `/BreakawayCore` (standalone)

See also `.cursor/rules/game-feature-asset-manager.mdc`.

---

### Locked decisions (grill summary)

| # | Decision |
|---|----------|
| 1 | **Two tracks:** **11-MM** (menu → travel) then **11-2→11b** (in-match FSM) |
| 2 | **PIE** for Part B sub-steps; **front-end** for **11-MM-3/4** and **11-8** |
| 3 | Remove phase grants from experience at **11-3**; remove `PlayingPhaseTag` listener at **11-4** |
| 4 | **11-1** applies **`PointsToWin` + `NumBots` only** (no orchestrator yet) |
| 5 | **`PointsToWin=3`** for **11-4–11-6**; **`PointsToWin=1`** for **11-7/11-8** |
| 6 | Queue goals: **`UMatchmakingGoalDefinition`** primary type; dev tiles may still use **`BwayUserFacingExperienceDefinition`** |
| 7 | Mock matchmaking **always `HostSession`** (offline listen-server) until EOS backend lands |
| 8 | **`UBwayMatchFlowConfig`** stays on experience — matchmaking only picks the experience |
| 9 | **Single `MapID` per goal** (option A); custom games use `SelectedMapOverride` on `UCustomGameConfig` |

#### Entry: one experience, URL flags for entry point only

| Path | Experience | URL / tile extras |
|------|------------|-------------------|
| **PIE (dev)** | `B_BW_Experience_Dev` | `SkipHeroSelection=1`, `NumBots`, `PointsToWin` (per sub-step) |
| **Front-end Dev tile (11-8)** | Same experience | `BwayUserFacingExperienceDefinition`: `bRouteThroughHeroSelectStaging=false`; `ExtraArgs` adds `SkipHeroSelection=1` |

Do **not** fork a second experience for menu vs PIE.

#### Data: `UBwayMatchFlowConfig` (primary asset)

- Holds phase ability classes (`BW_Phase_*`), phase durations, `PointsToWin`, `DefaultNumBots`, `bOrchestrateMatchFlow`.
- **Config resolution order** (`UBwayMatchFlowLibrary::ResolveMatchFlowSettings`):
  1. URL `MatchFlowConfig=<PrimaryAssetName>` (type `BwayMatchFlowConfig`)
  2. **`UBwayGameFeatureAction_MatchFlowConfig`** on experience **Actions** or action-set **Actions** ← **wire this in editor**
  3. `UBwayExperienceDefinition::MatchFlowConfig` soft ptr (only if experience is reparented — **avoid**)
  4. Hard fallback: `/BreakawayCore/MatchFlow/DA_BW_MatchFlow_Dev`

- **URL overrides** (read via `UBwayGameplayUrlLibrary` after `BreakawayGameMode::InitGame` augment): `PointsToWin`, `NumBots`, `MatchFlowConfig` (**11-1**); `PrematchDuration`, `WarmupDuration`, `PostRoundDuration`, `RoundDuration` (**11b**). See [MatchFlow_and_Phases.md](./MatchFlow_and_Phases.md).
- **`NumBots`:** URL wins when present; else `DA_BW_MatchFlow_Dev` `DefaultNumBots`; else BotCreation scaling (`NumBotsOverride = -1`).

#### Orchestration (11-3+): RoundManagement = brain

- **`UBwayRoundManagementComponent`** becomes the **only** `StartPhase` caller during a match.
- **`EBwayMatchPhase`:** Prematch → Warmup (once) → Playing → PostRound ↔ Playing → PostMatch.
- **`ERoundState`** stays nested inside **Playing** only.
- **Hybrid timing:** config timers authoritative; `PhaseEnded` may shorten a phase.
- **Presentation (5=A):** RM orchestrates; **`ABwayGameState`** shows results / `ReturnToFrontEnd` only.

#### Hero selection (11-2, Section 3)

- **11-2:** Skip path must **not** call `EndPhaseAndProgressToNext()`.
- **Section 3:** Add `EBwayMatchPhase::HeroSelection`; RM owns transition to Warmup.

---

### Sub-step table

| ID | Name | C++ / content | Test harness | Pass when |
|----|------|---------------|--------------|-----------|
| **11-MM-1** | Goal data + UI log | `UMatchmakingGoalDefinition`, `UBwayMatchmakingGoalLibrary`; `GM_*` assets | Front-end PIE | Log prints `QueueTypeTag` + `PlayersPerTeam`; no travel |
| **11-MM-2** | Custom config persist | `UCustomGameConfig` on `ULyraGameInstance` | Front-end menus | `CustomMaxPlayersPerTeam` survives navigation |
| **11-MM-3** | Mock queue | `ULyraMatchmakingSubsystem` | Front-end button | JSON log + 3s → `OnMockMatchFound` message |
| **11-MM-4** | Session travel | `OnMockMatchFound` → `HostSession`; `ExecuteCustomSessionCreation` | Front-end E2E | Queue → map load; custom → immediate override load |
| **11-1** | Config + rules | `UBwayMatchFlowConfig`, libraries, `DA_BW_MatchFlow_Dev` on experience **Actions** | PIE URL `PointsToWin=1&NumBots=3` | Resolved DA; 1-round match; 3 bots; Step 10 works |
| **11-2** | Hero skip de-conflict | Skip path: no `EndPhaseAndProgressToNext()` | Step 10 URL | Bot score; no duplicate Warmup |
| **11-3** | Prematch freeze | `EBwayMatchPhase`; RM `EnterPrematch()` | PIE | `Prematch`; no pawns; no round |
| **11-4** | Warmup → Playing | RM orchestrator live | `PointsToWin=3&NumBots=7` | Warmup once; round 1; scoring works |
| **11-5** | Playing FSM | Round timer, `EndRound`, sudden death | `PointsToWin=3` | Round completes; scores update |
| **11-6** | PostRound loop | PostRound timer | `PointsToWin=3` | Log shows **PostRound** between rounds |
| **11-7** | PostMatch + results | RM → PostMatch; `ReturnToFrontEnd` | `PointsToWin=1` | Continue → `L_LyraFrontEnd` |
| **11-8** | Results E2E | Full loop after **11-MM-4** + **11-7** | `L_LyraFrontEnd` | 3/3 front-end → match → results → menu |
| **11b** | Polish | URL duration overrides; sync [MatchFlow_and_Phases.md](./MatchFlow_and_Phases.md) | URL overrides | Overrides work; docs match code |

**PIE URL template (11-4–11-6):**

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3
```

Swap `PointsToWin=1` for **11-1**, **11-7**, **11-8**.

---

### 11-1 — Config + match rules

**Status:** **Complete** (C++ + content + PIE pass).

#### C++ (landed)

| Type | Path | Notes |
|------|------|-------|
| `UBwayMatchFlowConfig` | `GameModes/BwayMatchFlowConfig.*` | Primary asset; `GetPrimaryAssetId()` → `BwayMatchFlowConfig` |
| `UBwayGameFeatureAction_MatchFlowConfig` | `GameModes/BwayGameFeatureAction_MatchFlowConfig.*` | Data-only `UGameFeatureAction`; editor display name “Breakaway Match Flow Config” |
| `UBwayMatchFlowLibrary` | `GameModes/BwayMatchFlowLibrary.*` | Resolve + `ApplyMatchRulesOnly` + logging |
| `UBwayGameplayUrlLibrary` | `GameModes/BwayGameplayUrlLibrary.*` | `AugmentGameModeOptionsString` from `BreakawayGameMode::InitGame` |
| `UBwayExperienceLibrary` | `GameModes/BwayExperienceLibrary.*` | `RegisterPackagedBreakawayExperiences` before experience assignment |
| `UBwayExperienceDefinition` | `GameModes/BwayExperienceDefinition.*` | **Not used for content wiring** — keep BP on `LyraExperienceDefinition` |
| Hooks | `BwayBotCreationComponent`, `BwayRoundManagementComponent` | BotCreation: full rules + bot spawn on experience load; RM: rules-only fallback if no BotCreation |

**`DefaultGame.ini`:** `BwayMatchFlowConfig` scan uses `/Script/Engine.PrimaryDataAsset`; `LyraExperienceDefinition` + map scans include `/BreakawayCore/…`; `BreakawayCore` in `.uproject`; cook lists for standalone.

**Lyra tweak:** `ALyraWorldSettings::GetDefaultGameplayExperience` — package/name fallback when asset-manager path map is empty (packaged GF experiences).

#### Behavior (11-1 scope)

On experience load, **`UBwayBotCreationComponent`** resolves config and applies **`PointsToWin`** + **`NumBots`** only (`bOrchestrateMatchFlow` logged but orchestrator inactive until **11-3+**).

**Do not touch:** phase grants, `PlayingPhaseTag` listener, orchestrator phase chain, hero-select skip de-conflict (**11-2**).

#### Content (editor, after compile)

1. Create **`DA_BW_MatchFlow_Dev`** under `/BreakawayCore/MatchFlow/` (parent `UBwayMatchFlowConfig`).
2. Open **`B_BW_Experience_Dev`** — parent must stay **`LyraExperienceDefinition`**.
3. **Actions** → add **`BwayGameFeatureAction_MatchFlowConfig`** → `MatchFlowConfig` = `DA_BW_MatchFlow_Dev`.
4. Optional: set `DefaultNumBots = 3` on the DA when PIE URL is unreliable.
5. Phase grants (**`BW_Phase_*`**) **stay** until **11-3**.

**Anti-pattern:** Reparenting to `UBwayExperienceDefinition` → cylinder pawns, broken `DefaultPawnData` / action sets. Revert parent, re-save BP.

#### PIE URL

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=3&PointsToWin=1
```

Set in the **Play** toolbar URL field (avoid “Repeat Last Play” unless that session’s URL is correct), or **Editor → Project Settings → Maps & Modes → Advanced → Additional Launch Parameters**. Prefer **Net Mode: Standalone** so options reach the server `GameMode`.

**URL troubleshooting:** `UBwayGameplayUrlLibrary` merges World URL, PIE LastURL, launch params, and command line into `OptionsString`. If log shows `NumBots=default` or full hero-select for bots, URL did not reach authority — use DA `DefaultNumBots` or fix launch settings. `Experience (Source: WorldSettings)` is OK when the map’s default experience is `B_BW_Experience_Dev`.

#### Standalone / packaged

After **full repackage** (not incremental from an old cook):

- Expect: `BwayExperience: Registered packaged experience LyraExperienceDefinition:B_BW_Experience_Dev …`
- Expect: `Identified experience LyraExperienceDefinition:B_BW_Experience_Dev (Source: WorldSettings)`
- Failure: `Wanted to use … B_BW_Experience_Dev but couldn't find it` → experience not cooked; verify `BreakawayCore` in `.uproject` and `DirectoriesToAlwaysCook`
- Failure: fallback to `B_LyraDefaultExperience` → cylinder pawns; fix registration + cook before advancing

#### Pass checklist

- [x] Log: `BwayMatchFlow: Resolved config 'DA_BW_MatchFlow_Dev' — PointsToWin=1 NumBots=3 …`
- [x] Log: experience `B_BW_Experience_Dev` (not `B_LyraDefaultExperience`)
- [x] Match ends after **1** round win
- [x] **3** bots spawn (humanoid, `SkipHeroSelection=1`)
- [x] Relic / bot scoring still works (Step 10 baseline)
- [x] **3/3** cold-start PIE runs
- [ ] **1/1** standalone run on `L_BW_DevMap` with correct experience + pawns (deferred to packaging pass)

---

### 11-2 — Hero skip de-conflict

**Status:** **Complete** (C++).

Skip path in `UBwayHeroSelectionPhaseComponent` sets `bHeroSelectionCompleted = true` and returns without `EndPhaseAndProgressToNext()`.

---

### 11-3 — Prematch freeze

**Status:** **Complete** (C++ + content + PIE pass).

#### C++ (landed)

| Type | Path | Notes |
|------|------|-------|
| `EBwayMatchPhase` | `GameModes/BwayMatchPhaseTypes.h` | `Prematch`, `Warmup`, `Playing`, `PostRound`, `PostMatch` (HeroSelection deferred to Section 3) |
| RM orchestrator | `BwayRoundManagementComponent.*` | High-priority experience hook → `EnterPrematch()` when `bOrchestrateMatchFlow`; sole `StartPhase` caller |
| Pawn freeze | `BreakawayGameMode.*` | `ShouldDeferPlayerRestartForMatchFlow` while `CurrentMatchPhase == Prematch` |
| Config | `BwayMatchFlowConfig` / `BwayMatchFlowLibrary` | `PrematchPhaseAbility`, `PrematchDuration` resolved and cached on RM |

#### Behavior (11-3 scope)

- On experience load (high-priority hook), RM resolves `UBwayMatchFlowConfig` and honors `bOrchestrateMatchFlow`.
- When orchestrating: `EnterPrematch()` sets replicated `EBwayMatchPhase::Prematch`, starts prematch GAS phase + config timer (hybrid: timer authoritative; GAS `PhaseEnded` may shorten).
- **No** `StartRound()`, **no** pawns (human or bot) until **11-4** `EnterWarmup()`.
- Legacy `PlayingPhaseTag` listener registers **only** when `bOrchestrateMatchFlow=false`.
- Prematch completion logs hold message; does **not** advance to Warmup yet (**11-4**).

#### Content (editor, after compile)

1. Confirm **`DA_BW_MatchFlow_Dev`**: `bOrchestrateMatchFlow = true`, `PrematchDuration` tuned (default 1s OK for smoke test).
2. Open **`B_BW_Experience_Dev`** — parent stays **`LyraExperienceDefinition`**.
3. **Remove auto-granted phase abilities** from experience **Actions** and any linked action sets (otherwise experience still starts Warmup/Playing outside RM):
   - Remove **`BW_Phase_Warmup`** grant actions
   - Remove **`BW_Phase_HeroSelection`** grant actions (dev uses `SkipHeroSelection=1`)
   - Remove **`BW_Phase_Playing`** grant actions
   - Remove **`BW_Phase_PostRound`** / **`BW_Phase_PostGame`** grant actions if present
   - Keep **`BwayGameFeatureAction_MatchFlowConfig`** → `DA_BW_MatchFlow_Dev`
   - Keep gameplay action sets (relic, scoring, bots, input) unchanged
4. **Create** content **`BW_Phase_Prematch`** under `/BreakawayCore/Experiences/Phases/` (parent `LyraGamePhaseAbility`, tag `Breakaway.GamePhase.Prematch`) and assign on `DA_BW_MatchFlow_Dev` → `PrematchPhaseAbility`. Prematch freeze works without it (timer-only), but GAS phase start requires this asset.

#### PIE URL

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=3&PointsToWin=1
```

#### Pass checklist

- [x] Log: `BwayRoundManagement: Match phase -> Prematch` (or `EBwayMatchPhase::Prematch`)
- [x] Log: `EnterPrematch — orchestrator active; pawn spawn frozen`
- [x] Log: **no** `BwayRoundManagement: Starting Round` / `StartRound()`
- [x] Log: **no** duplicate Warmup from experience auto-grants (after content step 3)
- [x] No player or bot pawns in world during Prematch
- [x] Relic + goals still spawn (match objects only)
- [x] `PointsToWin=1` / `NumBots=3` still resolved (11-1 rules)
- [x] **3/3** cold-start PIE runs
- [x] Tier 1 compile pass

---

### 11-4 — Warmup → Playing

**Status:** **Complete**.

RM orchestrator advances Prematch → Warmup (once) → Playing; legacy `PlayingPhaseTag` listener disabled when `bOrchestrateMatchFlow=true`. Pawns spawn on Warmup; round 1 starts in Playing. Verified with `PointsToWin=3&NumBots=7`.

---

### 11-5 — Playing FSM

**Status:** **Complete**.

Round timer, `EndRound`, sudden death, and score updates run inside **Playing** while RM owns match phase. Step 10 relic/bot scoring regression passes under orchestrator.

---

### 11-6 — PostRound loop

**Status:** **Complete**.

Multi-round matches (`PointsToWin=3`) log **PostRound** between rounds; PostRound timer advances back to Playing until match win threshold.

---

### 11-7 — PostMatch + results

**Status:** **Complete** (PIE pass).

| Piece | Notes |
|-------|-------|
| RM `EnterPostMatch()` | Stops round FSM, sets `PostMatch`, starts `BW_Phase_PostMatch`, stops bot logic |
| `ABwayGameState` | Presentation only — `ShowResultsScreen()` / `ReturnToFrontEnd()`; **no** `StartPhase` |
| Results widget | Authoritative scores via `Client_ShowResults` + `ApplyAuthoritativeResults` |
| `ReturnToFrontEnd()` | PIE: `OpenLevel` → `L_LyraFrontEnd?Experience=B_LyraFrontEnd_Experience`; bot shutdown before travel |
| Content | `BW_Phase_PostMatch` replaces `BW_Phase_PostGame`; `Breakaway.GamePhase.PostMatch` tag |

**PIE URL:** `L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=1`

**Pass checklist:**

- [x] One round win → results screen shows correct team score (not 0-0)
- [x] Continue → `L_LyraFrontEnd` loads Lyra front-end experience
- [x] No BT aux-node crash on return (bots stopped/shutdown before travel)
- [x] Tier 1 compile pass

---

### 11-8 — Front-end E2E (post **11-MM-4**)

Full menu → queue/custom → match → results → menu. Queue path uses **`UMatchmakingGoalDefinition`**; optional dev tile under `/BreakawayCore/Playlists/` (`BwayUserFacingExperienceDefinition`, `bRouteThroughHeroSelectStaging = false`) for direct-play fallback.

**Deferred:** dedicated server + client-only Continue (post-11-8).

---

### 11b — Polish (URL duration overrides + docs)

**Status:** **Complete** (C++ + docs + Tier 1 compile pass).

#### C++ (landed)

| Type | Path | Notes |
|------|------|-------|
| Duration URL keys | `BwayGameplayUrlLibrary.cpp` | `PrematchDuration`, `WarmupDuration`, `PostRoundDuration`, `RoundDuration` in known gameplay keys |
| Float URL getter | `BwayGameplayUrlLibrary.*` | `TryGetGameplayUrlOptionFloat` / `WithSource` |
| Resolve overrides | `BwayMatchFlowLibrary.cpp` | Duration URL overrides applied after config copy; logged with source label |
| RM timers | `BwayRoundManagementComponent.*` | Unchanged — reads `ResolvedMatchFlowSettings` (already wired in 11-3–11-6) |

#### PIE URL (duration override smoke test)

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&WarmupDuration=5&PostRoundDuration=3
```

#### Pass checklist

- [x] Log: `BwayMatchFlow: WarmupDuration=5.0s (URL override from …)`
- [x] Log: `BwayMatchFlow: PostRoundDuration=3.0s (URL override from …)`
- [x] Log: `Warmup config timer started (5.0s, authoritative)` / `PostRound config timer started (3.0s, authoritative)`
- [x] [MatchFlow_and_Phases.md](./MatchFlow_and_Phases.md) synced with code
- [x] Tier 1 compile pass

---


# Section 2 — Match UI (before heroes)

No production HUD in Section 1. Add widgets **one at a time** to `B_BW_Experience_Dev`; same bulletproof bar as Section 1 (**3/3 cold-start PIE**, **Listen Server** from Step 12).

Reference layouts (Breakaway target): top-center **timer + team scores + round-win pips**; **team portrait rows** (4v4); center **relic/carrier status**; bottom **health** (ability bar stubbed until Section 3); post-match **VICTORY/DEFEAT** stats table with **Return to Lobby**.

---

## Progress (Section 2)

| Step | Status | Notes |
|------|--------|-------|
| **12** | **In progress** | C++ slot widgets landed (`12-0`); editor BP + action set open |
| **13** | **Not started** | Relic status widget |
| **14** | **Not started** | `WBP_BW_CoreHUD` shell — see [CoreHUD_Layout_Setup.md](./CoreHUD_Layout_Setup.md) |
| **15** | **Not started** | Between-round planning stub |
| **16** | **Not started** | Results polish + sudden-death banner |

---

## Section 2 principles

| Decision | Choice |
|----------|--------|
| Test harness | PIE on `L_BW_DevMap`, **`B_BW_Experience_Dev`**, `SkipHeroSelection=1` |
| Net mode | **Listen Server** from Step 12 onward |
| HUD injection | Lyra **`UGameFeatureAction_AddWidgets`** + ShooterCore layout (Steps 12–13 slot widgets) → CoreHUD layout (Step 14) |
| Widget logic | C++ slot bases (`UBwayCaptureTheRelicScoreWidget`, `UBwayRelicStatusWidget`); BP = layout/styling only |
| Experience parent | **`LyraExperienceDefinition`** — do not reparent |
| Pawn | **`DA_BW_PawnData_Humanoid`** — no hero select UI |
| Bulletproof bar | Pass Criteria Template + **3/3 cold starts** per step |

**Standard PIE URL (Section 2 multi-round):**

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&RoundDuration=90
```

**Experience baseline entering Section 2** (Section 1 / Step 11 complete):

| Present on `B_BW_Experience_Dev` | Absent until Section 2 |
|----------------------------------|------------------------|
| `LAS_BW_SharedInput`, teams, bots, relic pickup/throw/pass action sets | `LAS_ShooterGame_StandardHUD` (Step 12) |
| `BwayGameFeatureAction_MatchFlowConfig` → `DA_BW_MatchFlow_Dev` | `B_BW_CaptureTheRelic_Scoring` widget injection (Step 12) |
| Gameplay phases via RM orchestrator (no auto `BW_Phase_*` grants) | Match HUD widgets |
| `BP_BW_GameState` with Scoring, RelicManager, RoundManagement | `ResultsScreenWidgetClass` polish (Step 16) |

---

## Related docs (Section 2)

- [MatchUI_Setup.md](./MatchUI_Setup.md) — Steps 12–13 editor tutorials
- [CoreHUD_Layout_Setup.md](./CoreHUD_Layout_Setup.md) — Step 14 layout
- [UI_System.md](./UI_System.md) — C++ widget bases
- [MatchFlow_and_Phases.md](./MatchFlow_and_Phases.md) — PostRound, PostMatch, URL overrides
- [RelicBot_AI_Setup.md](./RelicBot_AI_Setup.md) — tutorial format reference
- [BLUEPRINT_INTEGRATION_GUIDE.md](../../../AI_Planning/BLUEPRINT_INTEGRATION_GUIDE.md) — Phases 2–5 HUD patterns
- [BLUEPRINT_ASSET_AUDIT.md](../../../AI_Planning/BLUEPRINT_ASSET_AUDIT.md) — asset paths under `/BreakawayCore/UI/Match/`

---

## C++ gaps (documented — optional fixes)

| Gap | Steps affected | Substep | Workaround |
|-----|----------------|---------|------------|
| `OnRoundTimeChanged` / `OnSuddenDeathWarning` authority-only broadcast | 14–16 | 14-3, 15-2, 16-3 | Poll `GetRoundTimeRemaining()` on clients (score widget C++ already polls timer) |
| Extended results stats (damage, healing, gold, fumbles) not on `ABwayPlayerState` | 16 | 16-1 | Show K/D/A/Objective only; hide or `"—"` extended rows |

**Resolved in C++ (May 2026):**

- Score/relic slot widgets — `UBwayCaptureTheRelicScoreWidget`, `UBwayRelicStatusWidget` (shared `UBwayMatchHUDWidgetBase`)
- `OnRelicPossessionChanged` in `UBwayCoreHUDWidget` — 0.25s poll via shared relic helpers

---

## Step 12 — Score widget

**Status:** **Not started**.

**Goal:** Team round-win scores visible in HUD and updating on each round win.

**Prerequisites:** Step **11** pass (3/3) — especially **11-5** (Playing FSM), **11-6** (PostRound loop), scoring component wired.

**Architecture note:**

| Layer | Owner |
|-------|-------|
| **Data** | `UBwayScoringComponent` on `BP_BW_GameState` — `OnTeamScoreChanged`; clients refresh via `OnRep_Scores` |
| **Layout host** | `LAS_ShooterGame_StandardHUD` — Lyra HUD layout with extension slots |
| **Injection** | `B_BW_CaptureTheRelic_Scoring` → `UGameFeatureAction_AddWidgets` → slot `HUD.Slot.TeamScore` |
| **Display** | `W_BW_CaptureTheRelic_ScoreWidget` — Blueprint child of **`UBwayCaptureTheRelicScoreWidget`** |

Reference: `/ShooterCore/Elimination/UI/W_ScoreWidget_Elimination` (layout only). Editor steps: [MatchUI_Setup.md](./MatchUI_Setup.md).

### Substeps

| ID | Tag | Task |
|----|-----|------|
| **12-0** | [C++] | **`UBwayMatchHUDWidgetBase`**, **`UBwayCaptureTheRelicScoreWidget`** — score/timer binding extracted from CoreHUD |
| **12-1** | [Editor] | Inspect ShooterCore reference widgets; note `HUD.Slot.TeamScore` in `LAS_ShooterGame_StandardHUD` |
| **12-2** | [Editor] | Configure `B_BW_CaptureTheRelic_Scoring` → **Add Widgets** → `W_BW_CaptureTheRelic_ScoreWidget` → `HUD.Slot.TeamScore` |
| **12-3** | [Editor] | Create `W_BW_CaptureTheRelic_ScoreWidget` BP (parent **`BwayCaptureTheRelicScoreWidget`**) — UMG layout + BindWidget names only |
| **12-4** | [Editor] | Add `LAS_ShooterGame_StandardHUD` + `B_BW_CaptureTheRelic_Scoring` to `B_BW_Experience_Dev` ActionSets |
| **12-5** | [PIE] | Pass checklist (3/3 cold starts) |

### Experience changes (`B_BW_Experience_Dev`)

| Add | Purpose |
|-----|---------|
| `LAS_ShooterGame_StandardHUD` | HUD layout + extension slots |
| `B_BW_CaptureTheRelic_Scoring` | Injects score widget |

| Remove / omit | |
|---------------|--|
| Relic status widget row in scoring set | Step 13 |
| `WBP_BW_CoreHUD` layout | Step 14 |

### PIE URL

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&RoundDuration=90
```

**PIE:** 1 player, **Listen Server**.

### Pass checklist

- [ ] Documented experience action sets match table above
- [ ] Team 1 and Team 2 scores visible at **0–0** on match start
- [ ] Score a round → winning team digit increments before PostRound overlay
- [ ] **3/3** cold-start PIE runs (Listen Server)
- [ ] No unexpected `Error` in Output Log
- [ ] Regression: relic pickup/throw/score, bot AI, Step 11 phase flow (Prematch → Warmup → Playing → PostRound)

### Regression

Section 1: pickup, throw, pass, walk-in/throw score, round reset, sudden death midfield rule, bot-initiated score, match-flow URL overrides (`WarmupDuration`, `PostRoundDuration`).

### Git discipline

`core-loop: step 12 passed (score widget)`

### Troubleshooting

| Symptom | Check |
|---------|--------|
| No HUD | `LAS_ShooterGame_StandardHUD` on experience; cold-start editor |
| Widget missing | Scoring action set on experience; Slot ID = `HUD.Slot.TeamScore` |
| Scores stuck at 0 | Recompile C++; BP parent = `BwayCaptureTheRelicScoreWidget`; BindWidget names `Text_Team1Score` / `Text_Team2Score` |
| Wrong/overlapping score UI | Only one widget registered to `HUD.Slot.TeamScore` |

---

## Step 13 — Relic status widget

**Status:** **Not started**.

**Goal:** Relic possession state (Neutral / Team 1 / Team 2 / carrier name) correct on client during pickup, throw, and pass.

**Prerequisites:** Step **12** pass (3/3).

**Architecture note:**

| Layer | Owner |
|-------|-------|
| **Data** | `UBwayRelicManagerComponent` — `GetRelicPossessingTeam()` (replicated); `ARelicActor::CurrentCarrier` for carrier name |
| **Injection** | Same `B_BW_CaptureTheRelic_Scoring` action — slot `HUD.Slot.ModeStatus` |
| **Display** | `W_BW_RelicStatusWidget` — Blueprint child of **`UBwayRelicStatusWidget`** |

**Binding:** C++ polls `GetRelicPossessingTeam()` + carrier name every **0.25s** (client-safe). No Blueprint Event Graph required for pass.

Editor steps: [MatchUI_Setup.md](./MatchUI_Setup.md) section 3.

### Substeps

| ID | Tag | Task |
|----|-----|------|
| **13-0** | [C++] | **`UBwayRelicStatusWidget`** — relic poll + BindWidget status/carrier text |
| **13-1** | [Editor] | Create `W_BW_RelicStatusWidget` BP (parent **`BwayRelicStatusWidget`**) — layout + BindWidget names |
| **13-2** | [Editor] | Add second **Widgets** row to `B_BW_CaptureTheRelic_Scoring` → `HUD.Slot.ModeStatus` |
| **13-3** | [PIE] | Listen server: pickup / throw / pass update UI |

### Experience changes

| Add / change | Purpose |
|--------------|---------|
| `B_BW_CaptureTheRelic_Scoring` | Add `W_BW_RelicStatusWidget` → `HUD.Slot.ModeStatus` |

No other experience ActionSet changes.

### PIE URL

Same as Step 12.

### Pass checklist

- [ ] Documented scoring set has both TeamScore and ModeStatus rows
- [ ] Round start shows **NEUTRAL** (or equivalent)
- [ ] Pickup → correct team label + carrier player name
- [ ] Throw / drop → returns to neutral
- [ ] Pass to teammate → new carrier name
- [ ] **3/3** cold-start Listen Server runs
- [ ] Regression: Step 12 scores still update on round win

### Regression

Step 12 score widget; Section 1 relic gameplay.

### Git discipline

`core-loop: step 13 passed (relic status widget)`

### Troubleshooting

| Symptom | Check |
|---------|--------|
| Always NEUTRAL | BP parent = `BwayRelicStatusWidget`; `RelicManager` on GameState; Listen Server PIE |
| Carrier name blank | Read `CurrentCarrier` → `PlayerState` → `GetPlayerName` |
| Updates on host only | Recompile — C++ poll should run on all clients |
| Widget off-screen | Slot ID = `HUD.Slot.ModeStatus` |

---

## Step 14 — Core HUD shell

**Status:** **Not started**.

**Goal:** Single `WBP_BW_CoreHUD` provides top bar (scores, timer, round #), team portraits, health, and relic status — composes or replaces Steps 12–13 standalone widgets.

**Prerequisites:** Step **13** pass (3/3).

**Architecture note:**

| Layer | Owner |
|-------|-------|
| **Data** | `UBwayCoreHUDWidget` getters + BP events (`OnScoreChanged`, `OnRoundTimeUpdated`, `OnHealthChanged`, …) |
| **Team rows** | `UBwayHUDHelpers::GetTeamPlayerHUDData` / `UpdateTeamPortraits` |
| **Display** | `WBP_BW_CoreHUD` parent **`UBwayCoreHUDWidget`** at `/BreakawayCore/UI/Match/` |

Layout tutorial: [CoreHUD_Layout_Setup.md](./CoreHUD_Layout_Setup.md).

### Substeps

| ID | Tag | Task |
|----|-----|------|
| **14-0** | [C++] optional | Wire `RelicManager` → `OnRelicPossessionChanged` in `BwayCoreHUDWidget.cpp`; Tier 1 compile |
| **14-1** | [Editor] | Create `WBP_BW_CoreHUD` parented to `UBwayCoreHUDWidget` |
| **14-2** | [Editor] | Layout: top bar, team portrait rows, bottom health, relic cluster (see CoreHUD_Layout_Setup.md) |
| **14-3** | [Editor + BP] | Implement `OnScoreChanged`, `OnRoundTimeUpdated`, `OnRoundStateChanged`, `OnHealthChanged`; Tick fallback for timer on clients |
| **14-4** | [Editor] | Inject CoreHUD via **Add Widgets → Layout** (`Lyra.HUD.PlayerHUD`); remove standalone 12–13 slot rows from scoring set |
| **14-5** | [PIE] | Pass checklist + regression 12–13 behavior via shell |

### Experience changes

| Add | Purpose |
|-----|---------|
| `B_BW_CoreHUD_Layout` (new action set) or Layout row on HUD action set | Injects `WBP_BW_CoreHUD` as full layout |

| Remove | Purpose |
|--------|---------|
| `W_BW_CaptureTheRelic_ScoreWidget` slot row | Avoid duplicate scores |
| `W_BW_RelicStatusWidget` slot row | Avoid duplicate relic UI |
| *(optional)* `LAS_ShooterGame_StandardHUD` | If custom `LAS_BW_MatchHUD_Dev` replaces Shooter layout entirely |

### PIE URL

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&RoundDuration=90
```

### Pass checklist

- [ ] Documented experience matches layout injection table
- [ ] Timer counts down **MM:SS** during active round
- [ ] Team scores and round number visible
- [ ] Team portrait row populated (placeholder portraits OK)
- [ ] Health bar tracks damage / respawn
- [ ] Relic status matches Step 13 behavior
- [ ] No duplicate score/relic widgets
- [ ] **3/3** cold-start Listen Server runs
- [ ] Regression: scoring, relic AI, match phases

### Regression

Steps 12–13 behavior (via CoreHUD, not standalone widgets); Section 1 gameplay.

### Git discipline

`core-loop: step 14 passed (core HUD shell)`

### Troubleshooting

| Symptom | Check |
|---------|--------|
| Double HUD elements | Remove Step 12–13 slot rows from scoring set |
| Timer frozen on client | Add 1s Tick poll using `GetRoundTimeFormatted()` |
| Health bar empty | Humanoid pawn spawned; health component present |
| Relic text stale | 0.25s poll or complete 14-0 C++ bind |

---

## Step 15 — Between-round planning stub

**Status:** **Not started**.

**Goal:** Minimal overlay during **PostRound** / `OnBetweenRoundPlanningStarted`; dismisses when next round (**Playing**) resumes.

**Prerequisites:** Step **14** pass (3/3).

**Architecture note:**

| Layer | Owner |
|-------|-------|
| **Data** | `UBwayRoundManagementComponent::OnBetweenRoundPlanningStarted(CompletedRoundNumber, PlanningDurationSeconds)`; `GetCurrentMatchPhase() == PostRound` |
| **Display** | `WBP_BW_BetweenRoundPlanning` — owned by CoreHUD (viewport overlay or child widget) |

Stub OK: title + countdown text only — no buildable shop until Section 3+.

### Substeps

| ID | Tag | Task |
|----|-----|------|
| **15-1** | [Editor] | Create `WBP_BW_BetweenRoundPlanning` at `/BreakawayCore/UI/Match/` — title `"Between Rounds"`, countdown `Text`, semi-transparent background |
| **15-2** | [Editor + BP] | On `WBP_BW_CoreHUD` Construct: get RM → bind `OnBetweenRoundPlanningStarted` → show overlay + start countdown; poll `GetCurrentMatchPhase()` → hide when `Playing` |
| **15-3** | [Editor] | No new experience action set — overlay spawned/managed from CoreHUD |
| **15-4** | [PIE] | Short PostRound URL test |

### Experience changes

No ActionSet changes. Overlay wired from CoreHUD Event Graph.

### PIE URL

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&PostRoundDuration=3&WarmupDuration=5
```

Use `PointsToWin=3` so PostRound loop runs between rounds.

### Pass checklist

- [ ] Overlay appears after round win, before next round starts
- [ ] Countdown roughly matches `PostRoundDuration` (URL or config)
- [ ] Overlay hidden when Playing resumes
- [ ] Gameplay input not permanently blocked (stub — no `UIOnly` input mode)
- [ ] **3/3** cold-start runs
- [ ] Regression: Steps 12–14 HUD still correct during rounds

### Regression

In-round HUD (Step 14); multi-round flow (Step 11-6).

### Git discipline

`core-loop: step 15 passed (between-round stub)`

### Troubleshooting

| Symptom | Check |
|---------|--------|
| Never appears | Bound to RM delegate; use `PointsToWin=3`; verify PostRound in log |
| Stuck visible | Poll `GetCurrentMatchPhase()`; hide when not `PostRound` |
| Wrong duration | URL `PostRoundDuration=3`; config `DA_BW_MatchFlow_Dev` |
| Skips PostRound | `PointsToWin=1` ends match — use 3 for this step |

---

## Step 16 — Results + sudden death polish

**Status:** **Not started**.

**Goal:** Full match outcome readable without log spelunking; sudden-death awareness banner at ≤60s remaining.

**Prerequisites:** Step **15** pass (3/3).

**Architecture note:**

| Layer | Owner |
|-------|-------|
| **Results data** | `ABwayGameState::ShowResultsScreen` → `Client_ShowResults` → `UBwayResultsScreenWidget::ApplyAuthoritativeResults` |
| **Per-player stats** | `ABwayPlayerState` — K/D/A/Objective only (extended screenshot columns deferred) |
| **Sudden death** | `UBwayRoundManagementComponent::OnSuddenDeathWarning` at `SuddenDeathWarningSeconds` (default **60**); optional sync with midfield divider visibility (Step 9 C++) |
| **Display** | `WBP_BW_ResultsWidget` at `/BreakawayCore/UI/`; sudden-death banner on `WBP_BW_CoreHUD` |

**Asset naming:** use existing `WBP_BW_ResultsWidget` (not `WBP_BW_ResultsScreen`).

### Substeps

| ID | Tag | Task |
|----|-----|------|
| **16-1** | [Editor] | Polish `WBP_BW_ResultsWidget` (parent `UBwayResultsScreenWidget`) — VICTORY/DEFEAT header, round-win pips, team stat rows (K/D/A/Objective), MVP block, Return to Lobby / Play Again buttons |
| **16-2** | [Editor] | `BP_BW_GameState` → **Results Screen Widget Class** = `WBP_BW_ResultsWidget` |
| **16-3** | [Editor + BP] | On CoreHUD: bind RM `OnSuddenDeathWarning` + poll `GetRoundTimeRemaining() <= 60` fallback; show/hide `Border_SuddenDeathBanner` |
| **16-4** | [PIE] | `PointsToWin=1` full match → results → Return to Lobby; **3/3** cold starts |
| **16-5** | [PIE] | Regression Steps 12–15 |

#### Results layout (Phase 1 — humanoid dev)

Mirror reference screenshot structure where data exists:

```
┌─────────────────────────────────────────────────────────────┐
│  DEFEAT / VICTORY                    MATCH TIME (optional)  │
│  [round-win pips: blue vs grey hexes]                       │
├─────────────────────────────────────────────────────────────┤
│  MY TEAM          │  STAT LABEL  │  OPPONENT TEAM           │
│  portraits+names  │  K / D / A   │  portraits+names         │
│                   │  OBJECTIVE   │                          │
│                   │  (damage, healing, gold → "—" or hide) │
├─────────────────────────────────────────────────────────────┤
│  MVP: [name]                                                │
│  [ PLAY AGAIN ]              [ RETURN TO LOBBY ]            │
└─────────────────────────────────────────────────────────────┘
```

**OnResultsReady (Results):**

```
Event OnResultsReady (Results)
  → If Results.bLocalPlayerWon → Header = "VICTORY!" else "DEFEAT"
  → Set final score from Results.Team1Score / Team2Score
  → Populate player rows from GameState PlayerArray → Cast Bway Player State
  → K/D/A/Objective from getters; extended rows = "—" or collapsed
  → Set MVP text from Results.MVPPlayerName
  → Return to Lobby button → Call ReturnToLobby()
  → Play Again button → Call PlayAgain()
```

Populate player rows by iterating `Get Game State → Player Array`, filter by team via `Bway Game State → Get Player Team`.

### Experience changes

| Change | Asset |
|--------|-------|
| Set **Results Screen Widget Class** | `BP_BW_GameState` |

No ActionSet changes.

### PIE URL

**Full match / results:**

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=1&RoundDuration=90
```

**Sudden-death banner smoke test** (optional — let round run to ≤60s without scoring):

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=7&PointsToWin=3&RoundDuration=90
```

### Pass checklist

- [ ] `BP_BW_GameState` → `ResultsScreenWidgetClass` = `WBP_BW_ResultsWidget`
- [ ] One round win at `PointsToWin=1` → results shows correct scores (**not 0–0**)
- [ ] VICTORY/DEFEAT reflects local team outcome
- [ ] **Return to Lobby** → `L_LyraFrontEnd` (Step 11-7 behavior)
- [ ] Sudden-death banner visible at ≤60s (or configured `SuddenDeathWarningSeconds` on RM)
- [ ] Match readable without Output Log filtering
- [ ] **3/3** cold-start full-match runs
- [ ] Regression: Steps 12–15 still pass

### Regression

All Section 2 steps; Step 11-7 results RPC + front-end return; Section 1 sudden death midfield rule at 0:00.

### Git discipline

`core-loop: step 16 passed (results + sudden death UI)`

### Troubleshooting

| Symptom | Check |
|---------|--------|
| Results 0–0 | Widget parent = `UBwayResultsScreenWidget`; `ApplyAuthoritativeResults` path via `Client_ShowResults` |
| No results screen | `ResultsScreenWidgetClass` set on `BP_BW_GameState`; match reached PostMatch |
| Return to Lobby fails | `ReturnToLobby()` on results widget → `Server_RequestReturnToFrontEnd` |
| No sudden-death banner | RM `SuddenDeathWarningSeconds` > 0; round reaches ≤60s; bind on CoreHUD |
| Extended stat columns empty | Expected — only K/D/A/Objective tracked on `ABwayPlayerState` today |

---

## Section 2 summary

| Step | Add to experience | Pass when |
|------|-------------------|-----------|
| **12** | `W_BW_CaptureTheRelic_ScoreWidget` + `B_BW_CaptureTheRelic_Scoring` + `LAS_ShooterGame_StandardHUD` | Team scores visible; updates on round win |
| **13** | `W_BW_RelicStatusWidget` (via scoring set) | Carrier / possession state correct on client |
| **14** | `WBP_BW_CoreHUD` shell | Layout: teams, round timer, health; composes or replaces 12–13 |
| **15** | Between-round planning UI (stub, CoreHUD-owned) | Shows on PostRound / `OnBetweenRoundPlanningStarted`; dismisses when Playing resumes |
| **16** | Polish `WBP_BW_ResultsWidget` + sudden-death warning at 60s | Full match readable without log spelunking |

---

# Section 3 — Hero integration (TBD)

Start only after Section 2 is bulletproof. Likely order:

1. Fixed single hero on humanoid fallback (dev)
2. Hero select staging map (resume paused work)
3. Per-hero ability sets in match
4. Hero buildables / economy hooks

---

## Action set split (recommended)

When you reach Step 3, **split** monolithic `EAS_BW_CaptureTheRelic` into:

```
EAS_BW_RelicPickup   → Step 3
EAS_BW_RelicThrow    → Step 4
EAS_BW_RelicPass     → Step 5
```

Keep **`LAS_BW_SharedInput`**, **`B_BW_TeamSetup_TwoTeams`**, **`B_BW_BotSpawner_BallMode`** from Step 1 onward.

---

## DevMap tuning cheat sheet

| Property | Steps 1–7 | Steps 8–10 | Step 11 E2E |
|----------|-----------|------------|-------------|
| `RoundDuration` | N/A | **90s** (fast) | **90s** or restore **180s** — on `UBwayRoundManagementComponent` / config |
| Sudden death warning | N/A | 60s remaining (or **30s** if using 90s rounds) | Same |
| `PointsToWin` | N/A | 3 (PIE) | **1** on `UBwayMatchFlowConfig` or URL `PointsToWin=1` |
| Phase durations | N/A | `RoundEndDelay` only | **`UBwayMatchFlowConfig`**: `WarmupDuration`, `PostRoundDuration` |

---

## Related docs

- [MatchUI_Setup.md](./MatchUI_Setup.md) — Section 2 Steps 12–13
- [CoreHUD_Layout_Setup.md](./CoreHUD_Layout_Setup.md) — Section 2 Step 14
- [RelicBot_AI_Setup.md](./RelicBot_AI_Setup.md)
- [Relic_System.md](./Relic_System.md)
- [MatchFlow_and_Phases.md](./MatchFlow_and_Phases.md)
- [UI_System.md](./UI_System.md)
- [Lyra_Integration.md](./Lyra_Integration.md)
- [HeroSelect_Staging_Setup.md](./HeroSelect_Staging_Setup.md) (paused until Section 3)
- [SYSTEMS_INDEX.md](./SYSTEMS_INDEX.md)

---

## Next actions

1. **Section 2 Step 12-1** — Inspect ShooterCore reference widgets (`W_ScoreWidget_Elimination`, `W_CPScoreWidget`) for AddWidgets slot tags; see [MatchUI_Setup.md](./MatchUI_Setup.md).
2. Optional: **11-8** front-end E2E (queue/custom tile → match → results → menu, **3/3** cold starts).
3. Optional cleanup: clear stale `FrontEndLevel` on `BP_BW_GameState`; standalone packaging pass for **11-1**.
