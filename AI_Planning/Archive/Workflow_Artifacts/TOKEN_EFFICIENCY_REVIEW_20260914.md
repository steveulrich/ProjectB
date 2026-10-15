# Token-efficiency review — September 14, 2026

## Scope

This review covers the last 12+ hours of ProjectB vertical-slice work and the buildable
placement continuation in candidate `LAN-20260914-02`. It measures context and workflow output,
not account-level token billing. Complete logs, patches, and JSON evidence remain under
`Saved/Logs/`; the machine-readable record is
`Saved/Logs/team-gameplay-20260912/workflow-token-audit.json`.

The continuation section extends this audit through packaged candidate `LAN-20260915-13`; older
candidate references remain historical evidence and are not the current artifact.

## Where context was being spent

- Repeated broad searches of long editor logs hid the one marker that decided a gate.
- Bridge console commands that required an editor-supplied world failed before reaching the
  intended PIE world, causing an avoidable retry.
- Buildable probes were split across separate lifecycles even though the observable sequence was
  short: cancel, spawn/quota, replication settling, and round reset.
- A reset probe called `StartRound` after the orchestrator had already moved to PostRound or
  PostMatch, so the result mixed phase timing with quota behavior.
- An invalid confirm left the placement spec active; a follow-up cancel attempted a second
  activation and produced a misleading harness failure.

## Changes applied

- Added `Get-BuildableFixtureBrief.ps1`, which parses spawn, quota, cancel, confirm, reset, peer
  convergence, and fixture-error markers into one bounded JSON record. Its default output is an
  inventory; `-IncludeSamples` is reserved for a changed replication diagnosis.
- Changed `bway.Test.AutoLockAllHeroes` to a no-world console command that resolves the
  authoritative PIE world internally. The command still refuses a missing or inactive selection
  phase.
- Made the buildable fixture record a 0.5-second transient sample and a 2.0-second settled
  sample, requiring all four PIE worlds at the settled point.
- Made round-reset diagnostics call `ResetRoundState` and each public
  `ABwayPlayerState::BeginRoundStatTracking`, separating persistent actor retention from quota
  reset without changing production encapsulation.
- Let the cancel fixture reuse an already active placement spec before issuing
  `LocalInputCancel`, so invalid-confirm cleanup can be exercised without a second activation.
- Added malformed target-data probes to the same native fixture surface and taught the brief to
  parse empty, wrong-type, and multi-entry terminal results without opening the full log.
- Made the brief resolve both bare filenames and explicit relative paths, eliminating a nested-path
  failure mode discovered during this run.
- Added a phase-aware replicated rejection fixture so one console invocation waits for Playing and
  ownership readiness before exercising the GAS target-data RPC; this replaces manual timing
  retries while retaining authority callback and no-spawn evidence.
- Added a `sourceIdentity` index to `New-SourceSnapshot.ps1`, allowing candidate provenance to be
  compared with one scalar before opening the complete patch, status, or untracked manifest.
- Updated `AGENTS.md`, `Scripts/Test/README.md`, `AI_Planning/WORKFLOW_EFFICIENCY.md`,
  `AI_Planning/SLICE_STATUS.md`, and the buildable verification document with these rules and
  their proof limits.
- Hardened `Tier1-CompileGate.ps1 -OutputJson` after a compile record was queried while UBT was
  still running. The helper now publishes a valid `status: running` record after launch and
  atomically replaces it with the final record, so an in-progress build cannot look like corrupt
  evidence or trigger a duplicate compile.
- Added `-SourceSnapshotPath` to `Get-ResumeBrief.ps1`. A contract-aware summary can now carry
  source identity, HEAD, dirty counts, and individual provenance hashes in the same decision
  packet as process and gate state.
- Made `Set-GatePhase.ps1` publish gate-contract changes through a temporary file and rename. A
  concurrent resume now sees either the prior complete contract or the new complete contract,
  never a half-written JSON file.
