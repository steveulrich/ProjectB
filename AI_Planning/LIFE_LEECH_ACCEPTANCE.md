# Life Leech Upgrade Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (Economy automation + 4-process packaged purchase, fractional healing across all 4 hero primaries, 7 exclusion/clamping edge cases, 4-slot loadout persistence, and rematch reset at 200ms lag).
- **Frozen Candidates:**
  - `LAN-20260922-08`: Exe SHA-256 `378582768F31400B52AA67FAFB392EE362B4962ACE70C18F979E9EEB969D4C35` (all-4 hero primaries + remote Rawlins 2-shot burst repair, 128 peer rows).
  - `LAN-20260922-11`: Exe SHA-256 `E42C74B38667EF3C6744CAA9DA843411B194E8F89417CE9F053D047AC5E45C4D` (7 limit/exclusion cases, 256 peer rows, 20 controls).

## 2. Production Architecture & Invariants

- **Attribute & Upgrade Effect (`UBwayHeroAttributeSet` & `UBwayUpgradeEffect_LifeLeech`):**
  - `LifeLeech` replicated attribute clamped to `[0.0, 1.0]` (`0.0` default).
  - 5th catalog item in `DA_BW_DefaultUpgradeCatalog` (`LifeLeech`): 4 ranks granting `+5% / +10% / +15% / +20%` (`0.05` per rank) at provisional prices `75 / 125 / 175 / 225` gold while preserving the 4-slot match equipment cap.
- **Authoritative Healing Listener (`UBwayLifeLeechComponent` on `ABwayPlayerState`):**
  - Listens to actual post-clamp health loss on enemy player `ULyraHealthSet::OnHealthChanged` (`ActualDamage = FMath::Max(0.0f, OldValue - NewValue)`).
  - **Exclusions:** Ignores non-damage changes, self-damage, friendly fire, buildable/non-player targets, dead attackers (`Health <= 0`), and overkill beyond the victim's remaining health (`OldValue`).
  - Applies `HealAmount = ActualDamage * LifeLeech` to the attacker's living pawn via an instant `Healing` meta-attribute `FGameplayEffectSpec` guarded by `bApplyingLifeLeechHeal` to prevent reentrant loops.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Economy Automation Suite | `Breakaway.Economy` (6 tests incl. `LifeLeech`) | **PASS** — `Saved/Logs/life-leech-20260922/automation-life-leech.json` |
| 4-Process Packaged All-4 Hero Primaries (200ms) | `Candidate08` (`LAN-20260922-08`) | **PASS** — 128 peer rows; exact damage & leech healing across Argus, Alona, Korryn, and Rawlins 2-shot burst (`candidate08-primaries-summary.json`) |
| 4-Process Packaged 7 Exclusion/Clamping Cases (200ms) | `Candidate11` (`LAN-20260922-11`) | **PASS** — 256 peer rows, 20 controls; MaxHealth clamp, overkill cap, friendly/self/buildable/dead-attacker/full-health exclusions (`candidate11-exclusions-summary.json`) |

## 4. Remaining Open Proof Gaps

- Non-primary combat ability Life Leech attribution across all 4 kits (Goal 1) and original September 2017 reference confirmation for in-flight post-mortem projectile policy and rank prices.
- Physical shop/combat input and second-PC LAN hardware verification.

## 5. Archive Pointer

- Full chronological gate diary: [LIFE_LEECH_ACCEPTANCE.md](./Archive/Full_History_20261013/LIFE_LEECH_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
