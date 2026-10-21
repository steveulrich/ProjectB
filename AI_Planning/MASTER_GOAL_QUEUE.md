# Master `/goal` execution queue — Vertical Slice 1.0 polish and 11-hero parity

**Last Updated:** October 16, 2026
**Companion Contracts:** [WORKFLOW_EFFICIENCY.md](./WORKFLOW_EFFICIENCY.md) · [SLICE_STATUS.md](./SLICE_STATUS.md) · [Breakaway Hero Stats Sheet](./Breakaway_Hero_Stats_Sheet.md) · [Design Spec](./Breakaway_Reborn_Design_Spec.md)

Commit review of `ed6008b9` found uncovered mechanics and evidence gaps; see [SLICE_STATUS.md](./SLICE_STATUS.md). The older October 15 “Goal 4” probe does not close the expanded Goals 4–9 below. **Goal 1 was resumed on October 16, 2026.** The ledger records concurrent authorized fixes and remaining pauses; reviewing this queue alone does not resume gameplay.

Each goal is a copy-pasteable `/goal` prompt that imports the shared contract in this file. Invoke a prompt only when its implementation is authorized. Goal numbers describe work, not verified completion; the ledger and linked acceptance records own current verdicts.

- **Phase 1 (Goals 1–3):** Close remaining core vertical-slice mechanics & technical debt.
- **Phase 2 (Goals 4–9):** Full Vertical Slice 1.0 Polish (**Game Feel**, **Front End & Hero Select**, **In-Game HUD & Results**, **Buffs/Debuffs Readability**, **Upgrade Shop Items/UI Overhaul**, and **1080p Human Playtest Capstone**).
- **Phase 3 (Goals 10–11):** Expand the roster to all 11 Final Public Alpha heroes only after the 4-hero Vertical Slice is completely polished.

## Shared execution and completion contract

Apply this contract to every goal. Root [AGENTS.md](../AGENTS.md) owns project instructions; the [workflow index](./WORKFLOW_EFFICIENCY.md) routes operational procedures. Acceptance criteria in this queue and linked acceptance documents remain required.

1. **Select current work and evidence.** On explicit resume, read the ledger and select one pending gate with an expected observable result. Run `Scripts/Test/Get-NextIterationBrief.ps1 -GateContractPath <contract> -ContractPhase <compile|runtime|package>` once. Use the same contract through its phases. After deliberate source or metadata edits, refresh snapshots/workflow as directed by the [resume runbook](./Runbooks/gate-resume-evidence.md). Stop artifact reuse on freshness or identity warnings; reconcile source, patch, assets, configuration, and candidate identity first.
2. **Reconcile gameplay targets before editing.** Use the [vertical-slice definition](./VERTICAL_SLICE_DEFINITION.md), [final-alpha evidence](./FINAL_ALPHA_PARITY.md), [hero stats sheet](./Breakaway_Hero_Stats_Sheet.md), and [hero codename map](./HERO_CODENAME_MAP.md). Record each disputed requirement's current implementation, intended behavior, source/date, uncertainty, and decision in the gate evidence. The stats sheet is the working kit specification; unresolved source columns or historical claims need reconciliation. Do not restore obsolete prototype behavior from an older queue. Keep unverified tuning labeled provisional; request only a missing consequential design decision. A project tuning decision does not establish historical parity.
3. **Define a complete verification matrix.** Map every criterion to a test or observation, expected result, candidate/configuration, and evidence path. Distinguish implementation, headless, same-process PIE, packaged transport, rendered presentation, physical input, and human acceptance. Treat new suite names below as planned until discovered; author missing meaningful tests or use a documented equivalent. A zero-test run, startup success, fixture setup failure, or fabricated PASS marker cannot close a criterion. Use production activation/input and ownership paths for integration proof; clean up fixture state.
4. **Use the build and package runbook.** Read [build and package](./Runbooks/build-package.md), the relevant [testing ladder](../Plugins/GameFeatures/BreakawayCore/Docs/Agent_Testing_Ladder.md) sections, and [helper parameters](../Scripts/Test/README.md) before those operations. Run affected cheaper checks first. Build only the needed `LyraEditor` or `LyraGame` target through `Scripts/Test/Tier1-CompileGate.ps1`; no solution-wide or standalone engine-target builds. Require successful exits and empty `failureSignals`. Before packaging, pass `Package-Preflight.ps1` with current game compile evidence and required write checks. Reuse a matching frozen candidate; one consolidated package is a maximum, not a required recook.
5. **Keep circuit breakers and ownership.** Hard per-gate caps are **5 Editor builds, 2 Game builds, and 1 BuildCookRun package**. Preserve counts across resumes; do not rename the same gate to reset them. After two attempts with the same failure and no new causal evidence, stop that operation, record cause/remedies/retry condition, and continue independent authorized work where possible. Reuse owned handles. Hidden editors are a stop signal: identify owners and close normally. Follow [editor and PIE](./Runbooks/editor-pie.md); dispatch gameplay RPCs and abilities through a native game-thread ticker with `guard=0`, never under the Python execution guard.
6. **Record what the topology proves.** On this single RTX 4070 SUPER workstation, four-process transport uses **2 rendered D3D12 peers + 2 NullRHI witnesses**. Render the host and active remote owner at `640x360`, windowed, `-ForceRes -MaxFPS=30`; cap witnesses at 30 FPS. Never launch four D3D12 peers on this GPU. Follow [packaged transport](./Runbooks/packaged-transport-shop.md), including `-csvNoProcessingThread` whenever CSV profiling is enabled. Rotate rendered roles when needed. This topology proves transport and replication; separate 1080p presentation, physical input, four-human play, and real second-PC LAN evidence remain required.
7. **Measure presentation and performance directly.** Goals 4–8 require fresh rendered host/remote observations for changed UI and effects, including readable text, focus, layering, timing, audio, and lifecycle cleanup. Capture the actual viewport and candidate. Headless, NullRHI, synthetic input, and widget presence cannot establish those results. Keep physical KBM/gamepad/haptics and full-flow sign-off in Goal 9. Missing UI/device access leaves explicit pending proof. Use unclamped timing samples and actual log counts; never infer zero warnings or passing focus from an unmeasured field.
8. **Close only proven scope.** Preserve full evidence under `Saved/Logs/`; start with bounded briefs and read a narrow raw range or historical record only when it resolves a specific diagnosis. Record expected/actual result, first causal error, scope, counts, retries, remaining proof, and next action. Retest affected invalidated results and complete the final end-to-end checks. Keep `SLICE_STATUS.md` within 4,000 characters and about 100 lines; detailed acceptance documents have no blanket line cap. Do not delete requirements or evidence to shorten them. After a pass or recorded blocker, continue the next authorized gate while the goal is active.

## Dependencies and release decisions

| Goals | Required closure |
| --- | --- |
| 1–3 | Close the post-review gaps listed in the ledger and linked contracts; reuse historical passes only within unchanged scope. |
| 4–8 | Close affected core dependencies before polish sign-off. Goal 6 depends on respawn/selection behavior; Goal 7 on reconciled kits; Goal 8 on purchase eligibility and upgrade lifecycle. Independent authorized checks can proceed while another gate is blocked. |
| 9 | Goals 1–8, all four baseline buildables, integrated placeholder art, physical devices, real LAN, and measured 1080p performance must pass on the final compatible candidate. Record explicit human sign-off before expansion. |
| 10 | Goal 9 sign-off is recorded. Integrate and verify Batch A without regressing the baseline heroes. |
| 11 | Batch A is accepted. Verify all 11 heroes, repeat the integrated capstone after expansion, and close the project-wide parity inventory before claiming final-alpha completion. |

