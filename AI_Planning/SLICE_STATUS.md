# Vertical Slice Status Ledger

Compact restart point. Detailed evidence and failed attempts remain in [packaged acceptance](./PACKAGED_LAN_ACCEPTANCE.md) and `Saved/Logs/`.

**Latest verified gate:** footstep fix `bcec5a3e` passed four-peer NullRHI and rendered natural matches. NullRHI observed up to 21 footstep actors with no Niagara components or Blueprint errors. Rendering observed up to 24 active components, 338 trigger changes on reused components, and complete actor cleanup after results. Cold semantic graph verification and Blueprint validation passed. Earlier 0803 manual rematch/shared frontend-return acceptance remains valid for its recorded scope.

**Current candidate:** `LAN-20260908-04`

**Current work:** goal active. Candidate 0804 completed one BuildCookRun in 179.04 seconds, exit 0; session 91103 is finished. Its packaged headless match reached natural 0–2 results at 16:15:22 UTC. The test-exit trigger requested status 0; OS exit capture is unavailable, and PID 66024 is absent. No editor/game/build remains from this work. Rendered LAN acceptance is next. On-disk settings: four clients, one process, listen server, empty extra options; DevMap restored.

| Gate | Status | Evidence / next action |
|---|---|---|
| Hero selection and roster | Manual rematch passed in two packaged processes | Korryn/Alona locked before timeout, one travel, correct kits; invalid/repeated locks remain open |
| Full PIE core loop | Natural matches and rematches passed | Final clean run: Argus initial 1–0, Korryn rematch 1–0. Purchased-upgrade reset remains open |
| Packaged build/cook/stage | 0804 passed | Source `bcec5a3e`; prefix `Saved/Logs/codex-packaged-LAN-20260908-04`; one attempt, exit 0, 179.04 seconds; packaged headless natural loop passed |
| Packaged late join + round reset | Valid choices and PostMatch snapshot delivery passed | 0802 PostMatch arrival at 07:47:27: pawn=None, resultsSent=1, unchanged eight-column breakdown; HUD stays hidden. Invalid/repeated locks and selection-time disconnect remain open |
| Individual selection timer / round-summary overlap | Passed in PIE and candidate 04 | Active countdown shown; individual inactive countdown absent; PostRound overlap suppressed/restored |
| Six-slot gameplay HUD | Passed | 0802 timeout-fallback rematch restored populated Argus and Rawlins kits |
| Duplicate frontend HUD entries | Source fix packaged earlier | Interactive visual verification pending |
| Hero-card presentation | Earlier fixtures plus two packaged manual reselection cycles passed | Both players can change and lock; initial remote preview can show old portrait with generic details. Gamepad/presentation polish remain |
| Results identity and portrait data | Fixtures and packaged visual samples passed | 0802 first match/arrival and rematch had matching columns, totals, portraits, goal credits and one MVP; full packaged field comparison remains |
| Final-results snapshot transport | Four-peer fixture/natural transitions plus actual packaged arrival passed | Earlier lag/loss with forced live-stat divergence passed; final LAN acceptance still required |
| DevMap navigation / personal goal credit | Fixed and verified | Natural scoring, cold map load, round resets, goal ownership and release/reset probes passed |
| Packaged match completion | Natural first match and manual rematch passed | 0803: 1–3 first match, 0–3 rematch; matching rendered results on both peers |
| Frontend return / second match | 0803 shared return, manual selection, natural completion passed | Client followed automatically; Argus500/Rawlins400, six slots, matching 0–3 results. Disconnected rediscovery remains open |
| Keyboard + gamepad acceptance | Pending | Physical all-kit actions once packaged loop is stable |
| Four-human / second-PC LAN | Pending | Final target: one PIE host plus three separate instances; also clean-device LAN |
| Waiting banner / phase lifecycle | Fixed; state and rendered results checks passed | `f50104ec`; zero/nonzero PostRound and round-two input restored; rendered four-peer host/Client 1 results have no waiting banner. 0804 packaged verification remains |
| Explicit bot-count override | Fixed in source; PIE and cold asset checks passed | `70bea97c`; one modern manager, no legacy manager, exact seven/zero counts. Four-player/four-bot client rosters agree after natural match |
| Footstep effect lifecycle | Fixed; NullRHI/rendered/cold checks passed | `bcec5a3e`; optional spawn guarded, valid components reused, cleanup complete. Packaged and final VFX appearance remain |
| Placeholder asset integration / visual polish | Pending | Mesh/animation contract, sample menus/debug text, performance budget remain |

**Next action:** launch 0804 through the LAN frontend, verify both peers' natural results without the waiting banner, and continue affected full-flow checks toward the required four-player topology. Retain GUI process handles at launch for exit-code capture. Preserve second-PC LAN, disconnected rediscovery, physical all-kit inputs, and placeholder integration gates. Do not repeat old 0803 checks without an invalidation reason.

