# Breakaway Core Loop — Implementation & Testing Plan

Iterative, step-by-step plan for getting Capture-the-Relic playable on **`L_BW_DevMap`** using **`B_BW_Experience_Dev`**, humanoid pawns only, and Lyra bots. Each step builds on the last; **do not advance until a step passes its checklist 3 cold starts in a row**.

Decisions captured from design review (May 2026).

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

**Status:** C++ implemented. Editor: strip `B_BW_Experience_Dev` to Step 1 baseline (see Step 1).

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

**Goal:** Thrown / passed relic crossing enemy goal plane scores.

### Pass checklist

- [ ] Throw into enemy goal → score registers
- [ ] Pass into enemy goal → score registers
- [ ] **`Net PktLag=100`**
- [ ] 3/3 cold starts

---

## Step 8 — Rounds & reset

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

Sudden death is **not** a mode change — goal and elimination work normally all round, including the final minute.

- **At 60s remaining:** awareness only (delegate / log; UI in Section 2)
- **At 0:00:** if round still active, team whose **half** the relic is in **loses** → other team wins
- **Replaces** current `TimeExpired` possession win in `OnRoundTimerExpired`
- The final minute does **not** turn on or off any win conditions — only awareness

### C++ work (real feature, not gating)

1. **`EBwayWinCondition::SuddenDeath`** (+ gameplay tag)
2. **Midfield boundary** — plane / volume on DevMap (content) + helper: `GetRelicHalf()` / `GetLosingTeamAtMidfield()`
3. **`OnRoundTimerExpired`** — use midfield rule, not `DetermineRelicPossessionTeam()`
4. **`OnSuddenDeathWarning`** broadcast at 60s remaining (for future UI)

### Pass checklist

- [ ] Round reaches 0:00 with no goal → midfield rule picks winner
- [ ] Relic on Team 1 half at 0:00 → Team 2 wins
- [ ] Relic on Team 2 half at 0:00 → Team 1 wins
- [ ] Goal during final minute still ends round immediately (no regression)
- [ ] 3/3 cold starts + listen server

---

## Step 10 — Bot relic AI

**Goal:** Bots play the ball — no combat, no buildables.

### Deliverables

- **`BT_BW_RelicBot`** + blackboard (relic location, enemy goal, carrier, sudden-death flag)
- Wire via **`B_BW_BotSpawner_BallMode`** (bot controller + BT)
- Behaviors: seek pickup, walk-in score, throw at goal, pass to forward teammate, chase enemy carrier

### Pass checklist

- [ ] Bot team scores without human touching relic (watch 1 full round)
- [ ] Human can still play normally alongside bots
- [ ] 3/3 runs where bot-initiated score happens

---

## Step 11 — Match flow (no hero staging)

**Goal:** Front-end → match → results → front-end. No post-match lobby.

### Changes

| Asset | Change |
|-------|--------|
| `BwayUserFacingExperienceDefinition` playlist tile | `MapID = L_BW_DevMap`, `ExperienceID = B_BW_Experience_Dev`, **`bRouteThroughHeroSelectStaging = false`** |
| Experience | Add **`BW_Phase_PostGame`**, results widget hook |
| GameState | **`PointsToWin = 1`** for faster E2E tests |
| Experience | **`WBP_BW_ResultsWidget`** (minimal — polish in Section 2) |

### Pass checklist (launch from **front-end**, not PIE)

- [ ] Pick Dev playlist tile → loads DevMap humanoid match
- [ ] No hero select UI
- [ ] Play to match end → results screen shows winner
- [ ] Continue → **`L_LyraFrontEnd`**
- [ ] 3/3 full runs

---

# Section 2 — Match UI (before heroes)

No production HUD in Section 1. Add widgets **one at a time** to `B_BW_Experience_Dev`; same bulletproof bar.

| Step | Add to experience | Pass when |
|------|-------------------|-----------|
| **12** | `W_BW_CaptureTheRelic_ScoreWidget` + scoring set | Team scores visible; updates on round win |
| **13** | `W_BW_RelicStatusWidget` | Carrier / state correct |
| **14** | `WBP_BW_CoreHUD` shell | Layout, teams, round timer |
| **15** | Between-round planning UI (stub OK) | Planning phase shows / dismisses |
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
| `RoundDuration` | N/A | **90s** (fast) | **90s** or restore **180s** |
| Sudden death warning | N/A | 60s remaining (or **30s** if using 90s rounds) | Same |
| `PointsToWin` | N/A | 3 | **1** for flow test |

---

## Related docs

- [Relic_System.md](./Relic_System.md)
- [MatchFlow_and_Phases.md](./MatchFlow_and_Phases.md)
- [Lyra_Integration.md](./Lyra_Integration.md)
- [HeroSelect_Staging_Setup.md](./HeroSelect_Staging_Setup.md) (paused until Section 3)
- [SYSTEMS_INDEX.md](./SYSTEMS_INDEX.md)

---

## Next actions

1. Implement **Step 0** C++ + strip `B_BW_Experience_Dev` to Step 1 config
2. Recompile and run Step 1 checklist
3. Commit when green; proceed to Step 2
