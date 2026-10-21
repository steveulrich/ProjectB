# Life Leech Upgrade Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED for the recorded historical scope** (Economy automation + 4-process packaged purchase, fractional healing across all 4 hero primaries, 7 exclusion/clamping edge cases, 4-slot loadout persistence, and rematch reset at 200ms lag). This verdict does not close the current Goal 1 review gaps.
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
| Economy Automation Suite | `Breakaway.Economy` (6 tests incl. `LifeLeech`) | **Historical PASS** — [six-test Economy report](../Saved/Logs/life-leech-20260922/automation-economy-r1.json) and [targeted Life Leech report](../Saved/Logs/life-leech-20260922/automation-life-leech-r1.json). These reports establish their recorded automation scope. |
| 4-Process Packaged All-4 Hero Primaries (200ms) | `Candidate08` (`LAN-20260922-08`) | **PASS within frozen candidate scope** — [all-kit primary leech verification](../Saved/Logs/all-kit-leech-20260922/runtime08-brief.json): 128 peer rows, host melee and three remote predicted requests, exact damage and leech healing, including Rawlins' two-shot burst. |
| 4-Process Packaged 7 Exclusion/Clamping Cases (200ms) | `Candidate11` (`LAN-20260922-11`) | **PASS within frozen candidate scope** — [leech boundary verification](../Saved/Logs/life-leech-limits-20260922/runtime11-brief.json): 256 peer rows, MaxHealth and overkill clamps, and friendly/self/buildable/dead-attacker/full-health exclusions. Delayed dead-source damage uses a native fixture call; it does not prove in-flight projectile behavior. |

## 4. Remaining Open Proof Gaps

**Projectile lifecycle policy (project decision, October 17, 2026):** Already-fired projectiles continue after shooter death until a hit or their original expiry. Damage retains attribution to the original player, but dead shooters receive no life-leech healing. Round transitions, direct round resets, and rematch resets remove existing projectiles. The implementation passed Editor8's [headless projectile lifecycle and life-leech checks](../Saved/Logs/goal1-review-20261016/automation-editor08-independent.json). Actual pawn replacement, owner/proxy, and packaged proof remain pending; this decision does not establish historical final-alpha parity.

- The [18-request replacement matrix](../Saved/Logs/goal1-review-20261016/async-matrix-editor09-final.json) proves nine actions through actual death/reset and fresh remote-owner use at 0ms. Rawlins' primary proves cancellation of its delayed second shot and eventual old-shot disappearance. It does not prove a surviving projectile's post-death hit, original-player attribution, or dead-source healing exclusion; those focused peer checks remain open.
- Non-primary combat ability Life Leech attribution across all 4 kits (Goal 1) and original September 2017 reference confirmation for in-flight post-mortem projectile policy and rank prices.
- Physical shop/combat input and second-PC LAN hardware verification.
- The [Goal 1 acceptance mapping](../Saved/Logs/commit-review-ed6008b9/goal1-20261016/acceptance-mapping.json) identifies the existing exclusion tests and remaining owner/authority/proxy proof. The current AllKit owner probe has no passing runtime evidence. Listener exclusions do not prove each ability's targeting filters, and historical primary results do not establish non-primary outcomes on the current candidate.

## 5. Archive Pointer

- Full chronological gate diary: [LIFE_LEECH_ACCEPTANCE.md](./Archive/Full_History_20261013/LIFE_LEECH_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
