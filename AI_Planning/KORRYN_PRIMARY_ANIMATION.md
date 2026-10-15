# Korryn Primary Animation Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (4-world PIE lifecycle + 4-process packaged remote-owner LAN delivery at 100ms lag).
- **Frozen Candidate:** `LAN-20261009-04` (`Saved/StagedBuilds/Windows/candidate-lan20261009-04.json`).
- **Source Provenance:** Commit `0592b324`, source snapshot ID `6F052E52BCD2214577CC2B552FEC8303C22D8047A41750D48D2033CD9207B899` (`Saved/Logs/korryn-primary-anim-20261009/source-snapshot-current.json`, 131 staged artifacts).
- **Executable Identity:** `LyraGame.exe` staged from `Saved/Logs/korryn-primary-anim-20261009/compile-game01.json` (passed `Package-Preflight.ps1` `prepackage01.json`).

## 2. Production Architecture & Invariants

- **Gameplay Ability:** `UBwayGameplayAbility_KorrynPrimary` (`Plugins/GameFeatures/Heroes/Hero_Morgan/Source/HeroMorganRuntime/Public/BwayKorrynAbilities.h`), asset `/Hero_Morgan/Characters/Heroes/Hexweaver/Abilities/GA_BW_Morgan_PrimaryAttack`.
  - Sets `StartupCueTag` to `GameplayCue.Breakaway.Weapon.KorrynPrimary.Fire` (registered in `BwayGameplayCueTags`).
  - Predicts the fire cue on the locally controlled owner and dispatches the authoritative cue on the server after spawning `ABwayKorrynPrimaryProjectile`.
- **GameplayCue Notify & Montage:**
  - `GCN_BW_Weapon_KorrynPrimary_Fire` (`/BreakawayCore/GameplayCues/GCN_BW_Weapon_KorrynPrimary_Fire`), parent class `UBwayGameplayCueNotify_PrimaryAction`.
  - Plays `AM_BW_Korryn_Primary_Placeholder` (`/BreakawayCore/Characters/Heroes/Korryn/Animations/AM_BW_Korryn_Primary_Placeholder`, `MM_Pistol_Melee` at rate `2.0` on `UpperBody` slot) via `ABwayCharacterWithAbilities::PlayPrimaryActionMontage`.
- **Upper-Body Layering Invariant:** Blends `Slot 'UpperBody'` above `spine_01` in `ABP_Mannequin_Base` while preserving lower-body movement and stopping active montages on death/respawn.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Asset & Cue Audit | `Breakaway.AssetAudit.KorrynPrimaryAnimation` | **PASS** — `Saved/Logs/korryn-primary-anim-20261009/automation01-korryn-primary.json` |
| 4-World PIE (Normal, Moving, Rapid 100ms Lag, Death/Respawn) | `bway.Test.PrimaryAnimation Korryn` (`Normal`, `Moving`, `Rapid`, `Death`) | **PASS** — 12 plays across 4 worlds, moving locomotion preserved, 8 rapid replacements, clean death/respawn stop (`pie01-korryn-primary-brief.json`) |
| 4-Process Packaged LAN (100ms Lag, Remote Korryn Owner `259`) | `bway.Test.PrimaryAnimationPackage Korryn` (`LAN-20261009-04`) | **PASS** — 36/36 controls, 3 authority shots, 12 total peer montage plays across host + 3 clients (`package04-delivery-verification.json`) |

## 4. Remaining Open Proof Gaps

- Direct inclusion of `GameplayCue.Breakaway.Weapon.KorrynPrimary.Fire` in the static `Breakaway.GameplayCueAudit` tag array.
- Packaged separate-process moving pose-delta, rapid replacement, and death/respawn montage cleanup cases (verified in 4-world PIE).
- Physical mouse/gamepad input verification, bespoke Korryn hex-bolt cast animations, and second-PC LAN verification.

## 5. Archive Pointer

- Full chronological gate diary: [KORRYN_PRIMARY_ANIMATION.md](./Archive/Full_History_20261013/KORRYN_PRIMARY_ANIMATION.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
