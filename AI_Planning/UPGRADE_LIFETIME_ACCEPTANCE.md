# Upgrade death and round persistence

The four-world PIE lifecycle gate passed on September 21, 2026 (UTC), using Dorado,
the Capture the Relic experience, four human controllers, and no bots. This proves
the current attack and armor upgrades survive death, respawn, and round changes.
It does not complete shop or packaged LAN acceptance.

Gate: `Saved/Logs/upgrade-persistence-20260920/gate-contract.json`.

## Verified behavior

The authority purchased rank 1 of both catalog entries for a remote human during
the production planning window. A development command applied lethal damage on
a native tick (`guard=0`). The production round timer provided the round changes;
the probe shortened its duration temporarily and restored it afterward.

The final arena run recorded 203 samples, including 101 after purchase convergence:

- Attack strength remained 45 from base 40 plus the catalog's +5 bonus; armor
  remained 3 from base 0 plus +3. No lost or duplicated modifiers appeared.
- Authority and the owning client retained both rank-1 entries. The other clients
  retained rank 0, as required by owner-only ownership replication, while observing
  the same public combat attributes.
- Every peer observed zero health and the dying state, then a different living pawn
  and exactly one added death. PlayerState and ASC identity stayed unchanged.
- The match advanced from round 1 through rounds 2 and 3. Purchase/round, respawn,
  and final-round acceptance each had its own stable interval exceeding two seconds.

`arena-lifecycle-brief.json` independently checks these requirements against
`lifecycle-arena-r3.json`. The observer source is frozen as `observer-arena-r3.py`.
`arena-error-inventory.json` retains two Pixel Streaming target-window errors and
zero other errors. Those two errors remain actionable for rendered/UI acceptance;
the gameplay assertion is not a UI pass. Native damage markers confirm health
350 -> 0 outside the editor script guard.

## Attempts and verifier quality

The first editor session exposed unsupported Python class/property access. Cached
working access uses native class paths and property names, and the public GAS
attribute-query functions with attributes taken from catalog modifiers. Protected
`SpawnedAttributes` is not read.

The preliminary observer let its settling clock span pawn/phase changes. Its raw
result is retained, but the independent brief rejects it. The corrected observer
resets settlement whenever the pawn, phase, round, or death count changes.

Full log screening then identified an invalid environment: the Dev experience had
run in the staging map without a relic. Those observations remain preliminary;
the final helper rejects that map. The accepted run explicitly required Dorado,
four human controllers, and one relic in every world before starting. A copy of
the accepted record with a removed attack bonus also fails verification.

Overhead: zero new C++ builds or packages, two editor launches, three PIE starts
(one setup-only), and three observer runs. Both editors closed normally. Each
observer released its callback, round duration was restored, PIE ended, and the
original four-player/bot settings were restored and checked after editor closure.

## Remaining scope and next action

This gate made authority-side purchases, so it adds no client RPC or physical shop
input proof. Existing packaged purchase and rematch reset evidence remains linked
from [packaged rematch economy](./PACKAGED_REMATCH_ECONOMY.md).

The subsequent native fixture passed across four packaged processes under 200 ms
latency in candidate `LAN-20260920-07`; see [packaged lifetime acceptance](./PACKAGED_UPGRADE_LIFETIME.md).
Both candidates remain frozen for their recorded scopes. Physical shop input, gameplay
impact on damage, the full equipment set, rendered frontend LAN flow, and
second-PC/clean-device proof remain open.

Invalidate affected evidence after changes to upgrade ownership/effects, hero
attribute initialization, pawn/ASC lifetime, death/respawn, round resets, or the
tested executable. Documentation and evidence-helper edits do not update a binary.
