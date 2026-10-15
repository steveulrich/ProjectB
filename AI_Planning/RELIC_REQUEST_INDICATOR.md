# Relic Request Indicator Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (4-world PIE + 4-process packaged LAN remote request, friendly-only screen-space visibility, 3s natural expiry, late-join active indicator visibility, live team-change filtering, and death/respawn cleanup at 200ms lag).
- **Frozen Candidate:** `LAN-20260926-01` (`Saved/StagedBuilds/Windows/candidate-lan20260926-01.json`, 131 staged artifacts).
- **Executable Identity:** `LyraGame.exe` SHA-256 `1EA1101F251D4BBA087D0F34CD3204C6CE4397DAD010866983BC1C78267D4EDF`.

## 2. Production Architecture & Invariants

- **Native Indicator Component (`UBwayRelicRequestIndicatorComponent`):**
  - Attached to `ABwayCharacterWithAbilities`; resolves `RequestingTag` (`Gameplay.Relic.Requesting`, `3.0s` duration) from `URelicSettings` (`DA_BW_RelicSettings_Default`) and binds to the pawn's `UAbilitySystemComponent` on initialization.
  - Evaluates friendly-only visibility (`Requester != LocalViewer && RequesterTeam == LocalViewerTeam`) reactively on tag change, team change, local controller change, and death without per-frame presentation ticking or extra RPCs.
  - Authority removes active request effects on death or ASC uninitialization so replacement pawns never inherit stale indicators.
- **2D Screen-Space Widget Presentation (`B_BW_Character_Base.RelicRequestWidgetComponent`):**
  - Uses `EWidgetSpace::Screen` and `DrawSize = (72, 72)` anchored at `(0, 0, 175)` with `W_BW_CaptureTheRelic_RelicRequestMarker` (`SelfHitTestInvisible`, non-focusable).
- **Widget Component Material Safety (`ULyraSystemStatics` & `ULyraTeamDisplayAsset`):**
  - Scalar, vector, and team display mesh-parameter helpers guard `UWidgetComponent` instances so death/team cues never overwrite widget material slots with invalid MID parents.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| 4-World PIE Screen-Space & Death/Respawn Lifecycle | `verify-marker-ui-20260908.py` | **PASS** — 3,883 initial + 374 post-respawn samples, 60 material cycles (`Saved/Logs/marker-ui-20260908/verification-brief.json`) |
| 4-Process Packaged Request, Late-Join, Team & Death Lifecycle (200ms) | `Candidate01` (`LAN-20260926-01`, `bway.Test.RelicIndicatorPeer`) | **PASS** — 45 observations, 171 peer rows, 33/33 controls (`Saved/Logs/relic-indicator-20260926/runtime01-brief.json`) |

## 4. Remaining Open Proof Gaps

- Off-screen viewport edge clamping, final custom Call-for-Relic icon art, physical gamepad/mouse input feel, and second-PC LAN hardware verification.

## 5. Archive Pointer

- Full chronological gate diary: [RELIC_REQUEST_INDICATOR.md](./Archive/Full_History_20261013/RELIC_REQUEST_INDICATOR.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
