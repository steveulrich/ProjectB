# Master `/goal` Execution Queue — Vertical Slice 1.0 Polish & 11-Hero Parity

**Last Updated:** October 15, 2026  
**Companion Contracts:** [WORKFLOW_EFFICIENCY.md](./WORKFLOW_EFFICIENCY.md) · [SLICE_STATUS.md](./SLICE_STATUS.md) · [Breakaway Hero Stats Sheet](./Breakaway_Hero_Stats_Sheet.md) · [Design Spec](./Breakaway_Reborn_Design_Spec.md)

Every section below is a **self-contained, copy-pasteable `/goal` prompt** with hard build/package circuit breakers and the single-GPU **2-Rendered D3D12 + 2-NullRHI** rule baked in.

- **Phase 1 (Goals 1–3):** Close remaining core vertical-slice mechanics & technical debt.
- **Phase 2 (Goals 4–9):** Full Vertical Slice 1.0 Polish (**Game Feel**, **Front End & Hero Select**, **In-Game HUD & Results**, **Buffs/Debuffs Readability**, **Upgrade Shop Items/UI Overhaul**, and **1080p Human Playtest Capstone**).
- **Phase 3 (Goals 10–11):** Expand the roster to all 11 Final Public Alpha heroes only after the 4-hero Vertical Slice is completely polished.

---

## PHASE 1: Core Vertical-Slice Mechanics & Technical Debt Closure

### Goal 1 — Vertical Slice All-Kit Combat & Ability Effect Verification Gate

