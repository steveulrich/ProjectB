# Vertical Slice Status Ledger

Compact restart point. Detailed evidence and failed attempts remain in [packaged acceptance](./PACKAGED_LAN_ACCEPTANCE.md) and `Saved/Logs/`.

**Latest verified gate:** phase-lifecycle fix `f50104ec` passed zero- and two-second PostRound checks in one-authority NullRHI PIE: eight player states, two natural rounds per run, protected Warmup/PostRound, restored input in round two, and no waiting widget at results. This is state evidence; rendered and multiplayer acceptance remain open. Earlier clean one-peer PIE physically changed the returning host from Argus to Korryn and completed a natural rematch.

**Current candidate:** `LAN-20260908-03`

**Current work:** goal active. Source phase cleanup is committed as `f50104ec`; it is not in candidate 0803. That candidate's existing host PID 48272 still shows its firewall prompt; the candidate-specific user handoff remains pending. No client or new package was launched. The two hidden editors used for phase checks exited normally; original four-client PIE settings were restored. The explicit NumBots override exposed a separate duplicate-spawner defect; normal roster scaling passed.

| Gate | Status | Evidence / next action |
|---|---|---|
| Hero selection and roster | Earlier PIE coverage passed; rematch fix verified in one-peer PIE | Returning host can change and lock; packaged two-human selection remains next |
| Full PIE core loop | Natural matches and rematches passed | Final clean run: Argus initial 1–0, Korryn rematch 1–0. Purchased-upgrade reset remains open |
| Packaged build/cook/stage | Candidate 0803 passed | Source `c5e11663`; prefix `Saved/Logs/codex-packaged-LAN-20260908-03`; one attempt, exit 0, 170.43 seconds |
| Packaged late join + round reset | Valid choices and PostMatch snapshot delivery passed | 0802 PostMatch arrival at 07:47:27: pawn=None, resultsSent=1, unchanged eight-column breakdown; HUD stays hidden. Invalid/repeated locks and selection-time disconnect remain open |
| Individual selection timer / round-summary overlap | Passed in PIE and candidate 04 | Active countdown shown; individual inactive countdown absent; PostRound overlap suppressed/restored |
| Six-slot gameplay HUD | Passed | 0802 timeout-fallback rematch restored populated Argus and Rawlins kits |
| Duplicate frontend HUD entries | Source fix packaged earlier | Interactive visual verification pending |
| Hero-card presentation | Earlier 16 synthetic checks passed; reuse fix verified physically in PIE | Both lock/unlock refresh all controls from current PlayerState. Packaged and gamepad checks remain |
| Results identity and portrait data | Fixtures and packaged visual samples passed | 0802 first match/arrival and rematch had matching columns, totals, portraits, goal credits and one MVP; full packaged field comparison remains |
| Final-results snapshot transport | Four-peer fixture/natural transitions plus actual packaged arrival passed | Earlier lag/loss with forced live-stat divergence passed; final LAN acceptance still required |
| DevMap navigation / personal goal credit | Fixed and verified | Natural scoring, cold map load, round resets, goal ownership and release/reset probes passed |
| Packaged match completion | Natural first match and timeout-fallback rematch passed | 0802: 1–3 first match, 0–3 rematch. Both humans selecting manually was blocked by host UI; fixed in source |
| Frontend return / second match | Connected return and fresh discovery passed earlier | 0803 must verify complete manual shared selection; same-party rehost still open |
| Keyboard + gamepad acceptance | Pending | Physical all-kit actions once packaged loop is stable |
| Four-human / second-PC LAN | Pending | Final target: one PIE host plus three separate instances; also clean-device LAN |
| Waiting banner / phase lifecycle | Fixed in source; headless state checks passed | `f50104ec`; zero/nonzero PostRound and round-two input restored. New packaged/rendered evidence required |
| Explicit bot-count override | Failed in full-flow PIE | NumBots=7 produced 15 player states and legacy AI startup errors; both spawners consume the override. Default scaling produced eight |
| Placeholder asset integration / visual polish | Pending | Mesh/animation contract, sample menus/debug text, performance budget remain |

