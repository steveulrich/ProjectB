# ProjectB working guidance

## Resume the vertical slice efficiently

- When working on the vertical-slice goal, start with `AI_Planning/SLICE_STATUS.md` and current Git HEAD/status. Follow the user's pause/resume state.
- Select one pending acceptance gate and define its expected observable result before editing. Read only the relevant evidence, source, and applicable skills.
- Use `rg` to locate symbols and narrow file ranges. Keep historical reports, full logs, and large skill references out of context unless needed for the current decision.
- Use the shared profiles and compact JSON helpers documented in `Scripts/Test/README.md`. Preserve full evidence under `Saved/Logs/`; report the outcome, first causal error, and evidence path.
- Run the cheapest applicable verification first. Compile a coherent set of related C++ fixes, then run affected tests. Package after those checks pass when packaged evidence is required.
- Reuse an existing candidate only when its recorded source, patch, assets, and configuration cover the tested change. Retest passed gates when relevant changes invalidate their evidence; complete the final end-to-end checks.
- Wait on an existing build or test process. Avoid duplicate launches and repeated unchanged log reads. Retry a failure after a change, a new hypothesis, or an explicit reproducibility check.
- Prefer structured bridge or CLI inspection where it answers the question. Use fresh UI observations for visual polish, physical input, frontend flow, and security dialogs.
- Keep progress updates concise and report meaningful changes. After a verified result, update the ledger's gate, evidence, invalidation conditions, and next action; put detailed history in the linked acceptance document.
- Preserve unrelated work. Follow `Plugins/GameFeatures/BreakawayCore/Docs/Agent_Testing_Ladder.md` for build, editor restart, and multiplayer evidence requirements.

Efficiency changes must preserve the target: final-public-alpha Breakaway gameplay parity, integrated placeholder art, and a working four-player listen-server LAN flow. Startup screening, headless runs, and same-process PIE each prove only their recorded scope.
