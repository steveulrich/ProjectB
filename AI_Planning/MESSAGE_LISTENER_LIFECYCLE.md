# Gameplay message listener lifecycle

September 26, 2026. Candidate `LAN-20260926-03` passes listener cleanup and controlled delivery across two packaged gameplay lifetimes at 200 ms latency. The fix removes invalid teardown calls from three components. Damage, healing, combat numbers, and carrier fumbles still work after frontend return, disconnect, browser rediscovery, and rehost. Physical input, visual appearance, and second-PC LAN remain unverified.

## Cause and change

`UBwayCombatFeedbackRouterComponent` and `UBwayRelicManagerComponent` register gameplay-message listeners only on authority. `UBwayCombatNumberPopComponent` registers only for a local player controller. Their old `EndPlay` implementations always looked up the world subsystem and tried to unregister every handle, including default handles on peers that never registered.

All three components now call each handle's `Unregister()`. The existing handle implementation follows its weak reference to the original subsystem, unregisters a live handle, and clears its state. Default handles perform no operation. Registration, damage/healing calculations, message routing, and relic rules are unchanged; the subsystem warning remains enabled.

The old candidate23-09 browser regression logged 36 invalid-handle warnings on the host and 12 on each client. Candidate26-02 logs zero on every peer. The evidence directory contains both inventories.

## Packaged cleanup result

Candidate26-02 passed the four-process browser rediscovery regression with 200 ms packet lag: six browser joins, 80 roster rows, 16 unique team groups, results return, three standalone recoveries, and a second playable match. All three clients exited with code 0; the runner separately stopped its owned host. All four logs contain zero runtime errors. Remaining warning counts are 355 on the host and 29, 30, and 29 on the clients; this is not a warning-free claim.

The first match used one fixture call to production `EndRound`; do not describe this attempt as natural scoring. All nine cleanup-verifier corruption controls and three roster-uniqueness controls were rejected. Combat diagnostics were enabled and confirmed by engine output, but no damage/heal/number/fumble events occurred. That quiet match cannot prove continued listener delivery, so `runtime01-brief.json` is explicitly partial.

## Controlled delivery coverage

The development-only `bway.Test.ListenerDelivery` command waits for four human pawns in Playing, then runs once in each of two distinct gameplay worlds. It applies small production damage effects from opposing humans and an instant Healing modifier that restores each target's prior health. Both effects execute the production HealthSet message path. Each player acts as a damage source and target.

The fixture invokes production relic pickup before carrier damage, requires a dropped relic and an exact forced-fumble increment, and resets the relic to the map spawn during cleanup. It does not move player transforms, change shared assets or effect defaults, or alter shipping behavior. Combat/stat changes are deliberate actions in a disposable test match. One bounded ticker stops after two completed lifetimes or a causal failure; clients use existing number-presentation diagnostics.

The delivery verifier requires real health deltas, both epoch completions, both fumbles, cleanup, and all three number categories on every rendered peer within five seconds of each controlled dispatch. Physical input, visual readability, and second-PC LAN remain separate acceptance requirements.

Candidate26-03's completed `delivery2` attempt passes all of those checks: eight damage/heal cycles, 24 number presentations, two carrier fumbles, and two cleanups. Every peer presents incoming damage, outgoing damage, and incoming healing exactly once in each controlled window. The host records three local routes and nine remote routes per lifetime. No invalid-handle warnings occur. The verifier rejects all 18 delivery corruption controls; the cleanup verifier rejects nine, and the roster checker rejects three.

The same run passes six browser joins, 80 matching roster rows, 16 unique team groups, results return, and a second playable match. It uses no fixture `EndRound` call; the deliberate damage/heal/pickup actions remain part of its scope. All three clients exit with code 0 before owned host teardown. Runtime errors are zero on every process. Remaining warning counts are 325 on the host and 30, 30, and 29 on the clients.

The first candidate26-03 attempt, `delivery1`, passed its first epoch but lost its command-session handle before saving a terminal result. All game and runner processes were confirmed absent; all four logs stop at first-match results without a reported runtime error. The cause remains unconfirmed. `delivery1-interruption.json` preserves that audit. One retry reused the unchanged, reverified artifact under a fresh name; no extra build or package was needed.

## Evidence and invalidation

Evidence root: `Saved/Logs/listener-cleanup-20260926/`. The gate contract records phase status and counts. Candidate26-02 records are `flow01-brief.json`, `uniqueness01-brief.json`, `runtime01-brief.json`, and `candidate26-02-artifact-verification.json`; raw logs and the 72-warning baseline remain alongside them.

Candidate26-02 contains 131 verified staged files. Its executable SHA-256 is `3101852C1CE47E0E20CDC29AA97A66C57CD508B9ECE34CF682BF728048C06ECB`. Preserve its cleanup scope when reviewing newer candidates. Revalidate after changes to registration/teardown, message producers or routing, local-controller ownership, game-feature component activation, travel, or affected package contents.

Candidate26-03 also contains 131 verified staged files. Its executable SHA-256 is `6E65AD5AA26F2BF7D67C51E2F5302314FE5068B008EBF6B7BD6ACC0E7CFE3DE4`. Final decision records are `flow03-brief.json`, `uniqueness03-brief.json`, `runtime03-brief.json`, `delivery03-brief.json`, and `candidate26-03-artifact-verification.json`. The terminal runner record is `packaged-LAN-20260926-03-rediscovery-delivery2-result.json`; all raw logs and corruption controls remain alongside it.

Overhead: two passing LyraGame builds, two packages, three runtime attempts (quiet coverage, interrupted delivery, completed delivery), and no editor launches. The second build adds only the focused development fixture; the production cleanup fix is identical in both candidates.

Prevention: use the registering handle for conditional subscription cleanup. Define a controlled event before a delivery gate; a successful quiet match proves travel and teardown, not message delivery. The first run's coverage gap led to a focused second fixture instead of an unchanged retry.