**Next action:** while the 0803 firewall handoff remains pending, audit legacy Lyra versus Breakaway bot-spawn ownership for explicit NumBots overrides. If the prompt clears, reuse host PID 48272/log for the pending c5e11663 manual two-human reselection check; do not count that old candidate as verification of f50104ec. A new candidate containing the phase fix is required for current end-to-end/rendered acceptance. Preserve the full four-human, same-party rehost, second-PC, and placeholder integration gates.

**Hero-reselection source verification:** one C++ build passed in 39.98 seconds; `BP_BW_GameState` and final `WBP_BW_HeroSelect` validate with zero errors/warnings. `codex-rematch-selection-clean-20260908.log`: initial match 13:54:26, staging 13:54:55, physical Korryn lock/travel 13:55:20, 335 health/six slots, natural rematch 13:55:42. One-peer PIE scope only. No ensure/assertion/fatal/Accessed None in the clean log; known PixelStreaming2 startup error is separate.

**Hero-reselection attempt accounting:** reused 0802 for one two-process test; then one source build, three editor launches, four one-peer PIE runs, one 0803 package attempt. Earlier incomplete unlock handling and a missing Blueprint override flag were corrected. A failed Python probe retained world wrappers and crashed editor 10892; no presentation-fixture pass is claimed. Full causes/retry changes are documented in packaged acceptance and BF-107–109.

**Candidate 0803 identity:** source/asset commit `c5e11663`; executable SHA-256 `8B4EBF8FB637A731A6D8A1D81F170DE293C930C29172A76EA374EBC051154BB0`, verified against the manifest. Packaging captures project/engine HEAD, complete `git diff HEAD --binary`, status, untracked chunk-layer input, exit code, and SHA-256 manifest. HEAD alone is not the artifact identity. Candidate 0802 remains a historical baseline; its behavior does not prove the new fix.

**Settings / ownership:** four PIE clients and empty extra server options restored on disk; no dirty assets at editor exit. Preserve unrelated planning/docs/bridge/project changes. No subagents used.

**Invalidate relevant results:** selection/ownership/session changes require affected multiplayer checks; HUD registration requires visual checks; bot/round changes require another natural loop. Final end-to-end LAN acceptance remains required.

**Resume efficiently:** ledger → `Scripts/Test/Get-ResumeBrief.ps1` once → relevant evidence → one gate. Keep runtime Python wrappers function-local and return serialized values; release them on exceptions before travel. Follow existing process sessions; do not launch duplicate builds or rerun unchanged failed probes.

**Compact flow helper:** `Scripts/Test/Get-PackagedFlowBrief.ps1` summarizes whole-file selection/travel/results events and runtime error lines. Known 0802 counts, prior crash detection, bounded output, and live-host reading passed. It is an inventory, not an acceptance verdict. Use it to avoid repeated broad log searches.

**Phase-fix evidence / overhead:** `codex-phase-cleanup-final-editor-20260908.log`, `codex-phase-cleanup-final-{zero,two}-second.json`, and before/after exports under `Saved/Logs/waiting-phase-audit-20260908*`. One read-only export commandlet, two builds (65.45/25.84 seconds), two hidden editor launches, four PIE runs, no package. First observer alias failure and the first implementation's missing round-two native GAS transition are retained in acceptance history. Final checks and shutdown had zero ensure/assertion/fatal/Accessed None/Python/PIE error lines. BF-111 documents prevention.

**Additional audit leads:** first full-flow logs also show a legacy Playing phase before native Warmup, and postmatch snapshots contain repeated relic-carrier effect entries. Inspect ownership/cleanup before claiming defects fixed; these were not changed by f50104ec.
