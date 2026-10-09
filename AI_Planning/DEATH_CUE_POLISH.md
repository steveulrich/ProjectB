# Death cue and visible-body VFX

Updated September 20, 2026. Gate: `Saved/Logs/gcnl-death-audit/gate-contract.json`.

## Changes and causes

- `GCNL_Death` used the optional `SpawnSystemAtLocation` result directly in `SetColorParameter`. The engine returns no Niagara component when rendering is unavailable. A validity branch now guards color initialization.
- The same asset added an unchecked `SpawnSystemAttached` result to `SpawnedEffects`, then configured and activated it. A second validity branch now guards that entire initialization chain. Failed spawns never enter the retained array. Successful effects keep their existing parameters, activation, timeline, and cleanup.
- `ApplyHeroVisuals` made the main Manny/Quinn mesh visible but retained Lyra's `ignore` component tag. `BPML_VFXMacros.IgnoreSkeletalMeshTag` therefore skipped the only visible skeletal mesh. Assigning a hero-owned visible mesh now removes that tag. Secondary hidden meshes and the no-custom-mesh path are unchanged.
- The editor rebuild exposed three existing incompatible JSON-key conversions in the bridge's foliage and level handlers. Explicit `FString` construction restores compatibility with this source-engine branch. The original changes remain captured in the pre-fix source snapshot.
- The apparent authority-only normal-death cue was a fixture artifact. Direct Python `DamageSelfDestruct` executes under `FEditorScriptExecutionGuard`; `AActor::GetFunctionCallspace` returns Local while that guard is active. The existing native delayed `bway.Test.DamageRemotePlayer` fixture ran with `guard=0`, sent three cue RPCs, and delivered the normal effect to every client. No production RPC workaround was needed.

## Evidence and limits

All paths below are relative to `Saved/Logs/gcnl-death-audit/`.

| Check | Result | Evidence |
|---|---|---|
| Blueprint compile/validation | Zero errors and warnings | Native bridge result; `structural-validation.json` |
| Graph comparison | Four added guard nodes; only the two spawn nodes and their immediate consumers changed links; no other parsed graph nodes changed | `graph-before.json`, `graph-after.json`, `graph-diff.json` |
| Editor build | Passed, exit 0, empty `failureSignals`; runtime and bridge DLL timestamps advanced | `compile-editor-20260920-r2.json` |
| NullRHI graph execution | Both kill-plane and normal paths passed; actual Niagara spawn returned None; retained array stayed empty; zero Accessed None, Python errors, or critical lines | `nullrhi-runtime-r3.json`, `nullrhi-process-r3.json`, `nullrhi-20260920-r3.log` |
| Rendered kill-plane effect | Four PIE worlds; 175 samples, 83 active electric-effect samples, matching team colors, no remaining electric components at final sample | `rendered-killplane-brief.json`; collected before the visible-body C++ change |
| Rendered normal effect after visible-body change | Authority-side production self-destruct confirmed health 500 to 0; 28 active effect samples, correct team color and two-second duration, retained array cleared | `rendered-normal-host-brief.json` |
| Four playable humans | One authority plus three clients, four controlled human pawns, eight pawns per world including bots, zero inherited ignore tags across 32 meshes | `four-player-selection-preflight.json`, `four-player-runtime-preflight.json` |
| Earlier Python-triggered normal death | Fixture limitation: 416 samples showed authority-only effects because the Python script guard forced local RPC callspace. Retained for diagnosis; does not establish a production replication failure | `rendered-normal-fourplayers-brief.json`, `normal-death-transport-trace-r3.json` |
| Native normal death across four peers | **Passed.** 194 samples; 29 target-correlated active effect samples per world. All four peers observed zero health and dying state. Three cue RPCs sent and received; correct team color/duration; zero retained nulls or remaining effects; zero trace errors | `native-normal-r1-correlated-brief.json`, `native-normal-r1.json` |
| Final-source KillZ across four peers | **Passed.** 208 samples; 22 target-correlated active electric effect samples per world. All four peers observed zero health and dying state. Three cue RPCs sent and received; correct color; all effects cleaned up; zero trace errors | `native-kill_plane-r1-brief.json`, `native-kill_plane-r1.json` |
| Game target and fresh package | **Passed.** LyraGame exit 0, empty failure signals, 39.43 seconds. Candidate `LAN-20260920-01` Cook/Stage/Pak exit 0, 94.07 seconds, 131 hashed staged files | `compile-game-20260920.json`, `package-LAN-20260920-01-result.json` |
| Packaged death regression and remote score | **Passed within scope.** Four NullRHI processes on one PC with 200 ms simulated latency; all remote damage/pass/goal/score/completion markers present. One production death ability activated and ended; zero Blueprint null-spawn warnings, errors, or critical lines across all peers. Each client emitted exactly one actual Results marker and one test-exit marker, then exited 0. Host teardown was separate | `packaged-death-regression-brief.json`, `packaged-remote-score-brief.json`, `packaged-flow-brief.json` |

