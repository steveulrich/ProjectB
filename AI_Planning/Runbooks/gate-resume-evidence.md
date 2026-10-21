# Resume gates and preserve evidence

Use this runbook for active vertical-slice implementation and acceptance testing. Read only the sections needed for the current decision. Documentation or planning while paused does not resume gameplay work. All command paths are relative to the project root unless an absolute path is supplied.

## Resume the selected gate

1. Read [SLICE_STATUS.md](../SLICE_STATUS.md), then run `Scripts/Test/Get-NextIterationBrief.ps1 -GateContractPath <contract> -ContractPhase <compile|runtime|package>` once with the selected contract and phase. Do not rely on a stale helper default. It combines the contract-aware resume inventory with candidate, provenance, processes, proof gaps, next action, source-snapshot freshness, workflow-helper freshness, and candidate artifact identity. Complete records remain on disk.
2. After deliberate source, ledger, contract, snapshot, or helper edits, add `-RefreshSourceSnapshot -RefreshWorkflow`. Treat either freshness warning, including `sourceSnapshotFreshness`, or `candidateIdentityMismatch` as a preflight stop for artifact reuse. Review the frozen scope and reconcile the mismatch before proceeding.
3. Keep the ledger's `**Current candidate:**` line synchronized with the selected gate contract's `candidate`. Resolve any mismatch before opening artifact logs or reusing a package.
4. Select one pending acceptance gate and define its expected observable result before editing. Read only the source, evidence, and skills relevant to that gate. After passing it or recording a blocker, continue the next authorized gate while the goal is active.

Use `Get-ResumeBrief.ps1` directly only for a narrower follow-up. On a gate resume, pass `-GateContractPath <contract> -ContractPhase <compile|runtime|package>`. When source provenance affects the decision, also pass `-SourceSnapshotPath <snapshot>` to include the stable source identity and individual hashes in the same bounded packet.

For routine follow-ups, add `-SummaryOnly`. The full snapshot remains in `Saved/Logs/codex-resume-brief.json`; the console shows the selected gate, process, and count projection. Prefer ProjectB-owned process records and counts. Use `-IncludeAllProcesses` only when ownership itself is the diagnosis, and the full default projection only when ownership or dirty-file detail is needed. The summary bounds the duplicate ledger next-action text; use the contract's `gate.nextAction` for the complete action. Open `fullSnapshot` only when ledger detail changes the diagnosis.

## Establish preflight and provenance

Before an expensive gate, record source revision, launch paths, process inventory, requested and observed topology, bridge readiness, and UI-channel availability. Stop before launch when the required scope cannot be proved. Recheck process IDs and artifacts on resume; historical state is not current state.

With staged, unstaged, or untracked inputs, run `Scripts/Test/New-SourceSnapshot.ps1` once before the expensive gate. Persist the complete tracked patch and untracked manifest. Generic snapshots record untracked names, not their complete source contents: explicitly archive untracked build source as an additional provenance artifact. Compare the bounded `sourceIdentity` first; inspect individual hashes or full files only when that identity or a required artifact/configuration check differs.

Keep one stable per-gate `source-snapshot-current` path for resume. Use `-Force` only after deliberate source and ledger edits are complete. Use numbered prefixes when immutable attempt history is needed. `Get-NextIterationBrief.ps1` uses a BOM-free, normalized-line-ending hash for text freshness; the snapshot's original byte hashes and complete files remain the audit record.

Reuse a candidate only when its recorded source, patch, assets, and configuration cover the tested change. Relevant input changes invalidate prior proof: retest the affected passed gates and complete the final end-to-end checks. Workflow efficiency does not reduce the target of final-public-alpha Breakaway gameplay parity, integrated placeholder art, and a working four-player listen-server LAN flow. Startup screening, headless automation, and same-process PIE prove only their recorded scopes.

## Maintain the contract and ledger

Create one process/evidence contract per gate and advance the same manifest through compile, runtime, and package phases. Use the gate record in [Scripts/Test/README.md](../../Scripts/Test/README.md) to retain the expected result, evidence, retry condition, and next action before a pause or handoff.

