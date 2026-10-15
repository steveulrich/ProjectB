# Rawlins Blazing Barrage & All-Kit Combat Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (4-world PIE all lifecycle/combat cases PASSED; `Breakaway.Abilities.AllKitCombatEffects` headless automation + 4-world PIE + 4-process packaged 100ms convergence PASSED; 4-process packaged normal and changing-aim 0ms/100ms transport + 27 negative controls PASSED on `LAN-20261013-13` within the `200ms` frame bound, with observed host frame peaks of `33.334ms` at `0ms` and `33.466ms` at `100ms`).
- **Frozen Candidates:**
  - `LAN-20261013-13`: Exe SHA-256 `59B7FB01A6407545443F57455E2B07C4F3A9BDB8BBF71DBAFEB9B1C98B25C24F`, source manifest `Saved/Logs/rawlins-barrage-animation-20261009/source-manifest-build43.json` (effective source identity `CD0AC92562C03FCFFCBCBBA1A2C836B41AAA89A7B3F2AC368BA0FFA14BD05433`, 131 staged artifacts). Packages Oct 1–9 slide physics repairs, Alona Radiance `UBwayHeroAttributeSet::AttackStrength` scaling fix, Rawlins Barrage presentation pointer cache, and `-NullRHI` montage pose tick parity.
  - `LAN-20261013-11` (retained record): Exe SHA-256 `54AA9FA99B53798BB00B0BC85E769465B6B2AE86A9064E27760691536B34F9F6`, source manifest `Saved/Logs/rawlins-barrage-animation-20261009/source-manifest-build39.json`.

## 2. Production Architecture & Invariants

- **Gameplay Ability:** `UBwayGameplayAbility_RawlinsBlazingBarrage` (`Plugins/GameFeatures/Heroes/Rawlins/Source/HeroRawlinsRuntime/Public/Abilities/BwayGameplayAbility_RawlinsBlazingBarrage.h`), asset `/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_BlazingBarrage`, config `DA_BW_RawlinsKitConfig`.
  - Channels `BarrageShotCount = 12` shots at `BarrageShotInterval = 0.10s` (`4.0` base damage + `0.3 * AttackStrength`, speed `3600`, upward juggle impulse `350`, cooldown `25s`).
  - Caches `ActiveProjectilePresentation` in `OnGiveAbility`/`ActivateAbility` and checks `.Get()` before `LoadSynchronous()` in `SpawnShot` to avoid per-shot synchronous soft-pointer loads.
  - Grants `Gameplay.MovementStopped` while channeling so walking input cannot displace the caster while live controller yaw rotates the firing direction each shot.
  - **Damage Interruption Invariant:** Tracks `ChannelReferenceHealth` across healing and interrupts the channel immediately on any net health decrease (`NewHealth < PreChangeHealth`).
- **All-16 Ability Authoritative Combat Suite (`Breakaway.Abilities.AllKitCombatEffects`):**
  - Verifies Argus (`Primary`, `Slide`, `NoRetreat`, `ForGlory`, `Retribution`), Alona (`Primary`, `SunsGrace`, `Radiance`, `SunBurst`, `Blessing`), Korryn (`Primary`, `Flock`, `BurdenOfSin`, `CircleOfSpite`, `AuraOfSilence`), and Rawlins (`Primary`, `DoubleDown`, `PowerShot`, `SlideShot`, `BlazingBarrage`), plus non-primary `UBwayLifeLeechComponent` healing, `ABwayCharacterWithAbilities` kill/assist credit, and `ARelicActor` forced fumbles.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Asset & Cue Audit | `Breakaway.AssetAudit.RawlinsBarrageAnimation` + `Breakaway.GameplayCueAudit` | **PASS** — `Saved/Logs/rawlins-barrage-animation-20261009/automation06-barrage-audit.json` |
| All-16 Ability Combat Automation | `Breakaway.Abilities.AllKitCombatEffects` | **PASS** — `Saved/Logs/allkit-combat-auto-5.json` (`succeeded: 1, errors: 0, warnings: 0`, 19.92s) |
| 4-World PIE All-Kit Combat & Barrage Matrix | `bway.Test.AllKitCombatPIE` + `bway.Test.RawlinsBarrageAnimation` | **PASS** — 4/4 worlds converged (`Saved/Logs/allkit-combat-pie-4-result.json`), full Barrage PIE matrix (`pie28`, `pie29`) |
| 4-Process Packaged Normal (0ms & 100ms) | `bway.Test.BarrageTransportHost` / `Client` (`LAN-20261013-11`) | **PASS** — 392 normal, 153 setup, 97 cleanup checks; 24 shots, 96 peer plays (`candidate11-normal-transport-independent-closure01.json`) |
| 4-Process Packaged Changing-Aim (0ms & 100ms, 2 D3D12 + 2 NullRHI) | `Saved/run-barrage-changing-aim-guard.ps1` (`LAN-20261013-13`) | **PASS** — 392/392 normal + 69/69 aim + 27/27 negative controls at both `0ms` (`peak=33.334ms`, `candidate13-changing-aim-zero01-aim-verification01.json`) and `100ms` (`peak=33.466ms`, `candidate13-changing-aim-lag01-aim-verification01.json`) |
| 4-Process Packaged Multi-Kit Combat (100ms, 2 D3D12 + 2 NullRHI) | `bway.Test.AllKitCombatHost` / `Client` (`LAN-20261013-13`) | **PASS** — Host + 3 clients converged on HP/ArmorShred/Slow/DamageAmp/Silence/Fumble, 3 client exits 0, 0 log errors (`Saved/Logs/allkit-combat-packaged-LAN-20261013-13-100ms-r3-result.json`) |

## 4. Remaining Open Proof Gaps

- Cooked separate-process damage-interruption and death/round-reset subcases (verified in 4-world PIE).
- Physical input verification, bespoke barrage animations, and second-PC LAN verification.

## 5. Archive Pointer

- Full chronological gate diary: [RAWLINS_BARRAGE_ACCEPTANCE.md](./Archive/Full_History_20261013/RAWLINS_BARRAGE_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