The four-hero slice, 11-hero roster, and final-public-alpha parity are distinct milestones. Track unresearched systems from `FINAL_ALPHA_PARITY.md` as unresolved requirements or explicit owner decisions; roster coverage alone cannot silently exclude them.

---

## PHASE 1: Core Vertical-Slice Mechanics & Technical Debt Closure

### Goal 1 — Vertical Slice All-Kit Combat & Ability Effect Verification Gate

```markdown
/goal Execute Goal 1: Vertical Slice All-Kit Combat & Ability Effect Verification Gate across all 20 abilities (16 kit abilities plus 4 primaries) (Argus, Alona, Korryn, Rawlins) plus closure of the Rawlins Blazing Barrage changing-aim packaged check and Oct 1–9 Slide C++ repairs.

### Execution requirements

- Follow the **Shared execution and completion contract** and **Dependencies and release decisions** in `AI_Planning/MASTER_GOAL_QUEUE.md`. Read `AI_Planning/SLICE_STATUS.md` and select the current gate contract before work.
- Read the relevant acceptance records under `AI_Planning/`: `RAWLINS_BARRAGE_ACCEPTANCE.md`, `SLIDE_LIFECYCLE_ACCEPTANCE.md`, `COMBAT_CREDIT_ACCEPTANCE.md`, `LIFE_LEECH_ACCEPTANCE.md`.

### Scope and acceptance criteria
1. **All-20 Ability Authoritative Combat Suite (`Breakaway.Abilities.AllKitCombatEffects`):**
   - Audit, fix defects, and verify authoritative outcomes in `Plugins/GameFeatures/BreakawayCore/` and `Plugins/GameFeatures/Heroes/`. Build a 20-action matrix from the stats sheet and reconciled kit data, including base stats, damage/healing scaling, cooldown, targeting, duration, CC, cancellation, and cleanup:
     - **Argus (`UBwayArgusKitConfig`):** melee primary, invulnerable directional `ArgusSlide`, straight-line `NoRetreat` charge, unstoppable `ForGlory` kick/knockback, and `Retribution` uppercut/downsmash.
     - **Alona (`UBwayAlonaKitConfig`):** light-projectile primary, invulnerable `SunsGrace` movement, targeted ally healing through `Radiance`, defensive `SunBurst` knockback, and initial plus sustained `Blessing` zone healing.
     - **Korryn (`UBwayKorrynKitConfig`):** primary with stacking armor reduction, `Flock` ethereal movement, `BurdenOfSin` slow, `CircleOfSpite` slow/damage amplification, and `AuraOfSilence` damage/silence.
     - **Rawlins (`UBwayRawlinsKitConfig`):** two-shot primary, invulnerable `DoubleDown` roll, `PowerShot` knockback, `SlideShot` launch, and aimable/interruption-sensitive 12-shot `BlazingBarrage` with airborne juggle.
   - Verify actual owning-client activation, authority outcomes, and settled owner/proxy state. Cover friendly/enemy filtering, misses, immunity, overlapping effects, death, cancellation, and round reset. Confirm silence/carrier restrictions and eligible forced fumbles without assuming every damaging action applies every status.
   - Verify that non-primary damaging abilities properly trigger `UBwayLifeLeechComponent` healing and `ABwayCharacterWithAbilities` kill/assist combat credit.
   - Revalidate fixture isolation after review: restore LifeLeech base values, velocity, controller aim, tags, and actors. Re-run affected cases without carrying fixture state into the next case.
2. **Rawlins Barrage Changing-Aim Spike & Consolidated 4-Process Packaged Verification:**
   - Reconcile historical evidence in `RAWLINS_BARRAGE_ACCEPTANCE.md`: the `299.810ms` spike is a prior failure, and changing aim has a later scoped pass. Reproduce only invalidated coverage; retain the `200ms` frame bound and all 27 controls at `0ms` and `100ms` lag.
   - Complete the still-open cooked damage-interruption and death/round-reset subcases. Use a compatible candidate for `bway.Test.RawlinsBarragePackage` and multi-kit combat verification; a pre-review executable cannot prove corrected source.
   - Verify held-jump momentum does not compound and a dedicated concave multi-contact slide fixture consumes one frame's simulation time. Require multiplayer owner/authority/proxy convergence and package the relevant slide fixes before accepting them.
3. **Documentation Closeout:**
   - Update `AI_Planning/SLICE_STATUS.md` and `AI_Planning/RAWLINS_BARRAGE_ACCEPTANCE.md`.
```

---

### Goal 2 — Relic Ballistics, Prepared Catch, Interceptions & Base Healing/Shop Gate

```markdown
/goal Execute Goal 2: Relic Ballistics, Prepared Catch, Interceptions & Base Healing/Shop Gate.

### Execution requirements

- Follow the **Shared execution and completion contract** and **Dependencies and release decisions** in `AI_Planning/MASTER_GOAL_QUEUE.md`. Read `AI_Planning/SLICE_STATUS.md` and select the current gate contract before work.
- Read the relevant acceptance records under `AI_Planning/`: `RELIC_PICKUP_OWNERSHIP.md`, `RELIC_CARRY_ACCEPTANCE.md`, `RELIC_REQUEST_INDICATOR.md`, `SHOP_INPUT_ACCEPTANCE.md`.

### Scope and acceptance criteria
1. **RMB Prepared Catch & Ballistic Flight Reception (`ARelicActor` & `URelicSettings`):**
   - Audit configured throw speeds/charge duration, directed passes, and RMB prepared catching (`State.Relic.PreparingCatch`). Treat current `800..2000` throw speed, `1.5s` charge, and `1500` pass speed as project tuning pending reference reconciliation.
   - Complete the production `GA_BW_Relic_Throw` press/hold/release path. Prove minimum, intermediate, and capped charge from the owning client's input through server-authoritative launch; math helpers alone cannot pass this criterion. Cancel/reset charge on release completion, drop, death, and possession change; reject stale or duplicate release requests without double launch.
   - Ensure a teammate holding Prepared Catch cleanly secures high-velocity passes/throws on overlap without bounce-off or duplicate ability grants, and verify `PickupCooldownAfterDrop` (`0.5s`) prevents immediate self-reacquisition by a fumbling carrier.
   - Reconcile held prepared-catch input separately from the timed call-for-relic indicator. Clear catch eligibility on release where required by the selected input design while preserving the indicator's intended three-second lifetime. Verify expiry/removal, ownership changes, granted abilities, and replicated presentation leave neither state stale.
2. **Mid-Air Pass Interception & Stat Tracking (`ABwayPlayerState::Interceptions`):**
   - Verify that when an opposing player catches or overlaps the Relic while `CurrentState == ERelicState::BeingPassed` (`bIsPassInFlight == true`) and `PassingTeamIndex != CatcherTeamIndex`, authority awards possession to the defender, increments `ABwayPlayerState::Interceptions` by `+1`, and replicates the stat to `FBwayMatchSummary`.
   - Exercise actual missed-pass flight and physics landing/contact. Clear pass-flight state and passing-team ownership on landing; a later opponent ground pickup must not count as an interception. Test ordinary throws, teammate catches, duplicate overlaps, and round reset as negative controls.
3. **Combat Fumble Thresholds & Forced Fumbles (`ABwayPlayerState::ForcedFumbles`):**
   - Verify that single hits or cumulative carrier damage meeting `URelicSettings::FumbleDamageThreshold` (`50.0`), as well as hard CC/knockback/silence/jail tags (`State.Status.Silenced`, `State.Status.Jailed`), force an immediate `DropRelic` with `FumbleImpulseStrength` and increment the attacker's `ABwayPlayerState::ForcedFumbles` by `+1`.
   - Revalidate the corrected forcing-opponent attribution through owner/server and all peers. The forcing CC source takes priority over earlier chip damage; award exactly once, never to the victim, a teammate, or a stale attacker. Record the reconciled threshold/accumulation policy as tuning.
4. **Team Base Healing Zone & Upgrade Shop Eligibility (`ABwayBaseZone` & `UBwayUpgradeComponent::CanPurchaseUpgrades`):**
   - Audit and verify `ABwayBaseZone` Team Base Healing volumes in `L_BW_Dorado` / `BreakawayCoreRuntime` (`SetInHealingZone` / `IsPlayerInHealingZone`):
     - Friendly players inside their own team's base healing zone regenerate health and satisfy `UBwayUpgradeComponent::CanPurchaseUpgrades` (allowing live shop purchases via `ServerTryPurchaseUpgrade` while alive at base).
     - Stepping outside the base healing zone during `Playing` while alive immediately revokes purchase eligibility (server rejects purchase requests with zero gold debit), while `Prematch` and `Dead` windows remain valid.
     - Cover enemy bases, overlapping zone enter/exit, death/respawn, and reconnect. Keep healing, UI eligibility, and server purchase rejection consistent without duplicate healing or persistent stale eligibility.
5. **Verification & Closeout:**
   - Pass headless automation (`Breakaway.Relic.BallisticsAndInterception` + `Breakaway.Economy.BaseHealingShopWindow`), 4-world PIE, and a consolidated 4-process packaged run at `100ms` lag (`2 D3D12 + 2 NullRHI`).
   - Update `AI_Planning/SLICE_STATUS.md`, `AI_Planning/RELIC_PICKUP_OWNERSHIP.md`, and `AI_Planning/SHOP_INPUT_ACCEPTANCE.md`.
```

