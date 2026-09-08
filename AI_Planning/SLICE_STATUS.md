# Vertical Slice Status Ledger

Compact restart point for the next active implementation turn. Detailed evidence remains in BreakawayCore docs and `Saved/Logs/`.

**Latest verified gate:** final-results snapshot transport passed on a listen host and three same-process PIE clients. Editor PID 56652 remains open; PIE is stopped, client count restored to 1, and no dirty editor packages remain.

| Gate | Status | Evidence / next action |
|---|---|---|
| Hero selection and roster | Passed | PIE listen-server coverage |
| Full PIE core loop | Passed in recorded run | Natural best-of-five; rematch reconnection verified separately, full rematch still open |
| Packaged build/cook/stage | Passed | `LAN-20260907-01`; source base `332ff649` plus recorded patch |
| Packaged late join + round reset | Passed | Candidate `LAN-20260906-07` |
| Six-slot gameplay HUD | Passed | Candidate `LAN-20260906-08` screenshot |
| Duplicate frontend HUD entries | In new package | Interactive visual verification pending |
| Hero-card visual-state targets | Fixed in source; 16 synthetic checks passed after cold restart | Child highlight, lock, and unavailable indicators are independent; physical input and packaged verification remain open |
| Results identity and portrait data | Fixed in source; two-peer fixture passed | Unique bot IDs, single MVP with duplicate names/negative ties, one local-player column; portraits match selected hero assets. Visible and packaged checks remain open |
| Final-results snapshot transport | Fixed in source; four-peer RPC fixture passed | Complete server snapshot survives live-stat reset with 150 ms packet lag and 5% loss configured; natural match transition and packaged checks remain open |
| Packaged bot pickup / scoring / match completion | Passed offline headless | New package scored 3 natural goals across round resets; interactive LAN still pending |
| Packaged frontend return / second match | Pending | Validate remote input and rehosting |
| Keyboard + gamepad acceptance | Pending | Run once packaged loop is stable |
| Second PC / clean-device LAN | Pending | Final network confidence pass |

**Source identity:** candidate `LAN-20260907-01` was built from `332ff649` plus the saved project patch; verify the current checkout independently.

**Profiles:** `ShortMatch` for direct DevMap, `FrontendLAN` for host startup screening. Repeatable settings do not guarantee a fixed match duration.

**Next action:** verify the snapshot during natural match completion and inspect physical keyboard selection/results presentation when the UI is accessible, then build a new LAN candidate. Revalidate processes and the prior firewall blocker before UI work; the dialog was not inspected in this snapshot turn. Candidate `LAN-20260907-01` predates the hero-card, bot-ID, results identity, and snapshot fixes.

**Evidence:** `Saved/Logs/codex-packaged01-bots-headless-20260907.log` proves 3–0 PostMatch offline; `Saved/Logs/codex-packaged-LAN-20260907-01-*` identifies the artifact. Full gate scope: [packaged acceptance](./PACKAGED_LAN_ACCEPTANCE.md).

**Latest evidence:** `Saved/Logs/codex-results-snapshot-runtime-20260907.log` and `codex-results-snapshot-4-peers.json`; reproducible verifier: `Scripts/Test/Verify-ResultsSnapshot.py`. Editor build passed in 104.29 seconds. This synthetic staging fixture did not play or end a match.

**Invalidate relevant results:** bot possession, behavior-tree, or round lifecycle changes require another bot-loop test. HUD registrations require visual checks. Session, input, or replication changes require affected multiplayer checks. Final end-to-end LAN acceptance remains required.

**Resume efficiently:** ledger → current Git status/HEAD → relevant candidate summary → one pending gate. Read full logs only for causal errors; preserve them on disk. Update this ledger after verified outcomes. Startup-screening JSON does not close gameplay gates.
