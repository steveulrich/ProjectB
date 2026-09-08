# Relic request indicator: lifecycle verification and team discrepancy

September 8, 2026. Request activation, visibility under Lyra's team rules, expiry, death cleanup and a fresh request after automatic respawn now pass in three-peer PIE. **Complete team correctness remains unproven:** Lyra teams and the match roster disagree about membership. Candidate `LAN-20260908-05` predates the request changes and the connected-frontend host fix.

## Current lifecycle implementation

The request effect CDO grants `Gameplay.Relic.Requesting` for three seconds; `DA_BW_RelicSettings_Default.RequestingTag` agrees. The old Blueprint listened for `State.RequestingRelic`, which explains its failure even on authority. It also bound only during server possession, lacked removal handling, and contained an unused event that compared the same pawn's team to itself.

`UBwayRelicRequestIndicatorComponent`, created by the native character, now resolves the tag from match relic settings and binds when the pawn extension initializes its ASC. It reads current state immediately and responds to both tag addition and removal. A local viewer must be a different PlayerState on the same valid Lyra team. Target-team, viewer-team and local-controller changes refresh that policy. Death hides the marker. ASC uninitialization and EndPlay release tag/team/controller bindings. There is no presentation tick or new RPC.

The existing `RelicRequestWidgetComponent` remains the art surface, referenced through an editable `FComponentReference`. No new AttributeSets, attributes, effects or duration/stacking rules were introduced. The three-second effect still comes from the existing ability and match data. Authority removes request effects when the current pawn dies or releases its persistent PlayerState ASC, preventing an old request from carrying to a new avatar. Presentation code does not apply effects.

Sixteen obsolete Blueprint nodes were removed from `B_BW_Character_Base`; all 51 surviving graph nodes are identical in the before/after structured read. The base and humanoid Blueprints validate after a cold load with zero errors and warnings.

## Lifecycle runtime evidence

One editor build passed in **64.30 seconds**. Three same-process PIE peers used three humans, zero bots, Argus and a long test round. Nine native indicator components were present, one per replicated pawn. Physical RMB was used for requests; the observers did not issue client gameplay RPCs.

| Check | Evidence |
|---|---|
| Activation, visibility and expiry | 3,744 samples, 66 containing active requests; zero observer errors or disagreements with replicated request state and Lyra team/self policy |
| Three-second natural expiry | Client request at 19:11:47.288 UTC; authoritative tag visible in sampling at 19:11:47.491, removed by 19:11:50.380; teammate marker cleared by 19:11:50.540 |
| Rendered request | Hand marker visibly appeared over the requesting teammate in the foreground Client 1 viewport; no forced visibility fixture was used for this check |
| Death before normal expiry | Real request at 19:15:32.821/.880, then a one-shot authority KillZ teleport at 19:15:32.895; recorded death at 19:15:32.901; replicated request and marker cleared by 19:15:33.218 |
| Automatic respawn | Normal game code called RestartPlayer at 19:15:36.251; a new pawn with zero request count and hidden marker was sampled on all three peers at 19:15:36.364 |
| Request after replacement | Physical request at 19:20:14.883/.943; 437 additional samples, 20 active, zero visibility mismatches/errors; teammate hand rendered again and all current widgets were hidden after expiry |

The death fixture only teleported the authoritative pawn below KillZ. Its intended fallback RestartPlayer call never ran: normal gameplay replaced the pawn between observer ticks, without exposing the null-pawn interval that the fixture expected. The fixture's stale `awaiting_unpossess` result is retained. The runtime log and pawn-identity/tag transitions, rather than that fixture status, prove death and automatic replacement. A future fixture should also detect a changed pawn identity.

Scope: three rendered peers in one editor process, physical mouse request, one automatic replacement. Packaged-host/four-player/second-PC, late-join-active-request, live team-change and gamepad acceptance remain open. Request policy currently follows Lyra's team subsystem; the contradictory match roster below prevents a full teammate-correctness pass.

## Priority follow-up: one source of team membership

`team-membership-snapshot.json` records the same disagreement in all three worlds:

| Player | Lyra team ID | Match team index |
|---|---|---|
| Host | 1 | 0 |
| Client 1 | 2 | 1 |
| Client 2 / requester | 2 | 0 |

This is not just zero-based versus one-based numbering. Host/Client 2 are together only in the match roster; Client 1/Client 2 are together only in Lyra. Each replica has two contradictory pairwise relationships. The visible Client 1 roster also showed one friendly and two enemies while Lyra reported two players on team 2. Inspect `ABwayGameState::AddPlayerToTeam`, assignment callers, and Lyra team creation; derive the match roster from the authoritative team system or synchronize through one owner. Recheck damage/passing eligibility, HUD, scoring/alive counts, respawn, and complete multiplayer flow after correcting it.

Death also produced three invalid MID-parent warnings, one on each replica of the old pawn, at 19:15:32.936–19:15:33.060. The earlier team-style material fix remains verified for its own path; another death-time material owner needs tracing. No current pawn retained a request after respawn. Do not call all material warnings resolved.

Lifecycle evidence: `Saved/Logs/request-lifecycle-20260908/` contains effect/settings exports, graph before/after, two observation files, `requester-state-transitions.json`, `post-respawn-expired.json`, `team-membership-snapshot.json` and `verification-brief.json`. The compact `lifecycle-brief.json` contains the last, post-respawn observation only; use the combined verification brief for both phases. Helpers: `Saved/verify-request-lifecycle-20260908.py` and `Saved/request-lifecycle-death-fixture-20260908.py`, plus the earlier material reader they import.

Overhead for lifecycle work: one build, two editor launches, one PIE run, no package. Audit editor 52412 and fixed editor 70228 exited OS code 0; monitors 7463/81081 completed. All callbacks stopped, no dirty content assets, and four-client/one-process/listen-server/empty-options/full-flow=false settings were restored and verified in the closed-editor INI. Preserve 37 unrelated dirty entries. BF-117 records the prevention rule. The audit had one unsupported Python superclass query; the runtime had one unmatched-parenthesis probe (three traceback lines), corrected before the fixture was armed. Runtime also has two unrelated PixelStreaming2 startup errors, the three death-material warnings, and zero fatal/assertion/ensure/Accessed None lines. A sandbox Git LFS diff failed to create its signal pipe; use normal reviewed Git access for the targeted binary-asset commit.

## Earlier renderer defect and change

`ULyraTeamDisplayAsset::ApplyToActor` visits mesh components, including `UWidgetComponent`. The old generic mesh path created an MID and installed it through the widget's virtual `SetMaterial`. `UWidgetComponent` then created its own renderer MID from that override, producing an invalid MID parent and disrupting the widget's render texture. The old warning is recorded in `Saved/Logs/codex-frontend-travel-client-20260908.log`.

`Source/LyraGame/Teams/LyraTeamDisplayAsset.cpp` now recognizes widget components, calls `UpdateWidget`, and applies team parameters to their existing renderer MID through `ApplyToMaterial`. Normal mesh behavior stays in the existing path. No assets, RPCs, input mappings, or material content changed.

Keep world-space widgets' base material slots as a Material or Material Instance Constant. Apply runtime scalar, color, and texture parameters through the renderer MID. Verify the `SlateUI` texture and rendered output in addition to checking warning counts. The placeholder widget currently uses `Widget3DPassThrough`, a soft-glow control-ring material, and the hand icon from ControlRigModules. The plugin descriptor supports content and has no editor-only module restriction; the icon's final cooked/drop-in-art acceptance remains open.

## Earlier renderer verification

One `LyraEditor` build passed in 50.08 seconds. A fresh editor ran three same-process PIE peers, three humans and zero bots, on DevMap. The long round was an explicit test override, not a gameplay tuning change.