**Hero-reselection source verification:** one C++ build passed in 39.98 seconds; `BP_BW_GameState` and final `WBP_BW_HeroSelect` validate with zero errors/warnings. `codex-rematch-selection-clean-20260908.log`: initial match 13:54:26, staging 13:54:55, physical Korryn lock/travel 13:55:20, 335 health/six slots, natural rematch 13:55:42. One-peer PIE scope only. No ensure/assertion/fatal/Accessed None in the clean log; known PixelStreaming2 startup error is separate.

**Hero-reselection attempt accounting:** reused 0802 for one two-process test; then one source build, three editor launches, four one-peer PIE runs, one 0803 package attempt. Earlier incomplete unlock handling and a missing Blueprint override flag were corrected. A failed Python probe retained world wrappers and crashed editor 10892; no presentation-fixture pass is claimed. Full causes/retry changes are documented in packaged acceptance and BF-107–109.

**Candidate 0804 identity:** source/asset commit `bcec5a3e`; executable SHA-256 `66FFCC527AAA53FE5541D7361C24F8959EE9DAC21AC975423A1E8776F39216C4`, matched to the manifest. 131 staged files / 4,257,298,125 bytes including local debug files. Packaging captures project/engine HEAD, complete `git diff HEAD --binary`, status, untracked chunk-layer input, exit code, and hashes. HEAD alone is not artifact identity. Older candidate records remain in packaged acceptance.

**Settings / ownership:** four clients, one process, listen server, empty extra server options verified on disk after editor exit. Reflected setters alone had not persisted the empty URL; always check the saved INI after test shutdown. No dirty assets at exit. Preserve unrelated planning/docs/bridge/project changes. No subagents used.

**Invalidate relevant results:** selection/ownership/session changes require affected multiplayer checks; HUD registration requires visual checks; bot/round changes require another natural loop. Final end-to-end LAN acceptance remains required.

**Resume efficiently:** ledger → `Scripts/Test/Get-ResumeBrief.ps1` once → relevant evidence → one gate. Keep runtime Python wrappers function-local and return serialized values; release them on exceptions before travel. Follow existing process sessions; do not launch duplicate builds or rerun unchanged failed probes.

**Compact flow helper:** `Scripts/Test/Get-PackagedFlowBrief.ps1` summarizes whole-file selection/travel/results events and runtime error lines. Known 0802 counts, prior crash detection, bounded output, and live-host reading passed. It is an inventory, not an acceptance verdict. Use it to avoid repeated broad log searches.

**Phase-fix evidence / overhead:** `codex-phase-cleanup-final-editor-20260908.log`, `codex-phase-cleanup-final-{zero,two}-second.json`, and before/after exports under `Saved/Logs/waiting-phase-audit-20260908*`. One read-only export commandlet, two builds (65.45/25.84 seconds), two hidden editor launches, four PIE runs, no package. First observer alias failure and the first implementation's missing round-two native GAS transition are retained in acceptance history. Final checks and shutdown had zero ensure/assertion/fatal/Accessed None/Python/PIE error lines. BF-111 documents prevention.

**Additional audit leads:** first full-flow logs also show a legacy Playing phase before native Warmup, and postmatch snapshots contain repeated relic-carrier effect entries. Inspect ownership/cleanup before claiming defects fixed; these were not changed by f50104ec.

**Bot-fix evidence / overhead:** `codex-bot-ownership-{dev-seven-recheck,dev-zero,capture-four-peers}.json`, editor log, and `bot-ownership-20260908/{before,after,cold-verification}.json`. One hidden editor, four PIE runs including an observer API-name failure, one cold commandlet, zero C++ builds/packages. All 10,415 Blueprint error lines were the separate footstep failure; no legacy AI-controller errors or ensure/assertion/fatal. BF-112 records prevention.

**0803 manual-flow evidence / overhead:** reused candidate and host, one client launch, zero new builds/packages. Staging starts at 15:37:30.685 and 15:42:27.902 UTC; manual locks/travels at 15:38:27.204 and 15:43:23.619, before each deadline. Natural results at 15:39:52.343 and 15:45:33.933; shared frontend return at 15:40:27.147. `codex-packaged0803-manual-flow-20260908.json`: three host starts/travels, three results per peer, both normal exits, zero error/critical lines. Full UI observations in packaged acceptance. Scope: two rendered processes on one PC; waiting-banner/mesh/debug issues remain visible in this older artifact.

**Footstep evidence / overhead:** `footstep-audit-20260908/` exports and semantic graph diff; `codex-footstep-{nullrhi-fixed,rendered-fixed,rendered-cleanup}-20260908.json`; runtime logs and log summary. Three editor launches (one failed for sandbox cache access), two four-peer PIE runs, no separate editor rebuild, one new package and one packaged headless run. Eight pre-test Python error lines record protected graph access; native bridge operations completed the fix. Both successful editors exited normally; callbacks and actors cleaned up. BF-113 updated. Full causes, scope, and attempt records are in packaged acceptance.

**0804 headless scope:** `codex-packaged0804-headless-20260908.log` and compact flow JSON: zero runtime error/critical lines, two natural rounds, real PostMatch trigger. Direct map, NullRHI, no rendered/physical/network acceptance. The test-exit request is recorded; actual OS exit code was not captured. This does not pass graceful frontend return.
