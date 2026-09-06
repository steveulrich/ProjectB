# Match upgrade economy implementation

The shop remains incomplete. This document records implemented foundations and their limits; original-game requirements are in [Final-alpha evidence](../../../../AI_Planning/FINAL_ALPHA_PARITY.md).

## Server payment primitive

`UBwayEconomyLibrary::TrySpendGold(PlayerState, Cost)` accepts a nonnegative integer price resolved by trusted server code. It rejects missing players, non-authority calls, missing gold attributes, non-finite balances, and insufficient funds. Rejections do not deduct a partial amount. Zero-cost operations succeed only with a valid authoritative economy.

A fixed native instant Gameplay Effect performs the debit, retaining the existing gold attribute replication and change notifications. `WasSuccessfullyApplied()` distinguishes successful instant execution from a rejected application; `IsValid()` alone cannot do that for an instant effect.

This function is not an RPC or a complete purchase transaction. Do not expose client-supplied prices through it. The future purchase service must resolve item identifiers against a server catalog, validate rank/slot/state, and commit the debit and upgrade together. Rank grants, repeated-request semantics, refunds, and rollback still need implementation. The current gold cap also needs reference tuning.

## Verification

`Breakaway.Economy.GoldDebit` uses a temporary world, PlayerState, real GAS component, and gold AttributeSet. It checks non-authority rejection, missing economy, negative and oversized prices, insufficient funds, exact deductions, repeated unaffordable attempts, zero-cost behavior, and spending down to zero. The role-switch test exercises the authority guard; it is not separate-process replication evidence.

Build: `Saved/Logs/codex-economy-debit-authority-build.log` passed. Runtime test result is recorded after execution.

## Next implementation work

Recover the catalog and rank values, including life leech and cooldown reduction; model upgrade definitions and owned ranks; validate base/pre-round/death access; implement server purchase transactions; connect real Gameplay Effects and respawn persistence; build and verify the shop UI. No completion gate for the playable shop is closed by the payment helper alone.


The corrected fixture passed at 01:20:57 UTC on September 6, 2026. Evidence: `Saved/Logs/codex-economy-debit-verified.log`; final fixture build: `codex-economy-fixture-build.log`. The initial test fixture incorrectly initialized a world twice and crashed the editor before exercising the debit. That fixture defect was fixed, documented in BF-062, rebuilt, and rerun successfully. Background throttling was restored afterward.
