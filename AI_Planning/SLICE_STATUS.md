# Vertical Slice Status Ledger

Compact restart point for the next active implementation turn. Detailed evidence remains in BreakawayCore docs and `Saved/Logs/`.

**Latest verified gate:** candidate 03 passed two-process frontend LAN discovery and valid late-join admission: no pawn before selection, server-accepted Argus lock, 500 health, six slots, and physical mobility input. Natural results reached; clean frontend return failed on the remote GameMode assertion. Four-peer PIE natural match/rematch evidence remains valid within its recorded scope.

**Current candidate:** `LAN-20260907-03`

**Current work:** goal active. Candidate 03 host 34708 and client 76840 exited normally after the test. Editor closed at this checkpoint. Reobserve processes before acting. Next fixes concern selection timer/overlay and client frontend initialization.

| Gate | Status | Evidence / next action |
|---|---|---|
| Hero selection and roster | Passed | PIE listen-server coverage |
| Full PIE core loop | Natural match and full rematch passed | Remote physical Play Again reconnected all four peers with fresh stats; both matches finished 3–0. Purchased-upgrade reset and separate-process LAN remain open |
| Packaged build/cook/stage | Passed | `LAN-20260907-03`; source base `3dc6a555` plus recorded project and engine patches; exit 0, 198.10 seconds |
| Packaged late join + round reset | Valid-choice path passed; UI defects open | Candidate 03: individual selection during Playing, accepted lock spawns selected kit, kit retained through reset. Invalid/repeated lock, disconnect while selecting, and PostMatch arrival still need probes |
| Six-slot gameplay HUD | Passed | Candidate `LAN-20260906-08` screenshot |
| Duplicate frontend HUD entries | In new package | Interactive visual verification pending |
| Hero-card visual-state targets | Fixed in source; 16 synthetic checks passed after cold restart | Child highlight, lock, and unavailable indicators are independent; physical input and packaged verification remain open |
| Results identity and portrait data | Fixed in source; two-peer fixture passed | Unique bot IDs, single MVP with duplicate names/negative ties, one local-player column; portraits match selected hero assets. Visible and packaged checks remain open |
| Final-results snapshot transport | Four-peer fixture and natural transitions passed | Forced live-stat divergence with lag/loss, then both natural 3–0 matches; all final stats matched authority. Packaged checks remain open |
| DevMap navigation | Rebuilt and saved; cold-load verified | Missing static path data prevented all bot paths. Saved map now supports natural scoring across round resets and rematch |
| Personal relic goal credit | Fixed; natural and release probes passed | Goal overlap captures scorer before detachment; last possessor survives release and clears on reset. Both matches credited all three goals, including the winner |
| Packaged bot pickup / scoring / match completion | Passed offline headless | New package scored 3 natural goals across round resets; interactive LAN still pending |
| Packaged frontend return / second match | Clean return failed | Candidates 02/03 remote Return to Lobby reaches frontend but asserts because clients have no GameMode; fix and verify rehosting |
| Keyboard + gamepad acceptance | Pending | Run once packaged loop is stable |
| Second PC / clean-device LAN | Pending | Final network confidence pass |

**Source identity:** candidate `LAN-20260907-03` was built from `3dc6a55504f934e05b309a34cdade13582ce8048` plus recorded worktree changes. Evidence prefix: `Saved/Logs/codex-packaged-LAN-20260907-03`; executable SHA-256 verified against manifest. Compare relevant changes before reuse; HEAD alone does not identify its contents.

**Profiles:** `ShortMatch` for direct DevMap, `FrontendLAN` for host startup screening. Repeatable settings do not guarantee a fixed match duration.

**Next action:** fix unlimited selection timer (`Lock in: -1s`), prevent PostRound overlays covering active selection, and remove the frontend's authority-only GameMode assumption while preserving hard-disconnect cleanup. Expected result: clear individual selection through round transitions, then client return without assertion and successful rehost. Compile one coherent batch and verify affected paths before another package. Four-human pre-match assembly and second-PC LAN remain open.

**Evidence:** `Saved/Logs/codex-packaged01-bots-headless-20260907.log` proves 3–0 PostMatch offline; `Saved/Logs/codex-packaged-LAN-20260907-01-*` identifies the artifact. Full gate scope: [packaged acceptance](./PACKAGED_LAN_ACCEPTANCE.md).

**Latest evidence:** `Saved/Logs/codex-packaged03-lan-host-20260907.log` and `codex-packaged03-lan-client1-20260907.log`; admission wait 05:43:54 UTC, accepted Argus spawn 05:44:52, natural results and remote return afterward. Late-join editor build: one pass, 86.66 seconds; one candidate-03 package. Two PIE diagnostic attempts did not add a late client; details and fixture limitations are in packaged acceptance. Earlier four-peer results JSON remains linked there.

**Invalidate relevant results:** bot possession, behavior-tree, or round lifecycle changes require another bot-loop test. HUD registrations require visual checks. Session, input, or replication changes require affected multiplayer checks. Final end-to-end LAN acceptance remains required.

**Resume efficiently:** ledger → `Scripts/Test/Get-ResumeBrief.ps1` → relevant candidate evidence → one pending gate. Use the gate record in `Scripts/Test/README.md` for counts, retry conditions, and invalidation reasons. Read full logs only for causal errors. Inventory and startup-screening JSON do not close gameplay gates.