Use `Scripts/Test/Set-GatePhase.ps1` to write phase fields atomically. Preserve partial results with explicit `scope`, `proofGaps`, and `nextAction`. Supply an overall status only when the gate state intentionally changes. When a selected phase has no current causal error, persist `firstCausalError: null`; it prevents `Get-GateBrief.ps1 -SummaryOnly` from resurfacing an older error.

After a verified result, update the ledger's gate, evidence link, invalidation conditions, and next action. Keep detailed commands, reports, attempt history, build/package counts, and retry reasons in the linked acceptance document and contract. Replace stale state instead of appending turn histories.

Keep root `AGENTS.md` at no more than 6,000 characters and `SLICE_STATUS.md` at no more than 4,000 characters. Line count alone is not a sufficient budget. Detailed acceptance and animation documents may exceed 100 lines; preserve their evidence and link to it. Never delete evidence to satisfy an index or ledger budget.

After contract, snapshot, or candidate metadata edits, run `Scripts/Test/Get-WorkflowValidationBrief.ps1` once. Its compact checks cover script parsing, JSON parsing, diff, candidate consistency, and owned processes. These checks are workflow hygiene and do not replace compile, runtime, UI, LAN, or acceptance evidence.

## Read the smallest useful evidence

Consult the shared-helper README for documented invocations and compatibility aliases. Use the documented invocation once instead of probing alternate parameter names. Preserve full evidence under `Saved/Logs/` and report outcome, first causal error, counts, scope, and evidence path.

Use `rg` to locate symbols and narrow file ranges. Default routine output to about 1,500 tokens per call; filter or summarize on disk first. Expand only when needed to understand a failure. Read each applicable skill once per available context, and load references only for the current decision.

Raw logs, full snapshots, and archives are not routine inputs. Start with bounded briefs. A brief that identifies a changed diagnosis can justify the relevant narrow raw range, full contract, persisted brief, or archived record. Do not ingest whole histories to reconstruct an unchanged verdict; keep raw evidence on disk.

After the gate starts, use `Scripts/Test/Get-GateBrief.ps1` for selected JSON/log evidence. Select contract evidence with `-ContractPhase compile|runtime|package|all` instead of retyping paths. Gate briefs deduplicate paths and project nested details into scalar summaries. Add `-SummaryOnly` for routine follow-ups; expand only when a changed diagnosis needs peer-level detail.

Use the relevant bounded projection:

| Evidence | Routine projection | Expand only when needed |
| --- | --- | --- |
| Packaged flow | Count-only brief; add `-OutputJson` for gate evidence | A small event tail for an ordering question |
| Runtime presentation | Pass, scope, count, and evidence path | Verbose peer detail for a changed diagnosis |
| Startup screening | `New-AcceptanceSummary.ps1 -SummaryOnly -OutputJson <path>` | Per-log rows for the changed diagnosis |
| Repeated attempts | `Scripts/Test/Get-RunFamilyBrief.ps1` before individual logs | Latest or causally relevant log, selected by family counts and error signatures |

## Poll and classify failures

For more than one poll of a live log, use `Scripts/Test/Get-LogDeltaBrief.ps1` with a gate-owned state file. The first call establishes the offset; later calls return appended errors, critical lines, and requested markers. Add `-QuietUnchanged` when no transition is expected and `-OutputJson` when the poll is gate evidence. Use `-Reset` only after log rotation or for a new evidence file. Retain full-log briefs in the acceptance record.

Wait on an existing build or test process; avoid duplicate launches and repeated unchanged reads. Retry only after a change, a new hypothesis, or an explicit reproducibility check. After two attempts with the same failure and no new causal evidence, stop that operation. Record the command, first causal error, attempted remedies, and retry condition in the contract and linked evidence document. Continue an independent authorized check when possible; preserve approval boundaries. The [build and package runbook](build-package.md#respect-per-gate-caps) owns the additional hard build/package caps.

Treat the recurring Zen security-configuration message as known nonfatal only when the compact brief classifies it. Pixel Streaming target-window errors remain actionable for rendered/UI gates; an authority-only combat fixture may classify that message within its own scope. Preserve total error counts, unknown-error counts, and critical lines. Never suppress or relabel an unclassified error.

Save a repeated diagnostic procedure as a parameterized script after it proves useful. Do not create a framework for a one-off check or repeat a passed check without a relevant invalidation reason.