```markdown
/goal Execute Goal 1: Vertical Slice All-Kit Combat & Ability Effect Verification Gate across all 16 abilities (Argus, Alona, Korryn, Rawlins) plus closure of the Rawlins Blazing Barrage changing-aim packaged check and Oct 1–9 Slide C++ repairs.

### MANDATORY OPERATING & CIRCUIT-BREAKER CONSTRAINTS
1. **Context & Resume Hygiene:** Read `AI_Planning/SLICE_STATUS.md` and run `Scripts/Test/Get-NextIterationBrief.ps1` once. NEVER read raw `Saved/Logs/*.log` files or `AI_Planning/Archive/` files into context; use bounded brief scripts (`Get-GateBrief.ps1 -SummaryOnly`, `Get-LogDeltaBrief.ps1`, `Get-RunFamilyBrief.ps1`).
2. **Cheapest Verification First & Hard Circuit Breakers:**
   - Prove all combat math, status tags, and ability lifecycles in `LyraEditor` headless automation (`Scripts/Test/Run-AutomationTests.ps1 -ForceMemoryDDC`) and 4-world PIE BEFORE compiling `LyraGame` or cooking.
   - **Hard Caps:** Max 5 `LyraEditor` builds, Max 2 `LyraGame` builds, and Max 1 `BuildCookRun` package in this gate. After 2 attempts with the same failure and no new causal evidence, STOP repeating that operation, record the blocker in the gate contract, and yield.
3. **Single-GPU 4-Process Topology Rule (RTX 4070 SUPER 12GB):**
   - NEVER launch 4 simultaneous D3D12 rendered processes. Always run **2 Rendered D3D12 peers** (Host + Active Remote Test Owner at `-ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30`) + **2 `-NullRHI` witness peers** (`-NullRHI -MaxFPS=30`). Always include `-csvNoProcessingThread` if CSV profiling is active.
4. **Python Bridge Safety:** Never invoke gameplay RPCs or `TryActivateAbility` directly under `FEditorScriptExecutionGuard` (`guard=1`); always dispatch via a no-world console command and native game-thread ticker (`guard=0`).

### SCOPE & ACCEPTANCE CRITERIA
1. **All-16 Ability Authoritative Combat Suite (`Breakaway.Abilities.AllKitCombatEffects`):**
   - Audit, fix (if any defect is found), and verify authoritative combat outcomes in `Plugins/GameFeatures/BreakawayCore/` and `Plugins/GameFeatures/Heroes/` against `AI_Planning/Breakaway_Hero_Stats_Sheet.md`:
     - **Argus (`UBwayArgusKitConfig`):** `GA_BW_Spartacus_PrimaryAttack` (`48 + 0.6*AttackStr`), `ArgusSlide` (movement + collision), `NoRetreat` (`55 + 0.5*AttackStr` leap AOE + `-15` `State.Status.ArmorShred` for `4s`), `ForGlory` (`+25%` ally move speed & `+10` AttackStr for `5s`), `Retribution` (`50%` directional damage reduction channel + `90–180 + 0.8*AttackStr` release wave + knockback).
     - **Alona (`UBwayAlonaKitConfig`):** `AlonaPrimary` (`42 + 0.55*AttackStr` projectile), `SunsGrace` (dash + `40 + 0.4*AttackStr` ally heal), `Radiance` (`65 + 0.6*AttackStr` AOE damage + `25%` slow `State.Status.Slowed` for `2.5s`), `SunBurst` (`50 + 0.45*AttackStr` cone + knockback), `Blessing` (`ABwayAlonaBlessingZone`: `80 HP/s` heal + `1.5s` invulnerability window).
     - **Korryn (`UBwayKorrynKitConfig`):** `KorrynPrimary` (`44 + 0.6*AttackStr` bolt), `Flock` (`State.Status.Ethereal` untargetable flight + `MaxFlySpeed` restoration), `BurdenOfSin` (`60 + 0.65*AttackStr` + `35%` slow `3s`), `CircleOfSpite` (`ABwayKorrynSpiteZone`: `25 DPS` + `+25%` `IncomingDamageMultiplier = 1.25` via `State.Status.DamageAmplified`), `AuraOfSilence` (`85 + 0.7*AttackStr` AOE + `2.5s` `State.Status.Silenced` blocking non-primary abilities + forcing Relic carrier fumble).
     - **Rawlins (`UBwayRawlinsKitConfig`):** `RawlinsPrimary` (2-shot burst, `28 + 0.45*AttackStr` per shot), `DoubleDown` (`+30%` fire rate & reload buff `4s`), `PowerShot` (`95 + 0.85*AttackStr` piercing round + knockback + Relic carrier fumble), `SlideShot` (slide entry + `50 + 0.5*AttackStr` shot), `BlazingBarrage` (12-shot channel, `4 + 0.3*AttackStr` per shot, upward juggle impulse `350`, post-heal damage interruption).
   - Verify that non-primary damaging abilities properly trigger `UBwayLifeLeechComponent` healing and `ABwayCharacterWithAbilities` kill/assist combat credit.
2. **Rawlins Barrage Changing-Aim Spike & Consolidated 4-Process Packaged Verification:**
   - Diagnose and eliminate the single host `299.810ms` frame spike observed during `bway.Test.RawlinsBarragePackage` changing-aim cast 2 (`AI_Planning/RAWLINS_BARRAGE_ACCEPTANCE.md`).
   - Compile `LyraGame` once, run `Package-Preflight.ps1`, stage a single consolidated candidate (also packaging the Oct 1–9 slide physics repairs), and pass `bway.Test.RawlinsBarragePackage` (`ChangingAim` at `0ms` and `100ms` lag within `200ms` frame bound + 27 controls) plus a 4-process multi-kit combat verification check.
3. **Documentation Closeout:**
   - Update `AI_Planning/SLICE_STATUS.md` and `AI_Planning/RAWLINS_BARRAGE_ACCEPTANCE.md` (`<= 100` lines each).
```

---

### Goal 2 — Relic Ballistics, Prepared Catch, Interceptions & Base Healing/Shop Gate

```markdown
/goal Execute Goal 2: Relic Ballistics, Prepared Catch, Interceptions & Base Healing/Shop Gate.

### MANDATORY OPERATING & CIRCUIT-BREAKER CONSTRAINTS
1. **Context & Resume Hygiene:** Read `AI_Planning/SLICE_STATUS.md` and run `Scripts/Test/Get-NextIterationBrief.ps1` once. Never read raw logs or `AI_Planning/Archive/` files.
2. **Cheapest Verification First & Hard Circuit Breakers:**
   - Prove all C++ mechanics in `LyraEditor` headless automation (`Run-AutomationTests.ps1 -ForceMemoryDDC`) and 4-world PIE BEFORE compiling `LyraGame` or cooking.
   - **Hard Caps:** Max 5 `LyraEditor` builds, Max 2 `LyraGame` builds, and Max 1 `BuildCookRun` package in this gate. Stop after 2 identical failures without new evidence.
3. **Single-GPU 4-Process Topology Rule:** Always run **2 Rendered D3D12 peers** (`-ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30`) + **2 `-NullRHI` witness peers** (`-NullRHI -MaxFPS=30`).
4. **Python Bridge Safety:** Never invoke gameplay RPCs or ability activations under `guard=1`; use a native game-thread ticker (`guard=0`).

### SCOPE & ACCEPTANCE CRITERIA
1. **RMB Prepared Catch & Ballistic Flight Reception (`ARelicActor` & `URelicSettings`):**
   - Audit and verify charged ballistic throws (`MinThrowSpeed=800`, `MaxThrowSpeed=2000`, `MaxChargeTime=1.5s`), directed teammate passes (`PassSpeed=1500`), and RMB prepared catching (`State.Relic.PreparingCatch`).
   - Ensure a teammate holding Prepared Catch cleanly secures high-velocity passes/throws on overlap without bounce-off or duplicate ability grants, and verify `PickupCooldownAfterDrop` (`0.5s`) prevents immediate self-reacquisition by a fumbling carrier.
2. **Mid-Air Pass Interception & Stat Tracking (`ABwayPlayerState::Interceptions`):**
   - Verify that when an opposing player catches or overlaps the Relic while `CurrentState == ERelicState::Passed` (`bIsPassInFlight == true`) and `PassingTeamIndex != CatcherTeamIndex`, authority awards possession to the defender, increments `ABwayPlayerState::Interceptions` by `+1`, and replicates the stat to `FBwayMatchSummary`.
3. **Combat Fumble Thresholds & Forced Fumbles (`ABwayPlayerState::ForcedFumbles`):**
   - Verify that single hits or cumulative carrier damage meeting `URelicSettings::FumbleDamageThreshold` (`50.0`), as well as hard CC/knockback/silence/jail tags (`State.Status.Silenced`, `State.Status.Jailed`), force an immediate `DropRelic` with `FumbleImpulseStrength` and increment the attacker's `ABwayPlayerState::ForcedFumbles` by `+1`.
4. **Team Base Healing Zone & Upgrade Shop Eligibility (`ABwayBaseZone` & `UBwayUpgradeComponent::CanPurchaseUpgrades`):**
   - Audit and verify `ABwayBaseZone` Team Base Healing volumes in `L_Dorado` / `BreakawayCoreRuntime` (`SetInHealingZone` / `IsPlayerInHealingZone`):
     - Friendly players inside their own team's base healing zone regenerate health and satisfy `UBwayUpgradeComponent::CanPurchaseUpgrades` (allowing live shop purchases via `ServerTryPurchaseUpgrade` while alive at base).
     - Stepping outside the base healing zone during `Playing` while alive immediately revokes purchase eligibility (server rejects purchase requests with zero gold debit), while `Prematch` and `Dead` windows remain valid.
5. **Verification & Closeout:**
   - Pass headless automation (`Breakaway.Relic.BallisticsAndInterception` + `Breakaway.Economy.BaseHealingShopWindow`), 4-world PIE, and 1 consolidated 4-process packaged run at `100ms` lag (`2 D3D12 + 2 NullRHI`).
   - Update `AI_Planning/SLICE_STATUS.md`, `AI_Planning/RELIC_PICKUP_OWNERSHIP.md`, and `AI_Planning/SHOP_INPUT_ACCEPTANCE.md` (`<= 100` lines each).
```

---

### Goal 3 — Respawn Delay Progression, Travel NetGUID Fix & Dorado VSM Polish Gate

```markdown
/goal Execute Goal 3: Respawn Delay Progression, Seamless Travel NetGUID Fix, Legacy GameMode Cleanup & Dorado VSM Polish Gate.

### MANDATORY OPERATING & CIRCUIT-BREAKER CONSTRAINTS
1. **Context & Resume Hygiene:** Read `AI_Planning/SLICE_STATUS.md` and run `Scripts/Test/Get-NextIterationBrief.ps1` once. Never read raw logs or `AI_Planning/Archive/` files.
2. **Cheapest Verification First & Hard Circuit Breakers:**
   - Prove all C++ changes in `LyraEditor` headless automation (`Run-AutomationTests.ps1 -ForceMemoryDDC`) and 4-world PIE BEFORE compiling `LyraGame` or cooking.
   - **Hard Caps:** Max 5 `LyraEditor` builds, Max 2 `LyraGame` builds, and Max 1 `BuildCookRun` package in this gate. Stop after 2 identical failures without new evidence.
3. **Single-GPU 4-Process Topology Rule:** Always run **2 Rendered D3D12 peers** (`-ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30`) + **2 `-NullRHI` witness peers** (`-NullRHI -MaxFPS=30`).
4. **Python Bridge Safety:** Never invoke gameplay RPCs or ability activations under `guard=1`; use a native game-thread ticker (`guard=0`).

### SCOPE & ACCEPTANCE CRITERIA
1. **Data-Driven Escalating Respawn Delay Progression (`UBwayRoundManagementComponent` & `UBwayMatchConfigDataAsset`):**
   - Replace the provisional fixed `3.0s` `RespawnDelaySeconds` in `UBwayRoundManagementComponent` with a configurable, data-driven progression (`BaseRespawnDelaySeconds = 10.0f`, `PerDeathRespawnIncrementSeconds = 5.0f`, `MaxRespawnDelaySeconds = 25.0f`, `bResetRespawnEscalationEachRound = true`; preserving fast dev overrides in `DA_BW_DevMatchConfig` while `DA_BW_DemoMatchConfig` uses the `10s` escalating rule per `AI_Planning/FINAL_ALPHA_PARITY.md`).
   - Track per-player within-round death count, reset it to `0` at each round boundary (`StartNewRound` / `ResetForNextMatch`), and verify exact composition with `UBwayHeroAttributeSet::RespawnTimeReduction` (`EffectiveDelay = EscalatedBaseDelay * (1.0f - ClampedReduction)`), including live pending-death timer shortening via `RefreshRespawnSchedule`.
2. **Staging-to-Dorado Seamless Travel NetGUID Warning Resolution (`AI_Planning/TRAVEL_GUID_INVESTIGATION.md`):**
   - Enable targeted scoped `LogNetPackageMap` diagnostics around `L_Bway_HeroSelect` -> `L_Dorado` seamless travel to identify the exact `UObject` path causing `LogRep: Warning: Attempt to reassign read-only NetGUID`.
   - Fix the underlying actor/component/reference lifecycle across seamless travel so `L_Bway_HeroSelect` -> `L_Dorado` completes with **zero** `Attempt to reassign read-only NetGUID` warnings on host and all clients.
3. **Legacy `ABreakawayGameMode` Debt Cleanup:**
   - Remove deprecated duplicate round/scoring/relic methods and properties (`StartRound`, `EndRound`, `ScoreGoal`, `SpawnRelic`, `ResetArenaForNextRound`, `TeamScores`, `RoundState`) from `ABreakawayGameMode`.
4. **Dorado VSM Non-Nanite Queue Overflow Fix (`L_Dorado`):**
   - Resolve `LogRenderer: Warning: Virtual Shadow Map Page Management: Non-Nanite job queue overflow` in `L_Dorado` via Nanite mesh conversion or VSM coarse page settings (`r.Shadow.Virtual.NonNanite.IncludeInCoarsePages`).
5. **Verification & Closeout:**
   - Pass headless automation (`Breakaway.Respawn.EscalatingProgression`), 4-world PIE seamless travel, and 1 consolidated 4-process packaged Connected travel + respawn run at `200ms` lag (`2 D3D12 + 2 NullRHI`).
   - Update `AI_Planning/SLICE_STATUS.md`, `AI_Planning/RESPAWN_COUNTDOWN_ACCEPTANCE.md`, `AI_Planning/RESPAWN_UPGRADE_ACCEPTANCE.md`, and `AI_Planning/TRAVEL_GUID_INVESTIGATION.md` (`<= 100` lines each).
```

---

## PHASE 2: Full Vertical Slice 1.0 Polish Track (Before Roster Expansion)

### Goal 4 — "Game Feel": Movement, Slide-Jump, Camera, Hit Feedback & Combat Readability Gate

```markdown
/goal Execute Goal 4: Vertical Slice "Game Feel" Polish Gate — Movement & Slide-Jump Tuning, Dorado Ramp Smoothness, Floating Damage/Heal Numbers, Directional Hit/Camera-Shake Feedback, Projectile Presentation & Relic Impact Feel.

### MANDATORY OPERATING & CIRCUIT-BREAKER CONSTRAINTS
1. **Context & Resume Hygiene:** Read `AI_Planning/SLICE_STATUS.md`, `AI_Planning/SLIDE_LIFECYCLE_ACCEPTANCE.md`, and `AI_Planning/GAMEPLAY_CUE_ACCEPTANCE.md`. Run `Scripts/Test/Get-NextIterationBrief.ps1` once. Never read raw logs or `AI_Planning/Archive/` files.
2. **Cheapest Verification First & Hard Circuit Breakers:**
   - Prove all C++ tuning, config validation, and feedback routing in `LyraEditor` headless automation (`Run-AutomationTests.ps1 -ForceMemoryDDC`) and 4-world PIE BEFORE compiling `LyraGame` or cooking.
   - **Hard Caps:** Max 5 `LyraEditor` builds, Max 2 `LyraGame` builds, and Max 1 `BuildCookRun` package in this gate. Stop after 2 identical failures without new evidence.
3. **Single-GPU 4-Process Topology Rule:** Always run **2 Rendered D3D12 peers** (`-ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30`) + **2 `-NullRHI` witness peers** (`-NullRHI -MaxFPS=30`).
4. **Python Bridge Safety:** Never invoke gameplay RPCs or ability activations under `guard=1`; use a native game-thread ticker (`guard=0`).

### SCOPE & ACCEPTANCE CRITERIA
1. **Movement, Slide-Jump & Dorado Ramp-to-Flat Feel (`UBwayCharacterMovementComponent` & `UBwayMovementFeelConfig`):**
   - Audit and tune `UBwayMovementFeelConfig` (`SlideJumpMomentumBoost=1.2f`, `SlideJumpMinHorizontalSpeed=400`, `SlideJumpMaxHorizontalSpeed=2000`, `SlideJumpAirControl=0.15f`, `SlideJumpLandedVelocityFactor=0.5f`) and `UBwayCharacterMovementComponent::PhysSliding`.
   - Resolve the Dorado ramp-to-flat transition step-down (`bway.Test.SlideGroundAirDiagnostic` Case 2 `18.28cm` floor step) so sliding down Dorado's ramps onto the flat arena floor maintains continuous ground support without false airborne transitions or momentum loss (`5/5` `SlideGroundAirDiagnostic` cases passing).
   - Verify smooth camera FOV/shake transitions (`ActiveSlideCameraShake`) on slide entry, slide-jump takeoff, and landing without pooled camera-shake warnings.
2. **Combat Readability: Floating Numbers, Hit Confirm & Nameplates (`UBwayCombatReadabilityConfig`, `UBwayCombatNumberPopComponent`, `UGCN_BwayCombatFeedback`, `UBwayNameplatePolicyComponent`):**
   - Verify and polish floating combat numbers (`UBwayCombatNumberPopComponent`) for outgoing damage (`White`), incoming damage (`Red` / colorblind override), and incoming heals (`Green` — Alona heals, Sun Shrine, Base Zone, Life Leech), ensuring `NumberPopLateralOffset` (`35uu`) and `NumberPopRiseDistance` (`80uu`) prevent stacked/illegible text during rapid multi-hit abilities like Rawlins' `BlazingBarrage` or `RawlinsPrimary`.
   - Verify `UBwayCombatFeedbackRouterComponent` and `UGCN_BwayCombatFeedback` deliver crisp local-only hit-confirm audio/reticle feedback, camera shake (respecting `bReduceCameraShake`), and force feedback (respecting `bReduceMotionHaptics`) on all 16 hero abilities and buildable hits.
   - Verify `UBwayNameplatePolicyComponent` distance scaling (`EvaluateNameplateScale`) and batched LOS traces (`NameplateLOSVerticalOffset = 90cm`) so enemy/friendly health bars are legible across Dorado without floor-occlusion flicker.
3. **Projectile & Relic Impact Presentation (`UBwayProjectilePresentationData` & `ARelicActor`):**
   - Audit `UBwayProjectilePresentationData` across Alona (`ABwayAlonaPrimaryProjectile`), Korryn (`ABwayKorrynPrimaryProjectile`), Rawlins (`ABwayRawlinsBulletProjectile`), and Argus (`ABwaySiegeEngineBuildable`) to ensure muzzle/launch cues, in-flight visuals, and impact bursts execute cleanly in rendered and `-NullRHI` modes.
   - Polish `ARelicActor` pickup, pass launch, prepared catch reception, fumble knock-loose, and Goal Score (`GameplayCue.Breakaway.Scoring.Goal`) audio/VFX feedback.
4. **Verification & Closeout:**
   - Pass `Breakaway.CombatPolish`, `Breakaway.Movement.Slide`, and 4-world PIE rendered verification, followed by 1 consolidated 4-process packaged run (`2 D3D12 + 2 NullRHI` at `100ms` lag).
   - Update `AI_Planning/SLICE_STATUS.md`, `AI_Planning/SLIDE_LIFECYCLE_ACCEPTANCE.md`, and `AI_Planning/GAMEPLAY_CUE_ACCEPTANCE.md` (`<= 100` lines each).
```

---

### Goal 5 — Front End, Server Browser, Pause Menu & Staging Hero Select Polish Gate

```markdown
/goal Execute Goal 5: Front End, Server Browser, Pause Menu & Staging Hero Select Polish Gate (`L_Expanse` & `L_Bway_HeroSelect`).

### MANDATORY OPERATING & CIRCUIT-BREAKER CONSTRAINTS
1. **Context & Resume Hygiene:** Read `AI_Planning/SLICE_STATUS.md`, `AI_Planning/PACKAGED_LAN_ACCEPTANCE.md`, and `AI_Planning/LATE_ARRIVAL_SELECTION_ACCEPTANCE.md`. Run `Scripts/Test/Get-NextIterationBrief.ps1` once. Never read raw logs or `AI_Planning/Archive/` files.
2. **Cheapest Verification First & Hard Circuit Breakers:**
   - Prove all widget view-models, state bindings, and asset validations in `LyraEditor` headless automation (`Run-AutomationTests.ps1 -ForceMemoryDDC`) and PIE BEFORE compiling `LyraGame` or cooking.
   - **Hard Caps:** Max 5 `LyraEditor` builds, Max 2 `LyraGame` builds, and Max 1 `BuildCookRun` package in this gate. Stop after 2 identical failures without new evidence.
3. **Single-GPU 4-Process Topology Rule:** Always run **2 Rendered D3D12 peers** (`-ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30`) + **2 `-NullRHI` witness peers** (`-NullRHI -MaxFPS=30`).

### SCOPE & ACCEPTANCE CRITERIA
1. **Front End Experience & Session Browser Polish (`B_LyraFrontEnd_Experience`, `UBwayFrontendStateSubsystem`, `UBwayUserFacingExperienceDefinition`):**
   - Clean up the Front End (`L_Expanse`) main menu and Play flow so only authentic *Breakaway* Capture-the-Relic modes (`Host LAN Match`, `Find LAN Match`, `Dev Practice / Bot Match`, `Settings`, `Quit`) are presented—eliminating duplicate frontend HUD entries or leftover Lyra Shooter sample tiles.
   - Polish the LAN Server Browser list view: display session name, map (`Dorado`), current player count (`X/8`), ping, join button focus, empty-list / searching spinner states, and clear error/retry feedback if a session is full or unreachable.
   - Polish `UBwayPauseMenuWidget` in-match (`Resume`, `Options`, `Return to Lobby` for Host / `Disconnect to Main Menu` for Client) with clean CommonUI `Menu` layer focus restoration.
2. **Staging Hero Select Overhaul (`UBwayHeroSelectWidget`, `WBP_BW_HeroSelect`, `UBwayAbilityPreviewWidget`, `UBwayHeroSlotWidget`):**
   - **Hero Roster Grid & Class Grouping:** Present the 4 vertical-slice heroes (`Argus` Tank, `Alona` Support, `Korryn` Mage, `Rawlins` Fighter/Marksman) with class badges (`FHeroDisplayInfo::ClassName`), portraits (`Portrait`), selection highlight, and lock state (`bIsLocked`).
   - **Live 4v4 Team Roster Sidebars (`OnOtherPlayerSelectionChanged`):** Display Blue Team (4 slots) and Red Team (4 slots) in staging so players see teammates' and bots' live hovered/locked hero portraits, player names, and `Ready (X/Y)` counts (`GetReadyPlayerCount`).
   - **Hero Kit & Buildable Inspection Panel (`Text_HeroDetails` & `UBwayAbilityPreviewWidget`):** When hovering or selecting a hero, populate their base stats (`FHeroStats`: Health, AttackStrength, Armor, MoveSpeed), all 4 combat abilities + Primary + Slide descriptions/cooldowns (`FHeroDisplayInfo::Abilities`), and their **Signature Buildable** card (`Siege Engine`, `Sun Shrine`, `Cursed Ward`, `Jail`).
   - **Countdown, Lock-In & Travel Failure UX:** Display the shared phase countdown (`OnSelectionTimerUpdated`), disable grid switching once locked (`OnLocalSelectionLocked`) while keeping `OnLocalSelectionUnlocked` functional on rematch/pooled reuse, and preserve the **Retry travel** / **Waiting for host** recovery banner (`RefreshTravelFailurePresentation`).
3. **Full Keyboard/Mouse & Gamepad Navigation:**
   - Verify `NativeGetDesiredFocusTarget()` and CommonUI navigation across Front End, Server Browser, Hero Select grid, Lock-In button, and Pause Menu without losing controller focus.
4. **Verification & Closeout:**
   - Pass headless UI/selection automation (`Breakaway.HeroSelection` + `Breakaway.UI.FrontEndAndHeroSelectAudit`), 4-world PIE, and 1 consolidated 4-process packaged LAN run (`Connected` + `SelectionEdges` at `200ms` lag).
   - Update `AI_Planning/SLICE_STATUS.md`, `AI_Planning/PACKAGED_LAN_ACCEPTANCE.md`, and `AI_Planning/LATE_ARRIVAL_SELECTION_ACCEPTANCE.md` (`<= 100` lines each).
```

---

### Goal 6 — In-Game Match HUD, Team Portrait Timers, Round Banners & Results Polish Gate

```markdown
/goal Execute Goal 6: In-Game Match HUD, Team Portrait Respawn Timers, Ability Bar State Overlays, Round Banners & Post-Match Results Polish Gate.

### MANDATORY OPERATING & CIRCUIT-BREAKER CONSTRAINTS
1. **Context & Resume Hygiene:** Read `AI_Planning/SLICE_STATUS.md`, `AI_Planning/RESPAWN_COUNTDOWN_ACCEPTANCE.md`, and `AI_Planning/DEMO_MATCH_RULES.md`. Run `Scripts/Test/Get-NextIterationBrief.ps1` once. Never read raw logs or `AI_Planning/Archive/` files.
2. **Cheapest Verification First & Hard Circuit Breakers:**
   - Prove all view-model bindings and widget state transitions in `LyraEditor` headless automation (`Run-AutomationTests.ps1 -ForceMemoryDDC`) and 4-world PIE BEFORE compiling `LyraGame` or cooking.
   - **Hard Caps:** Max 5 `LyraEditor` builds, Max 2 `LyraGame` builds, and Max 1 `BuildCookRun` package in this gate. Stop after 2 identical failures without new evidence.
3. **Single-GPU 4-Process Topology Rule:** Always run **2 Rendered D3D12 peers** (`-ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30`) + **2 `-NullRHI` witness peers** (`-NullRHI -MaxFPS=30`).

### SCOPE & ACCEPTANCE CRITERIA
1. **Top Broadcast Scorebug & 4v4 Team Portrait Respawn Timers (`UBwayCaptureTheRelicScoreWidget`, `UBwayTeamPortraitsHUDWidget`, `UBwayTeamPortraitSlotWidget`):**
   - Extend `FBwayTeamPortraitSlotViewModel` and `UBwayTeamPortraitSlotWidget` (`BwayTeamPortraitSlotWidget.h/.cpp`) to bind and display **per-player live respawn countdown seconds** (`RespawnRemainingSeconds` / `Text_RespawnTimer`) directly over each dead player's top-bar portrait (`!bIsAlive && RespawnSchedule.bIsWaitingToRespawn`), matching the September 7, 2017 GameSpot broadcast HUD (`AI_Planning/FINAL_ALPHA_PARITY.md`).
   - Ensure each portrait slot clearly shows Friendly (`Blue`) vs Enemy (`Red`) framing, Hero Portrait, Relic Carrier badge (`Text_RelicBadge` / gold border when `bHasRelic == true`), Death overlay (`Image_DeathOverlay`), and immediate reset to active on respawn or round start.
   - Polish `UBwayCaptureTheRelicScoreWidget` & `UBwayRelicStatusWidget`: Best-of-5 round-win pips (`First to 3`), `240s` round timer (`MM:SS` with final-30s urgency highlight), Midfield Territory possession indicator (`Blue Territory` / `Neutral` / `Red Territory` for tiebreaker awareness), and local `UBwayRespawnHUDWidget` center-bottom countdown banner when the local player is dead.
2. **6-Slot Ability Bar & Player Health/Gold HUD (`UBwayAbilityBarHUDWidget`, `UBwayMatchAbilitySlotWidget`, `UBwayHealthHUDWidget`, `UBwayCurrencyDisplayWidget`):**
   - Verify all 6 fixed slots (`Buildable [G]`, `Primary [LMB]`, `Mobility/Slide [Shift/F]`, `Ability 1 [Q]`, `Ability 2 [E]`, `Ultimate [R]`) dynamically update key/gamepad labels (`HandleControlMappingsRebuilt`).
   - Show clear state overlays on `UBwayMatchAbilitySlotWidget`:
     - **Cooldown:** `Image_CooldownOverlay` + `ProgressBar_Cooldown` + `Text_CooldownTimer` ( tenths of a second below `1.0s`, whole seconds above).
     - **Relic Carrier Lock (`bIsRelicAbility` / `State.Relic.Carrying`):** Swap or badge combat slots to show Relic Pass / Throw / Drop controls while carrying the Relic.
     - **Buildable Slot (`bIsBuildable`):** Show `Available (1/Round)` vs `Placed This Round` (`bIsConsumed`) plus current active structure count (`X/Max`).
   - Polish `UBwayHealthHUDWidget` (smooth health bar + recent damage trail + armor value badge) and `UBwayCurrencyDisplayWidget` (`CurrentGold / 500` + Shop keybind prompt `[B / D-Pad Up]` pulsing when in a valid purchase window with affordable upgrades).
3. **Round Phase Banners, Scoreboard & Post-Match Summary (`UBwayPostRoundSummaryWidget`, `UBwayPostMatchInterstitialWidget`, `UBwayResultsScreenWidget`, `UBwayMatchBreakdownWidget`, `UBwayScoreboardWidget`):**
   - Replace any remaining placeholder green wireframe PostRound effects or on-screen debug `PrintString` spam with clean CommonUI phase banners:
     - **Prematch:** Round number (`ROUND X — FIRST TO 3`) + `10s` gate countdown + `SHOP OPEN [B]` prompt.
     - **PostRound (`UBwayPostRoundSummaryWidget`):** Winning team banner + exact win reason (`GOAL SCORED BY <PLAYER>`, `ENEMY TEAM ELIMINATED`, or `TIME EXPIRED — TERRITORY TIEBREAKER`) + updated series score.
     - **Tab Scoreboard (`UBwayScoreboardWidget`) & Post-Match Results (`UBwayResultsScreenWidget`):** 8-player table showing Hero portrait, Player Name, `Kills / Deaths / Assists`, `ObjectiveScore` (Goals), `DamageDealt`, `HealingDone`, `ForcedFumbles`, `Interceptions`, `BuildablesDestroyed`, `MatchGoldEarned`, equipped Upgrade icons/ranks, MVP highlight, and Host `Rematch` / `Return to Lobby` buttons.
4. **Verification & Closeout:**
   - Pass headless HUD view-model automation (`Breakaway.UI.MatchHUDAndPortraits`), 4-world PIE, and 1 consolidated 4-process packaged run at `200ms` lag (`2 D3D12 + 2 NullRHI`).
   - Update `AI_Planning/SLICE_STATUS.md` and `AI_Planning/RESPAWN_COUNTDOWN_ACCEPTANCE.md` (`<= 100` lines each).
```

---

### Goal 7 — Buffs, Debuffs & Status Effect Visual/HUD Readability Gate

```markdown
/goal Execute Goal 7: Buffs, Debuffs & Status Effect Visual/HUD Readability Gate — Local Status Bar, Overhead Nameplate Status Badges, Ability-Bar Silence Lock & Character Status Cues.

### MANDATORY OPERATING & CIRCUIT-BREAKER CONSTRAINTS
1. **Context & Resume Hygiene:** Read `AI_Planning/SLICE_STATUS.md`, `AI_Planning/GAMEPLAY_CUE_ACCEPTANCE.md`, and `AI_Planning/Breakaway_Hero_Stats_Sheet.md`. Run `Scripts/Test/Get-NextIterationBrief.ps1` once. Never read raw logs or `AI_Planning/Archive/` files.
2. **Cheapest Verification First & Hard Circuit Breakers:**
   - Prove all tag listeners, duration queries, and status view-models in `LyraEditor` headless automation (`Run-AutomationTests.ps1 -ForceMemoryDDC`) and 4-world PIE BEFORE compiling `LyraGame` or cooking.
   - **Hard Caps:** Max 5 `LyraEditor` builds, Max 2 `LyraGame` builds, and Max 1 `BuildCookRun` package in this gate. Stop after 2 identical failures without new evidence.
3. **Single-GPU 4-Process Topology Rule:** Always run **2 Rendered D3D12 peers** (`-ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30`) + **2 `-NullRHI` witness peers** (`-NullRHI -MaxFPS=30`).
4. **Python Bridge Safety:** Never invoke gameplay RPCs or ability activations under `guard=1`; use a native game-thread ticker (`guard=0`).

### SCOPE & ACCEPTANCE CRITERIA
1. **Local Player Active Buff & Debuff Status Bar (`UBwayMatchHUDWidgetBase` / Status Tray Widget):**
   - Implement a data-driven **Status Effect Tray** on the local player HUD bound to the owning `UAbilitySystemComponent` active GameplayTags and GameplayEffect remaining durations:
     - **Debuffs (Red/Purple border + countdown sweep):**
       - `State.Status.Silenced` (*Korryn Aura of Silence*, `2.5s`) — also displays a prominent `SILENCED` lock overlay on non-primary slots in `UBwayMatchAbilitySlotWidget`.
       - `State.Status.Jailed` (*Rawlins Jail Cage*, `3.5s`) — displays `JAILED / TRAPPED` movement-lock indicator.
       - `State.Status.ArmorShred` (*Argus No Retreat*, `-15 Armor`, `4.0s`).
       - `State.Status.DamageAmplified` (*Korryn Circle of Spite / Cursed Ward*, `+10% / +25% Incoming Damage`).
       - `State.Status.Slowed` (*Alona Radiance / Korryn Burden of Sin / Cursed Ward*, `-20% to -35% MoveSpeed`).
     - **Positive Buffs (Gold/Cyan/Green border + countdown/aura indicator):**
       - *Argus For Glory* (`+25% MoveSpeed`, `+10 AttackStrength`, `5.0s`).
       - *Alona Blessing Invulnerability* (`1.5s` damage immunity + `80 HP/s` zone heal).
       - *Alona Sun Shrine Aura* (`+15 Armor` while in range).
       - *Korryn Flock* (`State.Status.Ethereal` untargetable flight).
       - *Rawlins Double Down* (`+30% Fire Rate`, `4.0s`).
       - *Base Healing Zone* (`ABwayBaseZone` active regeneration + `SHOP AVAILABLE` badge).
2. **Overhead Nameplate Status Badges (`UBwayNameplatePolicyComponent`):**
   - Surface compact high-priority status icons above enemy/friendly health bars (`Silenced`, `Jailed`, `ArmorShred`, `DamageAmplified`, `Invulnerable/Ethereal`) so teammates and attackers can immediately read combat vulnerability windows in 4v4 fights.
3. **World / Character Status GameplayCues (`BwayGameplayCueTags`):**
   - Ensure each status tag has a registered startup `GameplayCue` (`BwayGameplayCueTags`) that cleanly applies and removes its visual/audio indicator on pawn death, round reset, or duration expiry without invalid MID-parent warnings on `UWidgetComponent`s or `-NullRHI` crashes.
4. **Verification & Closeout:**
   - Pass headless automation (`Breakaway.UI.BuffDebuffReadability` + `Breakaway.GameplayCueAudit`), 4-world PIE across all 11 status/buff types, and 1 consolidated 4-process packaged run at `100ms` lag (`2 D3D12 + 2 NullRHI`).
   - Update `AI_Planning/SLICE_STATUS.md` and `AI_Planning/GAMEPLAY_CUE_ACCEPTANCE.md` (`<= 100` lines each).
```

---

### Goal 8 — Upgrade Shop Items, 6-Card/4-Slot UI Overhaul & Economy UX Polish Gate

```markdown
/goal Execute Goal 8: Match Upgrade Shop Items, 6-Card Catalog Grid, 4-Slot Equipped Loadout Tray & Economy UX Polish Gate (`UBwayUpgradeShopWidget` & `DA_BW_DefaultUpgradeCatalog`).

### MANDATORY OPERATING & CIRCUIT-BREAKER CONSTRAINTS
1. **Context & Resume Hygiene:** Read `AI_Planning/SLICE_STATUS.md`, `AI_Planning/SHOP_INPUT_ACCEPTANCE.md`, `AI_Planning/COOLDOWN_UPGRADE_ACCEPTANCE.md`, `AI_Planning/LIFE_LEECH_ACCEPTANCE.md`, and `AI_Planning/RESPAWN_UPGRADE_ACCEPTANCE.md`. Run `Scripts/Test/Get-NextIterationBrief.ps1` once. Never read raw logs or `AI_Planning/Archive/` files.
2. **Cheapest Verification First & Hard Circuit Breakers:**
   - Prove all catalog validation, widget bindings, and purchase flows in `LyraEditor` headless automation (`Run-AutomationTests.ps1 -ForceMemoryDDC`) and 4-world PIE BEFORE compiling `LyraGame` or cooking.
   - **Hard Caps:** Max 5 `LyraEditor` builds, Max 2 `LyraGame` builds, and Max 1 `BuildCookRun` package in this gate. Stop after 2 identical failures without new evidence.
3. **Single-GPU 4-Process Topology Rule:** Always run **2 Rendered D3D12 peers** (`-ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30`) + **2 `-NullRHI` witness peers** (`-NullRHI -MaxFPS=30`).

### SCOPE & ACCEPTANCE CRITERIA
1. **Catalog Data & Icon Completeness (`UBwayUpgradeCatalog` / `DA_BW_DefaultUpgradeCatalog`):**
   - Audit all 6 items in `/BreakawayCore/Data/DA_BW_DefaultUpgradeCatalog` (`AttackStrength`, `Armor`, `MoveSpeed`, `CooldownReduction`, `LifeLeech`, `RespawnTimeReduction`):
     - Ensure every item has a valid cooked `Icon` texture (`TSoftObjectPtr<UTexture2D>`), clear player-facing `DisplayName`, multi-line `Description` explaining exact per-rank magnitudes (`+5/+10/+15/+20 Attack Strength`, `+5/+10/+15/+20 Armor`, `+5%/+10%/+15%/+20% Move Speed`, `+5%/+10%/+15%/+20% Cooldown Reduction`, `+5%/+10%/+15%/+20% Life Leech`, `+10%/+20%/+30%/+40% Respawn Time Reduction`), and `IsDataValid` passing with zero warnings.
2. **Overhaul `UBwayUpgradeShopWidget` from Dropdown Harness to 6-Card Grid + 4-Slot Loadout UI (`BwayUpgradeShopWidget.h/.cpp`):**
   - Upgrade `UBwayUpgradeShopWidget` from the bare `UComboBoxString` prototype into a polished, readable *Breakaway* Match Upgrade Shop while **retaining full backwards compatibility** with existing automated test helpers (`bway.Test.ShopOpenAndBuy` / `SelectionChanged` / `BuySelected`):
     - **Header Bar:** Displays current Gold balance (`CurrentGold / 500g` with gold coin styling), Equipped Slots counter (`Equipped: X / 4 Slots`), and a live color-coded **Purchase Eligibility Banner**:
       - Green: `SHOP OPEN — PREMATCH`, `SHOP OPEN — AWAITING RESPAWN`, or `SHOP OPEN — IN BASE HEALING ZONE`.
       - Amber/Red (View-Only Mode): `VIEW ONLY — RETURN TO BASE HEALING ZONE OR WAIT FOR RESPAWN TO BUY`.
     - **6-Item Upgrade Card Grid:** Displays all 6 catalog upgrades simultaneously as selectable cards showing:
       - Upgrade Icon & Display Name.
       - **4-Rank Pip Bar** (filled gold pips for owned ranks `1..4`, highlighted preview pip for next rank).
       - Current Total Bonus vs Next Rank Bonus (`e.g., +10% -> +15% CDR`) and Next Rank Gold Cost (`75g / 125g / 175g / 225g` or `MAX RANK`).
       - Distinct visual states for **Affordable & Eligible**, **Insufficient Gold**, **4-Slot Cap Reached (Locked — 4/4 Other Items Equipped)**, and **Max Rank (4/4)**.
     - **4-Slot Equipped Loadout Tray & Live Hero Stat Summary:** Shows the player's 4 active equipment slots at the bottom of the shop alongside live computed `UBwayHeroAttributeSet` totals (`AttackStr`, `Armor`, `MoveSpeed%`, `CDR%`, `LifeLeech%`, `RespawnReduction%`) so players immediately see their build take shape.
3. **Keyboard, Mouse & Gamepad Ergonomics:**
   - Support direct card click / double-click to buy, Keyboard number hotkeys (`1–6` to select card, `Space/Enter` to Buy, `B/Esc` to Close), and full Gamepad `D-Pad` / Left Stick card navigation (`FaceButton_Bottom` Buy, `FaceButton_Right` / `DPad_Up` Close) while keeping movement/look input locked (`bIgnoreMoveInput = true`, `bIgnoreLookInput = true`) and surviving death/replacement-pawn respawn.
4. **Verification & Closeout:**
   - Pass `Breakaway.Economy` automation suite + new `Breakaway.UI.UpgradeShopPresentation` test, 4-world PIE, and 1 consolidated 4-process packaged shop probe + lifecycle run at `200ms` lag (`Get-PackagedShopProbeBrief.ps1`).
   - Update `AI_Planning/SLICE_STATUS.md` and `AI_Planning/SHOP_INPUT_ACCEPTANCE.md` (`<= 100` lines each).
```

---

### Goal 9 — Vertical Slice 1.0 Capstone Human-in-the-Loop Playtest & 1080p Sign-Off Gate

```markdown
/goal Execute Goal 9: Vertical Slice 1.0 Capstone — Full-Flow 1080p Verification, Physical KBM/Gamepad Playtest Checklist & Pre-Expansion Sign-Off Gate.

### MANDATORY OPERATING & CIRCUIT-BREAKER CONSTRAINTS
1. **Context & Resume Hygiene:** Read `AI_Planning/SLICE_STATUS.md` and `AI_Planning/RENDER_PERFORMANCE_ACCEPTANCE.md`. Run `Scripts/Test/Get-NextIterationBrief.ps1` once. Never read raw logs or `AI_Planning/Archive/` files.
2. **Cheapest Verification First & Hard Circuit Breakers:**
   - **Hard Caps:** Max 5 `LyraEditor` builds, Max 2 `LyraGame` builds, and Max 1 `BuildCookRun` package in this gate. Stop after 2 identical failures without new evidence.
3. **Single-GPU Topology Rule:** Run the 1080p full-flow HUD/performance capture as a single rendered D3D12 process (`-ResX=1920 -ResY=1080 -ForceRes -Windowed -csvNoProcessingThread`), and run the 4-process LAN capstone as **2 Rendered D3D12 peers + 2 `-NullRHI` witness peers**.

### SCOPE & ACCEPTANCE CRITERIA
1. **End-to-End 1080p Full-Flow Visual & Performance Audit:**
   - Launch the packaged game at `1920x1080` (`-ForceRes`) through the complete natural player journey:
     - **Front End (`L_Expanse`)** -> **Host LAN Session** -> **Staging Hero Select (`L_Bway_HeroSelect`)** -> **Dorado Arena (`L_Dorado`)** -> **Prematch Shop Purchase** -> **Combat, Buffs/Debuffs, Buildable Placement, Relic Carry/Pass/Throw/Goal** -> **Death & Respawn Portrait/HUD Countdown** -> **Base Healing Zone Shop Purchase** -> **PostRound Banner** -> **PostMatch Results & MVP Breakdown** -> **Rematch & Connected Return to Lobby**.
   - Verify `1080p` frame-time budget (`mean < 8.33ms / > 120 FPS`, `p95 < 11.0ms`) with the full polished HUD active, zero `Accessed None` / `NetGUID` / VSM overflow warnings, and zero focus traps.
2. **Interactive Human Playtest Checklist & Final Vertical Slice 1.0 Sign-Off:**
   - Present a structured interactive playtest summary covering all 4 heroes (`Argus`, `Alona`, `Korryn`, `Rawlins`), Game Feel, Front End, Hero Select, In-Game HUD, Buffs/Debuffs, and Shop UI for user sign-off before unlocking Phase 3 (Roster Expansion).
   - Update `AI_Planning/SLICE_STATUS.md`, `AI_Planning/RENDER_PERFORMANCE_ACCEPTANCE.md`, and `AI_Planning/PROJECT_ROADMAP.md` (`<= 100` lines for all acceptance docs).
```

---

## PHASE 3: Full 11-Hero Final Public Alpha Roster Expansion (After Vertical Slice 1.0 Sign-Off)

### Goal 10 — Roster Expansion Batch A (Thorgrim, Kyra, Merrick, Jarra)

```markdown
/goal Execute Goal 10: Roster Expansion Batch A — Implement C++ Ability Kits, DataAssets, UI/Buff/Shop Integration, and Persistent Buildables for Thorgrim, Kyra, Merrick, and Jarra.

### MANDATORY OPERATING & CIRCUIT-BREAKER CONSTRAINTS
1. **Context & Resume Hygiene:** Read `AI_Planning/SLICE_STATUS.md`, `AI_Planning/Breakaway_Hero_Stats_Sheet.md`, and `AI_Planning/UNREAL_ENGINE_BEST_PRACTICES_BIBLE.md`. Run `Scripts/Test/Get-NextIterationBrief.ps1` once. Never read raw logs or `AI_Planning/Archive/` files.
2. **Cheapest Verification First & Hard Circuit Breakers:**
   - Implement and verify all 4 new hero kits and buildables in `LyraEditor` headless automation (`Run-AutomationTests.ps1 -ForceMemoryDDC`) and 4-world PIE BEFORE compiling `LyraGame` or cooking.
   - **Hard Caps:** Max 5 `LyraEditor` builds, Max 2 `LyraGame` builds, and Max 1 `BuildCookRun` package in this gate. Stop after 2 identical failures without new evidence.
3. **Single-GPU 4-Process Topology Rule:** Always run **2 Rendered D3D12 peers** (`-ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30`) + **2 `-NullRHI` witness peers** (`-NullRHI -MaxFPS=30`).

### SCOPE & ACCEPTANCE CRITERIA
1. **Modular Per-Hero Game Feature Plugins & C++ Runtime Modules (`Plugins/GameFeatures/Heroes/`):**
   - **Thorgrim (`Hero_Thorgrim` / `HeroThorgrimRuntime`):** Tank/Brawler kit, `UBwayThorgrimKitConfig`, 4 GAS abilities + primary attack, and persistent **Wall** buildable (`ABwayThorgrimWallBuildable`) per `AI_Planning/Breakaway_Hero_Stats_Sheet.md`.
   - **Kyra (`Hero_Kyra` / `HeroKyraRuntime`):** Warrior/Marksman kit, `UBwayKyraKitConfig`, 4 GAS abilities + primary attack, and persistent **Ballista** turret buildable (`ABwayKyraBallistaBuildable`) per `AI_Planning/Breakaway_Hero_Stats_Sheet.md`.
   - **Merrick (`Hero_Merrick` / `HeroMerrickRuntime`):** Buccaneer/Ranged kit, `UBwayMerrickKitConfig`, 4 GAS abilities + primary attack, and persistent **Spike Trap** buildable (`ABwayMerrickSpikeTrapBuildable`) per `AI_Planning/Breakaway_Hero_Stats_Sheet.md`.
   - **Jarra (`Hero_Jarra` / `HeroJarraRuntime`):** Huntress/Assassin kit, `UBwayJarraKitConfig`, 4 GAS abilities + primary attack, and persistent **Snap Trap** buildable (`ABwayJarraSnapTrapBuildable`) per `AI_Planning/Breakaway_Hero_Stats_Sheet.md`.
2. **Full Polish Contract Integration:**
   - Wire all 4 heroes into `UBwayHeroRegistry`, `UBwayHeroSelectWidget` (portraits, class badges, ability/buildable previews), `UBwayAbilityBarHUDWidget`, the Goal 7 Buff/Debuff Status Tray & Nameplate badges, `UBwayDamageExecution`, `CooldownReduction`, `LifeLeech`, `ConsumeCombatCredit`, `BwayGameplayCueTags`, and `UBwayBuildableRegistryComponent`.
3. **Verification & Closeout:**
   - Pass `Breakaway.Abilities.BatchACombatAndBuildables` + `Breakaway.GameplayCueAudit` in headless automation, verify 4-world PIE replication, and pass 1 consolidated 4-process packaged LAN run (`2 D3D12 + 2 NullRHI` at `100ms` lag).
   - Update `AI_Planning/SLICE_STATUS.md` and `AI_Planning/PROJECT_ROADMAP.md`.
```

---

### Goal 11 — Roster Expansion Batch B (Sai, Greyven, Gallion) — Complete 11-Hero Parity

```markdown
/goal Execute Goal 11: Roster Expansion Batch B — Implement C++ Ability Kits, DataAssets, UI/Buff/Shop Integration, and Persistent Buildables for Sai, Greyven, and Gallion to complete the 11-Hero Final Public Alpha Roster.

### MANDATORY OPERATING & CIRCUIT-BREAKER CONSTRAINTS
1. **Context & Resume Hygiene:** Read `AI_Planning/SLICE_STATUS.md`, `AI_Planning/Breakaway_Hero_Stats_Sheet.md`, and `AI_Planning/UNREAL_ENGINE_BEST_PRACTICES_BIBLE.md`. Run `Scripts/Test/Get-NextIterationBrief.ps1` once. Never read raw logs or `AI_Planning/Archive/` files.
2. **Cheapest Verification First & Hard Circuit Breakers:**
   - Implement and verify all 3 new hero kits and buildables in `LyraEditor` headless automation (`Run-AutomationTests.ps1 -ForceMemoryDDC`) and 4-world PIE BEFORE compiling `LyraGame` or cooking.
   - **Hard Caps:** Max 5 `LyraEditor` builds, Max 2 `LyraGame` builds, and Max 1 `BuildCookRun` package in this gate. Stop after 2 identical failures without new evidence.
3. **Single-GPU 4-Process Topology Rule:** Always run **2 Rendered D3D12 peers** (`-ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30`) + **2 `-NullRHI` witness peers** (`-NullRHI -MaxFPS=30`).

### SCOPE & ACCEPTANCE CRITERIA
1. **Modular Per-Hero Game Feature Plugins & C++ Runtime Modules (`Plugins/GameFeatures/Heroes/`):**
   - **Sai (`Hero_Sai` / `HeroSaiRuntime`):** Monk/Disruptor kit, `UBwaySaiKitConfig`, 4 GAS abilities + primary attack, and persistent **Conduit** buildable (`ABwaySaiConduitBuildable`) per `AI_Planning/Breakaway_Hero_Stats_Sheet.md`.
   - **Greyven (`Hero_Greyven` / `HeroGreyvenRuntime`):** Storm/Bruiser kit, `UBwayGreyvenKitConfig`, 4 GAS abilities + primary attack, and persistent **Dragonspire** buildable (`ABwayGreyvenDragonspireBuildable`) per `AI_Planning/Breakaway_Hero_Stats_Sheet.md`.
   - **Gallion (`Hero_Gallion` / `HeroGallionRuntime`):** Cosmic/Support-Tank kit, `UBwayGallionKitConfig`, 4 GAS abilities + primary attack, and persistent **Star Light** buildable (`ABwayGallionStarLightBuildable`) per `AI_Planning/Breakaway_Hero_Stats_Sheet.md`.
2. **Full 11-Hero Polish & Subsystem Integration:**
   - Wire all 11 heroes into `UBwayHeroRegistry`, `UBwayHeroSelectWidget`, `UBwayAbilityBarHUDWidget`, the Buff/Debuff Status Tray & Nameplate badges, `ABwayRelicBotController` (`BT_BW_RelicBot`), `UBwayDamageExecution`, `CooldownReduction`, `LifeLeech`, `ConsumeCombatCredit`, `BwayGameplayCueTags`, and `UBwayBuildableRegistryComponent`.
3. **Verification & Closeout:**
   - Pass `Breakaway.Abilities.BatchBCombatAndBuildables` + full 11-hero `Breakaway.GameplayCueAudit` in headless automation, verify 4-world PIE replication, and pass 1 consolidated 4-process packaged LAN match (`2 D3D12 + 2 NullRHI` at `100ms` lag).
   - Update `AI_Planning/SLICE_STATUS.md`, `AI_Planning/PROJECT_ROADMAP.md`, and `AI_Planning/README.md` to record 11-Hero Final Public Alpha completion.
```
