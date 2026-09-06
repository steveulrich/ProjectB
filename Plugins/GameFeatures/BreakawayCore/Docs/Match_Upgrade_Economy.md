# Match upgrade economy implementation

The shop remains incomplete. This document records implemented foundations and their limits; original-game requirements are in [Final-alpha evidence](../../../../AI_Planning/FINAL_ALPHA_PARITY.md).

## Server payment primitive

`UBwayEconomyLibrary::TrySpendGold(PlayerState, Cost)` accepts a nonnegative integer price resolved by trusted server code. It rejects missing players, non-authority calls, missing gold attributes, non-finite balances, and insufficient funds. Rejections do not deduct a partial amount. Zero-cost operations succeed only with a valid authoritative economy.

A fixed native instant Gameplay Effect performs the debit, retaining the existing gold attribute replication and change notifications. `WasSuccessfullyApplied()` distinguishes successful instant execution from a rejected application; `IsValid()` alone cannot do that for an instant effect.

This function is not an RPC. Do not expose client-supplied prices through it. The upgrade component resolves prices from its server catalog before invoking this helper. The current gold cap still needs reference tuning.

## Catalog and ownership

`UBwayUpgradeCatalog` contains stable upgrade IDs, presentation fields, effect classes, slot limits, and per-rank cost/total bonus. It validates duplicate IDs, missing definitions, invalid prices/magnitudes, and unsupported effects. `/BreakawayCore/Economy/DA_BW_MatchUpgrades` now contains attack strength and armor, each with four ranks. Costs are provisionally 75/125/175/225; attack bonuses are +5/+10/+15/+20 and armor bonuses +3/+6/+9/+12. These are implementation tuning, not recovered final-alpha values. The full reference equipment set remains incomplete.

`UBwayUpgradeComponent` is a replicated default subobject on `ABwayPlayerState`. The catalog and owned ID/rank array replicate to the owner. The ASC's existing attribute replication carries the resulting combat values. `OnUpgradesChanged` notifies authority and owning-client UI. The component retains effect handles on the server and survives pawn replacement with the PlayerState.

## Purchase transaction

The reliable server RPC accepts only an upgrade ID and the client's expected current rank. Authority validates the purchase window, catalog, rank, slot capacity, required attributes, and funds. Duplicate/stale requests cannot advance a second rank. A reentrancy guard covers application, debit, ownership mutation, and notifications.

Each rank uses an infinite, non-periodic, non-stacking Gameplay Effect. The component creates a level-one self-targeted spec with the owner's ASC context and sets `UpgradeMagnitude` from the catalog. It applies the new effect, debits gold, then removes the previous rank's effect and records ownership. Failed payment removes the newly applied effect. Supported effects are deliberately restricted to additive attack-strength/armor modifiers using this named magnitude; executions, effect components, and health/gold modifications are rejected to keep removal reversible. Existing AttributeSet hooks retain their clamping behavior.

`ResetUpgrades` removes owned effects and ranks without refunding gold. New PlayerStates start empty and ownership is not copied through seamless travel. An explicit same-world new-match reset hook, pawn-respawn verification, and separate-process replication tests remain required before claiming lifecycle coverage.

Current purchase windows are prematch, warmup, non-final post-round planning, death during play, and a living teammate's base zone. Death eligibility also reads the persistent health attribute because pawn teardown clears Lyra death tags before respawn. A restored positive health value closes that fallback window. Winning-score post-round summaries and postmatch reject purchases. The component loads the catalog on authority at BeginPlay; the existing `/BreakawayCore` always-cook directory covers its content. Cooked runtime loading has not yet been tested.

## Shop screen

Press B during play to open `UBwayUpgradeShopWidget`; B, Escape, or Close dismisses it. The controller also exposes `ToggleUpgradeShop` for Blueprint/console integration. The initial B binding is a fixed controller key binding; remapping, a discoverable HUD entry, and gamepad opening still need integration. Gamepad back closes the active screen.

The local UMG/CommonUI screen lives in `UI.Layer.Menu` with menu input/no mouse capture and desired focus on its selector. It lists catalog choices, current gold/owned count, rank information, and the next price. A local intent invokes the owning component's server RPC; a reliable client result reports success/rejection. Buying is disabled while pending or when price, rank, slot, or access checks fail locally; authority repeats all validation. Gold/ownership updates use delegates; a 0.25-second active-screen timer checks spatial access. Deactivation removes subscriptions and the timer. Match-phase changes dismiss the shop to avoid covering summaries/results.

The native layout is an initial usable screen, not final UI polish. Responsive/safe-zone coverage, gamepad navigation, remote purchase transport, rejection messaging under latency, and full equipment coverage remain open. Current-gold-based match statistics also need auditing now that purchases can spend gold.