- Switched the active buildable gate to a stable `source-snapshot-current` path. Numbered
  snapshots are still kept for immutable history; deliberate `-Force` refreshes keep the ledger
  and next-resume command stable as workflow documentation evolves.
- Changed the packaged four-process harness to use client-owned result-screen exits and to keep the
  listen host alive until all clients finish. The first two attempts exposed a deterministic host
  shutdown race; the v3 fixture passed all three client result scopes without changing shipping code.
- Hardened the compile gate for a bundled UBT edge case: the .NET process returned 0 after UBT
  emitted `Result: Failed` and a C++ error. The gate now records the raw process exit separately,
  scans bounded explicit UBT/compiler failure signatures, and publishes an effective failure status.
  The original false-pass log is preserved beside the corrected compile record.
- Added a development-only delayed packaged shop probe. It waits for the local controller, opens the
  existing production shop widget, invokes its purchase handler, and verifies the data-driven catalog
  ID and owner state. Candidate 03 passed this scope without changing shipping behavior.

## Evidence and measured savings

- `compile-41m.json` passed the latest LyraEditor Win64 Development build after the phase-aware
  replicated rejection fixture and BF-031 bridge conversion repair.
- `editor-39v.log` and `buildable-fixture-39v-brief.json` show one native four-world PIE run with
  authority spawn, one-per-round quota rejection, cancel cleanup, persistent actor count, quota
  reset, and settled 4/4 peer convergence. The 0.5-second 1/4 sample is retained as replication
  settling evidence; the 2.0-second sample is `pass=1`.
- The same run reached a no-hit confirm with `active=1`, placement tag present, and quota unused.
  That is the expected invalid-target state; a valid physical target remains unproven.
- The buildable-specific brief reduced the saved marker projection from 3,619 to 1,851 characters
  (48.9%) while retaining the full editor log and fixture error count.
- The malformed probe brief retains three terminal records in a 1,252-character JSON projection
  while the full `editor-40c.log` remains available. Empty data reached `Valid=0`; wrong-type and
  multi-entry handles reached the callback but were rejected by the production shape check.
- `editor-41n.log` and `buildable-fixture-41n-brief.json` show the owning client submitting through
  GAS under `Net PktLag=100`; the authority received one entry, rejected it (`accepted=0`), kept
  the buildable count at 0, ended the ability, and cleaned up the test quota.
- Existing workflow measurements remain in the audit: `Get-GateBrief.ps1 -SummaryOnly` reduced a
  routine gate projection by 91.3%, and quiet unchanged log polling reduced its projection by
  31.0%. These are output reductions only; they do not weaken acceptance criteria.
- `Get-ResumeBrief.ps1 -SummaryOnly -GateContractPath ... -ContractPhase runtime` now returns
  candidate identity, `runtime-pass-unverified`, four proof gaps, and zero owned/hidden processes
  in one bounded result.
- `source-snapshot-buildable-41t.json` records the current source identity plus the complete
  individual hashes and provenance files; the audit now points at this newest snapshot.
- The atomic-record validation compile `compile-42e-atomic.json` passed `LyraGame` Win64
Development with no partial evidence files. Combining the runtime resume and source snapshot
projections measured 3,300 characters versus 4,552 characters as two separate reads (27.5%
  less routine context); the complete snapshot remains persisted.
- The final gate-contract update and a contract-aware resume both parsed successfully after the
  atomic publication change; the runtime record still reports four proof gaps and zero owned
  ProjectB processes.
- The current provenance snapshot `source-snapshot-current.json` matches the final tracked source
  identity used by the resume packet; its stable path is recorded in both the ledger and gate
  contract.
- Candidate `LAN-20260915-01` passed Cook/Stage/Pak, direct natural flow, structured frontend-host
  flow, and the changed four-process transport/results fixture. The v3 compact inventory is
  count-only in routine context and retains four full logs; all client logs have zero unknown or
  critical errors. The host's cleanup exit `-1` is recorded as harness teardown, not a gameplay
  failure. This prevents repeating the v1/v2 race while preserving the final UI and second-PC gaps.
