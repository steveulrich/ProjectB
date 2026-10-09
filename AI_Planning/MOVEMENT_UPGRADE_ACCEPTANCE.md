# Movement upgrade acceptance

Candidate: `LAN-20260920-16`. Evidence directory: `Saved/Logs/movement-upgrade-20260920/`.

## Implemented behavior

`UBwayUpgradeEffect_MoveSpeed` adds a reversible match-scoped bonus to the existing
`MoveSpeedMultiplier` GAS attribute. The catalog validator admits this attribute
while retaining the existing restrictions on persistent upgrade effects. The
native character movement component already consumes the live attribute for
walking and slide speed limits, including after avatar replacement.

`DA_BW_MatchUpgrades` now contains AttackStrength, Armor, and MoveSpeed. The new
movement ranks use bonuses of 5%, 10%, 15%, and 20%, with costs of 75, 125, 175,
and 225 gold. These values are provisional; dated final-alpha source material has
not established the original prices or magnitudes. Buying a later rank replaces
the earlier bonus. No new character art or animation asset is required.

## Evidence

- `automation-economy-r3.json`: all four economy tests passed, with zero test
  warnings/errors. `Breakaway.Economy.MovementUpgrade` exercises actual movement
  speed getters, exact debits, stale requests, debit-failure rollback, rank
  replacement, multiplicative slows, attribute base reinitialization, replacement
  avatars, and upgrade reset. It also checks slide speed limits; it does not
  simulate slide physics or a lethal-damage respawn lifecycle.
- `catalog-preview-r2.json`, `catalog-apply-r1.json`, and `catalog-cold-r1.json`:
  scoped editor asset loading, preview, one catalog save, and independent cold
  readback. Asset validation reports one valid asset without warnings. Existing
  attack/armor rows are preserved. Mutation was performed through Unreal APIs.
- `compile-editor-r4.json` and `compile-game.json`: final editor and game targets
  passed with empty failure signals.
- `package16-result.json`: package session and AutomationTool exited zero;
  all 131 staged artifact hashes matched. The tracked runtime patch and frozen
  native probe source matched before launch. Metadata/helper edits explain the
  broader source identity change; they do not change this candidate's runtime.

`candidate16-movement-brief.json` passed: one rendered listen host and three
clients at 200 ms packet lag, four production purchase RPCs with bounded wallet
checks, 16 matching peer attribute/speed-limit observations, private owner ranks,
and injected W movement on all four pawns. All four processes were alive at the
completion checkpoint. Three clients exited zero; host teardown followed those
exits. The four logs contain zero runtime errors, and cleanup left zero game/editor
processes. This is one-PC separate-process evidence.

`verifier-checks.json` passed 14 cases: the original evidence is accepted; mutated
caps, bonuses, movement, wallets, purchase markers, rank privacy, peers/cohort,
latency, errors, process count/exit, and catalog data are rejected. Mutated copies
are extractor tests, not runtime evidence.

## Retained failed attempts

The first automation fixture omitted native PawnData and the health attribute set.
It produced initialization errors despite matching numerical speed assertions.
The fixture was corrected, and the full economy suite passed twice afterward.

The first catalog preview ran before asynchronous asset discovery had completed.
Adding a synchronous scan of the single Economy directory fixed lookup; that
failed preview made no asset changes.

One editor compile failed because an unbraced `UE_LOG` macro expansion separated
an `else` from its `if`. Explicit braces fixed the development probe. Failed
records remain in the evidence directory; no assertion or error filter was relaxed.

## Remaining proof and invalidation

Physical keyboard/controller input, rendered shop selection of the new entry,
actual slide physics, second-PC LAN, and recovered final-alpha tuning remain open.
Earlier candidate15 proves a separate attack-upgrade shop/death/respawn scope;
it cannot establish the new movement entry's visual acceptance. The candidate14
first-attempt D3D12 pipeline-state startup crash also remains unresolved.

Changing the catalog, effect, attribute replication, movement consumer, purchase
component, or avatar/reset lifecycle invalidates the corresponding evidence.
Changing an extractor requires rechecking the saved evidence, not automatically
rebuilding or relaunching an unchanged candidate.

Recorded overhead: five builds (one failed), one
package, three economy runs (one failed), four asset commandlets (one failed),
zero GUI editor launches, and one passing four-process runtime attempt.
