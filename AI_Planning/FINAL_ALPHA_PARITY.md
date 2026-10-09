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
| Round duration | Four-minute rounds. The producer also describes a four-minute maximum. [PAX hands-on](https://massivelyop.com/2017/09/05/pax-west-2017-hands-on-with-amazons-breakaway/), [producer interview within PC Gamer](https://www.pcgamer.com/amazons-fantasy-sports-game-breakaway-shoots-but-may-not-score/) | Demo=240 seconds and Dev=90 seconds pass configuration checks. Candidate26-05 also passes a controlled five-round match with real timer expiry, all three outcomes and four-peer results; physical/final integrated proof remains. See [demo rules](./DEMO_MATCH_RULES.md). |
| Win conditions | Goal, simultaneous opposing-team elimination, or relic territory at timer expiry. [PAX hands-on](https://massivelyop.com/2017/09/05/pax-west-2017-hands-on-with-amazons-breakaway/) | Existing native paths require actual gameplay verification. Exact midfield ties and elimination timing still need reference evidence. |
| Passing and movement | Slower carrier movement, prepared catching with right mouse, and downhill sliding. [PAX hands-on](https://massivelyop.com/2017/09/05/pax-west-2017-hands-on-with-amazons-breakaway/) | Verify the corresponding systems and input flow; current implementation alone is not parity evidence. |
| Death and respawn | The September 5 first-hand play session describes increasing respawn delays with successive deaths. [Game Informer hands-on](https://gameinformer.com/games/breakaway/b/pc/archive/2017/09/05/landing-big-dunks-in-breakaway) | Native fixed three-second timer confirmed; no AutoRespawn grants. Bot restart, PostRound suppression and round-reset alive counts pass candidate09 four-process testing at 200ms, including a further non-eliminating death/respawn (77 checks/480 snapshots). Server deadline and countdown pass four-world PIE (73 checks/984 samples) and candidate10 four-process testing at 200ms (76 checks/674 samples). Candidate12 also passes direct late-join selection (483 samples) and widget lifetime through browser return/rehost/rediscovery (2,653 samples). Physical/visual proof remains. See [countdown acceptance](./RESPAWN_COUNTDOWN_ACCEPTANCE.md). Exact progression, cap and reset boundaries remain unknown. See [respawn audit](./RESPAWN_PARITY_AUDIT.md). |

## Current implementation evidence

A further September 5 hands-on report describes shop access while healing at base and while dead, with life-leech and cooldown-reduction equipment. Include these in catalog and eligibility research; purchase access must not be limited to pre-round and death solely because the earlier source omitted base access. [Game Informer hands-on](https://gameinformer.com/games/breakaway/b/pc/archive/2017/09/05/landing-big-dunks-in-breakaway.aspx).

The initial native economy audit found gold attributes and passive income, but no purchase path. Subsequent implementation adds `UBwayEconomyLibrary::TrySpendGold`, an immutable upgrade catalog type, a populated four-rank attack/armor catalog asset, and a PlayerState component for server-validated purchases and replicated owned ranks. The editor `Breakaway.Economy` suite passes GoldDebit, GoldEarnings, and UpgradePurchase. As of September 20, separate-process purchase transport, scoped death/round persistence and rematch reset have packaged evidence; candidate15 also proves synthetic Slate purchase/navigation and open-shop recovery through replacement-pawn respawn in four rendered processes at 200 ms. See [shop acceptance](./SHOP_INPUT_ACCEPTANCE.md), [upgrade lifetime](./PACKAGED_UPGRADE_LIFETIME.md), and [rematch economy](./PACKAGED_REMATCH_ECONOMY.md) for their exact scopes. Physical devices, complete frontend/second-PC LAN, final catalog/tuning, and missing upgrade effects remain incomplete.

Candidate16 adds a supported reversible MoveSpeed effect and a third four-rank catalog entry, using the existing live movement multiplier. The four-test economy suite now includes speed limits, transactions, slow composition, avatar replacement, and reset. Movement costs and magnitudes remain provisional; see [movement acceptance](./MOVEMENT_UPGRADE_ACCEPTANCE.md) for runtime status and exact scope. That audit also identified missing life-leech and cooldown-reduction mechanics; the subsequent cooldown implementation is described below. The source routes and unresolved semantics are recorded in `Saved/Logs/shop-respawn-focus-20260920/next-upgrade-parity-audit.json`. Generic web searches reconfirmed the September hands-on categories but did not recover final prices, magnitudes, or full equipment names. Target dated in-game shop footage or archived developer material next; older/undated sources cannot settle final-alpha tuning.

The subsequent cooldown gate adds a reversible CooldownReduction effect and a fourth catalog entry. All four current hero kits route through the shared duration helper; economy automation covers all 16 configured cooldown abilities and the effect-default fallback. Catalog save/cold validation passes. Candidate18 passes real activation, settled owner/authority duration, HUD countdown/expiry and reactivation in four rendered Argus processes at 200 ms. Candidates22-01 and22-03 subsequently passed all-four-upgrade death/round persistence and production rematch/next-match reset. Other-kit network and physical/visual proof remain pending. See [cooldown acceptance](./COOLDOWN_UPGRADE_ACCEPTANCE.md).

Life leech now has a replicated reversible modifier and an authoritative listener that heals from actual enemy player health loss. Its fifth catalog choice preserves four equipment slots. Catalog save/cold validation and the six-test economy suite pass. Candidate `LAN-20260922-04` passes four owner purchases and fractional healing/health convergence across four rendered processes at 200 ms. Candidate22-05 passes the alternative four-item loadout through actual death/respawn, rounds, production rematch, normal selection, and the next playable match. Candidate22-08 passes the four current heroes' actual granted primaries, exact damage/healing attribution, owner/server termination, and 128 peer rows; it also verifies the remote Rawlins two-shot burst repair. Other abilities, the complete roster, and physical/visual acceptance remain pending; see [life-leech acceptance](./LIFE_LEECH_ACCEPTANCE.md). The rates, prices, and current eligibility policy are provisional and do not establish final-alpha tuning.

The supplied `Supplemental_Examples/basecharacter_statedeath.lua` enables equipment purchases on death at line 95. Its version is unknown; it corroborates the purchase-window direction but cannot fix final-alpha tuning.

The demo match configuration now uses 240 seconds and three round wins, while the explicit development config retains 90 seconds. [Current evidence](./DEMO_MATCH_RULES.md) includes candidate26-05's controlled five-round packaged match and all three outcomes; physical/final integrated acceptance remains open. Candidate26-07 separately passes four-owner relic pickup/pass/throw and scoring cleanup under 200 ms ([ownership evidence](./RELIC_PICKUP_OWNERSHIP.md)). These engineering results do not establish original tuning. The gold attribute's comment about buying buildables is not evidence that the original game required buildable purchases.

## September 26 reference audit

The official `playbreakaway.com/news/post/plans-for-breakaway` page and a September 2017 Wayback
news snapshot were not accessible through the web tool. Broad searches did not recover final-build
patch notes or the exact equipment catalog. Keep those gaps open; do not repeat identical searches
or substitute an unrelated modern game also named Breakaway.

A [June 16 interview with creative director Dave Verfaillie](https://www.mmorpg.com/interviews/alpha-testing-and-beyond-with-dave-verfaillie-2000106514)
is useful primary background for first-to-three scoring, persistent buildables, and spectator
cameras. It predates the required September reference window and cannot settle final-alpha tuning
or prove which spectator features shipped in the last public build. The concrete new lead from
the September hands-on account is death-dependent respawn timing; investigate the implementation
and reference together instead of treating the existing fixed delay as parity.

## September 27 respawn footage lead

The [GameSpot match recording published September 7, 2017](https://www.gamespot.com/videos/breakaway-16-minutes-of-gameplay-from-amazons-espo/2300-6440780/)
is playable in the browser (15:47.933 duration). It provides stronger timing evidence than the
earlier prose accounts, but its publication date does not identify the final public build.
The opening combat excerpt cuts to a map introduction around 00:35; do not use that cut as
evidence of a continuous round transition or hero reselection.

The complete match segment has a fresh-round countdown at 00:54.974 and a 03:57 game clock
at 00:59.974. At 01:39.974, the blue second portrait shows 10 seconds following a death;
at 02:39.974, the red third portrait shows 15 after an earlier observed death. At 06:59.974,
the pre-round scoreboard shows `dylanmeeble` with four cumulative deaths. At 07:39.974,
the same player's new death shows 10 seconds. Further sampling finds Tyler's next observed
death at 02:54.974 with 17 seconds remaining, preventing adoption of an assumed personal
10+5 progression. Equipment modifiers and team-wide increments remain possible confounders.
See [the timestamped respawn audit](./RESPAWN_PARITY_AUDIT.md#september-27-video-observations).
Keep the fixed three-second implementation marked provisional until the replacement policy
and its multiplayer behavior are implemented and verified.

The [September 7 Destructoid hands-on](https://www.destructoid.com/amazons-breakaway-might-be-the-next-great-esport/)
identifies purchasable respawn reduction. The [respawn upgrade gate](./RESPAWN_UPGRADE_ACCEPTANCE.md)
implements that missing effect with provisional data: seven economy tests, catalog cold load,
and four-world PIE pass. Candidate27-06 additionally passes 133 checks and 17 corruption controls
over 19,933 client observations in four packaged processes at requested 200 ms lag. Remote
purchases shorten actual pending deaths and replicate deadlines/modifiers through pawn and round
changes; production rematch clears state in staging and the next playable match. The final raw-log
audit preserves a failed live-wrapper race and its verified correction. Physical input, visual
appearance, second-PC LAN, final natural flow, and original respawn progression/tuning remain open.

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