- Candidate `LAN-20260915-03` passed the delayed packaged shop probe: the real widget purchase path
  changed `AttackStrength` to rank 1 and gold from 500 to 425, with zero runtime/unknown/critical
  error lines. The first immediate-command attempt and the hard-coded-ID result are retained as
  changed setup diagnoses rather than rerun failures.
- `compile-shop-command-raw-failure.log` captures the original UBT `Result: Failed`/C++ error that
  incorrectly accompanied process exit 0; `compile-shop-command-fixed.json` and `compile-shop-id.json`
  show the corrected gate records with empty `failureSignals` and effective exit 0.
- `Package-Preflight.ps1 -CompiledTargetRecord` now requires the `failureSignals` field to be present
  and empty before it admits a skip-build package. The candidate 03 admission record passed with the
  corrected compile, while the restricted AutomationTool ACL probe remains an explicit setup blocker
  when `-RequireAutomationToolLogWrite` is requested.
- `Get-ResumeBrief.ps1 -SummaryOnly` now bounds the repeated ledger markdown next-action field to
  360 characters plus a full-snapshot pointer, cutting the observed field from 1,020 to 374
  characters at a word boundary. The full ledger line stays in the persisted resume JSON, while the
  contract's `gate.nextAction` remains the complete action used for decisions.
- A synthetic pre-change compile record with `failureSignals` removed was rejected by package
  preflight (`ready=false`), proving the new admission rule fails closed before an expensive cook.

## Further opportunities reviewed

- A single bridge command could dispatch all three malformed probes. I left that deferred because
  separate command markers make it easier to attribute a payload kind and preserve a clean retry
  boundary if one probe changes the active ability state.
- A contract-aware buildable brief could merge the settled lifecycle and malformed-only records.
  I left that deferred because the existing gate brief already joins both JSON records and keeping
  the specialized parser single-log keeps its scope and failure semantics clear.
- Hash-keyed acceptance caching remains deferred. The cache key would need source patch, assets,
  configuration, unsaved editor state, and staged artifact identity; a HEAD-only shortcut could
  reuse invalid evidence.
- A single all-in-one gate runner remains deferred. The compile, runtime, and package phases have
  different process owners and proof scopes; joining them would make a setup failure harder to
  attribute and could encourage a long retry. The bounded resume packet is the safer merge point.

## Next iteration protocol

1. Read `SLICE_STATUS.md` once and run the contract-aware summary resume command once, adding
   `-SourceSnapshotPath` when the gate has a provenance snapshot.
2. Reuse the latest source compile `compile-41m.json` and the settled lifecycle evidence in
   `buildable-fixture-39v-brief.json`; do not rerun the native
   fixture without a relevant source/configuration change.
3. When Computer Use exposes a targetable window, prove a valid physical target confirm. Empty,
   wrong-type, multi-entry, and client-to-server quota rejection under latency are already recorded
   by the native fixtures.
4. Promote the gate to packaged UI/LAN after the native proof is complete. Reuse candidate
   `LAN-20260915-09` for the passed remote packaged shop/RPC scope, and retain candidate
   `LAN-20260915-01` as the natural/transport baseline; either reuse is valid only while its
   source, configuration, staged binary, and asset provenance match.
5. Use `Get-BuildableFixtureBrief.ps1` once after each changed fixture run; expand raw samples only
   when its pass fields, first fixture error, or scope changes the diagnosis.
6. For a compile with `-OutputJson`, wait on the record's owned process while its status is
   `running`; read the final record only after it becomes `passed` or `failed`, then inspect the
   compiler log only if the compact record identifies a failure.
7. Apply phase updates through `Set-GatePhase.ps1`, then run the resume summary after the command
   returns. Treat a parse failure as an evidence-publication problem to diagnose once, not as a
   reason to relaunch the gate.
