# Vertical Slice Status Ledger

Compact restart point for the next active implementation turn. Detailed evidence remains in BreakawayCore docs and `Saved/Logs/`.

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
| Packaged bot pickup / scoring / match completion | Passed offline headless | New package scored 3 natural goals across round resets; interactive LAN still pending |
| Packaged frontend return / second match | Pending | Validate remote input and rehosting |
| Keyboard + gamepad acceptance | Pending | Run once packaged loop is stable |
| Second PC / clean-device LAN | Pending | Final network confidence pass |

**Source identity:** candidate `LAN-20260907-01` was built from `332ff649` plus the saved project patch; verify the current checkout independently.

**Profiles:** `ShortMatch` for direct DevMap, `FrontendLAN` for host startup screening. Repeatable settings do not guarantee a fixed match duration.

**Next action when resumed:** recheck the reported new-package firewall prompt and running processes; the last observation still required user handling after tool auto-review rejected inspection. Verify keyboard selection and the results presentation in the source editor, then build a new LAN candidate. The editor is open with PIE stopped and the client-count setting restored to 1. Candidate `LAN-20260907-01` predates the hero-card, bot-ID, and results fixes.

**Evidence:** `Saved/Logs/codex-packaged01-bots-headless-20260907.log` proves 3–0 PostMatch offline; `Saved/Logs/codex-packaged-LAN-20260907-01-*` identifies the artifact. Full gate scope: [packaged acceptance](./PACKAGED_LAN_ACCEPTANCE.md).

**Invalidate relevant results:** bot possession, behavior-tree, or round lifecycle changes require another bot-loop test. HUD registrations require visual checks. Session, input, or replication changes require affected multiplayer checks. Final end-to-end LAN acceptance remains required.

**Resume efficiently:** ledger → current Git status/HEAD → relevant candidate summary → one pending gate. Read full logs only for causal errors; preserve them on disk. Update this ledger after verified outcomes. Startup-screening JSON does not close gameplay gates.
