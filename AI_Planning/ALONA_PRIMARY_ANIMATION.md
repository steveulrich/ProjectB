# Alona Primary Animation Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (4-world PIE lifecycle + 4-process packaged LAN delivery at 100ms lag).
- **Frozen Candidate:** `LAN-20261009-03` (`Saved/StagedBuilds/Windows/candidate-lan20261009-03.json`).
- **Source Provenance:** Commit `0592b324` (`Saved/Logs/alona-primary-anim-20261009/source-snapshot-current.json`, 131 staged artifacts).
- **Executable Identity:** `LyraGame.exe` staged from `Saved/Logs/alona-primary-anim-20261009/compile-game01.json` (passed `Package-Preflight.ps1` `prepackage01.json`).

## 2. Production Architecture & Invariants

- **Gameplay Ability:** `UBwayGameplayAbility_AlonaPrimary` (`/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_PrimaryAttack`).
  - Predicts the launch GameplayCue on the locally controlled owner and dispatches the authoritative cue on the server immediately after spawning `ABwayAlonaPrimaryProjectile`.
  - Sets `StartupCueTag` to `GameplayCue.Breakaway.Weapon.AlonaPrimary.Fire` in the constructor for cold-startup GameplayCueManager registration.
- **GameplayCue Notify & Montage:**
  - `GCN_BW_Weapon_AlonaPrimary_Fire` (`/BreakawayCore/GameplayCues/GCN_BW_Weapon_AlonaPrimary_Fire`), parent class `UBwayGameplayCueNotify_PrimaryAction`.
  - Plays `AM_BW_Alona_Primary_Placeholder` (`/BreakawayCore/Characters/Heroes/Alona/Animations/AM_BW_Alona_Primary_Placeholder`, wrapping `MM_Pistol_Melee` at rate `2.0` on the `UpperBody` slot) through `ABwayCharacterWithAbilities::PlayPrimaryActionMontage`.
- **Upper-Body Layering Invariant:** `ABP_Mannequin_Base` caches `LocomotionPose` and blends `Slot 'UpperBody'` above `spine_01` (`BlendDepth=4`, `bMeshSpaceRotationBlend=true`), preserving lower-body locomotion while upper-body primary montages play or restart cleanly on rapid retrigger.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Asset & Cue Audit | `Breakaway.AssetAudit.AlonaPrimaryAnimation` + `Breakaway.GameplayCueAudit` | **PASS** — `Saved/Logs/alona-primary-anim-20261009/automation01-alona-primary.json` |
| 4-World PIE (Normal, Moving 600 cm/s, Rapid 100ms Lag, Death/Respawn) | `bway.Test.AlonaPrimaryAnimation` (`Normal`, `Moving`, `Rapid`, `Death`) | **PASS** — 12 plays across 4 worlds, preserved lower-body speed, 8 clean retrigger replacements (`pie01-alona-primary-brief.json`) |
| 4-Process Packaged LAN (100ms Lag) | `bway.Test.AlonaPrimaryAnimationPackage` (`LAN-20261009-03`) | **PASS** — 21/21 controls, 3 authority shots, 12 total peer montage plays (`package03-delivery-verification.json`) |

## 4. Remaining Open Proof Gaps

- Packaged separate-process moving pose-delta, rapid replacement, and death/respawn montage cleanup cases (covered in 4-world PIE; packaged run verified 4-peer delivery at 100ms lag).
- Physical mouse/gamepad input verification and final custom Alona animation art replacement.
- Second-PC physical LAN hardware verification.

## 5. Archive Pointer

- Full chronological gate diary: [ALONA_PRIMARY_ANIMATION.md](./Archive/Full_History_20261013/ALONA_PRIMARY_ANIMATION.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