8. Keep the active gate's provenance path stable. Refresh `source-snapshot-current` deliberately
   after tracked edits, and reserve numbered snapshots for attempt history or changed diagnoses.
9. For packaged four-process transport, use client-owned result exits and terminate the host only
   after all clients prove results. If a client times out, inspect the compact per-process inventory
   and the v1/v2 ordering before changing the fixture; do not relaunch a host-PostMatch shutdown
   attempt unchanged.
10. For packaged widget/RPC probes, dispatch only after controller/world readiness, use a
    data-driven catalog ID for verification, and preserve physical input as a separate gate. Treat
    a process-zero compile with explicit UBT failure signatures as failed before staging any package;
    require the compile record's `failureSignals` field so a pre-change record cannot bypass the
    admission check.
11. Keep compact resume output bounded: use the contract next action for decisions and open the
    persisted full snapshot only when the ledger wording changes the diagnosis.

Physical input, normal human selection, packaged buildable UI/LAN, second-PC LAN, and full
end-to-end acceptance remain open. Computer Use was unavailable for this run, so no UI claim was
made.

## Continuation findings — September 15, 2026

The packaged remote-shop gate added several concrete token and retry lessons while preserving
the same production proof. Candidate 04 exposed a false compile admission: direct UBT accepted a
relative project path, returned process exit 0, and wrote `Result: Failed` to runner stdout. The
compile helper now resolves a relative input against the configured project root before admission,
records both the original input and the normalization flag, and scans the requested log plus
captured stdout/stderr for explicit UBT/compiler failure signatures. Package preflight requires
the resulting `failureSignals` field to exist and be empty, so a stale process-zero record cannot
trigger another Cook/Stage/Pak run.

Candidate 05 showed that a client `FAutoConsoleCommandWithWorld` can bind to the frontend world
before travel; its timer is destroyed before gameplay. The probe now uses a no-world delegate and
one core-ticker poll that resolves the post-travel client world from its local controller and
upgrade component. Candidate 06 then showed that a hard-coded 425 balance was too strict because
passive gold can tick during the RPC. The assertion now reads the catalog's first rank cost and
compares pre/post gold within a five-point bounded income window. Candidate 08 exposed a raw
`UWorld` verification-timer crash after widget processing; the verifier now captures only a weak
widget and runs on the core ticker. Candidate 09 passed the real remote purchase (`AttackStrength`
rank 1, 500→425, client exit 0, host teardown separated), while its first wrapper read missed the
marker because Unreal emits `LogTemp: Display:`. The fixed matcher anchors on an emitted log
category, so command-line `-testexit` text cannot become a false positive. The reusable
`Scripts/Test/Get-PackagedShopProbeBrief.ps1` now performs this extraction from an existing
host/client pair and persists one compact JSON result without relaunching the artifact.

These attempts cost three extra package lifecycles, but each changed the source or evidence
hypothesis. The final wrapper reused the already valid candidate 09 artifact and avoided another
~95-second Cook/Stage/Pak run. The complete failed logs remain preserved; no shipping behavior
changed because all shop commands are compiled under `!UE_BUILD_SHIPPING`.

### Applied next-iteration rules

1. Run `New-SourceSnapshot.ps1` once after deliberate source/ledger edits, then use its scalar
   `sourceIdentity` in the compile and package preflight. Do not rebuild on a documentation-only
   change when the candidate provenance still matches.
2. Prefer an absolute project path for direct UBT. The compile gate canonicalizes relative inputs
   against the configured project root and records `projectFileWasRelative` so path provenance is
   visible. Treat `failureSignals` as authoritative even when the process exit is zero, and inspect
   stdout/stderr only when the compact record reports a signal.
3. For packaged client startup commands, use a no-world delegate, poll for a post-travel gameplay
   world, and let the lasting world own timers. Never bind a time-sensitive probe to the frontend
   world.
