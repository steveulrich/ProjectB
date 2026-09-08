# Connected frontend return

September 8, 2026. Normal Return to Lobby now uses server travel in PIE as well as packaged games. It preserves the actual listen port and clears the host's inherited match options before constructing the frontend URL. Connected clients follow the host; real host loss still uses network-failure recovery.

## Cause and change

The mixed 0805 run returned all four views to the frontend, but PIE's `ABwayGameState::ReturnToFrontEnd` called `OpenLevel`. That disconnected its clients, which recovered through `FailureReceived`, `ConnectionLost`, and `?closed`. The PIE exception had avoided an earlier reconnect-to-wrong-port problem rather than implementing connected return.

`BwayGameState.cpp` now uses a relative, nonseamless `ServerTravel` for every world type. It starts with clean host `LastURL` options and takes the port from the live net driver's local address. Unreal reparses travel against `LastURL`; a default or stale port would send peers to the wrong listener. On rejection, the previous base URL and pending flag are restored. Duplicate/pending travel is rejected; bot and session cleanup and results dismissal happen after travel is accepted. The frontend still uses its canonical Lyra experience and listen-server mode when appropriate.

This does not globally suppress network failures. Native client travel remains relative and can inherit client-local URL options; server options are cleared. The server's frontend experience and the client's subsequent staging/reselection were verified. Treat further client URL normalization as a separate change if a concrete defect requires it.

## Network contract

| Owner | Responsibility |
|---|---|
| Owning player's controller | Existing results widget submits the return intent through `Server_RequestReturnToFrontEnd`; server validates PostMatch. |
| Authoritative `ABwayGameState` | Selects the frontend, rejects pending travel, normalizes the host URL, requests travel, then cleans up. |
| Engine world/net driver | Existing client-travel transport notifies connected controllers and reconnects them to the actual listener. |
| Each frontend game instance | Existing frontend flow cleans up session state; hard disconnect also resets user state. |

No new replicated field, RPC, prediction system, relevance policy, dormancy rule, or periodic traffic was introduced. This is an infrequent server-owned world transition. Full adverse-network, second-PC and performance acceptance remain open; this change makes no bandwidth or latency improvement claim.

## Verification

One LyraEditor compile gate passed in **67.11 seconds**. Full build log: `Saved/Logs/codex-frontend-travel-build-20260908.log`. Source base was `30785fbc` plus the recorded project/engine working state and this one-file change; the tested diff is retained at `Saved/Logs/frontend-travel-20260908/source.patch`.

| Test | Observed result |
|---|---|
| Two-peer same-process PIE, actual port 17777 | Two humans/six bots completed a natural 1–0 match. A physical remote Return to Lobby click reached two frontend worlds on port 17777: exactly one authority, two humans on each peer, no bots. No network-failure event. |
| Rebuilt PIE host plus separate packaged 0805 client, port 7777 | Physical frontend LAN discovery/join, normal initial timeout selections (Rawlins/Alona), natural 0–3 in three rounds, physical client Return to Lobby. The client received normal travel, reconnected to the frontend, and remained in the host's two-human/zero-bot roster. No `?closed` recovery or error lines before deliberate host shutdown. |
| Connected frontend rehost | Host physically started another LAN game. Packaged client followed automatically into staging; both manually locked before timeout (host Argus, client Korryn). The next match showed 500/335 health and six populated slots. This match was deliberately stopped after its first round; its completion is not claimed. |
| Actual host disappearance | At the recorded checkpoint, packaged client error count was zero. Stopping PIE during the rehosted match produced `FailureReceived` and `ConnectionLost` at 18:15:11.645–646 UTC, then `?closed` frontend recovery at 18:15:11.652. Real failure reporting/recovery was preserved. |

Same-process details: results at 18:01:10.521 UTC; accepted return at 18:02:41.032; host reopened port 17777 at 18:02:45.104 and client was welcomed at 18:02:45.721. `frontend-connected.json` passed. The earlier `frontend.json` is a retained false observation before a return request was dispatched; the initial click had only focused the control. A fresh observation and click produced the request. `playing.json` was captured after natural results despite its filename; it proves the pre-return roster, not an in-progress phase.

Mixed details: first results at 18:10:29.110 UTC; accepted return at 18:11:04.462; host reopened port 7777 at 18:11:08.524; client welcomed into frontend at 18:11:09.595. Rehost selection completed/travelled at 18:14:16.252, with new kits observed. `mixed-frontend.json` observes only the editor host; packaged client evidence comes from its rendered view and separate log.

The reused client executable was `Saved/StagedBuilds/LAN-20260908-05/Windows/ProjectB/Binaries/Win64/LyraGame.exe`, SHA-256 rechecked as `E22A2AB67F24390DA633B8261B8B84A4A465B3C7707113632A40BD44B7C7E486`. It does not contain the new host-side implementation; it was used only as a client, with no protocol change. A newly packaged host and subsequent final four-player/second-PC acceptance remain required.

## Evidence and cleanup

`Saved/Logs/frontend-travel-20260908/` contains settings baseline, source patch, roster/map observations, `before-deliberate-host-stop.json`, and `flow-brief.json`. The helper is `Saved/verify-frontend-travel-20260908.py`; all runtime wrappers are function-local and only serialized data is returned. Full runtime logs:

- `Saved/Logs/codex-frontend-travel-editor-20260908.log`
- `Saved/Logs/codex-frontend-travel-client-20260908.log`

Editor PID 77764 and client PID 78524 both exited **OS code 0** through retained process handles; monitor sessions 35782 and 63939 finished. Log exits were 18:16:02.789 and 18:15:33.495 respectively. PIDs were absent. PIE stopped, no dirty assets existed, and DevMap was restored. CDO restoration again did not persist client-count/process preferences, so the closed editor's two exact INI entries were restored and verified: four clients, one process, listen server, empty extra options, full-flow false, DevMap.

Error classification: editor has three known PixelStreaming startup/PIE error lines; packaged client has exactly two network errors, both after the deliberate host-stop checkpoint; zero critical lines. No failure handlers were changed. Overhead: one build, one editor, two PIE runs, one packaged client, no package. No duplicate process launches or build retries.

Invalidation: repeat affected evidence after session/travel/URL/selection/frontend changes. Remaining polish includes invalid-handle unregister warnings, intrusive material/debug messages, bot crowding behind results, frontend presentation, and measured four-instance GPU-memory pressure. BF-115 in the local bug-fixer catalog records the cause and prevention.
