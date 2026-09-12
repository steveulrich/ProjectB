# ProjectB working guidance

## Task scope and skill selection

- Use skills for the requested operation and the behavior they own; an incidental keyword does not require another skill. The `projectb-*` utility skills are the canonical variants for this repository. Route matching utility requests to those variants; unprefixed personal counterparts are legacy alternatives for other projects. Load only the variant needed for the task.
- Load references only for the current decision. Full-system implementation sequences and test matrices apply to new systems, affected behavior, or the selected acceptance gate; a targeted fix does not require redesigning the subsystem. Preserve every proof required by the gate and invalidate evidence when relevant inputs change.
- Complete authorized implementation, relevant verification, and fixes before reporting completion. Existing explicit approval remains valid within its scope; ask only for a missing consequential decision, newly discovered consequences, expanded scope, unsaved-work conflict, or a permission the tools cannot satisfy.

## Resume the vertical slice efficiently

The gate, candidate, process, and ledger workflow below applies to active vertical-slice implementation and acceptance testing. Documentation, planning, and instruction maintenance use only the references needed for that request.

- When working on the vertical-slice goal, read `AI_Planning/SLICE_STATUS.md`, then run `Scripts/Test/Get-ResumeBrief.ps1` once for current Git, candidate, and process facts. Follow the user's pause/resume state.
- A report, planning request, or workflow edit while paused does not resume gameplay implementation. Complete that request, preserve the next action, and wait for the user to resume the goal.
- During active vertical-slice implementation, select one pending acceptance gate at a time and define its expected observable result before editing. After passing a gate or recording a blocker, continue the next authorized gate while the goal is active. Read only its relevant evidence, source, and applicable skills.
- Use `rg` to locate symbols and narrow file ranges. Keep historical reports, full logs, and large skill references out of context unless needed for the current decision.
- Default routine tool output to about 1,500 tokens per call. Filter or summarize on disk first; expand the limit when necessary to understand a failure. Read each applicable skill once per available context and load referenced material only as needed.
- Consult `Scripts/Test/README.md` when running the shared profiles or compact JSON helpers. Preserve full evidence under `Saved/Logs/`; report the outcome, first causal error, and evidence path.
- Run the cheapest applicable verification first. Compile a coherent set of related C++ fixes, then run affected tests. Package after those checks pass when packaged evidence is required.
- Reuse an existing candidate only when its recorded source, patch, assets, and configuration cover the tested change. Retest passed gates when relevant changes invalidate their evidence; complete the final end-to-end checks.
- Wait on an existing build or test process. Avoid duplicate launches and repeated unchanged log reads. Retry a failure after a change, a new hypothesis, or an explicit reproducibility check.
- After two attempts with the same failure and no new evidence, stop repeating that operation. Record the command, failure, attempted remedies, and condition for retry in the linked evidence document. While the goal is active, continue an independent gate if possible; preserve approval boundaries.
- Prefer structured bridge or CLI inspection where it answers the question. Use fresh UI observations for visual polish, physical input, frontend flow, and security dialogs.
- Put PIE Python inspections inside a function and return only serialized values. Never leave world, actor, widget, or component wrappers in interpreter globals; exceptions must also release them before travel or stopping PIE. If a probe fails with retained objects, clear its references or restart the editor before continuing the travel test.
- Keep progress updates concise and report meaningful changes. After a verified result, update the ledger's gate, evidence, invalidation conditions, and next action; put detailed history in the linked acceptance document.
- Keep the resume ledger under about 100 lines. Store commands and full reports by reference, and replace stale state instead of appending turn histories. Record build/package counts and retry reasons with each gate's evidence to assess workflow overhead.
- Use the gate record in `Scripts/Test/README.md` to preserve the expected result, evidence, retry condition, and next action before a pause or context handoff. Recheck process IDs and artifacts on resume; historical state is not current state.
- Use `Scripts/Test/Get-GateBrief.ps1` after the gate starts to summarize only selected evidence JSON/logs. Keep raw logs on disk and return pass/fail, first causal error, counts, scope, and the evidence path. Consult `AI_Planning/WORKFLOW_EFFICIENCY.md` when measuring or changing workflow efficiency.
- For each gate, create one process/evidence contract, reuse its editor/build handles, and stop callbacks in cleanup. Do not redeclare live Python observers or reuse stale UI screenshot state; select, observe, act once, then refresh.
- Treat `hiddenEditorCount` in the resume or gate brief as a stop signal: identify the exact owner and close it normally before launching another editor. Never compensate for a hidden or untargetable process with a duplicate launch.
- Save repeated diagnostic procedures as parameterized scripts after they prove useful. Do not build a new framework for a one-off check, reread an entire skill catalog, or repeat a passed check without a relevant invalidation reason.
- Preserve unrelated work. Consult `Plugins/GameFeatures/BreakawayCore/Docs/Agent_Testing_Ladder.md` when building, restarting the editor, or collecting multiplayer evidence.

Efficiency changes must preserve the target: final-public-alpha Breakaway gameplay parity, integrated placeholder art, and a working four-player listen-server LAN flow. Startup screening, headless runs, and same-process PIE each prove only their recorded scope.
