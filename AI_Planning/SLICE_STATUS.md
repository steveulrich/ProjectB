# Vertical Slice Status Ledger

Compact restart point for the next active implementation turn. Detailed evidence remains in BreakawayCore docs and `Saved/Logs/`.

**Latest verified gate:** natural match and a full rematch passed on four same-process PIE peers after DevMap navigation and personal goal-credit fixes. Candidate `LAN-20260907-02` subsequently completed build/cook/stage, exit 0, in 170.26 seconds. Packaged frontend acceptance remains open.

**Current candidate:** `LAN-20260907-02`

**Pause handoff:** this update prepares the next resumed goal run; it does not resume implementation. At the workflow check, editor PID 33040 had exited and packaged game PID 69364 remained running from candidate 02. Recheck with the resume helper before using either observation.

| Gate | Status | Evidence / next action |
|---|---|---|
| Hero selection and roster | Passed | PIE listen-server coverage |
| Full PIE core loop | Natural match and full rematch passed | Remote physical Play Again reconnected all four peers with fresh stats; both matches finished 3–0. Purchased-upgrade reset and separate-process LAN remain open |
| Packaged build/cook/stage | Passed | `LAN-20260907-02`; source base `684f4adc` plus recorded project and engine patches |
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

**Source identity:** candidate `LAN-20260907-02` was built from `684f4adc924b54778c5e0df160f83fe2a65cf71c` plus recorded worktree changes. Evidence prefix: `Saved/Logs/codex-packaged-LAN-20260907-02`. Compare relevant changes before reusing it; current HEAD alone does not identify its contents.

**Profiles:** `ShortMatch` for direct DevMap, `FrontendLAN` for host startup screening. Repeatable settings do not guarantee a fixed match duration.

**Next action:** after the goal is resumed, run the resume helper and inspect candidate 02's existing evidence. Continue packaged frontend host/discover/join and input if its relevant inputs still match. Expected result: host creates a LAN session through the frontend, separate clients discover and join it, and the four-human roster reaches selection. Reobserve any existing game window or permission prompt before input; do not rebuild merely because the ledger previously said to prepare this candidate.

**Evidence:** `Saved/Logs/codex-packaged01-bots-headless-20260907.log` proves 3–0 PostMatch offline; `Saved/Logs/codex-packaged-LAN-20260907-01-*` identifies the artifact. Full gate scope: [packaged acceptance](./PACKAGED_LAN_ACCEPTANCE.md).

**Latest evidence:** `Saved/Logs/codex-relic-scorer-runtime-20260907.log`, `codex-results-natural-scorer-fixed-20260907.json`, `codex-natural-rematch-start-20260907.json`, `codex-results-natural-rematch-complete-20260907.json`, and `codex-relic-scorer-identity.json`. Reusable checks: `Scripts/Test/Verify-ResultsSnapshot.py`. Goal-credit build passed in 106.09 seconds.

**Invalidate relevant results:** bot possession, behavior-tree, or round lifecycle changes require another bot-loop test. HUD registrations require visual checks. Session, input, or replication changes require affected multiplayer checks. Final end-to-end LAN acceptance remains required.

**Resume efficiently:** ledger → `Scripts/Test/Get-ResumeBrief.ps1` → relevant candidate evidence → one pending gate. Use the gate record in `Scripts/Test/README.md` for counts, retry conditions, and invalidation reasons. Read full logs only for causal errors. Inventory and startup-screening JSON do not close gameplay gates.