---

### Goal 3 — Respawn Delay Progression, Travel NetGUID Fix & Dorado VSM Polish Gate

```markdown
/goal Execute Goal 3: Respawn Delay Progression, Seamless Travel NetGUID Fix, Legacy GameMode Cleanup & Dorado VSM Polish Gate.

### Execution requirements

- Follow the **Shared execution and completion contract** and **Dependencies and release decisions** in `AI_Planning/MASTER_GOAL_QUEUE.md`. Read `AI_Planning/SLICE_STATUS.md` and select the current gate contract before work.
- Read the relevant acceptance records under `AI_Planning/`: `RESPAWN_COUNTDOWN_ACCEPTANCE.md`, `RESPAWN_UPGRADE_ACCEPTANCE.md`, `TRAVEL_GUID_INVESTIGATION.md`, `RENDER_PERFORMANCE_ACCEPTANCE.md`.

### Scope and acceptance criteria
1. **Data-Driven Escalating Respawn Delay Progression (`UBwayRoundManagementComponent` & `UBwayMatchFlowConfig`):**
   - Verify the implemented progression rather than replacing it again. Demo currently uses `10s` base, `5s` increment, `25s` cap, and per-round reset. These are project tuning choices; `FINAL_ALPHA_PARITY.md` does not establish the exact original progression, cap, or reset boundary.
   - Preserve `DA_BW_MatchFlow_Dev` fixed `3s` behavior (`3/0/3`) and verify both actual Dev and Demo assets, selected experience, and cooked runtime settings. The review's Dev asset-backed headless pass does not close runtime/cooked proof.
   - Track per-player within-round death count, reset it to `0` at each round boundary (`StartNewRound` / `ResetForNextMatch`), and verify exact composition with `UBwayHeroAttributeSet::RespawnTimeReduction` (`EffectiveDelay = EscalatedBaseDelay * (1.0f - ClampedReduction)`), including live pending-death timer shortening via `RefreshRespawnSchedule`.
2. **Staging-to-Dorado Seamless Travel NetGUID Warning Resolution (`AI_Planning/TRAVEL_GUID_INVESTIGATION.md`):**
   - Enable targeted scoped `LogNetPackageMap` diagnostics around `L_BW_HeroSelect_Staging` -> `L_BW_Dorado` seamless travel to identify the exact `UObject` path causing `LogRep: Warning: Attempt to reassign read-only NetGUID`.
   - Fix the underlying actor/component/reference lifecycle across seamless travel so `L_BW_HeroSelect_Staging` -> `L_BW_Dorado` completes with **zero** `Attempt to reassign read-only NetGUID` warnings on host and all clients.
   - Revalidate the reviewed GUID retirement/pin fixes in fresh PIE travel/teardown and compatible packaged travel/rematch, including delayed-client loading. Require zero invalid-GUID errors, preserved roster/replication, retired/dead/alias GUID timestamps, and no new pins after `STOP`. Verify driver scoping and module-hook cleanup; suppressing a warning does not prove a safe lifecycle.
3. **Legacy `ABreakawayGameMode` Debt Cleanup:**
   - Audit the completed removal of duplicate round/scoring/relic authority and any remaining serialized Blueprint references. Preserve required delegation to the production managers, including `RelicMgr->SpawnRelic()`; do not delete working delegation merely because a legacy method shared its name.
4. **Dorado VSM Non-Nanite Queue Overflow Fix (`L_BW_Dorado`):**
   - Reuse the recorded VSM fix only where source/assets/settings still match. Revalidate affected rendered coverage in `L_BW_Dorado`, using actual overflow counts and visible shadow quality. Select any further Nanite or coarse-page changes from evidence; do not hide warnings or degrade the presentation to obtain a pass.
5. **Verification & Closeout:**
   - Pass headless automation (`Breakaway.Respawn.EscalatingProgression`), 4-world PIE seamless travel, and a consolidated 4-process packaged Connected travel + respawn run at `200ms` lag (`2 D3D12 + 2 NullRHI`).
   - Update `AI_Planning/SLICE_STATUS.md`, `AI_Planning/RESPAWN_COUNTDOWN_ACCEPTANCE.md`, `AI_Planning/RESPAWN_UPGRADE_ACCEPTANCE.md`, and `AI_Planning/TRAVEL_GUID_INVESTIGATION.md`.
```

---

## PHASE 2: Full Vertical Slice 1.0 Polish Track (Before Roster Expansion)

### Goal 4 — "Game Feel": Movement, Slide-Jump, Camera, Hit Feedback & Combat Readability Gate

