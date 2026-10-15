# Cooldown Reduction Upgrade Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (Economy automation + 4-process packaged all-16 ability cooldown reduction across Argus, Alona, Korryn, and Rawlins, 4-upgrade slot persistence, and rematch reset at 200ms lag).
- **Frozen Candidate:** `LAN-20260922-14` (`Saved/StagedBuilds/Windows/candidate-lan20260922-14.json`, 131 staged artifacts).
- **Executable Identity:** `LyraGame.exe` SHA-256 `B6FB2327B036CC2F8D9ACFAEF00E0888276A1DA494D06AEC65DA01E80BD6C868`.

## 2. Production Architecture & Invariants

- **Attribute & Upgrade Effect (`UBwayHeroAttributeSet` & `UBwayUpgradeEffect_CooldownReduction`):**
  - `CooldownReduction` replicated attribute clamped to `[0.0, 1.0]` (`0.0` default).
  - Catalog entry `/BreakawayCore/Data/DA_BW_DefaultUpgradeCatalog` (`CooldownReduction`): 4 ranks granting `+5% / +10% / +15% / +20%` (`0.05` per rank) at provisional prices `75 / 125 / 175 / 225` gold within the 4-slot match equipment cap.
- **Shared Ability Cooldown Scaling (`UBwayGameplayAbility_Base`):**
  - `ApplyCooldown` and `GetCooldownDuration` scale authored kit cooldowns by `(1.0f - ClampedCooldownReduction)` across all four hero ability base classes (`UBwayGameplayAbility_ArgusBase`, `UBwayGameplayAbility_AlonaBase`, `UBwayGameplayAbility_KorrynBase`, `UBwayGameplayAbility_RawlinsBase`).
- **Korryn Flock Movement Restoration Invariant (`UBwayGameplayAbility_KorrynFlock`):**
  - Captures and restores `UCharacterMovementComponent::MaxFlySpeed` alongside `MovementMode` on `EndAbility` so repeated `Flock` activations do not leak flight speed (`Breakaway.Abilities.Korryn.FlockLifecycle`).

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Economy & Flock Automation | `Breakaway.Economy` (all 16 abilities) + `Breakaway.Abilities.Korryn.FlockLifecycle` | **PASS** — `Saved/Logs/cooldown-upgrade-20260922/automation-cooldown.json` |
| 4-Process Packaged 4-Upgrade Lifetime & Rematch Reset (200ms) | `LAN-20260922-01` & `LAN-20260922-03` | **PASS** — Death/round persistence + rematch reset across 4 peers (`candidate03-lifetime-summary.json`) |
| 4-Process Packaged All-16 Hero Abilities CDR (200ms) | `LAN-20260922-14` (`DA_BW_Argus`, `Alona`, `Korryn`, `Rawlins`) | **PASS** — 32 activation cycles, 64 HUD countdown samples, 512 peer rows, 28 controls (`candidate14-all16-cdr-summary.json`) |

## 4. Remaining Open Proof Gaps

- Authoritative combat/status/heal/knockback outcome verification for all 16 abilities (scheduled in Goal 1).
- Original September 2017 reference confirmation for exact CDR rank magnitudes and gold prices.
- Physical shop/ability input and second-PC LAN hardware verification.

## 5. Archive Pointer

- Full chronological gate diary: [COOLDOWN_UPGRADE_ACCEPTANCE.md](./Archive/Full_History_20261013/COOLDOWN_UPGRADE_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
