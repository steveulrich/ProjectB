# Rawlins Blazing Barrage & All-Kit Combat Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** Goal 1 remains **partial**. Historical Barrage PIE lifecycle, 20-action headless, and packaged normal/changing-aim evidence retain their recorded scope. The older AllKit runtime fixture covers nine authority-driven actions with coarse peer thresholds; it does not prove 20 owning-client actions. Review fixes and remaining cooked lifecycle cases need fresh acceptance.
- **Frozen Candidates:**
  - `LAN-20261013-13`: Exe SHA-256 `59B7FB01A6407545443F57455E2B07C4F3A9BDB8BBF71DBAFEB9B1C98B25C24F`, source manifest `Saved/Logs/rawlins-barrage-animation-20261009/source-manifest-build43.json` (effective source identity `CD0AC92562C03FCFFCBCBBA1A2C836B41AAA89A7B3F2AC368BA0FFA14BD05433`, 131 staged artifacts). Packages Oct 1–9 slide physics repairs, Alona Radiance `UBwayHeroAttributeSet::AttackStrength` scaling fix, Rawlins Barrage presentation pointer cache, and `-NullRHI` montage pose tick parity.
  - `LAN-20261013-11` (retained record): Exe SHA-256 `54AA9FA99B53798BB00B0BC85E769465B6B2AE86A9064E27760691536B34F9F6`, source manifest `Saved/Logs/rawlins-barrage-animation-20261009/source-manifest-build39.json`.

## 2. Production Architecture & Invariants

- **Gameplay Ability:** `UBwayGameplayAbility_RawlinsBlazingBarrage` (`Plugins/GameFeatures/Heroes/Rawlins/Source/HeroRawlinsRuntime/Public/Abilities/BwayGameplayAbility_RawlinsBlazingBarrage.h`), asset `/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_BlazingBarrage`, config `DA_BW_RawlinsKitConfig`.
  - Channels `BarrageShotCount = 12` shots at `BarrageShotInterval = 0.10s` (`4.0` base damage + `0.3 * AttackStrength`, speed `3600`, upward juggle impulse `350`, cooldown `25s`).
  - Copies the config's presentation soft pointer into `ActiveProjectilePresentation` during `ActivateAbility`; `SpawnShot` checks `.Get()` and loads synchronously only when unresolved.
  - Grants `Gameplay.MovementStopped` while channeling so walking input cannot displace the caster while live controller yaw rotates the firing direction each shot.
  - **Damage Interruption Invariant:** Compares the health-change delegate's `NewValue` with its `OldValue` and interrupts on a decrease greater than `KINDA_SMALL_NUMBER`, so earlier healing cannot mask damage.
- **All-20 Ability Headless Combat Suite (`Breakaway.Abilities.AllKitCombatEffects`):**
  - Verifies Argus (`Primary`, `Slide`, `NoRetreat`, `ForGlory`, `Retribution`), Alona (`Primary`, `SunsGrace`, `Radiance`, `SunBurst`, `Blessing`), Korryn (`Primary`, `Flock`, `BurdenOfSin`, `CircleOfSpite`, `AuraOfSilence`), and Rawlins (`Primary`, `DoubleDown`, `PowerShot`, `SlideShot`, `BlazingBarrage`), plus non-primary `UBwayLifeLeechComponent` healing, `ABwayCharacterWithAbilities` kill/assist credit, and `ARelicActor` forced fumbles.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Asset & Cue Audit | `Breakaway.AssetAudit.RawlinsBarrageAnimation` + `Breakaway.GameplayCueAudit` | **PASS** — `Saved/Logs/rawlins-barrage-animation-20261009/automation06-barrage-audit.json` |