```markdown
/goal Execute Goal 4: Vertical Slice "Game Feel" Polish Gate — Movement & Slide-Jump Tuning, Dorado Ramp Smoothness, Floating Damage/Heal Numbers, Directional Hit/Camera-Shake Feedback, Projectile Presentation & Relic Impact Feel.

### Execution requirements

- Follow the **Shared execution and completion contract** and **Dependencies and release decisions** in `AI_Planning/MASTER_GOAL_QUEUE.md`. Read `AI_Planning/SLICE_STATUS.md` and select the current gate contract before work.
- Read the relevant acceptance records under `AI_Planning/`: `SLIDE_LIFECYCLE_ACCEPTANCE.md`, `GAMEPLAY_CUE_ACCEPTANCE.md`, `RELIC_VFX_ACCEPTANCE.md`.

### Scope and acceptance criteria
1. **Movement, Slide-Jump & Dorado Ramp-to-Flat Feel (`UBwayCharacterMovementComponent` & `UBwayMovementFeelConfig`):**
   - Audit and tune `UBwayMovementFeelConfig` and `UBwayCharacterMovementComponent::PhysSliding` from the current reviewed baseline. Record chosen momentum/speed/air-control/landing values and expected feel; older queue defaults are not acceptance targets. Preserve Goal 1's held-jump and multi-contact regressions.
   - Resolve the Dorado ramp-to-flat transition step-down (`bway.Test.SlideGroundAirDiagnostic` Case 2 `18.28cm` floor step) so sliding down Dorado's ramps onto the flat arena floor maintains continuous ground support without false airborne transitions or momentum loss (`5/5` `SlideGroundAirDiagnostic` cases passing).
   - Verify smooth camera FOV/shake transitions (`ActiveSlideCameraShake`) on slide entry, slide-jump takeoff, and landing without pooled camera-shake warnings.
2. **Combat Readability: Floating Numbers, Hit Confirm & Nameplates (`UBwayCombatReadabilityConfig`, `UBwayCombatNumberPopComponent`, `UGCN_BwayCombatFeedback`, `UBwayNameplatePolicyComponent`):**
   - Verify and polish floating combat numbers (`UBwayCombatNumberPopComponent`) for outgoing damage (`White`), incoming damage (`Red` / colorblind override), and incoming heals (`Green` — Alona heals, Sun Shrine, Base Zone, Life Leech), ensuring `NumberPopLateralOffset` (`35uu`) and `NumberPopRiseDistance` (`80uu`) prevent stacked/illegible text during rapid multi-hit abilities like Rawlins' `BlazingBarrage` or `RawlinsPrimary`.
   - Verify `UBwayCombatFeedbackRouterComponent` and `UGCN_BwayCombatFeedback` route the appropriate damage, heal, movement, and status feedback across all 20 actions and four buildables. Hit-confirm audio/reticle feedback, camera shake (`bReduceCameraShake`), and force feedback (`bReduceMotionHaptics`) must respect ownership, settings, and action eligibility, with no duplicate predicted/server feedback.
   - Verify `UBwayNameplatePolicyComponent` distance scaling (`EvaluateNameplateScale`) and batched LOS traces (`NameplateLOSVerticalOffset = 90cm`) so enemy/friendly health bars are legible across Dorado without floor-occlusion flicker.
3. **Projectile & Relic Impact Presentation (`UBwayProjectilePresentationData` & `ARelicActor`):**
   - Audit `UBwayProjectilePresentationData` across Alona (`ABwayAlonaPrimaryProjectile`), Korryn (`ABwayKorrynPrimaryProjectile`), Rawlins (`ABwayRawlinsBulletProjectile`), and Argus (`ABwaySiegeEngineBuildable`) to ensure muzzle/launch cues, in-flight visuals, and impact bursts execute cleanly in rendered and `-NullRHI` modes.
   - Polish `ARelicActor` pickup, pass launch, prepared catch reception, fumble knock-loose, and Goal Score (`GameplayCue.Breakaway.Scoring.Goal`) audio/VFX feedback.
   - Integrate replaceable placeholder meshes, animations, VFX, and audio for every baseline hero/buildable. Observe sync, sockets, impact timing, mix, reduced-motion behavior, and death/cancel cleanup on rendered host and remote owner; carry physical feel/haptics observations into Goal 9.
4. **Verification & Closeout:**
   - Pass `Breakaway.CombatPolish`, `Breakaway.Movement.Slide`, and 4-world PIE rendered verification, followed by a consolidated 4-process packaged run (`2 D3D12 + 2 NullRHI` at `100ms` lag).
   - Update `AI_Planning/SLICE_STATUS.md`, `AI_Planning/SLIDE_LIFECYCLE_ACCEPTANCE.md`, and `AI_Planning/GAMEPLAY_CUE_ACCEPTANCE.md`.
```

---

### Goal 5 — Front End, Server Browser, Pause Menu & Staging Hero Select Polish Gate

```markdown
/goal Execute Goal 5: Front End, Server Browser, Pause Menu & Staging Hero Select Polish Gate (configured frontend and `L_BW_HeroSelect_Staging`).

### Execution requirements

- Follow the **Shared execution and completion contract** and **Dependencies and release decisions** in `AI_Planning/MASTER_GOAL_QUEUE.md`. Read `AI_Planning/SLICE_STATUS.md` and select the current gate contract before work.
- Read the relevant acceptance records under `AI_Planning/`: `PACKAGED_LAN_ACCEPTANCE.md`, `LATE_ARRIVAL_SELECTION_ACCEPTANCE.md`.

### Scope and acceptance criteria
1. **Front End Experience & Session Browser Polish (`B_LyraFrontEnd_Experience`, `UBwayFrontendStateSubsystem`, `UBwayUserFacingExperienceDefinition`):**
   - Verify the configured startup map/experience and natural launch path (currently `/Game/System/FrontEnd/Maps/L_LyraFrontEnd`), rather than using the ShooterMaps `L_Expanse` arena as a frontend shortcut. Clean up the menu so only Breakaway flows (`Host LAN Match`, `Find LAN Match`, `Dev Practice / Bot Match`, `Settings`, `Quit`) appear, with no duplicate HUD entries or leftover Shooter tiles.
   - Polish the LAN Server Browser list view: display session name, map (`Dorado`), current player count (`X/8`), ping, join button focus, empty-list / searching spinner states, and clear error/retry feedback if a session is full or unreachable.
   - Polish `UBwayPauseMenuWidget` in-match (`Resume`, `Options`, `Return to Lobby` for Host / `Disconnect to Main Menu` for Client) with clean CommonUI `Menu` layer focus restoration.
2. **Staging Hero Select Overhaul (`UBwayHeroSelectWidget`, `WBP_BW_HeroSelect`, `UBwayAbilityPreviewWidget`, `UBwayHeroSlotWidget`):**
   - **Hero Roster Grid & Class Grouping:** Present the four vertical-slice heroes with reconciled display data: Argus Fighter, Alona Support, Korryn Debuff Support, and Rawlins Shooter. Bind class badges (`FHeroDisplayInfo::ClassName`), portraits (`Portrait`), selection highlight, and lock state (`bIsLocked`) to hero data.
   - **Live 4v4 Team Roster Sidebars (`OnOtherPlayerSelectionChanged`):** Display Blue Team (4 slots) and Red Team (4 slots) in staging so players see teammates' and bots' live hovered/locked hero portraits, player names, and `Ready (X/Y)` counts (`GetReadyPlayerCount`).
   - **Hero Kit & Buildable Inspection Panel (`Text_HeroDetails` & `UBwayAbilityPreviewWidget`):** When hovering or selecting a hero, populate base stats (`FHeroStats`), primary plus four kit actions, shared movement controls where applicable, and the signature buildable (`Siege Engine`, `Sun Shrine`, `Cursed Ward`, `Jail`). Derive descriptions/cooldowns from Goal 1's reconciled data and active mappings; do not duplicate a mobility action as an extra ability.
   - **Countdown, Lock-In & Travel Failure UX:** Display the shared phase countdown (`OnSelectionTimerUpdated`), disable grid switching once locked (`OnLocalSelectionLocked`) while keeping `OnLocalSelectionUnlocked` functional on rematch/pooled reuse, and preserve the **Retry travel** / **Waiting for host** recovery banner (`RefreshTravelFailurePresentation`).
3. **Full Keyboard/Mouse & Gamepad Navigation:**
   - Verify `NativeGetDesiredFocusTarget()` and CommonUI navigation across Front End, Server Browser, Hero Select grid, Lock-In button, and Pause Menu without losing controller focus.
   - Observe focus and recovery through actual menu traversal, back/cancel, input-device changes, late join, disconnect, failed travel, and rematch reuse. Preserve duplicate-selection/lock rules, host-only controls, and agreed bot-fill behavior. Include settings application/restoration and actionable empty/full/unreachable session states; direct console travel does not prove the player-facing flow.
4. **Verification & Closeout:**
   - Pass headless UI/selection automation (`Breakaway.HeroSelection` + `Breakaway.UI.FrontEndAndHeroSelectAudit`), 4-world PIE, and a consolidated 4-process packaged LAN run (`Connected` + `SelectionEdges` at `200ms` lag).
   - Update `AI_Planning/SLICE_STATUS.md`, `AI_Planning/PACKAGED_LAN_ACCEPTANCE.md`, and `AI_Planning/LATE_ARRIVAL_SELECTION_ACCEPTANCE.md`.
```