Shop verification (September 6, 2026): editor builds passed (`codex-upgrade-shop-catalog-build.log`, final `codex-upgrade-shop-layout-build.log`); both economy tests passed. In listen PIE the catalog loaded automatically, gold updated on screen, and calling the widget's purchase handler bought attack-strength rank 1, displayed success, and changed the next rank price to 125. Closing deactivated the screen. Evidence: `Saved/Logs/codex-upgrade-shop-purchase-verified.log`. After fixing the zero-size backdrop, a second run confirmed focus and automatic dismissal on round end (`codex-upgrade-shop-layout-verified.log`); visual evidence: `Saved/Screenshots/Shop_Final00000.png`. The screenshot also shows pre-existing navmesh and relic-widget material warnings that remain unresolved. These checks exercise the widget/native authority path, not physical key presses, remote transport, or all navigation paths. Four-player/throttle settings were restored.

### Client RPC verification

Direct Python invocation of even a native widget handler is invalid RPC evidence: Unreal's editor-script guard forces nested actor RPCs to execute locally. The first attempted client probe consequently returned a local rejection; no multiplayer defect was established by that attempt (`codex-shop-script-guard-diagnosis.log`). See `Scripts/Test/README.md` and BF-066.

The development-only `bway.Test.ShopBuySelected` command queues the same widget purchase handler on a game timer and asserts the guard is off. In a fresh two-player, same-process listen PIE session, the log at 02:22:04 UTC records client netmode 3, authority 0, scriptguard 0. Temporary authority income effects funded both wallets to 500, then were removed before purchase. Server and owning client both changed to rank 1 and wallet 425; the host remained rank 0/wallet 500. Client UI displayed success and the next price. After a controlled round end and actual round-reset pawn replacement, both peers retained rank 1 with base attack 50/current attack 55 (Argus). Evidence: `Saved/Logs/codex-shop-client-rpc-verified.log`; build: `codex-shop-network-harness-build.log`.

This verifies real RPC/replication between PIE worlds, not separate-process or packaged LAN behavior. A subsequent Python-driven lethal-damage probe reached zero health on both peers without a verified respawn; repeat it on a normal native tick before drawing a gameplay conclusion. Death/respawn, new-match reset, latency/rejection races, and separate-process coverage remain open. Editor startup also reported a Turnkey C# build failure because `LyraTest.StartElimination.cs` references missing `LyraTestConfig`; packaging verification must resolve that failure. Four-player/throttle settings were restored.

## Team base zone

`ABwayBaseZone` provides an oriented box independent of presentation meshes. Author its bounds and team index (Breakaway 0/1) in the map or a Blueprint subclass. A player must have a current pawn avatar, matching canonical GameState team membership, positive finite health, no death tag, and a pawn center inside the box. Disabled zones and phases outside warmup/play deny access. Full-health teammates retain shop access while in base; this is a provisional usability choice pending reference confirmation.

The server evaluates eligible players at a 0.25-second tick interval and applies an instant self-targeted Gameplay Effect using `BaseHealing` as a named SetByCaller value on the existing Healing meta attribute. Lyra converts it into clamped Health and sends its normal health/healing notifications. Dead pawns are excluded before application. Healing defaults to a provisional 25 health/second; catch-up after a hitch is capped at one interval. Rate, enable state, team, and actor movement replicate. Bounds should be authored consistently in map/Blueprint defaults; runtime extent changes are not replicated.

Art can be attached to the zone or placed alongside it without changing healing or purchase logic. Dorado and DevMap now each contain two zones around the inspected team spawn clusters. `Scripts/Editor/ConfigureBaseZones.py` authors these layouts and synchronizes PlayerStartTag with the existing Lyra team gameplay tags. The zone currently has no player-visible marker. Presentation, tuning, and multiplayer evidence remain outstanding.

Authored-map verification: reloaded Dorado into one-player listen PIE, observed both saved zone extents/team indices, and confirmed the team-zero player spawned inside its own zone with enemy-zone access false. The saved zone healed the player from 250/350 to 350/350. The log contained no missing-team-spawn-tag warning. Evidence: `Saved/Logs/codex-authored-base-zones-verified.log` (`AUTHORED_BASES`, `DORADO_BASE_SPAWN`, `AUTHORED_BASE_DAMAGED`, `AUTHORED_BASE_RECOVERED`). DevMap was authored and saved but has not yet received a separate runtime placement check. Four-player/background-throttle settings were restored.