| Check | Result |
|---|---|
| Initial replicated widgets | Nine widgets; every base slot and renderer parent was a Material; every renderer MID existed |
| Parameter fixture | Five style applications per widget; scalar, color, and texture values reached the existing MID on all nine |
| Normal team callback | Three calls per pawn; renderer MID and base slot stayed stable on all nine |
| Render texture | With fixture visibility enabled, all nine `SlateUI` parameters matched their actual non-null render targets |
| Visual output | Hand markers rendered above the remote characters in the observed game view |
| Cleanup | Original material parameters restored; all nine widgets hidden; observer unregistered; no dirty content assets |
| Invalid dynamic-parent warning | Zero in the fixed runtime log |
| Critical failures | Zero fatal/assertion/ensure/Accessed None lines |

This is a renderer and team-style fixture pass. Forced visibility does not prove request gameplay, team filtering, duration, respawn cleanup, packaged operation, or second-PC LAN behavior.

## Historical request visibility failure, before the lifecycle fix

Physical RMB activation produced successful predicted/server request log pairs, including 18:41:30.965 and 18:41:31.031 UTC. An extended observer covered 18:41:07–18:45:56 UTC: 1,770 samples, nine widgets per sample, zero observer errors, and **zero visible markers**. This was before the visibility fixture. The first shorter observer ended too early to establish the request result; retain its evidence without calling it a pass.

The existing base-character Blueprint has several concrete lifecycle problems to address:

- `Event Possessed` → authority branch → experience-ready wait → `WaitGameplayTagAddToActor(State.RequestingRelic)` → visible=true. Remote proxies do not run this possession path.
- No tag-removal listener clears visibility after the request ends.
- A separate `HandleRelicRequestVisibilityChange` event is not connected to the above listener. Its team comparison reads the same pawn's PlayerState on both sides, so it does not establish viewer-team eligibility.

These findings do not yet explain every authority-side observation. Inspect the actual request effect CDO's granted tag/duration and the pawn ASC initialization order before replacing the binding. `ALyraCharacter::PossessedBy` calls the parent before the pawn-extension controller-change handler, so experience readiness alone is not evidence of ASC readiness. The cue Blueprint has empty application/removal event graphs.

The native lifecycle fix above supersedes this missing-marker failure. Team membership consistency and broader deployment/input checks remain open.

## Evidence and attempt accounting

- Detailed JSON, baseline asset exports and source patch: `Saved/Logs/request-widget-20260908/`.
- Compact verdict: `verification-brief.json`; material fixture: `team-style-fixture.json`; rendered textures: `renderer-fixture.json`; restored visibility: `fixture-restored.json`.
- Request observations: `physical-request-observation.json` (short/incomplete) and `physical-request-observation-extended.json` (failure reproduced).
- Reusable local diagnostic functions: `Saved/verify-request-widget-20260908.py`. They retain no PIE object wrappers in globals and do not drive gameplay RPCs. Do not rerun configuration over the existing baseline file.
- Build: `Saved/Logs/codex-request-widget-build-20260908.log`.
- Read-only audit editor: `Saved/Logs/codex-request-widget-editor-20260908.log`, PID 51292, actual exit 0.
- Fixed editor: `Saved/Logs/codex-request-widget-fixed-editor-20260908.log`, PID 38208, actual exit 0; monitor 83264 complete. Both PIDs were absent after shutdown.

Overhead: one build, two editor launches (audit and fixed), one three-peer PIE run, no new package. One material probe initially omitted the Blueprint team callback's required arguments; it restored its temporary material parameters in `finally` and was corrected. Its four Python traceback lines remain in the log. Setting transient style properties with normal editor notifications also emitted 12 null-world warnings; the corrected fixture uses `PropertyAccessChangeNotifyMode.NEVER`. Two unrelated PixelStreaming2 startup error lines remain. No invalid-material-parent warning appeared in either fixture attempt.

The reflected preference restore did not persist client count or extra server options. After normal editor exit, those exact INI fields were restored to four clients and empty options; listen-server mode, one-process mode, and full-flow=false were retained. Preserve the 37 unrelated dirty workspace entries. BF-116 records prevention in the local bug-fixer catalog. Repackage only after coherent affected fixes are ready; keep the final four-player, packaged-host and second-PC gates open.