---

### Goal 6 — In-Game Match HUD, Team Portrait Timers, Round Banners & Results Polish Gate

```markdown
/goal Execute Goal 6: In-Game Match HUD, Team Portrait Respawn Timers, Ability Bar State Overlays, Round Banners & Post-Match Results Polish Gate.

### Execution requirements

- Follow the **Shared execution and completion contract** and **Dependencies and release decisions** in `AI_Planning/MASTER_GOAL_QUEUE.md`. Read `AI_Planning/SLICE_STATUS.md` and select the current gate contract before work.
- Read the relevant acceptance records under `AI_Planning/`: `RESPAWN_COUNTDOWN_ACCEPTANCE.md`, `DEMO_MATCH_RULES.md`, `COMBAT_CREDIT_ACCEPTANCE.md`.

### Scope and acceptance criteria
1. **Top Broadcast Scorebug & 4v4 Team Portrait Respawn Timers (`UBwayCaptureTheRelicScoreWidget`, `UBwayTeamPortraitsHUDWidget`, `UBwayTeamPortraitSlotWidget`):**
   - Extend `FBwayTeamPortraitSlotViewModel` and `UBwayTeamPortraitSlotWidget` (`BwayTeamPortraitSlotWidget.h/.cpp`) to bind and display **per-player live respawn countdown seconds** (`RespawnRemainingSeconds` / `Text_RespawnTimer`) directly over each dead player's top-bar portrait (`!bIsAlive && RespawnSchedule.bIsWaitingToRespawn`), matching the September 7, 2017 GameSpot broadcast HUD (`AI_Planning/FINAL_ALPHA_PARITY.md`).
   - Ensure each portrait slot clearly shows Friendly (`Blue`) vs Enemy (`Red`) framing, Hero Portrait, Relic Carrier badge (`Text_RelicBadge` / gold border when `bHasRelic == true`), Death overlay (`Image_DeathOverlay`), and immediate reset to active on respawn or round start.
   - Polish `UBwayCaptureTheRelicScoreWidget` and `UBwayRelicStatusWidget`: first-to-three round-win pips, a server-clock round timer (`240s` in Demo), territory possession, and local respawn countdown. Bind values to the active match configuration and authoritative deadlines; verify expiry, late join, menu occlusion, replacement pawn, and round reset without stale or negative timers.
2. **6-Slot Ability Bar & Player Health/Gold HUD (`UBwayAbilityBarHUDWidget`, `UBwayMatchAbilitySlotWidget`, `UBwayHealthHUDWidget`, `UBwayCurrencyDisplayWidget`):**
   - Verify six action slots (buildable, primary, hero mobility, two abilities, and ultimate) and separate shared-slide/Relic prompts against actual grants and input mappings. Dynamically update keyboard/gamepad labels through `HandleControlMappingsRebuilt`; do not hardcode prototype keys or conflate hero dodge with shared slide.
   - Show clear state overlays on `UBwayMatchAbilitySlotWidget`:
     - **Cooldown:** `Image_CooldownOverlay` + `ProgressBar_Cooldown` + `Text_CooldownTimer` ( tenths of a second below `1.0s`, whole seconds above).
     - **Relic Carrier Lock (`bIsRelicAbility` / `State.Relic.Carrying`):** Swap or badge combat slots to show Relic Pass / Throw / Drop controls while carrying the Relic.
     - **Buildable Slot (`bIsBuildable`):** Show `Available (1/Round)` vs `Placed This Round` (`bIsConsumed`) plus current active structure count (`X/Max`).
   - Polish `UBwayHealthHUDWidget` (health, damage trail, and armor) and `UBwayCurrencyDisplayWidget` (live gold and configured cap, if applicable). Resolve the shop prompt from current input mappings and pulse only when an eligible, affordable purchase exists.
3. **Round Phase Banners, Scoreboard & Post-Match Summary (`UBwayPostRoundSummaryWidget`, `UBwayPostMatchInterstitialWidget`, `UBwayResultsScreenWidget`, `UBwayMatchBreakdownWidget`, `UBwayScoreboardWidget`):**
   - Replace any remaining placeholder green wireframe PostRound effects or on-screen debug `PrintString` spam with clean CommonUI phase banners:
     - **Prematch:** Round number, configured server countdown, and shop availability with the active key/gamepad label.
     - **PostRound (`UBwayPostRoundSummaryWidget`):** Winning team banner + exact win reason (`GOAL SCORED BY <PLAYER>`, `ENEMY TEAM ELIMINATED`, or `TIME EXPIRED — TERRITORY TIEBREAKER`) + updated series score.
     - **Tab Scoreboard (`UBwayScoreboardWidget`) & Post-Match Results (`UBwayResultsScreenWidget`):** 8-player table showing Hero portrait, Player Name, `Kills / Deaths / Assists`, `ObjectiveScore` (Goals), `DamageDealt`, `HealingDone`, `ForcedFumbles`, `Interceptions`, `BuildablesDestroyed`, `MatchGoldEarned`, equipped Upgrade icons/ranks, MVP highlight, and Host `Rematch` / `Return to Lobby` buttons.
   - Reconcile every displayed stat and MVP calculation with authoritative match summaries, including disconnected players and ties. Verify role permissions, frozen end-of-match totals, and clean next-match reset. Observe actual 1080p layout, overlap, contrast, and focus on both owner roles.
4. **Verification & Closeout:**
   - Pass headless HUD view-model automation (`Breakaway.UI.MatchHUDAndPortraits`), 4-world PIE, and a consolidated 4-process packaged run at `200ms` lag (`2 D3D12 + 2 NullRHI`).
   - Update `AI_Planning/SLICE_STATUS.md` and `AI_Planning/RESPAWN_COUNTDOWN_ACCEPTANCE.md`.
```

