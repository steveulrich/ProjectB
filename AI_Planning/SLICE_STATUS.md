# Vertical Slice Status Ledger

Compact restart point for the next active implementation turn. Detailed evidence remains in BreakawayCore docs and `Saved/Logs/`.

**Latest verified gate:** natural match and a full rematch passed on four same-process PIE peers after DevMap navigation and personal goal-credit fixes. Editor PID 33040 remains open; PIE is stopped, client count restored to 1, and no dirty editor packages remain. Preparing candidate `LAN-20260907-02`.

| Gate | Status | Evidence / next action |
|---|---|---|
| Hero selection and roster | Passed | PIE listen-server coverage |
| Full PIE core loop | Natural match and full rematch passed | Remote physical Play Again reconnected all four peers with fresh stats; both matches finished 3–0. Purchased-upgrade reset and separate-process LAN remain open |
| Packaged build/cook/stage | Passed | `LAN-20260907-01`; source base `332ff649` plus recorded patch |
| Packaged late join + round reset | Passed | Candidate `LAN-20260906-07` |
| Six-slot gameplay HUD | Passed | Candidate `LAN-20260906-08` screenshot |
| Duplicate frontend HUD entries | In new package | Interactive visual verification pending |
| Hero-card visual-state targets | Fixed in source; 16 synthetic checks passed after cold restart | Child highlight, lock, and unavailable indicators are independent; physical input and packaged verification remain open |
| Results identity and portrait data | Fixed in source; two-peer fixture passed | Unique bot IDs, single MVP with duplicate names/negative ties, one local-player column; portraits match selected hero assets. Visible and packaged checks remain open |
| Final-results snapshot transport | Four-peer fixture and natural transitions passed | Forced live-stat divergence with lag/loss, then both natural 3–0 matches; all final stats matched authority. Packaged checks remain open |
| DevMap navigation | Rebuilt and saved; cold-load verified | Missing static path data prevented all bot paths. Saved map now supports natural scoring across round resets and rematch |
| Personal relic goal credit | Fixed; natural and release probes passed | Goal overlap captures scorer before detachment; last possessor survives release and clears on reset. Both matches credited all three goals, including the winner |
| Packaged bot pickup / scoring / match completion | Passed offline headless | New package scored 3 natural goals across round resets; interactive LAN still pending |
| Packaged frontend return / second match | Pending | Validate remote input and rehosting |
| Keyboard + gamepad acceptance | Pending | Run once packaged loop is stable |
| Second PC / clean-device LAN | Pending | Final network confidence pass |

**Source identity:** candidate `LAN-20260907-01` was built from `332ff649` plus the saved project patch; verify the current checkout independently.

**Profiles:** `ShortMatch` for direct DevMap, `FrontendLAN` for host startup screening. Repeatable settings do not guarantee a fixed match duration.

**Next action:** build and test `LAN-20260907-02`, then exercise packaged frontend host/discover/join and input. The prior packaged process has exited; Unreal windows were accessible for results inspection and a remote Play Again click. Recheck any new candidate permission prompt from fresh UI state. Candidate `LAN-20260907-01` predates the latest fixes.

**Evidence:** `Saved/Logs/codex-packaged01-bots-headless-20260907.log` proves 3–0 PostMatch offline; `Saved/Logs/codex-packaged-LAN-20260907-01-*` identifies the artifact. Full gate scope: [packaged acceptance](./PACKAGED_LAN_ACCEPTANCE.md).

**Latest evidence:** `Saved/Logs/codex-relic-scorer-runtime-20260907.log`, `codex-results-natural-scorer-fixed-20260907.json`, `codex-natural-rematch-start-20260907.json`, `codex-results-natural-rematch-complete-20260907.json`, and `codex-relic-scorer-identity.json`. Reusable checks: `Scripts/Test/Verify-ResultsSnapshot.py`. Goal-credit build passed in 106.09 seconds.

**Invalidate relevant results:** bot possession, behavior-tree, or round lifecycle changes require another bot-loop test. HUD registrations require visual checks. Session, input, or replication changes require affected multiplayer checks. Final end-to-end LAN acceptance remains required.

**Resume efficiently:** ledger → current Git status/HEAD → relevant candidate summary → one pending gate. Read full logs only for causal errors; preserve them on disk. Update this ledger after verified outcomes. Startup-screening JSON does not close gameplay gates.