| All-20 Ability Headless Combat Automation | `Breakaway.Abilities.AllKitCombatEffects` | **PASS** — `Saved/Logs/allkit-combat-auto-5.json` (`succeeded: 1, errors: 0, warnings: 0`, 19.92s); transient-world scope |
| 4-World PIE Authority Combat Smoke & Barrage Matrix | `bway.Test.AllKitCombatPIE` + `bway.Test.RawlinsBarrageAnimation` | **PASS within scope** — nine authority-driven combat actions and coarse four-world observations (`Saved/Logs/allkit-combat-pie-4-result.json`); separate Barrage PIE matrix (`pie28`, `pie29`) |
| 4-Process Packaged Normal (0ms & 100ms) | `bway.Test.BarrageTransportHost` / `Client` (`LAN-20261013-11`) | **PASS** — 392 normal, 153 setup, 97 cleanup checks; 24 shots, 96 peer plays (`candidate11-normal-transport-independent-closure01.json`) |
| 4-Process Packaged Changing-Aim (0ms & 100ms, 2 D3D12 + 2 NullRHI) | `Saved/run-barrage-changing-aim-guard.ps1` (`LAN-20261013-13`) | **PASS** — 392/392 normal + 69/69 aim + 27/27 negative controls at both `0ms` (`peak=33.334ms`, `candidate13-changing-aim-zero01-aim-verification01.json`) and `100ms` (`peak=33.466ms`, `candidate13-changing-aim-lag01-aim-verification01.json`) |
| 4-Process Packaged Authority Combat Smoke (100ms, 2 D3D12 + 2 NullRHI) | `bway.Test.AllKitCombatHost` / `Client` (`LAN-20261013-13`) | **PASS within scope** — nine authority-driven actions; coarse HP/status/fumble thresholds on four peers, 3 client exits 0, 0 log errors (`Saved/Logs/allkit-combat-packaged-LAN-20261013-13-100ms-r3-result.json`) |
| October 16 historical evidence revalidation | Existing candidate13 captures; no new game processes | **PASS** — each of 0/100ms retains 392 normal checks, 69 aim checks, and 27 corruption controls; `Saved/Logs/goal1-review-20261016/reuse-candidate13-{0,100}ms-{normal,aim}.json`. Frozen candidate scope only; peaks remain 33.334/33.466ms within the 200ms bound. |
| October 17 Editor8 headless verification | Ten independent selected filters, including all-20 combat, restrictions, targeting/immunity, zone/projectile lifecycles, and life leech | **PASS within headless scope** — [independent run](../Saved/Logs/goal1-review-20261016/automation-editor08-independent.json), process/framework exit0. The original eleven-test run failed only the Flock death collision expectation; that failed aggregate remains preserved. |
| October 17 Editor9 lifecycle verification | Affected headless test plus nine remote-owner actions, each with death and reset | **PASS within recorded scope** — [30 headless cases](../Saved/Logs/goal1-review-20261016/automation-editor09-async.json) and [18 strict PIE requests](../Saved/Logs/goal1-review-20261016/async-matrix-editor09-final.json). PIE proves actual pawn replacement, natural cooldown expiry, fresh action, and four-peer cleanup at 0ms. It excludes persistent-zone/projectile policy and Barrage's separate cooked lifecycle cases. |

## 4. Remaining Open Proof Gaps

- Cooked separate-process damage-interruption and death/round-reset subcases (verified in 4-world PIE).
- Twenty-action production owning-client/server outcomes, exact settled peer comparison, and fixture isolation after review. Coverage audit: `Saved/Logs/commit-review-ed6008b9/goal1-20261016/allkit-coverage-audit.json`; continuation: `Saved/Logs/goal1-review-20261016/gate-contract.json`.
- AllKit PIE04's floor/topology failures remain preserved. Editor9's corrected floor passed Sun's Grace in PIE06, but [PIE06 then failed Double Down](../Saved/Logs/goal1-review-20261016/allkit-pie06-failed.json) at owner lifecycle observation. The reviewed immediate speed sample and diagnostics are integrated but uncompiled. The later retained-fixture topology capture proves observation only; no complete all-20 owning-client pass exists.
- The first Retribution reset request missed its 50ms intervention window before any reset. Its retry passed with an observed 44ms intervention and unchanged criteria. Preserve the [timing diagnosis](../Saved/Logs/goal1-review-20261016/async-pie09-c2-reset-timing-diagnosis.json) and [failed cadence preflight/unintended dispatch record](../Saved/Logs/goal1-review-20261016/async-pie09r1-c2-reset-cadence-procedure.json). A 120 FPS cap does not establish measured 120 FPS.
- Physical input verification, bespoke barrage animations, and second-PC LAN verification.

## 5. Archive Pointer

- Full chronological gate diary: [RAWLINS_BARRAGE_ACCEPTANCE.md](./Archive/Full_History_20261013/RAWLINS_BARRAGE_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