---

### Goal 7 — Buffs, Debuffs & Status Effect Visual/HUD Readability Gate

```markdown
/goal Execute Goal 7: Buffs, Debuffs & Status Effect Visual/HUD Readability Gate — Local Status Bar, Overhead Nameplate Status Badges, Ability-Bar Silence Lock & Character Status Cues.

### Execution requirements

- Follow the **Shared execution and completion contract** and **Dependencies and release decisions** in `AI_Planning/MASTER_GOAL_QUEUE.md`. Read `AI_Planning/SLICE_STATUS.md` and select the current gate contract before work.
- Read the relevant acceptance records under `AI_Planning/`: `GAMEPLAY_CUE_ACCEPTANCE.md`, `Breakaway_Hero_Stats_Sheet.md`.

### Scope and acceptance criteria
1. **Local Player Active Buff & Debuff Status Bar (`UBwayMatchHUDWidgetBase` / Status Tray Widget):**
   - Implement a data-driven **Status Effect Tray** on the local player HUD bound to the owning `UAbilitySystemComponent` active GameplayTags and GameplayEffect remaining durations:
     - Build the status inventory from the reconciled production kits and buildables. Cover silence, Jail, Korryn primary armor stacks, damage amplification, slows, ethereal/invulnerable movement windows, healing effects/auras, and base healing/shop eligibility wherever active. Do not retain obsolete For Glory ally buffs, Double Down fire-rate buffs, or Sun Shrine armor labels.
     - Bind names, sources, magnitudes, stack counts, and remaining time to authoritative effects/configuration. Show duration sweeps only for timed effects; use active/conditional labels for zones or Jail until their actual removal condition. Silence overlays must match real activation restrictions, including primary and Relic exceptions.
     - Define ordering and aggregation for overlapping sources, refresh/stack changes, expiry, zone exit, death, pawn replacement, late join, and round/rematch reset. Verify no stale listeners, duplicate badges, fabricated durations, or local timer drift.
     - Use icons/text as well as color to distinguish positive and negative effects; preserve readable priority during a full 4v4 fight.
2. **Overhead Nameplate Status Badges (`UBwayNameplatePolicyComponent`):**
   - Surface compact high-priority status icons above enemy/friendly health bars (`Silenced`, `Jailed`, `ArmorShred`, `DamageAmplified`, `Invulnerable/Ethereal`) so teammates and attackers can immediately read combat vulnerability windows in 4v4 fights.
3. **World / Character Status GameplayCues (`BwayGameplayCueTags`):**
   - Map each status to its intended HUD, world, and audio presentation. Register required `GameplayCue` handlers at startup; remove indicators at their actual lifecycle boundary without invalid MID-parent warnings, duplicate effects, or NullRHI crashes. UI-only state does not require an invented world cue.
4. **Verification & Closeout:**
   - Pass meaningful registered status tests (`Breakaway.UI.BuffDebuffReadability` is planned), runtime cue auditing (`bway.Test.CueAudit` or documented successor), and 4-world PIE across the complete reconciled inventory. Complete a consolidated packaged run at `100ms` lag and fresh rendered status observations; a fixed count of 11 obsolete prototype labels is not coverage.
   - Update `AI_Planning/SLICE_STATUS.md` and `AI_Planning/GAMEPLAY_CUE_ACCEPTANCE.md`.
```

---

### Goal 8 — Upgrade Shop Items, 6-Card/4-Slot UI Overhaul & Economy UX Polish Gate

```markdown
/goal Execute Goal 8: Match Upgrade Shop Items, 6-Card Catalog Grid, 4-Slot Equipped Loadout Tray & Economy UX Polish Gate (`UBwayUpgradeShopWidget` & `DA_BW_DefaultUpgradeCatalog`).

### Execution requirements

- Follow the **Shared execution and completion contract** and **Dependencies and release decisions** in `AI_Planning/MASTER_GOAL_QUEUE.md`. Read `AI_Planning/SLICE_STATUS.md` and select the current gate contract before work.
- Read the relevant acceptance records under `AI_Planning/`: `SHOP_INPUT_ACCEPTANCE.md`, `COOLDOWN_UPGRADE_ACCEPTANCE.md`, `LIFE_LEECH_ACCEPTANCE.md`, `RESPAWN_UPGRADE_ACCEPTANCE.md`.

### Scope and acceptance criteria
1. **Catalog Data & Icon Completeness (`UBwayUpgradeCatalog` / `DA_BW_DefaultUpgradeCatalog`):**
   - Audit all 6 items in `/BreakawayCore/Data/DA_BW_DefaultUpgradeCatalog` (`AttackStrength`, `Armor`, `MoveSpeed`, `CooldownReduction`, `LifeLeech`, `RespawnTimeReduction`):
     - Ensure every item has a valid cooked `Icon`, clear `DisplayName`, and descriptions of its actual per-rank costs and effects. Derive these from the catalog/effects and pass `IsDataValid` with zero warnings. Record current six-item tuning as the project baseline; reconcile final-alpha catalog/prices separately instead of presenting provisional numbers as historical facts.
2. **Overhaul `UBwayUpgradeShopWidget` from Dropdown Harness to 6-Card Grid + 4-Slot Loadout UI (`BwayUpgradeShopWidget.h/.cpp`):**
   - Upgrade `UBwayUpgradeShopWidget` from the bare `UComboBoxString` prototype into a polished, readable *Breakaway* Match Upgrade Shop while **retaining full backwards compatibility** with existing automated test helpers (`bway.Test.ShopOpenAndBuy` / `SelectionChanged` / `BuySelected`):
     - **Header Bar:** Displays live Gold balance and configured cap if applicable, equipped slots (`X / 4`), and a live purchase-eligibility banner with text as well as color:
       - Green: `SHOP OPEN — PREMATCH`, `SHOP OPEN — AWAITING RESPAWN`, or `SHOP OPEN — IN BASE HEALING ZONE`.
       - Amber/Red (View-Only Mode): `VIEW ONLY — RETURN TO BASE HEALING ZONE OR WAIT FOR RESPAWN TO BUY`.
     - **6-Item Upgrade Card Grid:** Displays all 6 catalog upgrades simultaneously as selectable cards showing:
       - Upgrade Icon & Display Name.
       - **4-Rank Pip Bar** (filled gold pips for owned ranks `1..4`, highlighted preview pip for next rank).
       - Current total bonus, next-rank bonus, and next-rank cost from authoritative catalog data, or `MAX RANK`. Distinguish percentage points, multipliers, and resulting effective stats.
       - Distinct visual states for **Affordable & Eligible**, **Insufficient Gold**, **4-Slot Cap Reached (Locked — 4/4 Other Items Equipped)**, and **Max Rank (4/4)**.
     - **4-Slot Equipped Loadout Tray & Live Hero Stat Summary:** Shows the player's 4 active equipment slots at the bottom of the shop alongside live computed `UBwayHeroAttributeSet` totals (`AttackStr`, `Armor`, `MoveSpeed%`, `CDR%`, `LifeLeech%`, `RespawnReduction%`) so players immediately see their build take shape.
3. **Keyboard, Mouse & Gamepad Ergonomics:**
   - Support card selection and deliberate purchase, keyboard navigation/hotkeys, and gamepad D-pad/stick navigation with confirm/back. Resolve labels from input mappings. Define shop-toggle versus D-pad navigation routing so one press cannot both move focus and close the shop; double-click/repeated input must not produce an unintended duplicate purchase.
   - Preserve an open shop's focus, interactivity, and movement/look lock through death and replacement-pawn respawn, including `ClientRestart`. Restore prior gameplay input only when the shop/menu actually exits; route travel or menu interruption through that cleanup. Preserve helper entry points without retaining the prototype dropdown as the player-facing UI.
   - Show authoritative success/rejection feedback under latency. Verify invalid IDs, unaffordable/max-rank/fifth-item requests, eligibility changes in flight, and repeated requests cause no partial debit or duplicate effect. Preserve upgrade benefits through death/rounds and clear them on a new match; recheck CDR, LifeLeech, movement, and pending respawn schedules through actual gameplay.
4. **Verification & Closeout:**
   - Pass `Breakaway.Economy` automation suite + new `Breakaway.UI.UpgradeShopPresentation` test, 4-world PIE, and a consolidated 4-process packaged shop probe + lifecycle run at `200ms` lag (`Get-PackagedShopProbeBrief.ps1`).
   - Update `AI_Planning/SLICE_STATUS.md` and `AI_Planning/SHOP_INPUT_ACCEPTANCE.md`.
```

