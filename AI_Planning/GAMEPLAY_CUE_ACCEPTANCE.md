# GameplayCue Registration & Combat Feedback Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (Cold-startup GameplayCue registration, local-only HitConfirm camera shake, Relic state transition audio multicast, and slide/scoring cue routing across 4-world PIE and 4-process packaged runs).
- **Frozen Candidates:**
  - `LAN-20260927-35`: Exe SHA-256 `028D823F7C9CC2C3463079E45E1CB636962ADFE0B08230DFCA24159F98A9F0F0` (Alona primary fire cue, 244 checks, 30 controls).
  - `LAN-20260927-38`: HitConfirm empty prediction key & local-only camera shake (54 checks, 13 controls).
  - `LAN-20260927-40`: Relic state audio multicast & silent round-reset suppression (101 checks, 11 controls).
  - `LAN-20260929-46` & `LAN-20260930-49`: Connected match `HitConfirm`, `Scoring.Goal`, and `Movement.Slide` cues.

## 2. Production Architecture & Invariants

- **Cold-Startup Cue Registration (`BwayGameplayCueTags`):**
  - Registers native GameplayCue tags in the primary module dictionary before `UGameplayCueManager` initializes, eliminating cold-startup unregistered-tag warnings across packaged builds.
- **Server-Confirmed HitConfirm & Local Feedback Guard (`UBwayCombatFeedbackRouterComponent` & `UGCN_BwayCombatFeedback`):**
  - `UBwayCombatFeedbackRouterComponent` dispatches `GameplayCue.Breakaway.Combat.HitConfirm` on authority using a scoped empty `FPredictionKey` so `NetMulticast_InvokeGameplayCueExecuted_WithParams` reaches predicting clients without being dropped as a duplicate prediction.
  - `UGCN_BwayCombatFeedback` checks `Pawn->IsLocallyControlled()` before triggering camera shakes or controller force feedback, preventing remote hits from shaking non-owning local viewports.
- **Relic State Audio Multicast (`ARelicActor::Multicast_PlayStateAudio`):**
  - Fires an unreliable net multicast carrying a resolved `USoundBase*` and `FVector_NetQuantize` only when `CurrentState != PreviousState` and `bPlayTransitionAudio` is true (suppressing false audio on initial spawn and round-reset neutral placement).

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Cold-Startup Cue Audit | `Breakaway.GameplayCueAudit` | **PASS** — Zero unregistered-cue warnings (`automation-gameplaycue-audit.json`) |
| 4-Process Packaged Alona Fire Cue (200ms) | `Candidate35` (`LAN-20260927-35`) | **PASS** — 244 checks, 30 controls (`candidate35-cue-summary.json`) |
| 4-Process Packaged HitConfirm & Local Shake Guard (200ms) | `Candidate38` (`LAN-20260927-38`) | **PASS** — 54 checks, 13 controls; predicting client receives cue, non-owners do not shake (`candidate38-hitconfirm-summary.json`) |
| 4-Process Packaged Relic Audio & Connected Scoring/Slide Cues | `Candidate40`, `Candidate46`, `Candidate49` | **PASS** — 101 Relic audio checks + Connected `Scoring.Goal` & `Movement.Slide` routes (`candidate40-relic-audio-summary.json`) |

## 4. Remaining Open Proof Gaps

- Perceived audio mix balance, bespoke sound effects/VFX assets, and physical controller haptics verification.
- Second-PC LAN hardware verification.

## 5. Archive Pointer

- Full chronological gate diary: [GAMEPLAY_CUE_ACCEPTANCE.md](./Archive/Full_History_20261013/GAMEPLAY_CUE_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
