# Team assignment consistency

September 8, 2026. The reproduced Lyra/match-roster disagreement is fixed in source and verified in PIE. This passes team identity, observed HUD roster sizes, request visibility policy, respawn counts, late arrival and disconnect cleanup. It does not pass all combat, passing, scoring, packaged or second-PC gameplay checks.

## Cause and implementation

Breakaway assigns a player through `ABwayGameState::AddPlayerToTeam` before `Super::PostLogin` can start the pawn. That writes both `Teams[0/1].TeamMembers` and the PlayerState's Lyra team ID `1/2`. Lyra's team-creation component subsequently handles experience loading and `OnGameModePlayerInitialized`. It previously reassigned even players with valid teams, counting the player being reconsidered in its population calculation. The match roster did not follow that reassignment.

`UBwayTeamBridgeComponent` already resynchronizes after experience loading, but that one-time repair does not cover subsequent player-initialization callbacks. In the prior three-player run, the host and requester shared the match team while a different client and requester shared the Lyra team. All three replicas had two contradictory pairwise relationships. See the historical snapshot under `Saved/Logs/request-lifecycle-20260908/team-membership-snapshot.json`.

`ULyraTeamCreationComponent::ServerChooseTeamForPlayer` now preserves a nonspectator's existing assignment when that team is configured in the current experience. Spectators retain the existing NoTeam behavior. Unassigned players still use the least-populated team and lowest-ID tie break. An assignment carried from an experience with different teams is eligible for reassignment; population counting now ignores IDs absent from the current configuration instead of asserting.

Breakaway's existing roster assignment remains the owner of its match teams. Explicit match team changes still use `AddPlayerToTeam`. This change makes generic initialization idempotent; it does not add a second synchronization loop, replicated field, RPC or client-side assignment.

## Acceptance gate and results

Expected result: each active player's Lyra ID equals its match-team index plus one; all pairwise teammate relationships agree on every inspected peer. Initial assignments remain stable through death/respawn, another human's arrival and removal. HUD roster counts and request eligibility agree with that identity.

One build passed in **33.67 seconds**. One listen-server PIE run started three human Argus players with zero bots. A fourth human joined later through a separate `UnrealEditor.exe -game` process using the rebuilt source and uncooked project assets. This was not a packaged client or a second PC.

| Check | Result |
|---|---|
| Initial three peers | Host `979FA0B84B` and requester `73C9AC7F47`: Lyra 1 / match 0. Client `E60915C043`: Lyra 2 / match 1. All replicas agree |
| Continuous observation | 2,192 samples across three PIE worlds; zero identity or pairwise relationship disagreements; zero observer errors |
| HUD | Foreground requester and host showed two friendly / one enemy portraits initially, then two / two after the fourth arrival |
| Real requests | Two physical RMB requests, before and after respawn; 30 active samples; zero visibility-flag disagreements with request/self/team policy |
| Death and automatic respawn | Authority KillZ fixture at 19:40:12.493 UTC; Team 1 alive count 2 → 1 at 19:40:12.512, then 1 → 2 at 19:40:15.833. The normal game restarted the player; fixture did not call RestartPlayer |
| Pawn replacement | Requester changed from authority pawn C_5 to C_6; all three replicas retained Lyra 1 / match 0 |
| Separate-process arrival | Fourth player `A05C45444C` joined at 19:40:49.971; Lyra 2 / match 1, two players per team on all inspected peers, existing assignments unchanged |
| Abrupt disconnect | Disposable fourth client terminated after verification; server removed it at 19:45:27.353. Remaining replicas returned to two / one members and alive counts without reassignment |

Player suffixes above abbreviate the recorded `ShhtevePC-...` names. The requested `?Name=TeamLateJoin` was superseded by the local user identity. The fourth process's own replicated values were not inspected through the editor bridge; arrival and its identity are established on the server and two existing client worlds.

## Remaining visual and gameplay work

The request widget's visibility flag and render target were correct, but a hand was not discernible in the host screenshots during these two requests. Do not reuse the earlier rendered-blue-team hand observation as proof that all viewing directions now work. The current widget is a **one-sided world-space plane** with relative rotation zero, located 175 units above the pawn. Back-face visibility is a plausible next hypothesis, not a verified root cause. Geometry evidence is in `marker-geometry.json`.

Three invalid MID-parent warnings recurred during the old pawn's death. The earlier team-style material fix does not cover this separate death-time path. Nameplate colors also appeared inconsistent with the roster/mesh relationships; inspect viewer styling during the visual follow-up.

Physical damage, pass targeting, goal credit/results, a natural multi-round loop after this change, bots, seamless travel, and spectator/foreign-team edge cases were not independently accepted in this test. Recheck the affected gameplay and complete the final packaged four-player LAN flow. Candidate `LAN-20260908-05` predates this fix and the connected-frontend, widget-material and request-lifecycle source commits.

## Evidence, cleanup and attempt accounting

- Evidence folder: `Saved/Logs/team-assignment-20260908/`; `verification-brief.json`, complete `observations.json`, initial/arrival/disconnect snapshots, `killz-fixture.json`, `disconnect-fixture.json`, `marker-geometry.json` and `log-screen.json`.
- Reusable scoped observer: `Saved/verify-team-assignment-20260908.py`, using the saved request/material readers. Runtime UObject wrappers remain function-local; samples retain serialized values only.
- Build: `Saved/Logs/codex-team-assignment-build-20260908.log`, exit 0.
- Editor: `Saved/Logs/codex-team-assignment-editor-20260908.log`, PID 7548, monitor 76304, normal OS exit 0.
- Separate game client: `Saved/Logs/codex-team-assignment-late-client-20260908.log`, PID 70136, monitor 11990, exit -1 from the deliberate disconnect fixture. It had no targetable window and `CloseMainWindow()` returned false. Verified its exact executable, command line and `-game` mode before terminating only this disposable process. No editor was force-closed.

Overhead: **one build, one editor launch, one PIE run, one separate game-client launch, no package**. No retries of the team fix. A geometry probe used nonexistent `bTwoSided` and produced five Python error lines; it was corrected to `bIsTwoSided`, and traceback references were cleared before stopping PIE. One orchestration syntax error failed before any Unreal call. A sandboxed process-inventory query was denied; normal scoped process access succeeded with escalation.

Editor log: zero fatal/assertion/ensure/Accessed None lines, two known PixelStreaming2 startup errors, the five diagnostic Python lines and three death-time MID warnings. The uncooked `-game` client emitted 58 Python initialization error lines from experimental engine toolset plugins expecting editor-only types (`AgentSkill`, `ToolsetDefinition`, `PythonTestRunner`); no fatal/assertion/ensure/Accessed None. This is not a clean packaged-log pass.

Observer stopped before PIE shutdown. No dirty content/map packages. DevMap retained, editor closed normally, both processes absent. Closed-editor INI restored and verified: listen server, four clients, one process, empty additional server options, full-flow test flag false. Preserve the 37 unrelated dirty entries. BF-118 records the initialization-order prevention rule in the local bug-fixer catalog.

Next gate: request-marker viewing direction and death-material cleanup, followed by the affected full gameplay flow and a new package. Retain the broader vertical-slice acceptance requirements.