---

### Goal 9 — Vertical Slice 1.0 Capstone Human-in-the-Loop Playtest & 1080p Sign-Off Gate

```markdown
/goal Execute Goal 9: Vertical Slice 1.0 Capstone — Full-Flow 1080p Verification, Physical KBM/Gamepad Playtest Checklist & Pre-Expansion Sign-Off Gate.

### Execution requirements

- Follow the **Shared execution and completion contract** and **Dependencies and release decisions** in `AI_Planning/MASTER_GOAL_QUEUE.md`. Read `AI_Planning/SLICE_STATUS.md` and select the current gate contract before work.
- Read the relevant acceptance records under `AI_Planning/`: `PACKAGED_LAN_ACCEPTANCE.md`, `RENDER_PERFORMANCE_ACCEPTANCE.md`, `BUILDABLE_AUTHORITY_ACCEPTANCE.md`, `DEMO_MATCH_RULES.md`.

### Scope and acceptance criteria
1. **End-to-End 1080p Full-Flow Visual & Performance Audit:**
   - Launch the compatible final package through its configured frontend at a measured `1920x1080` viewport. Follow the natural journey: **Host/find/join LAN** -> **Staging selection (`L_BW_HeroSelect_Staging`)** -> **Dorado (`L_BW_Dorado`)** -> **Prematch purchase** -> **Combat/statuses/buildables/Relic carry/pass/charged throw/catch/goal** -> **Death/respawn HUD** -> **Base purchase** -> **PostRound** -> **Results/MVP** -> **Rematch** -> **Connected lobby return and rediscovery**. Do not replace player actions with console travel or state injection.
   - Capture performance with one rendered D3D12 process on the reference workstation, full HUD and representative 4v4 human/bot workload. Record hardware, build/configuration, scalability, resolution scale, VSync/FPS limits, warmup, and capture windows. The 30-FPS transport profile cannot measure the 120-FPS target.
   - Require gameplay `mean < 8.33ms` and `p95 < 11.0ms` from raw unclamped CSV samples. Retain max frame time, hitch counts, sample count/duration, and separate travel/loading segments; do not trim failures or silently lower workload. Verify the real viewport and actual log counts, requiring zero actionable `Accessed None`, invalid/reassigned NetGUID, or VSM overflow occurrences. Record any classified nonfatal exclusions explicitly. Require directly observed layout/focus with zero focus traps, and reconcile discrepancies with the earlier diagnostic summary before acceptance.
2. **Baseline Buildables, Match Rules, and Lifecycle:**
   - Close all four buildables against `BUILDABLE_AUTHORITY_ACCEPTANCE.md` and the buildables/rematch runbook: physical valid placement, cancel/invalid recovery, server rejection, free once-per-round quota, explicit active caps, targeting/damage/healing, destruction, survival across rounds, and new-match cleanup. Do not substitute synthetic valid-confirm proof for cursor/controller placement.
   - Complete a natural match and production rematch with bots filling the eight-player 4v4 roster. Cover goal, simultaneous elimination, and timer/territory outcomes, authoritative results, upgrade/gold/respawn resets, and connected lobby return. Use separately recorded scenarios where one natural match cannot exercise every outcome.
3. **Four-Human, Real-LAN, and Distribution Acceptance:**
   - Record four simultaneous human operators using a listen host and three clients, with at least a second physical PC over a real LAN switch/router. Record machines, peer roles, devices, package identity, and bot slots; respect each machine's GPU limits. NullRHI witnesses and same-PC synthetic clients do not satisfy this requirement.
   - Prove frontend discovery/join, selection, full match, rematch, return, and rediscovery on that topology. Include disconnect/rejoin and failed-join/travel recovery. Verify a clean-machine package launch with required redistributables, cooked assets, and actionable firewall/session errors without reliance on editor-only files.
4. **Physical Playtest and Final Vertical Slice 1.0 Sign-Off:**
   - Exercise physical keyboard/mouse and gamepad across all four heroes, menus, shop, buildable placement, Relic charge/catch, combat, and rematch. Record actual focus, movement/look restoration, game feel, audio mix, camera-reduction settings, and haptics. Provide a structured checklist with pass/fail/pending, observer/date, candidate, and evidence for each item.
   - Resolve defects and repeat invalidated checks before explicit user sign-off. If participants, hardware, UI access, or a required reference decision are unavailable, record the remaining proof and retry condition; keep Goal 9 incomplete and Phase 3 locked while continuing independent authorized work.
   - Update `SLICE_STATUS.md`, `PACKAGED_LAN_ACCEPTANCE.md`, `RENDER_PERFORMANCE_ACCEPTANCE.md`, affected subsystem records, and `PROJECT_ROADMAP.md` with only the accepted scope and sign-off.
```

---

## PHASE 3: Full 11-Hero Final Public Alpha Roster Expansion (After Vertical Slice 1.0 Sign-Off)

### Goal 10 — Roster Expansion Batch A (Thorgrim, Kyra, Merrick, Jarra)