Verification on September 6, 2026: `codex-base-zone-build.log` passed; both economy automation tests passed at 01:44:53 UTC. The expanded fixture verifies own-team spatial access, immediate loss on exit, enemy rejection, disabled state, dead-pawn rejection, and missing-avatar rejection. In a one-player listen PIE match on DevMap, a temporary base restored an Alona pawn from 250/350 to 350/350 health. After moving the base away and damaging the pawn again, health remained 250 for the next 19 seconds and purchase eligibility was false. Evidence: `Saved/Logs/codex-base-zone-verified.log` (`BASE_START`, `BASE_DAMAGED`, `BASE_RECOVERED`, `BASE_OUTSIDE`, `BASE_OUTSIDE_LATER`). The test used an authority-created temporary zone and real damage/healing effects. Remote replication, exact healing-rate timing, HUD presentation, and authored map placements are not verified. PIE was stopped and the four-player/background-throttle settings restored.

## Verification

Packaging prerequisite repair (September 6, 2026): the legacy Gauntlet test now uses the engine's existing `EpicGameTestConfig`; project automation scripts compile and Turnkey reports Win64 SDK 10.0.22621.0 Valid with exit code 0 (`Saved/Logs/codex-packaging-sdk-verification.log`). `DefaultEditorTarget=LyraEditor` resolves the ambiguity introduced by the ProjectBEditor alias. The subsequent Development Win64 BuildCookRun reaches UnrealBuildTool (`Saved/Logs/codex-lan-package-20260906-retry.log`); build/cook/stage and packaged LAN acceptance are still pending. These supersede the earlier unresolved Turnkey note. BF-068 records both setup failures.

Death lifecycle regression (September 6, 2026): native server-tick damage reproduced zero health with death state still NotDead. The common humanoid ability set was missing `GA_Hero_Death`, leaving `GameplayEvent.Death` without its handler. The shared asset now grants that existing ability, and the character death override calls its parent to disable movement and collision. After a successful compile and editor restart, two consecutive native lethal-damage probes started death and produced replacement pawns. Server and owning client agreed on death counts 1 then 2, restored 400 health, NotDead state, and movement input enabled. Evidence: `Saved/Logs/codex-death-lifecycle-verified.log`; build: `Saved/Logs/codex-death-lifecycle-fix-build.log`. This was two same-process PIE network worlds on DevMap. Enemy kill credit, physical movement input, death-shop purchases, and separate-process/packaged replication remain unverified. Root cause and prevention are recorded as BF-067.

`Breakaway.Economy.GoldDebit` uses a temporary world, PlayerState, real GAS component, and gold AttributeSet. It checks non-authority rejection, missing economy, negative and oversized prices, insufficient funds, exact deductions, repeated unaffordable attempts, zero-cost behavior, and spending down to zero. The role-switch test exercises the authority guard; it is not separate-process replication evidence.

Build: `Saved/Logs/codex-economy-debit-authority-build.log` passed. Runtime test result is recorded after execution.

## Next implementation work

`Breakaway.Economy.UpgradePurchase` passed alongside `GoldDebit` at 01:33:16 UTC on September 6, 2026 after an editor restart. It uses a controlled warmup/playing phase fixture and real GAS attributes/effects to verify unknown IDs, authority and combat-window rejection, duplicate requests, slot/rank caps, exact costs, effect replacement, base-value reinitialization, reset, and insufficient funds. Evidence: `Saved/Logs/codex-upgrade-purchase-verified.log`; build: `Saved/Logs/codex-upgrade-purchase-build.log`. This does not exercise network transport, a real pawn respawn, or a debit failure after effect application. Background throttling was restored afterward.

Recover the catalog and rank values, including life leech and cooldown reduction; integrate base/pre-round/death access; verify transaction rollback and lifecycle behavior; build and verify the shop UI. Separate-process purchase and replication evidence is still required.

The expanded economy suite passed at 01:39:29 UTC on September 6, 2026. `UpgradePurchase` now rejects the debit through GAS application queries after applying an upgrade and proves that both first-purchase and rank-increase failures preserve the prior balance, bonus, and rank. It also exercises reentrant purchase/reset attempts from an attribute delegate, death-tag cleanup with persistent zero health, restored respawn health, non-final planning access, and rejection at the winning score/postmatch. Evidence: `Saved/Logs/codex-upgrade-lifecycle-verified.log`; final build: `Saved/Logs/codex-upgrade-lifecycle-fixture-build.log`. These are controlled integration fixtures, not separate-process or full pawn lifecycle evidence. Background throttling was restored.


The corrected fixture passed at 01:20:57 UTC on September 6, 2026. Evidence: `Saved/Logs/codex-economy-debit-verified.log`; final fixture build: `codex-economy-fixture-build.log`. The initial test fixture incorrectly initialized a world twice and crashed the editor before exercising the debit. That fixture defect was fixed, documented in BF-062, rebuilt, and rerun successfully. Background throttling was restored afterward.
