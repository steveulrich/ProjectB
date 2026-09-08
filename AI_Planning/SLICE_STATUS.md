# Vertical Slice Status Ledger

Compact restart point for the next active implementation turn. Detailed evidence remains in BreakawayCore docs and `Saved/Logs/`.

**Latest verified gate:** candidate 04 passed two-process LAN discovery, valid late-join selection, natural completion, and connected Return to Lobby without a GameMode assertion. The same host process created a second session that a fresh client discovered and joined. PostMatch arrival and the subsequent packaged rematch exposed missing hero/results state; these are the next fixes.

**Current candidate:** `LAN-20260907-04`

**Current work:** goal active; next fixes are recorded below. Candidate 04 host and interactive clients exited normally; the discarded untargetable sandbox client required termination after its graceful-close request failed. All candidate-04 test processes were absent at 06:50 UTC. Editor closed normally, no dirty assets; four-client PIE defaults and empty extra options restored on disk. Recheck current process state on resume.

| Gate | Status | Evidence / next action |
|---|---|---|
| Hero selection and roster | Passed | PIE listen-server coverage |
| Full PIE core loop | Natural match and full rematch passed | Four-peer direct-map PIE passed. Packaged staging-derived rematch fails: SkipHeroSelection survives travel while human hero state is lost. Purchased-upgrade reset remains open |
| Packaged build/cook/stage | Passed | `LAN-20260907-04`; source base `a24c3ec5` plus recorded complete/staged project patches and engine patch; exit 0, 149.62 seconds |
| Packaged late join + round reset | Valid-choice path passed; PostMatch arrival fails | Candidates 03/04: accepted lock spawns selected kit. Candidate 04 PostMatch arrival gets a default pawn, empty slots, no results screen. Invalid/repeated lock and pending-selection disconnect remain open |
| Individual selection timer / round-summary overlap | Passed in PIE and candidate 04 | Timed countdown visible; inactive countdown absent; selection stays clear through PostRound. PIE also verifies restoration of a still-active summary when selection closes |
| Six-slot gameplay HUD | Passed | Candidate `LAN-20260906-08` screenshot |
| Duplicate frontend HUD entries | In new package | Interactive visual verification pending |
| Hero-card visual-state targets | Fixed in source; 16 synthetic checks passed after cold restart | Child highlight, lock, and unavailable indicators are independent; physical input and packaged verification remain open |
| Results identity and portrait data | Fixtures passed; packaged visual sample passed | Candidate 04 natural results showed one MVP and three credited goals. Full packaged stat/portrait comparisons and late-result delivery remain open |
| Final-results snapshot transport | Four-peer fixture and natural transitions passed | Forced live-stat divergence with lag/loss, then both natural 3–0 matches; all final stats matched authority. Packaged checks remain open |
| DevMap navigation | Rebuilt and saved; cold-load verified | Missing static path data prevented all bot paths. Saved map now supports natural scoring across round resets and rematch |
| Personal relic goal credit | Fixed; natural and release probes passed | Goal overlap captures scorer before detachment; last possessor survives release and clears on reset. Both matches credited all three goals, including the winner |
| Packaged bot pickup / scoring / match completion | Passed offline and interactive LAN | Candidate 04 first match finished naturally 3–0 with a connected client |
| Packaged frontend return / second match | Clean connected return and fresh discovery/join passed; playable rematch failed | Candidate 04: no GameMode assertion; same host re-created session. Subsequent Play Again retained SkipHeroSelection=1 and both human pawns lacked hero data |
| Keyboard + gamepad acceptance | Pending | Run once packaged loop is stable |
| Second PC / clean-device LAN | Pending | Final network confidence pass |

**Source identity:** candidate 04 built from `a24c3ec5` with the follow-up edits subsequently committed as `8816d0f6`. Prefix: `Saved/Logs/codex-packaged-LAN-20260907-04`. Initial diff omitted staged changes; use supplementary `-project-complete.patch`, `-project-staged.patch`, and `-staged-source-supplement.json` as well as original records. Executable SHA-256 matched manifest. HEAD alone does not identify the artifact.

**Profiles:** `ShortMatch` for direct DevMap, `FrontendLAN` for host startup screening. Repeatable settings do not guarantee a fixed match duration.

**Next action:** fix packaged rematch option cleanup and PostMatch arrival behavior. Expected result: staging-derived Play Again gives every human valid selection and a populated kit; a PostMatch arrival sees appropriate finished-match state without a default gameplay pawn. Inspect BwayGameState rematch URL construction, BwayGameplayUrl option augmentation, and GameMode login/results catch-up. Then verify source/PIE before one new package. Four-human assembly, same-party rehost, second-PC LAN, and placeholder mesh/animation integration follow.

**Evidence:** `Saved/Logs/codex-packaged01-bots-headless-20260907.log` proves 3–0 PostMatch offline; `Saved/Logs/codex-packaged-LAN-20260907-01-*` identifies the artifact. Full gate scope: [packaged acceptance](./PACKAGED_LAN_ACCEPTANCE.md).

**Latest evidence:** candidate-04 host log plus `codex-packaged04-lan-client1-20260907.log` (valid late join, connected return) and `codex-packaged04-lan-client3-20260907.log` (fresh discovery, PostMatch arrival, rematch failure). Re-create success 06:38:15 UTC; fresh client welcomed 06:45:59; rematch LoadMap retained SkipHeroSelection at 06:46:39; both humans logged no selected hero. Candidate 04 required one package, 149.62 seconds; runtime checks added no builds.

**Follow-up evidence:** `codex-selection-frontend-verify2-20260907.log`, `codex-selection-frontend-{timed,untimed,return,postround-overlap,postround-restored}.json`. Two editor builds passed (49.21/33.25 seconds); first timer binding broke Blueprint pins, corrected without asset changes. A worktree regression in the bridge caused a modal-timeout crash; restored committed protection and existing `ProjectB.MCP.QueuedTimeoutLifetime` test passed. Full attempt scope is in packaged acceptance.

**Invalidate relevant results:** bot possession, behavior-tree, or round lifecycle changes require another bot-loop test. HUD registrations require visual checks. Session, input, or replication changes require affected multiplayer checks. Final end-to-end LAN acceptance remains required.

**Resume efficiently:** ledger → `Scripts/Test/Get-ResumeBrief.ps1` → relevant candidate evidence → one pending gate. Use the gate record in `Scripts/Test/README.md` for counts, retry conditions, and invalidation reasons. Read full logs only for causal errors. Inventory and startup-screening JSON do not close gameplay gates.
