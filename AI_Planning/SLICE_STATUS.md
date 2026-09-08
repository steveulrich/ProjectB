# Vertical Slice Status Ledger

Compact restart point. Detailed evidence and failed attempts remain in [packaged acceptance](./PACKAGED_LAN_ACCEPTANCE.md) and `Saved/Logs/`.

**Latest verified gate:** clean one-peer PIE physically changed the returning host from Argus to Korryn, locked before timeout, traveled once with the selected kit, and completed a natural rematch. Candidate 0802 separately passed actual PostMatch admission with retained results, no pawn at login, and the gameplay HUD kept hidden after loading.

**Current candidate:** `LAN-20260908-03`

**Current work:** goal active. Candidate 0803 completed build/cook/stage, exit 0, in 170.43 seconds; executable hash matches its manifest. No editor, test game, or package is running. Editor 4964 exited normally at 13:57:21 UTC with no dirty assets. Source/Blueprint fix: `c5e11663`. Use the ready artifact for the next gate; no additional build is needed.

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
| Placeholder asset integration / visual polish | Pending | Mesh/animation contract, sample menus/debug text, waiting banner behind results, performance budget remain |

**Next action:** launch ready candidate 0803 as two visible processes through the LAN frontend, using fresh `codex-packaged0803-lan-{host,client1}-20260908.log` paths. Require both humans to choose and lock on rematch without timeout, one staging start and one arena travel, both selected kits, and natural completion. No source rebuild is needed unless this gate finds a new defect. Follow with four-human assembly, same-party rehost, second-PC LAN, and placeholder integration.

**Current source verification:** one C++ build passed in 39.98 seconds; `BP_BW_GameState` and final `WBP_BW_HeroSelect` validate with zero errors/warnings. `codex-rematch-selection-clean-20260908.log`: initial match 13:54:26, staging 13:54:55, physical Korryn lock/travel 13:55:20, 335 health/six slots, natural rematch 13:55:42. One-peer PIE scope only. No ensure/assertion/fatal/Accessed None in the clean log; known PixelStreaming2 startup error is separate.

**Attempt accounting:** reused 0802 for one two-process test; then one source build, three editor launches, four one-peer PIE runs, one 0803 package attempt. Earlier incomplete unlock handling and a missing Blueprint override flag were corrected. A failed Python probe retained world wrappers and crashed editor 10892; no presentation-fixture pass is claimed. Full causes/retry changes are documented in packaged acceptance and BF-107–109.

**Artifact identity:** source/asset commit `c5e11663`; executable SHA-256 `8B4EBF8FB637A731A6D8A1D81F170DE293C930C29172A76EA374EBC051154BB0`, verified against the manifest. Packaging captures project/engine HEAD, complete `git diff HEAD --binary`, status, untracked chunk-layer input, exit code, and SHA-256 manifest. HEAD alone is not the artifact identity. Candidate 0802 remains a historical baseline; its behavior does not prove the new fix.

**Settings / ownership:** four PIE clients and empty extra server options restored on disk; no dirty assets at editor exit. Preserve unrelated planning/docs/bridge/project changes. No subagents used.

**Invalidate relevant results:** selection/ownership/session changes require affected multiplayer checks; HUD registration requires visual checks; bot/round changes require another natural loop. Final end-to-end LAN acceptance remains required.

**Resume efficiently:** ledger → `Scripts/Test/Get-ResumeBrief.ps1` once → relevant evidence → one gate. Keep runtime Python wrappers function-local and return serialized values; release them on exceptions before travel. Follow existing process sessions; do not launch duplicate builds or rerun unchanged failed probes.