```markdown
/goal Execute Goal 10: Roster Expansion Batch A — Implement C++ Ability Kits, DataAssets, UI/Buff/Shop Integration, and Persistent Buildables for Thorgrim, Kyra, Merrick, and Jarra.

### Execution requirements

- Follow the **Shared execution and completion contract** and **Dependencies and release decisions** in `AI_Planning/MASTER_GOAL_QUEUE.md`. Read `AI_Planning/SLICE_STATUS.md` and select the current gate contract before work.
- Read the relevant acceptance records under `AI_Planning/`: `Breakaway_Hero_Stats_Sheet.md`, `HERO_CODENAME_MAP.md`, `UNREAL_ENGINE_BEST_PRACTICES_BIBLE.md`, `BUILDABLE_AUTHORITY_ACCEPTANCE.md`.

### Scope and acceptance criteria
1. **Modular Per-Hero Game Feature Plugins & C++ Runtime Modules (`Plugins/GameFeatures/Heroes/`):**
   - **Thorgrim (`Hero_Thorgrim` / `HeroThorgrimRuntime`):** Tank kit, `UBwayThorgrimKitConfig`, four GAS abilities plus primary, and **Wall** (`ABwayThorgrimWallBuildable`) per the reconciled stats sheet.
   - **Kyra (`Hero_Kyra` / `HeroKyraRuntime`):** Shooter kit, `UBwayKyraKitConfig`, four GAS abilities plus primary, and **Ballista** (`ABwayKyraBallistaBuildable`).
   - **Merrick (`Hero_Merrick` / `HeroMerrickRuntime`):** Tank kit with melee slash, armor/thorns, and pulls, `UBwayMerrickKitConfig`, four GAS abilities plus primary, and **Spike Trap** (`ABwayMerrickSpikeTrapBuildable`).
   - **Jarra (`Hero_Jarra` / `HeroJarraRuntime`):** Assassin kit with concealment/charge behavior, `UBwayJarraKitConfig`, four GAS abilities plus primary, and **Snap Trap** (`ABwayJarraSnapTrapBuildable`).
   - Confirm names/paths against the codename map before creating modules. For each action/buildable, record exact targets, scaling, timing, immunity/CC rules, and lifecycle from reconciled evidence; do not infer a kit from its role label.
2. **Full Polish Contract Integration:**
   - Wire all 4 heroes into `UBwayHeroRegistry`, `UBwayHeroSelectWidget` (portraits, class badges, ability/buildable previews), `UBwayAbilityBarHUDWidget`, the Goal 7 Buff/Debuff Status Tray & Nameplate badges, `UBwayDamageExecution`, `CooldownReduction`, `LifeLeech`, `ConsumeCombatCredit`, `BwayGameplayCueTags`, and `UBwayBuildableRegistryComponent`.
   - Supply GameFeatureData, ability sets/input tags, hero/buildable data assets, experience activation, and AssetManager discovery/cook rules. Cold-start the cooked package and prove actual selection, correct grants, spawn, respawn, and travel for every new hero. Validate soft references without editor-preloaded assets.
   - Integrate replaceable placeholder meshes, compatible animation classes/skeletons/sockets, portraits, previews, cues, and audio. Verify rendered movement/action timing and KBM/gamepad prompts; empty assets or headless-only kits do not pass.
   - Define each buildable's active cap explicitly; do not inherit the generic default accidentally. Verify free once-per-round placement separately from surviving actor count, authoritative confirm/reject/cancel, team filtering, damage/healing/CC, destruction/eviction, late join, round persistence, and rematch cleanup through `Runbooks/buildables-rematch.md`.
3. **Verification & Closeout:**
   - Implement/discover meaningful Batch A tests (`Breakaway.Abilities.BatchACombatAndBuildables` is planned), run the existing runtime cue audit, and verify four-world PIE plus packaged transport at `100ms` lag. Exercise each new hero as active remote owner, record all 20 new actions/four buildables, and rerun affected baseline regressions.
   - Verify bot compatibility, hero changes, death/respawn, effect cleanup, and authoritative results across the eight-hero roster. Complete rendered 1080p and physical-input checks for the additions; carry them into Goal 11's final integrated LAN capstone.
   - Update `AI_Planning/SLICE_STATUS.md` and `AI_Planning/PROJECT_ROADMAP.md`.
```

---

### Goal 11 — Roster Expansion Batch B (Sai, Greyven, Gallion) and Final Parity Acceptance

```markdown
/goal Execute Goal 11: Roster Expansion Batch B — Implement C++ Ability Kits, DataAssets, UI/Buff/Shop Integration, and Persistent Buildables for Sai, Greyven, and Gallion to complete the 11-Hero Final Public Alpha Roster.

### Execution requirements

- Follow the **Shared execution and completion contract** and **Dependencies and release decisions** in `AI_Planning/MASTER_GOAL_QUEUE.md`. Read `AI_Planning/SLICE_STATUS.md` and select the current gate contract before work.
- Read the relevant acceptance records under `AI_Planning/`: `Breakaway_Hero_Stats_Sheet.md`, `HERO_CODENAME_MAP.md`, `FINAL_ALPHA_PARITY.md`, `BUILDABLE_AUTHORITY_ACCEPTANCE.md`.

### Scope and acceptance criteria
1. **Modular Per-Hero Game Feature Plugins & C++ Runtime Modules (`Plugins/GameFeatures/Heroes/`):**
   - **Sai (`Hero_Sai` / `HeroSaiRuntime`):** Electrical Nuker kit, `UBwaySaiKitConfig`, four GAS abilities plus primary, and **Conduit** (`ABwaySaiConduitBuildable`); cover unique-target chains, static-charge lifecycle, and airborne targeting.
   - **Greyven (`Hero_Greyven` / `HeroGreyvenRuntime`):** Fighter with lance/dragon abilities, `UBwayGreyvenKitConfig`, four GAS abilities plus primary, and **Dragonspire** (`ABwayGreyvenDragonspireBuildable`); cover armor reduction and damage-over-time behavior.
   - **Gallion (`Hero_Gallion` / `HeroGallionRuntime`):** Tank/Healer kit, `UBwayGallionKitConfig`, four GAS abilities plus primary, and **Star Light** (`ABwayGallionStarLightBuildable`); cover healing, CC, and beam damage ramp/reset.
   - Apply Goal 10's per-hero requirements for source reconciliation, cold discovery/cook, feature activation/grants, placeholder presentation, and full buildable lifecycle to all three additions.
2. **Full 11-Hero Polish & Subsystem Integration:**
   - Wire all 11 heroes into `UBwayHeroRegistry`, `UBwayHeroSelectWidget`, `UBwayAbilityBarHUDWidget`, the Buff/Debuff Status Tray & Nameplate badges, `ABwayRelicBotController` (`BT_BW_RelicBot`), `UBwayDamageExecution`, `CooldownReduction`, `LifeLeech`, `ConsumeCombatCredit`, `BwayGameplayCueTags`, and `UBwayBuildableRegistryComponent`.
3. **Verification & Closeout:**
   - Implement/discover Batch B tests (`Breakaway.Abilities.BatchBCombatAndBuildables` is planned), audit runtime cues, and verify PIE plus packaged transport at `100ms` lag. Maintain an all-11-hero matrix covering 55 primary/kit actions, 11 buildables, actual grants/input, attributes/upgrades, bot use, owner/proxy replication, and lifecycle cleanup. Include relevant mixed-hero effect interactions and earlier-batch regressions.
   - Repeat Goal 9's final candidate checks after expansion: measured 1080p performance and visual quality, physical KBM/gamepad play across all additions, four-human second-PC LAN, clean-machine launch, complete match/rematch/lobby flow, and user sign-off. Rotate hero coverage across matches; a single eight-player match cannot cover 11 heroes.
   - Reconcile the project-wide inventory in `FINAL_ALPHA_PARITY.md`, including unresolved equipment, tuning, buildable durability, movement, presentation, and unresearched spectator/replay/social requirements. Record dated evidence or explicit owner scope decisions; do not silently exclude unknown systems. Retain provisional choices as such and do not claim historical parity while mandatory gaps remain.
   - Update `SLICE_STATUS.md`, `PROJECT_ROADMAP.md`, `README.md`, and linked acceptance records under `AI_Planning/`. Report four-hero slice, 11-hero roster, and full parity completion separately, with evidence and unresolved decisions. Mark final completion only after all required proof and human sign-off are recorded.
```