4. Derive expected values from runtime data (catalog IDs and costs), capture the immediate
   baseline, and allow only a documented bounded replication/income tolerance.
5. After `ProcessEvent` or travel-sensitive UI work, verify through weak UObject references and a
   ticker or resolved live world. Do not schedule a callback through a raw world pointer whose
   lifetime may end in the same frame.
6. Match evidence against emitted log categories (`LogTemp: .*`, `LogExit: .*`) and keep the
   command-line arguments out of marker patterns. Persist wrapper output as JSON without rerunning
   a passed artifact merely to reconstruct its console response.
7. The current resume command is `Get-NextIterationBrief.ps1 -GateContractPath ...
   -ContractPhase package -SourceSnapshotPath ... -WorkflowValidationPath ...`; it writes the
   full resume packet and underlying snapshots itself. Add the explicit refresh switches only
   after deliberate metadata edits; keep routine context to the bounded projection.

### Additional applied optimization — compile path admission

The earlier review required callers to pass an absolute project path, but the gate itself still
accepted a relative value and only converted it while writing the result record. That left a
cheap-to-detect ambiguity until UBT had already started. `Tier1-CompileGate.ps1` now resolves
relative inputs against `BwayTestConfig.psd1`'s `ProjectRoot` before the existence check and UBT
launch, while recording `projectFileInput` and `projectFileWasRelative` in both running and final
records. A `-ValidateOnly -DirectUbt -ProjectFile ProjectB.uproject` check passed, resolved to the
canonical ProjectB file, and launched no build. This closes a retry class without changing build,
package, runtime, or acceptance criteria.

### Additional applied optimization — candidate consistency

The final package continuation exposed a metadata mismatch: the gate contract selected candidate
10 while the compact ledger still selected candidate 09. That mismatch could send a resume turn to
the wrong staged logs and spend context rechecking a frozen scope. The ledger now names candidate
10, and `Get-ResumeBrief.ps1` compares the ledger and contract candidates, includes the contract
candidate in the gate projection, and warns before artifact reuse when they differ. The guard only
prevents ambiguous selection; source snapshots, asset/configuration review, compile admission, and
all runtime/package proof requirements remain unchanged.

The refreshed current snapshot is persisted at `Saved/Logs/buildable-placement-20260914/source-snapshot-current.json`.
Candidate 10 remains frozen at its package-time identity and is qualified for the hero-reselect
package scope; candidate 09 remains qualified only for its earlier remote-shop scope. This keeps
routine resume context on one current snapshot and one selected artifact brief while preserving the
complete provenance and logs on disk.

### Additional applied optimization — compact workflow hygiene

The final validation itself had become a repeated inline PowerShell pipeline: parse every test
script, parse selected JSON, run `git diff --check`, reconcile the candidate, and inspect owned
process counts. `Scripts/Test/Get-WorkflowValidationBrief.ps1` now performs that workflow-only check
in one bounded invocation and writes a compact JSON record. It reports missing or malformed records
as warnings, so shortening the conversation output cannot hide a broken evidence file. The helper
does not compile, launch, inspect UI, or change any acceptance gate.

### Additional applied optimization — one-command next-iteration packet

The audit still left one repeated context path: a resume call and a workflow-hygiene call each
returned the same candidate, source, process, and proof-gap fields after a deliberate metadata
change. `Scripts/Test/Get-NextIterationBrief.ps1` now composes the contract-aware resume result and
the optional workflow-hygiene result into one bounded packet. It persists the underlying records,
reports stale or missing hygiene as a warning, and exposes the selected phase's retry condition and
next action in the same decision object.

The packet has two explicit refresh switches. `-RefreshSourceSnapshot` updates the stable
provenance snapshot after deliberate source or ledger edits; `-RefreshWorkflow` reruns the parser,
JSON, diff, candidate, and process checks after contract, snapshot, ledger, or helper edits. With
both switches omitted, an unchanged gate reuses the last hygiene record and reports any identity
drift instead of silently treating it as current. The helper is workflow inventory only and does
not compile, launch, inspect UI, or establish acceptance.

