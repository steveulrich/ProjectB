# Breakaway Project Roadmap

**Last Reviewed:** October 15, 2026  
**Active Gate Ledger:** [SLICE_STATUS.md](./SLICE_STATUS.md)  
**Execution Queue:** [MASTER_GOAL_QUEUE.md](./MASTER_GOAL_QUEUE.md)  
**Operating Contract:** [WORKFLOW_EFFICIENCY.md](./WORKFLOW_EFFICIENCY.md)

This roadmap tracks proven engineering milestones through October 2026 and sequences the **Vertical Slice 1.0 Mechanics & Full Game Polish Track (Goals 1–9)** ahead of **11-Hero Roster Expansion (Goals 10–11)**.

---

## 1. Completed Milestones (Proven Through October 2026)

| Subsystem | Verified Scope | Gate Acceptance Record |
|---|---|---|
| **Demo Match Rules & Scoring** | 240s rounds, First-to-3 Bo5, Goal, Simultaneous Elimination, Midfield Territory Tiebreaker across 4 packaged processes (`LAN-20260926-05`) | [DEMO_MATCH_RULES.md](./DEMO_MATCH_RULES.md) |
| **Relic Pickup Ownership & Release** | Single `AS_BW_BallAbilities` grant, duplicate/competing pickup rejection, pass/throw/drop/goal cleanup across 4 packaged processes (`LAN-20260926-07`) | [RELIC_PICKUP_OWNERSHIP.md](./RELIC_PICKUP_OWNERSHIP.md) |
| **Call-for-Relic 2D Indicator** | Friendly-only `72x72` screen-space hand widget, 3s expiry, late-join visibility, team-change filtering, death cleanup (`LAN-20260926-01`) | [RELIC_REQUEST_INDICATOR.md](./RELIC_REQUEST_INDICATOR.md) |
| **Combat Credit & Gold Economy** | Authoritative damage ledger, 50g Kill + 25g Assist (10s window), disconnect safety, rematch reset (`LAN-20260927-05`) | [COMBAT_CREDIT_ACCEPTANCE.md](./COMBAT_CREDIT_ACCEPTANCE.md) |
| **6-Item, 4-Slot Match Upgrade Shop** | Server-validated purchases (`AttackStrength`, `Armor`, `MoveSpeed`, `CooldownReduction`, `LifeLeech`, `RespawnTimeReduction`), B-key/D-pad UI, death/round persistence, rematch reset (`LAN-20260920-15`, `LAN-20260922-11`, `LAN-20260922-14`, `LAN-20260927-06`) | [SHOP_INPUT_ACCEPTANCE.md](./SHOP_INPUT_ACCEPTANCE.md) · [COOLDOWN_UPGRADE_ACCEPTANCE.md](./COOLDOWN_UPGRADE_ACCEPTANCE.md) · [LIFE_LEECH_ACCEPTANCE.md](./LIFE_LEECH_ACCEPTANCE.md) · [RESPAWN_UPGRADE_ACCEPTANCE.md](./RESPAWN_UPGRADE_ACCEPTANCE.md) |
| **Respawn Countdown HUD** | Server-deadline replication (`FBwayRespawnSchedule`), `UBwayRespawnHUDWidget`, menu occlusion, late-join self-dismissal (`LAN-20260926-10`, `LAN-20260926-12`) | [RESPAWN_COUNTDOWN_ACCEPTANCE.md](./RESPAWN_COUNTDOWN_ACCEPTANCE.md) |
| **Persistent Buildables (4 Heroes)** | Shared `FBwayPlacementQuery`, obstructed recovery, 3-round persistence, active caps (`Siege=1`, `Shrine/Ward/Jail=3`), client Jail trap immunity (`LAN-20260927-30`, `LAN-20260927-33`) | [BUILDABLE_AUTHORITY_ACCEPTANCE.md](./BUILDABLE_AUTHORITY_ACCEPTANCE.md) |
| **GameplayCue & Feedback Routing** | Startup cue registration, empty-prediction-key `HitConfirm`, local-only camera shake, Relic state audio multicast (`LAN-20260927-35`..`40`, `LAN-20260930-49`) | [GAMEPLAY_CUE_ACCEPTANCE.md](./GAMEPLAY_CUE_ACCEPTANCE.md) |
| **Packaged LAN & Late-Arrival Flow** | 4-process Connected, Rediscovery, SelectionEdges, 5s settle-window newcomer cancellation, and rejected-travel host retry (`LAN-20260930-51`, `LAN-20260930-53`, `LAN-20261009-02`) | [PACKAGED_LAN_ACCEPTANCE.md](./PACKAGED_LAN_ACCEPTANCE.md) · [LATE_ARRIVAL_SELECTION_ACCEPTANCE.md](./LATE_ARRIVAL_SELECTION_ACCEPTANCE.md) |
| **Slide Physics & Lifecycle** | 4-process packaged slide at 100ms (`LAN-20260930-50`) + 20/20 Oct 1–9 single-timestep gravity, wall-brake, ground-support, and camera-shake repairs | [SLIDE_LIFECYCLE_ACCEPTANCE.md](./SLIDE_LIFECYCLE_ACCEPTANCE.md) |
| **Single-GPU Performance Isolation** | 2-Rendered D3D12 + 2-NullRHI split (`7.2 GiB` VRAM, `<10ms` frame) + 1080p single-process baseline (`5.56ms`) + `-csvNoProcessingThread` clean exit | [RENDER_PERFORMANCE_ACCEPTANCE.md](./RENDER_PERFORMANCE_ACCEPTANCE.md) |
| **4-Hero Primary Attack Animations** | UpperBody montage layering & 4-process 100ms delivery for Alona (`LAN-20261009-03`), Korryn (`04`), Argus (`05`), and Rawlins 2-shot burst (`06`) | [ALONA_PRIMARY_ANIMATION.md](./ALONA_PRIMARY_ANIMATION.md) · [ARGUS_PRIMARY_ANIMATION.md](./ARGUS_PRIMARY_ANIMATION.md) · [KORRYN_PRIMARY_ANIMATION.md](./KORRYN_PRIMARY_ANIMATION.md) · [RAWLINS_PRIMARY_ANIMATION.md](./RAWLINS_PRIMARY_ANIMATION.md) |

