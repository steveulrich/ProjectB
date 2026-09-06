# Final public alpha parity evidence

Reviewed September 5, 2026. This is a partial requirements reconciliation, not a completion report.
The owner's final-public-alpha direction in [Vertical slice definition](./VERTICAL_SLICE_DEFINITION.md) takes precedence over historical exclusions.

## Reference window

Use September 2017 evidence as the current reference window. Contemporary reporting places the final public alpha closure at the end of September. The exact final build and patch notes remain unverified. [PCGamesN retrospective with October 26, 2017 report](https://www.pcgamesn.com/breakaway/breakaway-cancelled).

Prefer dated first-hand gameplay observations and developer statements. The two September hands-on accounts below describe builds their authors played; neither identifies a final build hash. Do not silently substitute December 2016 behavior or later development prototypes.

## Requirements established by the current evidence

| System | September evidence | Project disposition |
|---|---|---|
| Match upgrades | Four trait slots, with traits upgradeable up to four times using match gold. Examples include speed, damage reduction, and attack damage. [PC Gamer, September 21](https://www.pcgamer.com/amazons-fantasy-sports-game-breakaway-shoots-but-may-not-score/) | Earn-only gold is insufficient. Implement purchasing, rank state, gameplay effects, and a usable shop. Exact catalog, costs, and effect values remain unresolved. |
| Purchase windows | Purchases before rounds and while dead. [PAX hands-on, September 5](https://massivelyop.com/2017/09/05/pax-west-2017-hands-on-with-amazons-breakaway/) | Add authoritative purchase eligibility and UI access in those states. Additional windows require evidence. |
| Buildables | One character-specific placement per round; surviving structures persist and accumulate. [PC Gamer, September 21](https://www.pcgamer.com/amazons-fantasy-sports-game-breakaway-shoots-but-may-not-score/) | Retain the current one-buildable baseline and verify persistence. No evidence here requires restoring the earlier two-buildable model. |
| Round duration | Four-minute rounds. The producer also describes a four-minute maximum. [PAX hands-on](https://massivelyop.com/2017/09/05/pax-west-2017-hands-on-with-amazons-breakaway/), [producer interview within PC Gamer](https://www.pcgamer.com/amazons-fantasy-sports-game-breakaway-shoots-but-may-not-score/) | Current 90-second config is useful for tests, but does not establish reference match timing. Separate test settings from the demo ruleset. |
| Win conditions | Goal, simultaneous opposing-team elimination, or relic territory at timer expiry. [PAX hands-on](https://massivelyop.com/2017/09/05/pax-west-2017-hands-on-with-amazons-breakaway/) | Existing native paths require actual gameplay verification. Exact midfield ties and elimination timing still need reference evidence. |
| Passing and movement | Slower carrier movement, prepared catching with right mouse, and downhill sliding. [PAX hands-on](https://massivelyop.com/2017/09/05/pax-west-2017-hands-on-with-amazons-breakaway/) | Verify the corresponding systems and input flow; current implementation alone is not parity evidence. |

## Current implementation evidence

A further September 5 hands-on report describes shop access while healing at base and while dead, with life-leech and cooldown-reduction equipment. Include these in catalog and eligibility research; purchase access must not be limited to pre-round and death solely because the earlier source omitted base access. [Game Informer hands-on](https://gameinformer.com/games/breakaway/b/pc/archive/2017/09/05/landing-big-dunks-in-breakaway.aspx).

The initial native economy audit found gold attributes and passive income, but no purchase path. Subsequent implementation adds `UBwayEconomyLibrary::TrySpendGold`, an immutable upgrade catalog type, and a PlayerState component for server-validated purchases and replicated owned ranks. The catalog is not yet populated/assigned, supported effects remain limited, and the shop UI and base-healing access are unfinished. See `Plugins/GameFeatures/BreakawayCore/Docs/Match_Upgrade_Economy.md` for precise implementation and verification limits. No Breakaway content filenames in the initial audit matched shop, upgrade, or purchase; this was not an exhaustive Blueprint graph audit.

The supplied `Supplemental_Examples/basecharacter_statedeath.lua` enables equipment purchases on death at line 95. Its version is unknown; it corroborates the purchase-window direction but cannot fix final-alpha tuning.

`BwayMatchFlowConfig.h` defaults round duration to 90 seconds, also observed in recent runtime logs. The gold attribute's comment about buying buildables is not evidence that the original game required buildable purchases.

## Upgrade implementation and acceptance work

These are engineering acceptance requirements derived from the owner's multiplayer and replacement-asset bar, not additional claims about the original game:

1. Recover the final catalog, prices, rank values, slot-selection rules, rank cap semantics, refunds, and purchase windows from dated footage or archived developer material. Record tuning that remains provisional.
2. Define replaceable upgrade data with stable identifiers, text/icons, rank costs, and Gameplay Effects. Keep content and tuning out of widget code.
3. Validate purchases on the server using the owning player's balance, allowed state, chosen slots, and rank. Reject invalid or unaffordable requests without partial deductions or effects.
4. Preserve purchased benefits through death and round resets; clear them at a new match. Verify how travel and hero changes interact with the reference rules.
5. Build the shop into pre-round/death flow with current rank, price, affordability, rejection feedback, and keyboard/controller focus. Keep the existing gold display synchronized.
6. Verify purchases from separate clients, repeated requests under latency, effect application to actual gameplay, respawn persistence, and new-match reset.

## Remaining reference gaps

This inventory is incomplete. Still reconcile the final roster and kits, equipment special effects, gold awards, buildable costs and durability, relic tuning, movement/combat timings, spectator/replay features, matchmaking/social flow, and all presentation contracts. The owner has not excluded unresearched systems. The current four heroes remain a development baseline, not proof of full roster parity.

For actual multiplayer verification and limitations, see [LAN front-end verification](../Plugins/GameFeatures/BreakawayCore/Docs/LAN_FrontEnd_Verification.md).