This removes one routine tool-response boundary and avoids retyping the next action while keeping
the full evidence needed to diagnose a changed gate. It does not merge compile, runtime, package,
physical-input, LAN, or end-to-end acceptance phases.

### Additional applied optimization — source and helper freshness guard

The final review found one remaining way to spend a full gate on stale context. A dirty file can be
edited in place without changing the commit, so a snapshot's HEAD and file counts can remain
unchanged. Likewise, a workflow record can predate a change to one of the test helpers while still
matching the old source identity. `Get-NextIterationBrief.ps1` now hashes the current tracked patch,
worktree status, and untracked manifest and compares those hashes with the recorded source snapshot.
`Get-WorkflowValidationBrief.ps1` records a sorted content hash for every test PowerShell helper;
the packet compares that inventory on routine resumes.

The guard is deliberately conservative. It adds one bounded freshness object and a warning that
requires `-RefreshSourceSnapshot` or `-RefreshWorkflow`; it does not skip a build, invalidate an
acceptance result, or replace any runtime evidence. Positive and negative checks passed in
`Saved/Logs/buildable-placement-20260914/workflow-freshness-guard-measurement.json`: the current
packet is fresh, an altered patch hash is rejected, and an altered helper inventory is rejected.

### Updated next-iteration protocol

12. Treat `sourceSnapshotFreshness.fresh=false` or a workflow script-inventory warning as a
preflight stop. Refresh the affected record with the explicit packet switch before compiling,
packaging, or interpreting candidate evidence. This catches in-place dirty-file edits without
adding a broad log read or weakening the provenance requirement.

13. When the gate contract includes `candidateSourceIdentity`, treat a packet warning about a
candidate mismatch as a scope review, not as permission to launch the frozen artifact for a newer
source state. Reuse it only for its recorded frozen scope; create a new candidate for current-source
packaged work. This closes the HEAD-only artifact-selection gap without adding a package lifecycle.

### Additional applied optimization — lifecycle fixture admission and projection

The open rematch-reset requirement exposed one more repeat cost: a lifecycle run could be launched
before its prerequisite purchase and nonzero match-stat state existed, then require a broad editor-log
read to distinguish setup failure from a travel failure. The development-only
`bway.Test.MatchUpgradeRematchReset` command now admits the fixture only after it finds a real owned
upgrade and nonzero match stats, then waits for a fresh authoritative game state and checks every
destination wallet, upgrade list, and match-stat snapshot. `Get-MatchUpgradeLifecycleBrief.ps1`
reduces the result to marker counts, setup-blocked state, first causal error, unknown errors, critical
lines, and scope. A synthetic pass and the source compile passed; runtime proof remains correctly
open until the fixture is run after a real completed match.

14. For a changed lifecycle fixture, perform prerequisite admission inside the development command
and run the dedicated compact brief once after the log exists. Do not spend a second editor lifecycle
or a full-log context transfer on an impossible setup. Keep the full log and the packaged/physical
acceptance gaps separate; this optimization only removes redundant setup and log inspection.

The first lifecycle attempt also confirmed two existing retry guardrails. The game-target compile
passed after the ticker delegate fix in `compile-match-upgrade-lifecycle-20260915-r2.json`. The
editor-target admission first stopped on an engine ACL, then the elevated retry exposed the known
UE 5.8 shared-JSON-key regression in `FoliageHandlers.cpp` and `LevelHandlers.cpp`; restoring the
explicit `FString(*Key)` conversions produced the clean `compile-editor-match-upgrade-lifecycle-20260915-r3.json`.
The same editor lifecycle reached four-world Playing, but its remote combat setup timed out before
an upgrade purchase, so `match-upgrade-lifecycle-brief-20260915.json` is intentionally unverified.

