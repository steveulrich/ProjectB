# Packaged rematch economy verification

## All-four-upgrade rematch gate, September 22, 2026

Current gate: `Saved/Logs/all-upgrade-rematch-20260922/gate-contract.json`.
Candidate `LAN-20260922-03` passed this scope: 16 purchases, 32 correlated public-attribute
rows, four simultaneously live peers, three normal client exits, and zero runtime errors.
Each client settled in the fresh reset state for 2.1 seconds, then reached the next match with
500 health, no owned upgrades, and zero damage stats. All four attribute bonuses had cleared.

The development fixture buys rank 1 of `AttackStrength`, `Armor`, `MoveSpeed`, and
`CooldownReduction` for each of four humans during the production purchase window.
It verifies all 16 catalog prices and debits. Each client must observe its four owned ranks,
positive wallet and damage stat, and the catalog bonuses on all four players' public attributes.
The fixture then uses production round completion and `RestartMatchFromResults`.

Fresh GameState and PlayerState objects must clear upgrades, stats, and wallets. The normal
hero-selection timeout must lead into the next playable match. Each peer then compares all
four public attributes with their new-match bases, requiring no residual upgrade modifiers.
All four rendered processes must be alive at the final checkpoint before clients exit normally.
The host is cleaned up separately after the clients exit.

Candidate `LAN-20260922-02` completed authority reset and next-match checks, but its client
observer incorrectly required catalogs from remote players. Catalogs and ranks intentionally
replicate owner-only. All three clients timed out without claiming purchased-state acceptance.
The corrected observer uses its local player's shared catalog to check public attributes;
production catalog and rank privacy are unchanged. The failed artifact and logs are retained.

The two LyraGame compiles passed in 39.03 and 35.1 seconds. Both packages passed with 131
hashed files each. The second package recovered from one internal Zen oplog read retry.
`Get-AllUpgradeRematchBrief.ps1` checks the frozen catalog, transaction and player correlation,
observed latency and D3D12 startup, reset/selection ordering, attributes, process exits, and errors.
All 21 extractor controls passed: the real baseline was accepted, and missing/malformed owner,
purchase, reset, ordering, renderer, latency, live-process, exit, and attribute evidence was rejected.
The controls also rejected a 0.005 residual movement bonus and a different frozen catalog magnitude.
Copied-log controls are separate extractor validation and do not add gameplay proof.

Evidence is retained beside the gate contract: `candidate22-03-rematch-brief.json`,
`packaged-LAN-20260922-03-all-upgrade-rematch-result.json`, `candidate22-03-flow-inventory.json`,
`package22-03-result.json`, `accepted-input-check.json`, and `verifier-checks.json`.
The complete artifact hashes are in `Saved/Logs/codex-packaged-LAN-20260922-03-sha256.csv`.
Overhead was two game builds, two packages, two four-process runs, and zero editor launches.
The second run was justified by the diagnosed observer ownership error; no unchanged rerun was used.

This gate covers one PC, one listen host, three clients, and 200 ms simulated latency.
Physical shop/rematch controls, visual polish, client purchase RPC transport, session discovery,
and second-PC LAN require their own evidence. The current catalog's final-alpha tuning remains
provisional. Invalidate this gate when relevant catalog, reset/travel, attribute replication,
fixture, or artifact inputs change.

## Historical single-upgrade reset gate

Gate: `Saved/Logs/packaged-rematch-20260920/gate-contract.json`.
The target is a four-player packaged listen-server match whose production rematch clears owned
upgrades, match stats, and wallets on the server and replicated clients. Physical purchase/rematch
input, session discovery, rendering, and second-PC acceptance remain separate requirements.

## Fixture and evidence contract

The historical `BwayMatchUpgradeLifecycleTestCommands.cpp` resolved PIE and packaged game worlds from native
ticks, waits for four human PlayerStates with catalogs and wallets, and buys one real catalog rank
for each player during the production planning window. Setup verifies each exact debit and seeds
nonzero damage stats. It reaches PostMatch through production round APIs and calls
`RestartMatchFromResults`; it does not substitute a direct map-open command for rematch.

