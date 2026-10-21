# Combat Credit & Economy Attribution Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED for the recorded historical scope** (4-world PIE + 4-process packaged remote-owner kill/assist gold attribution, 10s assist expiry, round persistence, rematch reset, and contributor-disconnect safety at 200ms lag). This verdict does not close the current Goal 1 review gaps.
- **Frozen Candidates:**
  - `LAN-20260927-02`: Exe SHA-256 `4EB9744626631567EB9F0AFD8BF19C3E62F516D3CD40E5DE3D1013FF2B8501F4` (remote-owner primary attribution, 715 checks).
  - `LAN-20260927-03`: Exe SHA-256 `1E5CFBB3E02A8F99AA0BD2CD9D4A67F0B27EFD04B9C6FD7F912B4F55F5DE4DA7` (native countdown clock timing, 102 checks).
  - `LAN-20260927-05`: Exe SHA-256 `5D8E0DB6622639EC1FB3EDC2197858478FB8F1377201CBE5526534D73E434C36` (complete 4-process credit, results, rematch reset, and contributor disconnect, 659 checks, 22 controls).

## 2. Production Architecture & Invariants

- **Damage Contribution Ledger (`ABwayCharacterWithAbilities`):**
  - Authority binds `ULyraHealthSet::OnHealthChanged` (`HandleCombatHealthChanged`) and records weak `ABwayPlayerState` contributor references with server timestamps (`LastDamageServerTimeSeconds`) and cumulative damage.
  - Excludes self-damage, friendly fire, zero-delta changes, and post-mortem hits; clears ledger on respawn or round reset (`ClearDamageContributors`).
- **Death Credit Resolution (`UBwayRoundManagementComponent::OnPlayerDied`):**
  - Calls `ABwayCharacterWithAbilities::ConsumeCombatCredit` once per dying pawn before elimination evaluation or match summary snapshotting.
  - Awards killer `+1 Kills` and `50g` (`GoldAwardForKill`, clamped to `MaxGold = 500g`, incrementing `MatchGoldEarned`).
  - Awards each living/valid opposing contributor within `AssistCreditWindowSeconds = 10.0f` `+1 Assists` and `25g` (`GoldAwardForAssist`); safely skips disconnected (`nullptr` / non-opposing) contributors without crashing or double-awarding.
- **Death Ability Lifecycle (`GA_Hero_Death`):**
  - Captures the dying avatar actor on activation and ends cleanly when `OnPawnAvatarSet` replaces the pawn during respawn or round reset.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Historical automation reference | `Breakaway.Economy.CombatCredit` | **Evidence unresolved** — the formerly cited automation report is absent, and this test name is not registered in current source. Do not use this row as current automation proof. |
| 4-Process Packaged Remote Primary Credit (200ms) | `Candidate02` (`LAN-20260927-02`) | **PASS within frozen candidate scope** — 715 checks across 4 hero primaries; [remote primary credit verification](../Saved/Logs/combat-credit-20260926/packaged-credit02-verification.json). Actual remote attacks, authority awards, and client KDA/wallet convergence on one PC. |
| 4-Process Packaged Full Lifecycle & Disconnect (200ms) | `Candidate05` (`LAN-20260927-05`) | **PASS within frozen candidate scope** — 659 checks; [lifecycle and disconnect verification](../Saved/Logs/combat-credit-20260926/packaged-disconnect01-verification-v2.json). Real damage, synthetic round-end triggers, production rematch, empty-ledger checks, and actual client departure; not natural scoring or second-PC LAN proof. |

## 4. Remaining Open Proof Gaps

- Non-primary ability credit attribution across all 16 hero abilities (Goal 1), environmental/knockback ring-out attribution, and non-damage support assist rules.
- Physical input verification and second-PC LAN hardware verification.
- The [Goal 1 acceptance mapping](../Saved/Logs/commit-review-ed6008b9/goal1-20261016/acceptance-mapping.json) separates existing assertions, historical candidates, invalidated evidence, and required retries. The current AllKit owner probe has no passing runtime evidence. Its planned per-action contributor checks cannot be inferred from these historical primary and lifecycle results.

## 5. Archive Pointer

- Full chronological gate diary: [COMBAT_CREDIT_ACCEPTANCE.md](./Archive/Full_History_20261013/COMBAT_CREDIT_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