15. When an editor compile is required after a game-only compile, check the editor binary's source
age before launching. If UBT reports an ACL setup failure, use the authorized elevated retry once;
if that reveals a known bridge regression, fix the exact conversion and rerun. Do not launch a stale
editor or repeat a timed-out lifecycle without changing the setup hypothesis.

### Additional applied optimization — compile startup truthfulness

The corrected lifecycle run found that UBT can exit 0 after failing during startup while rotating
its trace or generated intermediate files. The prior Tier1 pattern set did not classify every such
exception, so it could admit a stale editor binary and waste an editor/test lifecycle. The helper
now treats unhandled exceptions, `UnauthorizedAccessException`, access-denied path writes, and
failed generated-file renames as authoritative `failureSignals` across the bounded log, stdout, and
stderr sources. The full startup log remains available for diagnosis, while the next resume sees a
compile failure immediately and requests the one authorized ACL retry.

The fresh editor compile then passed with empty signals and advanced the runtime DLL timestamp. The
new development-only `bway.Test.MatchUpgradeLifecycleSeed` fixture admitted a real catalog-backed
purchase and stat, reached PostMatch through production round APIs, invoked production rematch
travel, and passed the destination reset check for eight players (`match-upgrade-lifecycle-brief-20260915-r3.json`).
This proves the same-process reset scope; packaged LAN, physical UI, and second-PC gates remain
separate.

The regression measurement `tier1-startup-failure-signal-measurement-20260915.json` matches the
access-denied startup text in the earlier process-zero record, demonstrating that the new bounded
patterns catch the previously admitted false pass without rerunning the editor lifecycle.

16. Treat a Tier1 record with any startup exception or access-denied signal as failed even when
`processExitCode` is zero. Confirm the editor/game binary age before launching, and preserve the
generated trace backup when elevated cleanup is needed so the next build has a clean trace path.

### Additional applied optimization — packaged candidate provenance

The first current-source package attempt also exposed a small evidence-shape mismatch: the package
script wrote records inside a candidate directory, while `Get-ResumeBrief.ps1` intentionally reads
the canonical `codex-packaged-<candidate>-*.{txt,patch,csv,log}` prefix. Candidate 11's records were
copied to that canonical prefix and the script now writes there directly, so one resume call proves
the executable, source head, exit code, patches, and hash inventory without a directory-shape retry.
The current-source candidate then passed Cook/Stage/Pak and a NullRHI match smoke; its packaged
buildable UI/LAN, physical input, second-PC, and clean-device gaps remain explicit.

17. Keep every package candidate's required resume records at the canonical prefix expected by
`Get-ResumeBrief.ps1`; a directory may hold supporting evidence, but do not make the resume helper
discover it. Run package preflight with the current `LyraGame` compile record before Cook/Stage/Pak,
then record one bounded packaged smoke brief.

### Additional applied optimization — packaged transport verdict persistence

The four-process listen-server harness initially printed its verdict only to the console. When the
long-running process output was truncated, recovering the result required reconstructing state from
four large logs, and a second package lifecycle would have been an unnecessary temptation. The
harness now writes a compact result JSON beside the flow brief and prints only status, candidate,
attempt, and result path. Candidate 11's completed run was persisted as
`packaged-LAN-20260915-11-fourprocess-v2-result.json`: the host reached a listen marker, all three
clients reached the Results screen with exit code 0, and host termination is recorded separately as
owned teardown. This preserves the same transport evidence while making a context truncation
recoverable with one bounded read.

18. Every multi-process harness must persist its final verdict and teardown classification before
returning. Keep process-level exit status, client/host acceptance status, and owned cleanup status
as separate fields; a deliberately terminated listen host must not turn a successful client run into
a false failure. Read the result JSON and one count-only flow brief on resume instead of recovering
the verdict from raw logs or rerunning the artifact.