Each client runs `bway.Test.MatchUpgradeLifecycleObserve`. It must first observe its owned upgrade,
positive wallet, and damage stat. It then requires different GameState and PlayerState objects,
the replicated destination roster, cleared owner upgrades, and cleared roster stats and wallets
for two seconds. Owner-only upgrades are asserted on the owner, not on unrelated clients.

`Get-PackagedRematchBrief.ps1` joins four authority purchase records, server reset markers, and
three clients' before/reset/test-exit records. Client player IDs and upgrade IDs must correlate
with authority purchases, and the observed destination must match the production rematch URL.
Transaction debits are exact; client wallet samples permit up to 20 additional gold for passive
income between transaction and observation. The host remains alive until all clients finish;
owned host teardown is not presented as graceful frontend exit.

The saved runner uses one PC, four NullRHI processes, no bots, 200 ms simulated latency, five
initial points to win, and an eight-second planning window. No shipping gameplay was changed.
Only LyraGame is rebuilt for these packaged fixtures; rebuild LyraEditor before using the new
fixture commands in a source editor.

## Attempts

- Candidate `LAN-20260920-02`: build and Cook/Stage/Pak passed. All four players purchased
  `AttackStrength` rank 1 for 75 gold (500 to 425), all three clients observed their purchase and
  damage stat, and production rematch reached `L_BW_HeroSelect_Staging`. The server verified four
  fresh players with zero ranks, stats, and wallets. Client acceptance remained incomplete: its
  periodic ticker accumulated frame deltas, stretching the intended two-second window. The four
  owned processes were stopped deliberately, and the runner records failed client exit status.
  Preserve `rematch-candidate02-brief.json` and `attempt02-clock-stop-processes.json`; this was a
  fixture timing failure, not proof of a gameplay reset defect.
- Candidate `LAN-20260920-03`: **passed the scoped packaged reset gate**. Setup, server verification,
  and client deadlines now use monotonic elapsed time. Client settling starts at a qualifying
  destination sample and resets on changed/missing state. Destination diagnostics are bounded to
  once every five seconds. LyraGame compile and Cook/Stage/Pak passed. Four distinct players bought
  `AttackStrength` rank 1 at 500 to 425 gold. Each of the three clients observed rank 1, 425 gold,
  and damage stat 1 before rematch, then passed settled reset in the exact production destination.
  Host verification cleared all four players. All four logs had zero errors and critical lines;
  clients exited 0, and the host was stopped separately after their completion. See
  `rematch-candidate03-brief.json` and `candidate03-flow-brief.json`.

The evidence helper also rejected copied-log negative cases with a missing client before-state
and a wrong reset destination. These parser checks are recorded separately in `helper-validation.json`
and do not add gameplay acceptance. Overhead: two builds, two packages, two four-process runs, no
editor launches; the first run was stopped for the diagnosed clock defect. Final process inventory
confirmed zero Unreal processes.

## Follow-up selection defect and verified fix

The reset passes, but the normal selection handoff remains unverified. In candidate 03's host log,
the rematch URL correctly omits `SkipHeroSelection`; `BwayGameplayUrlLibrary` then adds
`SkipHeroSelection=1` from the persistent launch command. Consequently the staging world receives
the initial direct-match shortcut again. This is evidence about a direct-launch rematch with that
startup flag, not proof that every normal frontend launch fails.

Candidate `LAN-20260920-04` fixes option precedence in `BwayGameplayUrlLibrary`, with a regression
test covering explicit staging versus stale startup shortcuts while preserving direct-match
launches. A four-process 200 ms run passed normal rematch selection, the real timeout fallback,
and a fresh playable match after reset. See [rematch selection flow](./REMATCH_SELECTION_FLOW.md)
for evidence and the remaining physical/UI/LAN scope.

Full logs and frozen source snapshots remain in `Saved/Logs/packaged-rematch-20260920/`, with
packaging logs and complete staged-file manifests under `Saved/Logs/codex-packaged-<candidate>*`.
Invalidate affected evidence after changes to rematch travel/reset behavior, replication,
catalog/wallet semantics, fixture timing/readiness, or the tested artifact.