The headless check calls the production `OnBurst` graph directly in an editor world. It does not prove multiplayer delivery or timed cleanup. The rendered checks use same-process PIE and do not prove physical controls, packaged behavior, LAN, or second-PC acceptance.

The rebuilt editor log retains three Pixel Streaming target-window errors (`GEngine is not valid`). These remain actionable for rendered/UI acceptance even though the generic delta helper categorized them as known startup errors. Component samples prove the stated Niagara parameter/lifetime scope, not a clean visual or UI gate. No Accessed None or critical lines appeared in that editor log.

The current staged candidate is `LAN-20260920-01`. Its frozen source snapshot is `source-snapshot-LAN-20260920-01.json`; the package result links the complete staged SHA-256 manifest. Candidate 13 remains frozen historical evidence and does not contain these changes. The new packaged run proves the affected headless regression, not rendered death presentation or full LAN acceptance.

## Next investigation

The selected source/runtime/package regression has passed. Keep rendered/UI acceptance open and proceed with the independent packaged rematch economy lifecycle gate. Its existing `BwayMatchUpgradeLifecycleTestCommands.cpp` resolver accepts only PIE worlds; extend the development fixture deliberately for packaged game worlds and startup readiness before trying it in the package. Require a real purchase and nonzero stats before production rematch, then prove reset and destination arrival for the host and three clients. A server-only reset verdict does not prove client state.

For future gameplay RPC fixtures, schedule production actions on a native timer/ticker after the editor Python call returns. Use Python for observation and dispatch, and require a native `guard=0` marker when validating this route. `Saved/verify-death-native-20260920.py` and `Saved/summarize-native-death.ps1` preserve the successful bounded observer and target-correlated extraction. The earlier unsupported protected ASC property reads are setup failures, not gameplay evidence.

For the next PIE attempt, wait for four human controllers and their PlayerStates before auto-locking. After selection, require four controlled human pawns and the expected pawn counts on all peers. Calling auto-lock immediately after PIE starts can advance the host before remote players arrive; an earlier five-pawn run therefore proved only authority presentation.

Keep physical buildable confirmation, normal human selection, full LAN flow, second-PC, and clean-device gates open.

## Attempts and cleanup

- Three builds: two editor builds (the first failed on the three bridge conversions; the second passed), followed by one passing LyraGame build.
- Three editor launches, six PIE lifecycles, one package, and one four-process packaged regression. The sixth PIE lifecycle replaced the Python-tainted host death state, then ran both successful native-path checks without another launch.
- Three headless commandlets: editor placement rejected the non-placeable cue; the next attempt identified a Blueprint-internal spawn API unavailable as a generated Python method; the reflected `call_method` path passed. Use that final script rather than rediscovering the API.
- Five rendered observers: the first retained valid KillZ evidence before a normal-probe API failure; a damage probe did not establish death; self-destruct exposed the ignored visible mesh; the rebuilt mesh passed locally; the final four-human run left remote presentation unverified. Every observer unregisters on completion/error and stores only serialized state between ticks.
- No PIE settings or production test flags were changed. PIE teardown owns fixture worlds. The first editor's bridge shutdown reported forced termination after a graceful-close timeout; its exact PID was verified absent before compilation. The final editor used `SystemLibrary.quit_editor` after zero PIE worlds and zero dirty content/maps were confirmed.
- The transport investigation added three observer attempts (two failed before triggering because protected ASC properties were unavailable), followed by two successful native-path observers. Each callback unregistered on exit, and RPC/log verbosity was restored. Editor PID 70328 closed normally with zero dirty packages before the game-target compile; see `native-runtime-cleanup.json`.
- Current source provenance: `source-snapshot-current.json`. Preserve the pre-fix exports and failed attempts. Runtime evidence is invalidated by changes to the cue, hero visual ownership, death ability/ASC dispatch, fixture prerequisites, or relevant engine behavior.