19. Treat transport, UI/input, and natural-play evidence as separate projections of one candidate.
Record the narrow transport pass immediately, retain the UI/physical/second-PC proof gaps in the
same contract, and avoid reopening a passed transport run when only a UI channel becomes available.
This reduces future context and process churn without relaxing the final acceptance gate.

### Additional applied optimization — provenance encoding normalization

Refreshing the current source snapshot exposed a false stale warning caused only by the UTF-8 BOM
written by Windows PowerShell 5.1. The status lines and untracked manifest were identical, but their
byte hashes differed between the snapshot helper and the resume helper. `Get-NextIterationBrief.ps1`
now removes an optional BOM and normalizes line endings before comparing the three provenance files.
The refreshed packet reports `sourceSnapshotFreshness.fresh=true`; real HEAD, patch, status, and
untracked changes still produce a stale result. This avoids a needless refresh command or an
expensive gate caused by shell encoding rather than source drift.

20. Compare provenance files through a documented canonical text hash (BOM-free, normalized line
endings) while retaining the original byte hashes and full files for audit. Treat a genuine content
or HEAD change as stale, but do not spend a retry on a PowerShell encoding difference.

### Additional applied optimization — packaged fixture dispatch and phase admission

The next packaged replication attempt found a setup failure that was invisible in the process
verdict: a semicolon-delimited `-ExecCmds=Net PktLag=200; bway.Test.TeamCombatRemoteScoreDelayed`
started the host but did not reliably invoke the fixture. The runner now sends the cvar and fixture
as separate `-ExecCmds` arguments and persists the exact argument array in its result JSON. This
turned a missing-marker diagnosis into one controlled rerun without reopening the package or reading
all four logs.

That run then showed the fixture was selecting actors during Warmup. A real combat event could kill
one of the selected players before pass and goal assertions, producing a misleading partial result.
The development command now waits for authoritative `Playing` before actor selection, using one
bounded 0.5-second retry loop. Candidate `LAN-20260915-13` then observed every remote-owner score
marker at 200 ms latency, exact team/player deltas, cleanup, and three client Results exits. The
compact brief still reports one unclassified `GCNL_Death` Blueprint `Accessed None` warning in both
bot-filled and four-human attempts; that warning remains visible as a polish lead and is not hidden
to manufacture a pass.

21. For staged commands that combine cvars and fixtures, use separate `-ExecCmds` arguments and
persist the full argument list with the compact verdict. Admit authority fixtures only after their
required gameplay phase, and stop after one isolated configuration retry when the same unclassified
asset warning remains. This removes dispatch and warmup retries without relaxing marker, error, or
physical/LAN acceptance requirements.

### Further opportunities reviewed after the 12-hour audit

The remaining worthwhile savings are evidence-index improvements rather than acceptance shortcuts:

- A candidate evidence index could map each candidate and gate phase to its preflight, compile,
  result, and brief files. It would replace manual filename assembly during resume, but should be
  added only once the package and UI gates settle so it does not become another mutable source of
  truth.
- A warning ownership map could attach recurring Blueprint warnings to an asset and first-seen
  candidate. This would let the brief distinguish a known, separately tracked polish issue from a
  new regression while retaining total and unknown counts. It should be implemented only after the
  `GCNL_Death` asset warning is inspected, so a broad allowlist cannot hide a real defect.
- The current next-iteration packet still requires an explicit refresh after deliberate docs or
  helper edits. Automatic refresh would save one command, but would also erase the useful stale
  provenance stop before an expensive gate; the explicit switch is the safer quality-preserving
  choice.
- A single compile/package wrapper would shorten the conversation, but the phases have different
  process owners and permission boundaries. Keeping compile and package records separate makes ACL
  and `failureSignals` failures cheap to diagnose and avoids long all-in-one retries.

These opportunities remain intentionally deferred. The applied rules above are sufficient to reduce
routine context and setup retries while preserving the final-public-alpha parity bar, integrated
placeholder art, four-player listen-server LAN target, and every open physical/UI proof gap.
