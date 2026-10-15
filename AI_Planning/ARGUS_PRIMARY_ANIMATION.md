# Argus Primary Animation Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (4-world PIE lifecycle + 4-process packaged remote-owner LAN delivery at 100ms lag).
- **Frozen Candidate:** `LAN-20261009-05` (`Saved/StagedBuilds/Windows/candidate-lan20261009-05.json`).
- **Source Provenance:** Commit `0592b324`, source snapshot ID `6BA009829FA2A10EC174F5ED4176665BF862D4129786AEE3E56CAEE461674FFE` (`Saved/Logs/argus-primary-anim-20261009/source-snapshot-current.json`, 131 staged artifacts).
- **Executable Identity:** `LyraGame.exe` staged from `Saved/Logs/argus-primary-anim-20261009/compile-game01.json` (passed `Package-Preflight.ps1` `prepackage01.json`).

## 2. Production Architecture & Invariants

- **Gameplay Ability:** `UBwayGameplayAbility_MeleePrimary` (`Plugins/GameFeatures/BreakawayCore/Source/BreakawayCoreRuntime/Public/Abilities/BwayGameplayAbility_MeleePrimary.h`), asset `/Hero_Spartacus/Characters/Heroes/Argus/Abilities/GA_BW_Spartacus_PrimaryAttack`.
  - Sets `AttackGameplayCueTag` to `GameplayCue.Breakaway.Weapon.ArgusPrimary.Attack` (registered in `BwayGameplayCueTags`).
  - Predicts the attack cue on the locally controlled owner and executes the authoritative cue on the server prior to performing the melee capsule sweep (`PerformMeleeSweep`).
- **GameplayCue Notify & Montage:**
  - `GCN_BW_Weapon_ArgusPrimary_Attack` (`/BreakawayCore/GameplayCues/GCN_BW_Weapon_ArgusPrimary_Attack`), parent class `UBwayGameplayCueNotify_PrimaryAction`.
  - Plays `AM_BW_Argus_Primary_Placeholder` (`/BreakawayCore/Characters/Heroes/Argus/Animations/AM_BW_Argus_Primary_Placeholder`, `MM_Pistol_Melee` at rate `2.0` on `UpperBody` slot) via `ABwayCharacterWithAbilities::PlayPrimaryActionMontage`.
- **Upper-Body Layering Invariant:** Blends `Slot 'UpperBody'` above `spine_01` in `ABP_Mannequin_Base` while preserving lower-body movement and cleanly stopping on `ClearPrimaryActionMontages` during death/respawn.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Asset & Cue Audit | `Breakaway.AssetAudit.ArgusPrimaryAnimation` | **PASS** — `Saved/Logs/argus-primary-anim-20261009/automation01-argus-primary.json` |
| 4-World PIE (Normal, Moving, Rapid 100ms Lag, Death/Respawn) | `bway.Test.PrimaryAnimation Argus` (`Normal`, `Moving`, `Rapid`, `Death`) | **PASS** — 12 plays across 4 worlds, moving locomotion preserved, 8 rapid replacements, clean death/respawn stop (`pie01-argus-primary-brief.json`) |
| 4-Process Packaged LAN (100ms Lag, Remote Argus Owner `257`) | `bway.Test.PrimaryAnimationPackage Argus` (`LAN-20261009-05`) | **PASS** — 47/47 controls, 3 authority sweeps, 12 total peer montage plays across host + 3 clients (`package05-delivery-verification.json`) |

## 4. Remaining Open Proof Gaps

- Direct inclusion of `GameplayCue.Breakaway.Weapon.ArgusPrimary.Attack` in the static `Breakaway.GameplayCueAudit` tag array.
- Packaged separate-process moving pose-delta, rapid replacement, and death/respawn montage cleanup cases (verified in 4-world PIE).
- Physical mouse/gamepad input verification, bespoke Argus sword/shield animations, and second-PC LAN verification.

## 5. Archive Pointer

- Full chronological gate diary: [ARGUS_PRIMARY_ANIMATION.md](./Archive/Full_History_20261013/ARGUS_PRIMARY_ANIMATION.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