---

## 2. Sequenced `/goal` Queue (See [MASTER_GOAL_QUEUE.md](./MASTER_GOAL_QUEUE.md))

Goals 1–3 have scoped completion evidence, with review gaps in [SLICE_STATUS.md](./SLICE_STATUS.md). The historical October 15 “Goal 4” experiment does not complete this expanded polish sequence or unlock roster expansion. The master queue owns detailed dependencies and acceptance criteria; its shared contract distinguishes project tuning, verified parity, and pending proof. Planning maintenance does not resume gameplay.

### Phase 1 — Core Vertical-Slice Mechanics & Tech Debt Closure (Goals 1–3)
1. **Goal 1 — Vertical Slice All-Kit Combat & Ability Effect Verification Gate:** Verify combat math, buffs/debuffs, heals, silences, knockbacks, invulnerability, Life Leech, combat credit, and carrier fumbles across all 20 abilities (16 kit abilities plus 4 primaries) (`Argus`, `Alona`, `Korryn`, `Rawlins`) + close the Rawlins `Blazing Barrage` changing-aim frame spike + package Oct 1–9 slide physics repairs.
2. **Goal 2 — Relic Ballistics, Prepared Catch, Interceptions & Base Healing/Shop Gate:** Verify RMB prepared catching (`State.Relic.PreparingCatch`), mid-air pass interception (`Interceptions` stat), combat fumble thresholds (`ForcedFumbles`), and `ABwayBaseZone` healing + live Shop purchase eligibility at base.
3. **Goal 3 — Respawn Delay Progression, Travel NetGUID Fix & Dorado VSM Polish Gate:** Verify implemented Demo progression and fixed three-second Dev settings as project tuning, revalidate reviewed travel/teardown GUID fixes, audit remaining legacy authority references, and preserve measured VSM/shadow-quality acceptance. Exact original respawn progression remains unresolved.

