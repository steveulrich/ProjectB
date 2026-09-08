# Vertical Slice Status Ledger

Compact restart point for the next active implementation turn. Detailed evidence remains in BreakawayCore docs and `Saved/Logs/`.

**Latest verified gate:** candidate 0801 passed actual separate-process PostMatch results delivery and login pawn gating, and proved rematch URL flags stay removed. It exposed editor-only direct-arena selection startup and late-HUD visibility. Follow-up fixes are compiled and packaged in candidate 0802; their live verification is next. Earlier candidate 04 passed connected frontend return and fresh discovery after rehosting.

**Current candidate:** `LAN-20260908-02`

**Current work:** goal active. Candidate 0802 completed build/cook/stage, exit 0, and its executable hash matches the manifest. No editor, test game, or build is running. Follow-up fixes are committed as `fd99a9ce`; compile and two Blueprint validations passed. Editor 56980 exited normally at 07:30 UTC with no dirty assets. Use this candidate for the next check; no additional build is needed.

| Gate | Status | Evidence / next action |
|---|---|---|
| Hero selection and roster | Passed | PIE listen-server coverage |
| Full PIE core loop | Natural match and full rematch passed | Four-peer direct-map PIE passed. Packaged staging-derived rematch fails: SkipHeroSelection survives travel while human hero state is lost. Purchased-upgrade reset remains open |
| Packaged build/cook/stage | Passed | `LAN-20260908-02`; source base `fd99a9ce` plus recorded project/engine patches and untracked input; exit 0, 167.37 seconds |
| Packaged late join + round reset | Valid-choice path and PostMatch snapshot delivery passed | Candidate 0801 actual PostMatch arrival: pawn=None, retained eight-column breakdown, no visitor column. Gameplay HUD reappears behind results; source fix compiled. Invalid/repeated lock and pending-selection disconnect remain open |
| Individual selection timer / round-summary overlap | Passed in PIE and candidate 04 | Timed countdown visible; inactive countdown absent; selection stays clear through PostRound. PIE also verifies restoration of a still-active summary when selection closes |
| Six-slot gameplay HUD | Passed | Candidate `LAN-20260906-08` screenshot |
| Duplicate frontend HUD entries | In new package | Interactive visual verification pending |
| Hero-card visual-state targets | Fixed in source; 16 synthetic checks passed after cold restart | Child highlight, lock, and unavailable indicators are independent; physical input and packaged verification remain open |
| Results identity and portrait data | Fixtures passed; packaged visual sample passed | Candidate 04 natural results showed one MVP and three credited goals. Full packaged stat/portrait comparisons and late-result delivery remain open |
| Final-results snapshot transport | Four-peer fixture and natural transitions passed | Forced live-stat divergence with lag/loss, then both natural 3–0 matches; all final stats matched authority. Packaged checks remain open |
| DevMap navigation | Rebuilt and saved; cold-load verified | Missing static path data prevented all bot paths. Saved map now supports natural scoring across round resets and rematch |
| Personal relic goal credit | Fixed; natural and release probes passed | Goal overlap captures scorer before detachment; last possessor survives release and clears on reset. Both matches credited all three goals, including the winner |
| Packaged bot pickup / scoring / match completion | Passed offline and interactive LAN | Candidate 04 first match finished naturally 3–0 with a connected client |
| Packaged frontend return / second match | Connected return and discovery/join passed; playable rematch still open | Candidate 0801 fixed URL cleanup but direct arena load did not start packaged hero selection. Follow-up routes rematch through shared staging; compiled and awaiting candidate 0802 |
| Keyboard + gamepad acceptance | Pending | Run once packaged loop is stable |
| Second PC / clean-device LAN | Pending | Final network confidence pass |

**Source identity:** candidate `LAN-20260908-02` built from `fd99a9ce`; prefix `Saved/Logs/codex-packaged-LAN-20260908-02`. Project and engine patches include staged and unstaged changes. Executable SHA-256 matched manifest: `AD521761655C1175ECEBE3F20D7C350AA9F8515162F0C420953FDBDE6F111672`. Retain untracked-input records; HEAD alone does not identify the artifact.

**Profiles:** `ShortMatch` for direct DevMap, `FrontendLAN` for host startup screening. Repeatable settings do not guarantee a fixed match duration.

**Next action:** launch ready candidate 0802 from the frontend and repeat PostMatch arrival, now requiring its gameplay HUD to remain hidden. Then request Play Again from the remote visitor, require both humans to select in staging, return to the same arena with populated kits, and finish naturally. Use approved interactive launches and fresh `codex-packaged0802-lan-{host,client1}-20260908.log` paths. Four-human assembly, same-party rehost, second-PC LAN, and placeholder mesh/animation integration follow.

**Evidence:** `Saved/Logs/codex-packaged01-bots-headless-20260907.log` proves 3–0 PostMatch offline; `Saved/Logs/codex-packaged-LAN-20260907-01-*` identifies the artifact. Full gate scope: [packaged acceptance](./PACKAGED_LAN_ACCEPTANCE.md).

**Latest evidence:** `codex-packaged0801-lan-host-20260908.log` and `codex-packaged0801-lan-client1-20260908.log`: natural 1–3 completion, actual PostMatch admission at 07:18:51 (pawn=None, resultsSent=1), then correct URL cleanup but missing packaged selection startup. Follow-up compile: 55.14 seconds; two Blueprint validations and staging default pass; no new PIE run. Candidate 0802 package: 167.37 seconds; automatic Zen retry recovered during staging. Full scope and earlier evidence are in packaged acceptance.

**Follow-up evidence:** `codex-selection-frontend-verify2-20260907.log`, `codex-selection-frontend-{timed,untimed,return,postround-overlap,postround-restored}.json`. Two editor builds passed (49.21/33.25 seconds); first timer binding broke Blueprint pins, corrected without asset changes. A worktree regression in the bridge caused a modal-timeout crash; restored committed protection and existing `ProjectB.MCP.QueuedTimeoutLifetime` test passed. Full attempt scope is in packaged acceptance.

**Current source verification:** `Breakaway.FrontEnd.RematchTravelBase` passed; `BP_BW_GameState` validates cleanly. `codex-results-rematch-arrival-source.json` verifies all four peers after a natural 1–0 match, and `codex-rematch-arrival-postmatch-source.json` verifies the authority spawn gate. Build attempts: one include error (52.23 seconds), one pass (5.88 seconds). One four-peer PIE run; packaged verification pending. See the final section of packaged acceptance for contracts, logs, and limitations.

**Invalidate relevant results:** bot possession, behavior-tree, or round lifecycle changes require another bot-loop test. HUD registrations require visual checks. Session, input, or replication changes require affected multiplayer checks. Final end-to-end LAN acceptance remains required.

**Resume efficiently:** ledger → `Scripts/Test/Get-ResumeBrief.ps1` → relevant candidate evidence → one pending gate. Use the gate record in `Scripts/Test/README.md` for counts, retry conditions, and invalidation reasons. Read full logs only for causal errors. Inventory and startup-screening JSON do not close gameplay gates.
