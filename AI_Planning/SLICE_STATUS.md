# Vertical Slice Status Ledger

Compact restart point. Detailed evidence and failed attempts remain in [packaged acceptance](./PACKAGED_LAN_ACCEPTANCE.md) and `Saved/Logs/`.

**Latest verified gate:** candidate 0803 passed two-process manual rematch selection, a natural 0–3 rematch, shared frontend return, and another manually selected natural 0–3 match. Korryn/Alona, then Argus/Rawlins locked before each timeout, travelled once, and received correct health/six-slot kits. Both rendered results matched. Source bot fix `70bea97c` separately passed exact seven/zero counts, four-peer matching rosters, and cold asset verification.

**Current candidate:** `LAN-20260908-03`

**Current work:** goal active. Candidate 0803 host 48272 and client 53320 exited normally after all three natural matches. No test game/editor/build remains from this work. Source fixes f50104ec and 70bea97c are not in 0803. Four-client settings and DevMap remain restored. No new package was built. Next source gate is the footstep effect lifecycle.

| Gate | Status | Evidence / next action |
|---|---|---|
| Hero selection and roster | Manual rematch passed in two packaged processes | Korryn/Alona locked before timeout, one travel, correct kits; invalid/repeated locks remain open |
| Full PIE core loop | Natural matches and rematches passed | Final clean run: Argus initial 1–0, Korryn rematch 1–0. Purchased-upgrade reset remains open |
| Packaged build/cook/stage | Candidate 0803 passed | Source `c5e11663`; prefix `Saved/Logs/codex-packaged-LAN-20260908-03`; one attempt, exit 0, 170.43 seconds |
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
| Waiting banner / phase lifecycle | Fixed in source; headless state checks passed | `f50104ec`; zero/nonzero PostRound and round-two input restored. New packaged/rendered evidence required |
| Explicit bot-count override | Fixed in source; PIE and cold asset checks passed | `70bea97c`; one modern manager, no legacy manager, exact seven/zero counts. Four-player/four-bot client rosters agree after natural match |
| Footstep effect lifecycle | Pending | Four-peer NullRHI Dorado run produced 10,415 B_FootStep NS_FootPrint Accessed None lines; inspect optional Niagara spawn handling |
| Placeholder asset integration / visual polish | Pending | Mesh/animation contract, sample menus/debug text, performance budget remain |

**Next action:** inspect `/Game/Effects/Blueprints/B_FootStep` (`Content/Effects/Blueprints/B_FootStep.uasset`) and its missing NS_FootPrint handling. Reproduce and fix the lifecycle defect, verify affected NullRHI and rendered behavior, then build a candidate containing the newer fixes. Preserve four-human topology, second-PC LAN, disconnected rediscovery, and placeholder integration gates. Do not relaunch 0803 to repeat these passed manual selection checks without an invalidation reason.

**Hero-reselection source verification:** one C++ build passed in 39.98 seconds; `BP_BW_GameState` and final `WBP_BW_HeroSelect` validate with zero errors/warnings. `codex-rematch-selection-clean-20260908.log`: initial match 13:54:26, staging 13:54:55, physical Korryn lock/travel 13:55:20, 335 health/six slots, natural rematch 13:55:42. One-peer PIE scope only. No ensure/assertion/fatal/Accessed None in the clean log; known PixelStreaming2 startup error is separate.

**Hero-reselection attempt accounting:** reused 0802 for one two-process test; then one source build, three editor launches, four one-peer PIE runs, one 0803 package attempt. Earlier incomplete unlock handling and a missing Blueprint override flag were corrected. A failed Python probe retained world wrappers and crashed editor 10892; no presentation-fixture pass is claimed. Full causes/retry changes are documented in packaged acceptance and BF-107–109.

**Candidate 0803 identity:** source/asset commit `c5e11663`; executable SHA-256 `8B4EBF8FB637A731A6D8A1D81F170DE293C930C29172A76EA374EBC051154BB0`, verified against the manifest. Packaging captures project/engine HEAD, complete `git diff HEAD --binary`, status, untracked chunk-layer input, exit code, and SHA-256 manifest. HEAD alone is not the artifact identity. Candidate 0802 remains a historical baseline; its behavior does not prove the new fix.

**Settings / ownership:** four PIE clients and empty extra server options restored on disk; no dirty assets at editor exit. Preserve unrelated planning/docs/bridge/project changes. No subagents used.

**Invalidate relevant results:** selection/ownership/session changes require affected multiplayer checks; HUD registration requires visual checks; bot/round changes require another natural loop. Final end-to-end LAN acceptance remains required.

**Resume efficiently:** ledger → `Scripts/Test/Get-ResumeBrief.ps1` once → relevant evidence → one gate. Keep runtime Python wrappers function-local and return serialized values; release them on exceptions before travel. Follow existing process sessions; do not launch duplicate builds or rerun unchanged failed probes.

**Compact flow helper:** `Scripts/Test/Get-PackagedFlowBrief.ps1` summarizes whole-file selection/travel/results events and runtime error lines. Known 0802 counts, prior crash detection, bounded output, and live-host reading passed. It is an inventory, not an acceptance verdict. Use it to avoid repeated broad log searches.

**Phase-fix evidence / overhead:** `codex-phase-cleanup-final-editor-20260908.log`, `codex-phase-cleanup-final-{zero,two}-second.json`, and before/after exports under `Saved/Logs/waiting-phase-audit-20260908*`. One read-only export commandlet, two builds (65.45/25.84 seconds), two hidden editor launches, four PIE runs, no package. First observer alias failure and the first implementation's missing round-two native GAS transition are retained in acceptance history. Final checks and shutdown had zero ensure/assertion/fatal/Accessed None/Python/PIE error lines. BF-111 documents prevention.

**Additional audit leads:** first full-flow logs also show a legacy Playing phase before native Warmup, and postmatch snapshots contain repeated relic-carrier effect entries. Inspect ownership/cleanup before claiming defects fixed; these were not changed by f50104ec.

**Bot-fix evidence / overhead:** `codex-bot-ownership-{dev-seven-recheck,dev-zero,capture-four-peers}.json`, editor log, and `bot-ownership-20260908/{before,after,cold-verification}.json`. One hidden editor, four PIE runs including an observer API-name failure, one cold commandlet, zero C++ builds/packages. All 10,415 Blueprint error lines were the separate footstep failure; no legacy AI-controller errors or ensure/assertion/fatal. BF-112 records prevention.

**0803 manual-flow evidence / overhead:** reused candidate and host, one client launch, zero new builds/packages. Staging starts at 15:37:30.685 and 15:42:27.902 UTC; manual locks/travels at 15:38:27.204 and 15:43:23.619, before each deadline. Natural results at 15:39:52.343 and 15:45:33.933; shared frontend return at 15:40:27.147. `codex-packaged0803-manual-flow-20260908.json`: three host starts/travels, three results per peer, both normal exits, zero error/critical lines. Full UI observations in packaged acceptance. Scope: two rendered processes on one PC; waiting-banner/mesh/debug issues remain visible in this older artifact.
