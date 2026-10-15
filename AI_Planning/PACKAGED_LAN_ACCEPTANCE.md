# Packaged LAN & End-to-End Transport Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (4-process packaged LAN Host/Find/Join, Hero Selection, Staging-to-Dorado Travel, 4-Hero Combat/Scoring/Economy/Buildables, Results Screen, Rematch, and Connected-Lobby-Return/Rediscovery at 200ms lag).
- **Frozen Candidate:** `LAN-20261009-02` (`Saved/StagedBuilds/Windows/candidate-lan20261009-02.json`).
- **Source Provenance:** Commit `0d82f54f` (`Saved/Logs/packaged-lan-20261009/source-snapshot-current.json`, 131 staged artifacts).

## 2. Production Architecture & Invariants

- **Session Hosting, Discovery & Travel (`B_LyraFrontEnd_Experience` -> `L_Bway_HeroSelect` -> `L_Dorado`):**
  - Host advertises a LAN session and travels to `L_Bway_HeroSelect?listen`; clients discover the session via `ULyraSessionSubsystem`, join, select/lock heroes through `UBwayHeroSelectionManager`, and seamless-travel to `L_Dorado` (`B_BW_Experience_CaptureTheRelic`).
- **Connected Lobby Return vs Local Disconnect (`ABwayGameState::ReturnToFrontEnd`):**
  - Host `Return to Lobby` executes `World->ServerTravel("/BreakawayCore/Maps/L_Bway_HeroSelect?listen")`, keeping connected clients on the active listen `NetDriver` port and resetting match/selection state for immediate re-selection or rediscovery.
- **NullRHI & Bot Teardown Safety Invariants:**
  - `B_FootStep` Blueprint guards `SpawnSystemAttached` return values with `IsValid` so `-NullRHI` witness peers never log `Accessed None` on disabled Niagara components.
  - `ABwayRelicBotController::OnUnPossess` stops `UBehaviorTreeComponent` before calling `Super::OnUnPossess`, preventing null pawn errors during round reset and seamless travel.
- **Gamepad & Keyboard Ability Input Routing:**
  - `DA_BW_HeroInputConfig` and `IMC_BW_HeroAbilities` bind primary, secondary, movement, buildable, ultimate, and relic actions across both Keyboard/Mouse and Gamepad (`Gamepad_RightTrigger`, `LeftTrigger`, `FaceButton_Right`, `LeftShoulder`, `RightShoulder`).

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| 1 PIE Host + 3 Packaged Clients Natural Match | `Build 0805` (`E22A2AB6...`) | **PASS** — 3-0 match win, 0 errors across all 4 logs (`Saved/Logs/lan-natural-match-0805/`) |
| 4-Process Packaged Connected, Rediscovery & SelectionEdges (200ms) | `LAN-20261009-02` (`Test-PackagedLanFlow.ps1`) | **PASS** — All 3 transport modes passed with 0 errors and clean host teardown (`Saved/Logs/packaged-lan-20261009/lan02-summary.json`) |

## 4. Remaining Open Proof Gaps

- Four simultaneous human operators on physical input devices (Goal 4).
- Second physical PC over a real LAN switch/router and clean-machine redistribution check.

## 5. Archive Pointer

- Full chronological gate diary: [PACKAGED_LAN_ACCEPTANCE.md](./Archive/Full_History_20261013/PACKAGED_LAN_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