### Phase 2 — Full Vertical Slice 1.0 Game Polish Track (Goals 4–9 — Before Roster Expansion)
4. **Goal 4 — "Game Feel": Movement, Slide-Jump, Camera, Hit Feedback & Combat Readability Gate:** Tune `UBwayMovementFeelConfig` (`SlideJump`), smooth Dorado's ramp-to-flat slide transition (`SlideGroundAirDiagnostic` Case 2), polish floating damage/heal numbers (`UBwayCombatNumberPopComponent`), directional hit-confirm/camera-shake/haptics (`UGCN_BwayCombatFeedback`), projectile trails/impacts (`UBwayProjectilePresentationData`), and nameplate LOS/scaling (`UBwayNameplatePolicyComponent`).
5. **Goal 5 — Front End, Server Browser, Pause Menu & Staging Hero Select Polish Gate:** Verify the configured frontend launch path, remove leftover sample menus, and polish the browser, pause menu, and hero selection. Show live team pick/lock state, reconciled hero stats/roles, primary plus four kit actions, shared movement controls, and signature buildable previews with full KBM/gamepad navigation.
6. **Goal 6 — In-Game Match HUD, Team Portrait Timers, Round Banners & Results Polish Gate:** Add live per-player respawn countdown timers to top-bar `UBwayTeamPortraitSlotWidget` portraits, polish `UBwayCaptureTheRelicScoreWidget` Bo5 pips & territory indicator, add Silence/Relic-Carrier/Buildable state overlays to `UBwayMatchAbilitySlotWidget`, replace green wireframe PostRound placeholders with clean phase banners, and polish `UBwayScoreboardWidget` & `UBwayResultsScreenWidget`.
7. **Goal 7 — Buffs, Debuffs & Status Effect Visual/HUD Readability Gate:** Build the status tray, nameplate badges, and required world/audio cues from reconciled production effects. Show actual sources, stacks, duration or removal conditions, and activation restrictions; remove obsolete prototype labels and verify lifecycle cleanup.
8. **Goal 8 — Upgrade Shop Items, 6-Card/4-Slot UI Overhaul & Economy UX Polish Gate:** Upgrade `UBwayUpgradeShopWidget` from the `UComboBoxString` test harness to a full **6-Item Upgrade Card Grid + 4-Slot Equipped Loadout Tray** with bespoke icons, 4-rank pips, stat-delta tooltips, live purchase-window banner (`Prematch` / `Dead` / `Base Healing Zone`), live hero stat totals, and KBM hotkeys + Gamepad navigation while preserving `bway.Test.ShopOpenAndBuy` compatibility.
9. **Goal 9 — Vertical Slice 1.0 Capstone Human-in-the-Loop Playtest & 1080p Sign-Off Gate:** Verify unclamped 1080p performance, the complete frontend/match/rematch flow, physical KBM/gamepad input, four simultaneous humans, second-PC LAN, and clean-machine distribution. Record explicit human sign-off before roster expansion; mixed-rendering same-PC transport cannot close this gate.

### Phase 3 — Full 11-Hero Final Public Alpha Roster Expansion (Goals 10–11)
10. **Goal 10 — Roster Expansion Batch A (Thorgrim, Kyra, Merrick, Jarra):** Implement C++ kits, DataAssets, full HUD/Buff/Shop polish integration, and persistent buildables (*Wall*, *Ballista*, *Spike Trap*, *Snap Trap*).
11. **Goal 11 — Roster Expansion Batch B (Sai, Greyven, Gallion) and Final Parity Acceptance:** Integrate the remaining kits and buildables (*Conduit*, *Dragonspire*, *Star Light*), verify all 55 primary/kit actions and 11 buildables, repeat the final capstone, and reconcile the project-wide parity inventory. Report roster completion separately from final-alpha completion.
