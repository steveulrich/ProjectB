# Upgrade Shop Input, Base Healing Zone & Lifecycle Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (`goal2-relic-base-20261018` / `LAN-20261018-01`: Headless `Breakaway.Economy.BaseHealingShopWindow`, 4-world PIE at `100ms` lag, and consolidated 4-process packaged verification at `100ms` lag (`2 D3D12 + 2 NullRHI`), plus prior B-key/D-pad toggle, CommonUI menu focus, movement/look input lock while open, rank-1 purchase debit, and open-shop recovery across lethal damage and replacement-pawn respawn).
- **Frozen Candidates:**
  - `LAN-20261018-01` (`Saved/StagedBuilds/LAN-20261018-01`, `Saved/Logs/goal2-relic-base-20261018/package-result.json`, 131 staged artifacts, `LyraGame.exe` SHA-256 `BC5B279D6AE7B066F23DCE9EADE16737E4EAB09F7CE2F4B88FE8C053EBC8C707`).
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
  - `ABwayBaseZone::CanUseBase` verifies friendly team ownership (`0`-based `TeamIndex` / `OwnerTeamIndex` and `1`-based `OwnerTeamId` compatibility), active/connected player state (`!IsInactive()`), and alive pawn state inside the box volume.
  - Overlapping friendly `ABwayBaseZone` volumes deduplicate healing per `(World, PlayerState)` per simulation timestamp so a player standing inside overlapping friendly volumes heals once at `HealingPerSecond` (`0.25s` interval) without double healing.
  - `UBwayUpgradeComponent::SetInHealingZone(bInZone, ZoneTeamIndex)` tracks overlapping friendly zone depth (`ExplicitHealingZoneDepth`), rejects opposing-team zones (`ZoneTeamIndex != PlayerTeam`) without clobbering active friendly eligibility, invalidates explicit overrides when the pawn changes (`Respawn`) or moves away from the recorded zone position, and resets explicit zone state on `ABwayPlayerState::CopyProperties` (reconnect) and `SetIsInactive(true)` (disconnect).
  - Leaving friendly base bounds while alive during `Playing` immediately revokes `IsPlayerInHealingZone()` and `CanPurchaseUpgrades()`; `ServerTryPurchaseUpgrade` rejects outside-base requests with zero gold debit, while `Prematch` and `Dead` purchase windows remain valid.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Headless Base Healing, Overlap Deduplication, Enemy Base, Death/Respawn & Reconnect | `Breakaway.Economy.BaseHealingShopWindow` | **PASS** — `0` errors, `0` warnings (`Saved/Logs/goal2-relic-base-20261018/headless-goal2-2.json`) |
| 4-World PIE Base Healing Regeneration & Live Shop Eligibility (100ms) | `bway.Test.Goal2RelicBasePIE` (4 PIE worlds, `Net PktLag=100`) | **PASS** — `p0HP=243.5 inBase0=1 canShop0=1` + outside-base rejection with zero gold debit (`Saved/Logs/goal2-relic-base-20261018/pie-goal2-5.log`) |
| 4-Process Packaged Base Healing & Live Upgrade Purchase Window (100ms, 2 D3D12 + 2 NullRHI) | `LAN-20261018-01` (`bway.Test.Goal2RelicBaseHost`/`Client`) | **PASS** — Host (`p0HP=293.1`) + 3 clients (`p0HP=243.5`) converged on `inBase0=1 canShop0=1` (`Saved/Logs/goal2-relic-base-20261018/packaged-summary.json`) |
| 4-Process Packaged B-Toggle & Same-Pawn Restart | `Candidate11` (`LAN-20260920-11`) | **PASS** — Open/close toggle, move/look lock, restart restoration (`candidate11-shop-input.json`) |
| 4-Process Packaged Gamepad D-Pad & Purchase Debit | `Candidate13` & `Candidate14` (`LAN-20260920-14`) | **PASS** — D-pad selector navigation, Accept purchase, gold debit (`candidate14-shop-purchase.json`) |
| 4-Process Packaged Open-Shop Death & Replacement-Pawn Respawn (200ms) | `Candidate15` (`LAN-20260920-15`) | **PASS** — Shop stays interactive across death & replacement pawn spawn (`candidate15-shop-respawn.json`) |

## 4. Remaining Open Proof Gaps

- Physical human keyboard/mouse/gamepad playtest (current Goal 9) and second-PC LAN hardware verification.

## 5. Archive Pointer

- Full chronological gate diary: [SHOP_INPUT_ACCEPTANCE.md](./Archive/Full_History_20261013/SHOP_INPUT_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
