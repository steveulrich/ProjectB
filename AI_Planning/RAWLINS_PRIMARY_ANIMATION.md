# Rawlins Primary Animation Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (4-world PIE lifecycle including mid-burst cancellation + 4-process packaged remote-owner 2-shot burst delivery & Life Leech parity at 100ms lag).
- **Frozen Candidate:** `LAN-20261009-06` (`Saved/StagedBuilds/Windows/candidate-lan20261009-06.json`).
- **Source Provenance:** Base commit `8e2ce403`, source snapshot ID `A9D2DF0E6D9FB275051ABAD70F54B37E14A0A7417206D4C6F40A6863645236C0` (`Saved/Logs/rawlins-primary-anim-20261009/source-snapshot-current.json`, 131 staged artifacts).
- **Executable Identity:** `LyraGame.exe` staged from `Saved/Logs/rawlins-primary-anim-20261009/compile-game01.json` (passed `Package-Preflight.ps1` `prepackage01.json`).

## 2. Production Architecture & Invariants

- **Gameplay Ability:** `UBwayGameplayAbility_RawlinsPrimary` (`Plugins/GameFeatures/Heroes/Hero_Rawlins/Source/HeroRawlinsRuntime/Public/BwayRawlinsAbilities.h`), asset `/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_PrimaryAttack`.
  - Executes a 2-shot dual-pistol burst (`PrimaryShotCount = 2`, `PrimaryShotInterval = 0.12s`).
  - Predicting client keeps the ability active until the server finishes the burst (or cancels mid-burst), emitting `GameplayCue.Breakaway.Weapon.RawlinsPrimary.Fire` per shot (`2` cues per activation).
- **GameplayCue Notify & Montage:**
  - `GCN_BW_Weapon_RawlinsPrimary_Fire` (`/BreakawayCore/GameplayCues/GCN_BW_Weapon_RawlinsPrimary_Fire`), parent class `UBwayGameplayCueNotify_PrimaryAction`.
  - Plays `AM_BW_Rawlins_Primary_Placeholder` (`/BreakawayCore/Characters/Heroes/Rawlins/Animations/AM_BW_Rawlins_Primary_Placeholder`, non-additive `A_BW_Rawlins_PrimaryFire_Placeholder`, `AAT_NONE`, on `UpperBody` slot) via `ABwayCharacterWithAbilities::PlayPrimaryActionMontage`.
- **Mid-Burst Cancellation Invariant:** `EndAbility` clears `BurstTimerHandle` on authority; cancelling after shot 1 stops shot 2 from firing and leaves `playCount == 1` across all 4 peers.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Asset & Cue Audit | `Breakaway.AssetAudit.RawlinsPrimaryAnimation` + `Breakaway.GameplayCueAudit` | **PASS** — `Saved/Logs/rawlins-primary-anim-20261009/automation03-rawlins-primary.json` |
| 4-World PIE (Normal, Moving, Rapid 100ms, Death/Respawn) | `bway.Test.PrimaryAnimation Rawlins` | **PASS** — 3 activations = 6 authority shots / 24 peer plays, moving locomotion preserved (`pie01-rawlins-primary-brief.json`) |
| 4-World PIE Mid-Burst Cancel (0ms & 100ms) | `bway.Test.PrimaryAnimationCancelBurst Rawlins` | **PASS** — 50/50 checks, shot 2 suppressed (`burstShots=1`), 4 peer plays, clean follow-up 2-shot burst (`pie02-cancelburst-brief.json`) |
| 4-Process Packaged LAN (100ms Lag, Remote Owner `259`) | `bway.Test.PrimaryAnimationPackage Rawlins` (`LAN-20261009-06`) | **PASS** — 53/53 controls, 6 authority shots, 24 peer plays, 2-shot burst damage `36.86` & Life Leech heal `1.843` (`package06-delivery-verification.json`) |

## 4. Remaining Open Proof Gaps

- Natural gameplay mid-burst interruption (e.g., silence/stun/knockback before shot 2) and packaged separate-process moving/cancel/death subcases.
- Physical mouse/gamepad input verification, bespoke dual-pistol animations, and second-PC LAN verification.

## 5. Archive Pointer

- Full chronological gate diary: [RAWLINS_PRIMARY_ANIMATION.md](./Archive/Full_History_20261013/RAWLINS_PRIMARY_ANIMATION.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
