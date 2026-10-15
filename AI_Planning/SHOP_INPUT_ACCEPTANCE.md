# Upgrade Shop Input, Base Healing Zone & Lifecycle Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (Headless `Breakaway.Economy.BaseHealingShopWindow` + 7 `Breakaway.Economy.*` automation tests, 4-world PIE, and 4-process packaged B-key/D-pad toggle, CommonUI menu focus, movement/look input lock while open, rank-1 purchase debit, open-shop recovery across lethal damage and replacement-pawn respawn, and live Team Base Healing Zone regeneration / shop eligibility at `100ms` / `200ms` lag).
- **Frozen Candidates:**
  - `LAN-20261014-01` (`Saved/StagedBuilds/LAN-20261014-01`, `Saved/Logs/codex-packaged-LAN-20261014-01/package-result.json`).
  - `LAN-20260920-11` through `LAN-20260920-15` (`Saved/StagedBuilds/Windows/candidate-lan20260920-15.json`, 131 staged artifacts).

## 2. Production Architecture & Invariants

- **Enhanced Input Binding (`ABwayPlayerController` & `IMC_BW_PlayerUI`):**
  - `IA_BW_UpgradeShop` bound on `ABwayPlayerController` (`ETriggerEvent::Started` -> `HandleToggleUpgradeShopInput`) so shop toggle works whether the player pawn is alive, dead (`Pawn == nullptr`), or respawning.
  - `IMC_BW_PlayerUI` registered at priority `2` with Keyboard `B` and `Gamepad_DPad_Up`.
- **CommonUI Layer & Input Mode (`UBwayUpgradeShopWidget` & `WBP_BW_MatchHUDLayout`):**
  - `WBP_BW_MatchHUDLayout` uses `ELyraWidgetInputMode::GameAndMenu` without locking movement/look; `UBwayUpgradeShopWidget` activates on the `Menu` layer with `bIgnoreMoveInput = true` and `bIgnoreLookInput = true`.
- **Pawn Replacement Input-Mode Preservation (`ULyraHeroComponent::HandleChangeInitState`):**
  - Queries the active CommonUI leaf widget before applying default `Game` input config and reapplies `SetIgnoreMoveInput` / `SetIgnoreLookInput` after `ClientRestart` clears controller ignore flags on replacement pawn possession.
- **Team Base Healing Zone & Live Upgrade Purchase Window (`ABwayBaseZone` & `UBwayUpgradeComponent::CanPurchaseUpgrades`):**
  - `ABwayBaseZone::CanUseBase` verifies friendly team ownership (`0`-based `OwnerTeamIndex` and `1`-based `OwnerTeamId` compatibility) and tracks active occupants cleanly across overlap and pawn end-play.
  - Friendly players overlapping their own team's `ABwayBaseZone` receive `SetInHealingZone(true)` (`bIsInHealingZone` replicated on `UBwayUpgradeComponent`), regenerate health at `HealRatePerSecond` via `UBwayGameplayEffect_BaseHealing`, and satisfy `UBwayUpgradeComponent::CanPurchaseUpgrades()` while alive during `ERoundState::Playing`.
  - Stepping outside the friendly base healing zone while alive during `ERoundState::Playing` immediately sets `SetInHealingZone(false)` and revokes `CanPurchaseUpgrades()`, causing `ServerTryPurchaseUpgrade` to reject purchase requests with zero gold debit while `Prematch` and `Dead` windows remain valid.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Headless Base Healing & Upgrade Shop Window Automation | `Breakaway.Economy.BaseHealingShopWindow` + `Breakaway.Economy.*` | **PASS** — 8/8 economy tests in 11/11 suite (`Saved/Logs/automation-goal2-relic-base-run3.json`) |
| 4-World PIE Base Healing Regeneration & Shop Eligibility | `bway.Test.RunGoal2Gate` (4 PIE worlds) | **PASS** — `p0HP=236.7 inBase0=1 canShop0=1` + outside-base rejection with zero gold debit (`Saved/Logs/pie-goal2/pie-goal2-result.json`) |
| 4-Process Packaged B-Toggle & Same-Pawn Restart | `Candidate11` (`LAN-20260920-11`) | **PASS** — Open/close toggle, move/look lock, restart restoration (`candidate11-shop-input.json`) |
| 4-Process Packaged Gamepad D-Pad & Purchase Debit | `Candidate13` & `Candidate14` (`LAN-20260920-14`) | **PASS** — D-pad selector navigation, Accept purchase, gold debit (`candidate14-shop-purchase.json`) |
| 4-Process Packaged Open-Shop Death & Replacement-Pawn Respawn (200ms) | `Candidate15` (`LAN-20260920-15`) | **PASS** — Shop stays interactive across death & replacement pawn spawn (`candidate15-shop-respawn.json`) |
| 4-Process Packaged Base Healing & Live Upgrade Purchase Window (100ms, 2 D3D12 + 2 NullRHI) | `Candidate01` (`LAN-20261014-01`, `bway.Test.RunGoal2Gate`) | **PASS** — Host (`p0HP=286.4`) + 3 clients (`p0HP=236.7`) converged on `inBase0=1 canShop0=1`, `0` error/critical lines (`Saved/Logs/goal2-packaged-LAN-20261014-01-100ms-r2-result.json`) |

## 4. Remaining Open Proof Gaps

- Physical human keyboard/mouse/gamepad playtest (Goal 4) and second-PC LAN hardware verification.

## 5. Archive Pointer

- Full chronological gate diary: [SHOP_INPUT_ACCEPTANCE.md](./Archive/Full_History_20261013/SHOP_INPUT_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
