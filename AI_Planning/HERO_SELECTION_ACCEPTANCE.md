# Hero selection acceptance

This gate checks server validation of owned selection requests, selection-time disconnect
and browser rejoin, and team hero uniqueness after travel. Physical selection UI and
second-PC LAN remain separate requirements.

## Production defects and repairs

- A preselected-hero request could reject a new ID, then lock the player's previous choice.
  `TrySetSelectedHeroId` now reports the authority result. The controller and manager stop
  assignment when validation fails. Repeated locks remain idempotent.
- The saved `BP_BW_GameState` selection component allowed duplicate heroes despite the
  architecture's unique-per-team rule. Its override is now false. Blueprint compilation,
  validation, and saved-default readback passed; the original asset is backed up in the gate directory.
- Candidate08 reproduced duplicate bot heroes after successful human selection and rejoin.
  Availability queried `PlayerSelections`, a staging projection that is empty during gameplay.
  It now queries current GameState players and teams. Bot creation also waits until travelling
  human controllers have loaded the world and joined their teams, preserving their hero choices
  before filling remaining slots. The retry timer is cleared after readiness, travel shutdown,
  and component end play.

## Runtime contract

`bway.Test.SelectionEdgesHost` and `bway.Test.SelectionEdgesClient <1|2|3>` use the real
frontend, session browser entry, and owned server RPCs. The host temporarily holds selection
for 120 seconds and restores the original 60-second property before releasing its lock.
This fixture does not prove normal selection timeout timing.

All three clients attempt an empty lock, select a valid preview, submit invalid IDs, lock
the valid hero, then repeat locks and request a different hero. Clients with a selected
teammate also attempt that teammate's hero. Client3 disconnects during selection, reaches
the standalone frontend, finds the same hosted match through a new browser entry, and locks
again using its new PlayerState. The host requires removal of the old roster and selection
entry before completing selection.

Acceptance requires one gameplay travel, four live rendered processes, confirmed 200 ms
packet lag, two teams of four unique heroes, matching eight-player rosters across all peers,
preserved human choices, three normal client exits, and owned host cleanup. Server logs must
show the rejected requests and one initial lock per original remote PlayerState.

## Attempts and evidence

Evidence directory: `Saved/Logs/hero-selection-edges-20260923/`.

| Candidate | Outcome | Reason for next attempt |
|---|---|---|
| `LAN-20260923-07` | Partial: initial rejection/lock checks and authority removal; no rejoin | Fixture incorrectly required a gameplay controller in the standalone Lyra frontend. Fixed that observation and added a 30-second frontend timeout. |
| `LAN-20260923-08` | Rejection, removal, standalone recovery, browser rejoin and travel observed; uniqueness failed on all peers | Production availability and bot creation lifecycle defects described above. |
| `LAN-20260923-09` | Selection gate and normal-flow regression passed, each with four peer passes, three client exits 0, and no runtime errors | Continue other acceptance gates. |

The failed attempts remain in `runtime07-brief.json`, `runtime08-brief.json`, their transport
results, and full logs. Each candidate has immutable source snapshots, changed-input hashes,
compile records, package records, and artifact verification. Candidate09's game SHA-256 is
`F3C9595CEDAEC5D723AE789477938A2D7A1DFD66474D78BA7CC8BEBB848744BE`.

The detailed checker and corruption controls are `verify-selection.py` and
`verify-controls.py` in the evidence directory. They validate actual emitted markers and
authority records; command-line text cannot satisfy them. Controls modify in-memory copies
of actual evidence and preserve mutation recipes without changing original logs.

## Scope and next action

Current overhead: four builds (one editor, three game), three packages, one editor inspection,
three packaged selection attempts, and one normal-flow regression. Candidate07 retried a demonstrated fixture defect;
candidate08 retried a demonstrated gameplay defect. No unchanged attempt was relaunched.

Candidate09 selection evidence is in `runtime09-brief.json`; all 34 corruption controls were
rejected in `verifier-controls-candidate09.json`.

The same package passed the normal-flow regression in `normal-flow09-brief.json`:
six real browser joins, three standalone recoveries, 80 matching roster rows, 11 local-layout
checks, distinct advertised session IDs before and after rehost, and three client exits 0.
Both normal 60-second selection timeouts completed. The first match reached results without
the fixture invoking EndRound. This is natural bot-driven completion, not physical human combat proof.
`normal-uniqueness09-brief.json` requires all 16 team snapshots across the two matches to be
unique and rejects three additional corruption controls. The unchanged browser verifier
retains its earlier 29-control coverage.

Warning counts are preserved: selection host/clients 233/28/29/33; normal-flow host/clients
387/40/42/41. `normal09-warning-inventory.json` retains host signatures, including diagnostic
hero initialization and 36 invalid message-listener unregister warnings. Those warnings remain
polish work; zero runtime errors does not mean zero warnings.

Next, verify the packaged 2D relic-request indicator, including late join and team changes.
Keep physical input, visual polish, second-PC transport, clean-device distribution, and the
other vertical-slice gates open. Reuse candidate09 only for behavior covered by its frozen inputs;
new development fixture code requires a new game build and candidate.
