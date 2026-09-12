# Workflow efficiency contract

Reviewed September 9, 2026 after the September 8 implementation and verification sequence. This document records how to reduce repeated context and tool work while keeping the final-public-alpha parity bar and evidence scope unchanged.

## What the last 12+ hours showed

The work advanced through a dense chain of source fixes and acceptance records: the latest commits ran from `bb7d2aca` through `d194fd8c`, covering frontend travel, phase cleanup, bot ownership, footsteps, hero visuals, team assignment, request lifecycle, and screen-space request UI. The current ledger is 69 lines and the worktree has 37 unrelated dirty entries. The final request-UI gate required one build, a three-peer audit, a four-peer fixed run, two editor lifecycles, material fixtures, physical input, death/respawn, remote rendering, cleanup, and a targeted commit.

The quality was good, but the process spent avoidable tokens and wall time in five places:

1. Broad `Get-Content` calls pulled large skills, historical acceptance documents, and log tails into context. One diagnostic call emitted 27,859 tokens and was truncated before the useful part.
2. The same state was restated in prose, the ledger, evidence JSON, and tool output. The authoritative JSON existed, but it was not consistently used as the only input to the next decision.
3. Runtime diagnostics were sometimes loaded or redeclared in a live interpreter. That created stale callback risk, a retained-wrapper risk, and one viewer binding error that required extra recovery work.
4. Computer Use actions occasionally reused stale window state or screenshot IDs. Fresh selection and state capture eventually solved it, but failed retries added no evidence.
5. Editor/build setup was repeated for adjacent checks, and one editor was left running by an interrupted probe. A single gate manifest and explicit process ownership would have prevented the extra lifecycle.

These are workflow costs, not reasons to remove proof. Remote rendered views, physical input, death/respawn, packaging, adverse networking, second-PC LAN, and normal shutdown remain required whenever their gate is in scope.

## Operating contract for the next iteration

1. During active vertical-slice implementation, read `AI_Planning/SLICE_STATUS.md` once, run `Scripts/Test/Get-ResumeBrief.ps1` once, and select one pending gate at a time. After a pass or recorded blocker, continue to the next authorized gate while the goal is active. Do not reread either file unless the gate changes or a mutation updates it.
2. Write the gate contract in the first evidence record: expected observable result, required topology, selected source/assets, retry condition, and cleanup owner. Use `Scripts/Test/Get-GateBrief.ps1` for every follow-up inventory.
3. Batch independent, narrow reads in one tool call. Use `rg` with a path and a small match cap, then read only the surrounding source lines. Never send a whole historical document or full log to the model when a compact summary can answer the question.
4. Keep full logs and raw captures on disk. Return only: pass/fail, first causal error, counts, scope, evidence path, and next action. `Get-GateBrief.ps1` summarizes selected JSON and whole-file error counts without moving the file into context.
5. Treat each live Python observer as a single owned resource. Load it once, keep wrappers inside functions, register one callback, stop it in `finally` or the explicit cleanup step, and serialize results. A new hypothesis may justify a new callback; a repeated read does not.
6. For Windows UI, select one exact returned window, capture fresh state, perform one action, and capture fresh state again. Never reuse coordinates, screenshot IDs, or an activation result after another action. If the target is missing, stop and reselect instead of clicking again.
6a. Treat a compact brief's `hiddenEditorCount > 0` as a hard preflight failure. Resolve the exact process owner and close it normally; do not launch a duplicate editor or rely on a hidden process as UI evidence.
7. Reuse one editor and one build for a coherent gate. Record process IDs and log paths before launch, wait on those handles, and close the editor normally before rebuilding or changing reflected classes. Do not launch a second copy to compensate for an unclassified failure.
8. Stage only owned paths after the gate passes. Keep unrelated dirty files out of the commit and record their count, not their contents, in the ledger.
9. Update the ledger with one current status row and one next action. Put attempt history, retries, commands, and detailed observations in the gate document or JSON manifest; replace stale summaries instead of appending a transcript.

## Quality guardrails

| Saving | Preserved proof |
|---|---|
| Compact evidence output | Full logs, screenshots, traces, and JSON stay under `Saved/Logs/` |
| One active gate at a time | All requirements in that gate's checklist and topology still run |
| Reusing a process | Only while its source, configuration, and unsaved-state contract remain unchanged |
| Fewer retries | Retry after a changed hypothesis, new evidence, or explicit reproducibility check; never hide an unknown result |
| Narrow reads | Expand only the specific file/range needed to explain a failure |
| Candidate reuse | Requires source patch, assets, configuration, and artifact identity review; HEAD equality alone is insufficient |

The compact brief is an inventory, not an acceptance verdict. A gate passes only from evidence whose topology and artifact identity match the requirement. This contract therefore saves context without lowering the standard for final LAN, packaged, physical-input, or end-to-end proof.
