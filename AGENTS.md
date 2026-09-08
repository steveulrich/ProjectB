# ProjectB working guidance

## Resume the vertical slice efficiently

- When working on the vertical-slice goal, read `AI_Planning/SLICE_STATUS.md`, then run `Scripts/Test/Get-ResumeBrief.ps1` once for current Git, candidate, and process facts. Follow the user's pause/resume state.
- A report, planning request, or workflow edit while paused does not resume gameplay implementation. Complete that request, preserve the next action, and wait for the user to resume the goal.
- Select one pending acceptance gate and define its expected observable result before editing. Read only the relevant evidence, source, and applicable skills.
- Use `rg` to locate symbols and narrow file ranges. Keep historical reports, full logs, and large skill references out of context unless needed for the current decision.
- Default routine tool output to about 1,500 tokens per call. Filter or summarize on disk first; expand the limit when necessary to understand a failure. Read each applicable skill once per available context and load referenced material only as needed.
- Use the shared profiles and compact JSON helpers documented in `Scripts/Test/README.md`. Preserve full evidence under `Saved/Logs/`; report the outcome, first causal error, and evidence path.
- Run the cheapest applicable verification first. Compile a coherent set of related C++ fixes, then run affected tests. Package after those checks pass when packaged evidence is required.
- Reuse an existing candidate only when its recorded source, patch, assets, and configuration cover the tested change. Retest passed gates when relevant changes invalidate their evidence; complete the final end-to-end checks.
- Wait on an existing build or test process. Avoid duplicate launches and repeated unchanged log reads. Retry a failure after a change, a new hypothesis, or an explicit reproducibility check.
- After two attempts with the same failure and no new evidence, stop repeating that operation. Record the command, failure, attempted remedies, and condition for retry in the linked evidence document. While the goal is active, continue an independent gate if possible; preserve approval boundaries.
- Prefer structured bridge or CLI inspection where it answers the question. Use fresh UI observations for visual polish, physical input, frontend flow, and security dialogs.
- Put PIE Python inspections inside a function and return only serialized values. Never leave world, actor, widget, or component wrappers in interpreter globals; exceptions must also release them before travel or stopping PIE. If a probe fails with retained objects, clear its references or restart the editor before continuing the travel test.
- Keep progress updates concise and report meaningful changes. After a verified result, update the ledger's gate, evidence, invalidation conditions, and next action; put detailed history in the linked acceptance document.
- Keep the resume ledger under about 100 lines. Store commands and full reports by reference, and replace stale state instead of appending turn histories. Record build/package counts and retry reasons with each gate's evidence to assess workflow overhead.
- Use the gate record in `Scripts/Test/README.md` to preserve the expected result, evidence, retry condition, and next action before a pause or context handoff. Recheck process IDs and artifacts on resume; historical state is not current state.
- Save repeated diagnostic procedures as parameterized scripts after they prove useful. Do not build a new framework for a one-off check, reread an entire skill catalog, or repeat a passed check without a relevant invalidation reason.
- Preserve unrelated work. Follow `Plugins/GameFeatures/BreakawayCore/Docs/Agent_Testing_Ladder.md` for build, editor restart, and multiplayer evidence requirements.

Efficiency changes must preserve the target: final-public-alpha Breakaway gameplay parity, integrated placeholder art, and a working four-player listen-server LAN flow. Startup screening, headless runs, and same-process PIE each prove only their recorded scope.
