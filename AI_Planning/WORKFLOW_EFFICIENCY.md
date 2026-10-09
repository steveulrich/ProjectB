# Workflow efficiency contract

Reviewed September 14, 2026 after the September 8–14 implementation and verification sequence. This document records how to reduce repeated context and tool work while keeping the final-public-alpha parity bar and evidence scope unchanged.

## What the last 12+ hours showed

The work advanced through a dense chain of source fixes and acceptance records: the latest commits ran from `bb7d2aca` through `d194fd8c`, covering frontend travel, phase cleanup, bot ownership, footsteps, hero visuals, team assignment, request lifecycle, and screen-space request UI. The ledger was 69 lines at the first audit and remains below the 100-line cap; the worktree had 37 unrelated dirty entries. The final request-UI gate required one build, a three-peer audit, a four-peer fixed run, two editor lifecycles, material fixtures, physical input, death/respawn, remote rendering, cleanup, and a targeted commit.

The quality was good, but the process spent avoidable tokens and wall time in five places:

1. Broad `Get-Content` calls pulled large skills, historical acceptance documents, and log tails into context. One diagnostic call emitted 27,859 tokens and was truncated before the useful part.
2. The same state was restated in prose, the ledger, evidence JSON, and tool output. The authoritative JSON existed, but it was not consistently used as the only input to the next decision.
3. Runtime diagnostics were sometimes loaded or redeclared in a live interpreter. That created stale callback risk, a retained-wrapper risk, and one viewer binding error that required extra recovery work.
4. Computer Use actions occasionally reused stale window state or screenshot IDs. Fresh selection and state capture eventually solved it, but failed retries added no evidence.
5. Editor/build setup was repeated for adjacent checks, and one editor was left running by an interrupted probe. A single gate manifest and explicit process ownership would have prevented the extra lifecycle.

## Follow-up audit from September 12, 2026

The next gameplay gate exposed five additional costs. Each one had a low-cost prevention that keeps the acceptance bar intact:

1. The bridge launcher could not resolve the source-built engine, so one failed launch path preceded a working direct launch. The repository now has `Scripts/Test/Start-ProjectBEditor.ps1`, which resolves the verified executable, refuses a duplicate ProjectB editor, supports a no-launch validation pass, records the process and log, and preserves the exact launch arguments.
   The engine source shows that `-Multiprocess` skips the editor's `ValidatePlatforms -AllPlatforms` startup call, and `-Unattended` skips Turnkey's `VerifySdk` launch. The editor's own DDC error recommends `-DDC-ForceMemoryCache` when no writable cache node exists. The helper exposes these as `-SkipPlatformValidation`, `-SkipTurnkeyValidation`, and `-ForceMemoryDDC` for runtime-only gates after separate Win64 SDK and cache evidence; packaging and platform gates retain the default path.
2. A four-player setting was present, but the first runtime snapshot proved only one authority world. The gate contract did not admit topology before the expensive run. Future gates record topology settings and observed peer worlds in preflight, then reject a scope mismatch before interpreting gameplay output.
3. A reflected ability query was unavailable. The call was useful once, but repeating an unsupported API would spend tokens without adding evidence. Future probes record supported and unsupported bridge/reflection operations in a session capability cache and select a supported inspection path once.
4. The computer-use service was unavailable in this session. A missing UI channel cannot produce physical-input evidence, so future gates record `uiChannel: unavailable` once and continue only with structured checks whose scope is still valid.
5. The gate manifest stayed at `compile-pass` after runtime observations existed. Future gates advance the manifest after compile, runtime, and packaged phases, including partial results and proof gaps. This keeps the next decision machine-readable without rereading the log.

## Additional follow-up optimizations

The September 12 continuation exposed three smaller sources of repeat work. They are now handled by the shared helpers rather than by agent memory:

1. `Start-ProjectBEditor.ps1` probes a supplied Zen directory before launching Unreal. An access-denied cache path now fails during `-ValidateOnly`, and the launch record includes the source snapshot plus `zenDataPath` and `zenWritable`. This prevents spending an editor lifecycle on a cache lock failure or repeating a Git/status read just to identify the launch input.
2. `Get-GateBrief.ps1` accepts `-ContractPath` as an alias and automatically includes that record in the compact inventory. The previous unsupported-parameter detour is therefore removed without changing the evidence schema.
3. Resume and gate briefs now identify ProjectB processes from their command lines. `hiddenEditorCount` and `projectProcessCount` no longer treat an unrelated Unreal project as a duplicate or a hidden-editor stop condition; the full known-process list remains available for build-host diagnosis.
4. `Set-GatePhase.ps1` writes phase status, scope, evidence, proof gaps, and next action in one bounded operation. This removes repeated manual JSON edits while requiring an explicit overall status when the gate state changes.
5. Temporary PIE flags and cheats are restored as part of cleanup and checked in the closed-editor INI. This prevents a blocked fixture from contaminating the next gate or forcing an extra baseline run.
6. The resume brief now keeps the full process inventory on disk while returning all ProjectB-owned records and only a bounded unrelated-process sample. Use `-IncludeAllProcesses` for an ownership investigation; routine resumes no longer spend context on hundreds of unrelated `dotnet` workers.
7. `Get-PackagedFlowBrief.ps1` now defaults to counts and error examples without a recent-event tail. Request a small `-LastEventCount` only when event ordering is the active question; raw logs remain unchanged.
8. `Verify-SelectionPresentation.py` now prints a compact result while retaining the full peer report on disk and in its return value. Use `verbose=True` only when row-level presentation details are needed.
9. `Get-GateBrief.ps1` now deduplicates contract/evidence paths and projects nested runtime/preflight records into bounded scalar summaries. Capability details remain in the contract on disk while routine output returns only supported/unsupported/unavailable counts.
10. Evidence helpers accept the prior `-JsonFile` name for `-EvidenceFile` and allow up to eight bounded delta examples. This prevents parameter discovery retries without making routine output unbounded.
11. Gate summaries now project a phase's nested `firstCausalError` before falling back to log-order errors. Older logs can remain in the evidence directory, so this keeps a current setup blocker from being displaced by a stale prior fixture failure while retaining every scanned error on disk.
12. The direct editor launcher now accepts `-CreateZenDataPath`. A project-local Zen directory can be provisioned and write-probed during the same preflight, while the default strict existence check remains available for shared or sensitive cache paths.
13. A selected gate phase can explicitly set `firstCausalError` to null. `Get-GateBrief.ps1 -SummaryOnly` now honors that as authoritative and does not resurrect an older error from the phase's historical logs; the complete inventory and raw diagnostics remain persisted.

## Team-combat gate follow-up

The four-peer team-aware combat gate exposed four more repeat-cost patterns. The development fixture and shared workflow now address them without broadening the acceptance claim:

1. Hero kits use different native primary classes. Select a loaded primary/melee spec by its runtime class family and record the concrete class in the marker log; do not require one hero's native class as a proxy for every kit.
2. Sequential activations must respect the real ability cadence. Defer the second activation on a game timer when the first ability can remain active for a fire interval; immediate reactivation can report a false rejection and waste a rebuild cycle.
3. Relic fixtures must isolate unrelated pawns. Temporarily move non-test pawns away, force a loose relic state before setup, and restore every saved transform during cleanup. This prevents bot overlap from replacing the intended carrier while preserving the production pickup/pass path.
4. Use `Get-TeamCombatFixtureBrief.ps1` for one bounded marker read. It returns marker counts, first matches, natural-goal/round/results counts, error totals, and the first causal error while preserving the full log and JSON report on disk. A `passed-scope-limited` result proves only the authority fixture; the remaining transport, latency, physical, and packaged gates stay explicit.

When an editor shutdown reports a forced kill, check the exact UnrealEditor PID before compiling. If the known PID still owns a module DLL, close that process once and retry the compile; do not launch another editor or repeat the build while the lock owner is unchanged.

These changes reduce setup retries and false stops while retaining the same raw logs, process ownership checks, topology proof, and acceptance requirements. They are workflow improvements only; they do not make a partial runtime result a gate pass.

These are workflow costs, not reasons to remove proof. Remote rendered views, physical input, death/respawn, packaging, adverse networking, second-PC LAN, and normal shutdown remain required whenever their gate is in scope.

## Continuation audit from September 12, 2026

The continuation added 20 compile records, 24 editor logs, and 12 repeated team-combat editor runs. Those files occupy about 55 KB, 24.9 MB, and 12.9 MB, respectively. The 12 repeated fixture logs contain 48 error lines and zero critical lines; 12 are the recurring Zen startup message, and 36 are unknown errors from failed probes or setup attempts. The final successful fixture has three classified nonfatal startup errors and zero unknown or critical lines. A late Python convergence observer also spent one 30-second timeout without producing valid evidence because it could not observe the short-lived authority health change.

This exposes four further savings that preserve the proof bar:

1. Repeated live-log polling should read only appended lines. `Scripts/Test/Get-LogDeltaBrief.ps1` now keeps per-log byte offsets in a gate-owned state file and returns new counts and marker matches. Full-log helpers remain the acceptance source.
2. Known startup noise should not look like a causal gameplay failure. The authority-only `Get-TeamCombatFixtureBrief.ps1` separates its known Zen and Pixel Streaming messages; generic and packaged briefs classify only the Zen security message. All helpers retain total error counts, unknown-error counts, and critical lines, so Pixel Streaming remains actionable for rendered/UI gates.
3. Observer timeouts need a declared evidence budget. Register one callback, wait for the expected marker or scalar transition once, and record `observerTimeout` with its hypothesis when the transition is not observed. Do not rerun the same observer against the same log and source state; change the observation point or use a native fixture command.
4. Repeated fixture attempts should be summarized as a run family. `Scripts/Test/Get-RunFamilyBrief.ps1` keeps one compact per-attempt inventory with byte/line totals and repeated error signatures. Keep one final acceptance log, use the family counts to decide whether a retry has a new hypothesis, and load only the latest or causally relevant log.

The audit record is `Saved/Logs/team-gameplay-20260912/workflow-token-audit.json`. It records these measurements and the changes applied in this iteration.

## Compile runner follow-up

The native peer-convergence fixture exposed a source-build-specific retry trap. `Build.bat` invokes a dependency scan that can fan out hundreds of reusable .NET nodes, while direct UBT can inherit a locked engine trace backup or a protected system UBA root. The Tier 1 config now selects `DirectBundledUbt`: it uses the bundled runtime, sets `IsBuildMachine=1` so UBT writes trace data under its automation log tree, supplies a project-local `-UBARootDir`, disables remote UBA, and records one bounded log. The helper probes the UBA directory before launch and preserves stdout/stderr beside the acceptance log. This removes repeated wrapper and storage failures from the next compile while leaving the source-built editor output and compile proof unchanged.

## Evidence routing and quiet-poll follow-up

Two small sources of avoidable context remained after the first audit. A live log that had not
changed still returned its full delta envelope, and gate follow-ups repeated evidence filenames
that were already recorded in the contract. `Get-LogDeltaBrief.ps1` now accepts `-QuietUnchanged`
and returns only zero-delta paths when no bytes were appended; `-OutputJson` preserves the full
poll result for the gate record. `Get-GateBrief.ps1 -ContractPhase <compile|runtime|package|all>`
loads the selected phase's `evidence` array, routes existing `.log` entries through its bounded
scanner, and still keeps the complete contract on disk. `Tier1-CompileGate.ps1 -OutputJson`
records source revision, runner, process ID, exit code, elapsed time, and log paths immediately
after the process exits. These options remove path and result reconstruction from the next
iteration while leaving unchanged logs, full evidence, and acceptance topology intact.

`Get-PackagedFlowBrief.ps1 -OutputJson <path>` applies the same persistence rule to packaged
selection/travel/results inventories. The routine console response stays count-only; an event tail
is still opt-in for an ordering diagnosis.

`Get-GateBrief.ps1` now accepts `-OutputJson <path>` as well as its existing default
`gate-brief.json`. Use the switch when a gate has multiple phase briefs so the runtime, package,
and compile inventories cannot overwrite one another; the console response remains the same
bounded summary.

Routine gate follow-ups can add `-SummaryOnly`. The helper still writes the complete evidence and
contract inventory to `-OutputJson`, while the console returns only phase, counts, error totals,
first causal error, process state, scope, and output path. This keeps a large contract phase from
being copied into the agent context when no changed diagnosis needs peer-level detail.

## Separate-process probe follow-up

The September 12 peer run added a development-only replicated GameState probe because client and
GameState multicast RPCs executed in the listen host under this PIE topology. The probe reached all
three child processes, but their first sample read health 400 while the authority had already applied
damage to 370. A delayed local sample now measures settled state when the child has finished loading.
The next retry remained on `Experience still loading`, so it is recorded as a readiness limitation
and is not repeated without a changed hypothesis. The source compile passed; no shipping replication
property is emitted for the probe.

Separate-process logs are copied with `Scripts/Test/Capture-PIEClientLogs.ps1` before teardown or a
new launch because Unreal reuses `ProjectB.log`, `ProjectB_2.log`, and `ProjectB_3.log`. The capture
record is `Saved/Logs/team-gameplay-20260912/capture-pie-client-logs-settled-readiness.json`; the
probe record is `team-peer-separate-replicated-prop-brief.json`. These records preserve the mismatch
and the readiness failure without promoting transport evidence into settled combat parity.

The readiness retry also showed that a timed manual poll can arrive after the staging phase has
already auto-traveled. Use `Scripts/Test/Wait-SeparateProcessReady.ps1` once per separate-process
attempt to watch only appended bytes and return one compact `ready`/`timeout` result for every child.
For staging-to-gameplay commands, pass the server log with `-HostLog` and the staging experience
with `-HostExperienceMarker`; the host's `... Server` completion marker is then required alongside
the child `... Client` markers. When the starting world matters, add `-HostMapMarker` so a wrong PIE
map fails the checkpoint before the authority command. Dispatch time-sensitive authority fixtures
only after that combined checkpoint. The helper records loading-marker counts and read errors,
preserves its JSON result, and explicitly does not claim combat or replication settlement.
Use its comma-separated `-LogFiles` list so PowerShell binding does not become a second setup
experiment.

## Current 12-hour efficiency findings

The September 12 continuation added 20 compile records, 24 editor logs, and 12 repeated fixture
logs before the current peer retry. The audit measured about 55 KB of compile metadata, 24.9 MB of
editor logs, and 12.9 MB of repeated fixture logs. Compact gate summaries reduced one routine
projection from 7,767 to 678 console characters (91.3%); quiet unchanged polling reduced another
projection by 31%. Those savings came from keeping complete evidence on disk and returning only
counts, scope, causal error, and paths.

Two runtime mistakes still spent a full process lifecycle without improving proof. Runs 25 and 26
issued `Net PktLag 100`, which Unreal rejects; the accepted command is `Net PktLag=100`. Run 26
also dispatched the fixture before the `Started playing GAS phase` marker, so `EnemyDamage` was
zero and the result was correctly withheld. Run 27 gated the host phase, but all three children
remained on `Experience still loading` while the authority advanced through the round. The
readiness timeout and the resulting stale child logs are setup evidence, not a combat failure.
Editor shutdown also left 45 source-engine `.NET` workers; cleanup now records and clears that
known owner before the next resume so a cache worker cannot trigger a duplicate-editor/build
diagnosis.

These observations add two bounded workflow changes. `Get-ResumeBrief.ps1` now accepts
`-GateContractPath` and `-ContractPhase` so one resume call returns both process/candidate facts and
the current gate decision. `Get-ReplicationGateBrief.ps1` joins readiness records, the authority
fixture, and the multi-sample peer comparison into one compact status, while persisting the two
detailed helper records and every raw log. A composite `unverified` result still withholds
acceptance whenever any child, marker, sample, or authority comparison is missing. The next retry
therefore needs one resume projection, one readiness checkpoint with host/map/phase markers, the
exact latency syntax, and one composite brief before any raw-log expansion.

The readiness helper itself now accepts `-BaselineJson`. A destination checkpoint can inherit the
staging checkpoint's per-file offsets, so reused child logs are evaluated only for bytes written
after staging completed. This closes a false-positive path without changing the normal first
checkpoint behavior or discarding the raw logs.

The first checkpoint has the same stale-log risk when Unreal reuses `ProjectB*.log` across PIE
lifecycle attempts. After the explicit staging-map load and before `play_in_editor start`, run
`New-LogOffsetBaseline.ps1` and pass its JSON as `-BaselineJson` to staging readiness. The baseline
records only current byte lengths and does not claim readiness; taking it after PIE starts can
exclude a host marker that was already emitted during startup.

The source-built engine branch currently reports `IsServerMapNameOverrideActive()` as false, so
`ServerMapNameOverride` is ignored for listen-server PIE. Load the required staging map through the
editor bridge before starting PIE, then require the host map marker in the readiness checkpoint. This
single cheap setup action avoids spending a full four-process lifecycle on a wrong-world diagnosis;
the explicit map load and marker remain part of the gate evidence.

When readiness times out, use `Scripts/Test/Get-SeparateProcessTravelBrief.ps1` against the
captured child logs and the host log before inspecting raw lines. It reports bounded load, seamless
travel, experience, loading-screen, failure, and error counts plus a small recent-marker tail per
process. This avoids repeating broad `Get-Content`/`rg` scans that can flood context while keeping
the complete logs available for a changed diagnosis.

The routing-filter retry then showed that child readiness alone was insufficient: PIE created
`L_BW_DevMap` even though the staging override requested `L_BW_HeroSelect_Staging`, and the host
reached CaptureTheRelic while clients stayed loading. The attempt is recorded as setup-blocked in
`gate-contract.json`; it is not routing-filter proof. The readiness helper now accepts optional
`-HostLog`, `-HostExperienceMarker`, `-HostMapMarker`, and `-RequiredMarker` arguments so the next
attempt can stop before an authority command when server staging, map selection, or a true
client-side readiness marker is wrong. Run30 showed that `Local player locked hero` is downstream
of the authority AutoLock command in this flow, so that child marker must remain optional here;
the five-second source travel settle window is the correct fix for this ordering.

Runs28–32 converted those findings into a bounded retry sequence. Run28 proved travel but issued
the fixture after the one-point match ended. Run29 isolated immediate server travel before child
lock replication. Run30 validated the optional child-marker switch and exposed its causal cycle;
run31 exposed the late-baseline ordering mistake. Run32 then used a pre-PIE baseline, explicit
staging-map load, the five-second settle window, exact `Net PktLag=100`, and one composite brief:
all three children completed travel and produced three authority-matched health/team samples
each. The saved result is scope-limited evidence, not full LAN acceptance.

Run 33 exposed two proof-cost issues that the earlier run family could not show. The authority
fixture brief only recognized the older `BwayTeamCombatTest` markers, so a valid remote-owner run
looked like a missing-marker failure. The brief now detects `BwayTeamCombatRemote` and records its
narrower required set separately. The composite brief also projects fixture mode and per-child
known/unknown error counts, while the complete raw logs remain authoritative.

The first remote-owner retry timed out while the relic was still in `BeingPassed` under
`Net PktLag=100`. The fixture now holds only its selected receiver, restores that movement mode and
request tag during cleanup, spaces convergence sequence updates by three seconds, and allows a
bounded 60-attempt receive check. These are development-fixture timing aids; they do not change
shipping gameplay. Run 33m then passed with a fresh baseline, one connected remote owner, nine
authority-matched child samples, and zero authority mismatches. Its detailed record is
`Saved/Logs/team-gameplay-20260912/replication-gate-brief-remote-owner-33m.json`.

The remote score gate added four further savings without reducing proof. A native console command
issued from the bridge targeted the editor world, so the fixture correctly rejected it as
non-authoritative; `EditorLevelLibrary.get_pie_worlds(False)` plus
`SystemLibrary.execute_console_command` is now the recorded authority-dispatch fallback. Cache that
capability once per editor lifecycle and do not spend another PIE run trying the editor console
route. The editor settings also must be written before launch; applying multiprocess PIE settings
after the editor starts silently produced one-process PIE and an unusable readiness timeout.

Unreal rotated one reused child log during a readiness checkpoint, making its file shorter than the
baseline. Archive the three `ProjectB*.log` files before a fresh multiprocess attempt when their
lengths or timestamps are not stable, then baseline the empty/new files after the explicit staging
map load. This preserves stale-marker exclusion while avoiding repeated timeouts caused by a
baseline that no longer describes the file.

The first score fixture measured remote damage once and advanced while a hero's real fire interval
was still pending. The remote fixture now polls the replicated enemy health for a bounded ten-second
window before pass setup and records a timeout when production damage never lands. After pass
receipt, it collects the three paired health/team samples before the goal ends the round; scoring
legitimately resets health, so sampling afterward can create a false mismatch. The new
`remote-owner-score` marker mode requires the production damage/pass markers, opposing-goal
overlap, exact team/player score deltas, `RoundComplete`, and cleanup, while retaining the 20-point
paired health tolerance, exact team equality, and three-sample requirement.

The first packaged candidate exposed a provenance-specific proof cost. `-skipbuild` staged an older
`LyraGame.exe` because only the editor target had been compiled; the cooked asset then referenced the
new runtime component and crashed during async loading. The failure was diagnosed from the packaged
log and staged binary, then prevented by compiling the game target and passing its JSON result to
`Package-Preflight.ps1 -CompiledTargetRecord` before another skip-build package. This keeps the
fast package path while proving that the executable, source revision, and selected game target agree.

The same candidate also failed before Cook/Stage/Pak because AutomationTool could not open its engine
`Saved/Logs/ErrorLog.txt` under the current permission context. `Package-Preflight.ps1` now has an
optional `-RequireAutomationToolLogWrite` probe for that exact file. A failed probe is a cheap setup
record that tells the next iteration to request build permission once, instead of learning the ACL
failure after a long package launch.

The economy check also required two manual launch retries: one for Windows argument quoting and
one for the source engine's unwritable DDC. `Scripts/Test/Run-AutomationTests.ps1` now owns that
sequence: it resolves the editor, builds one correctly quoted command, refuses stale report
directories, supports `-ForceMemoryDDC`, bounds the editor lifetime, and joins report status,
framework exit marker, and process exit in one JSON record. A report-only success with a nonzero
process exit remains an explicit anomaly, so compaction does not turn an unhealthy process into a
pass.

The first polish run also demonstrated why a green individual test is insufficient: an abstract
`NewObject<UObject>()` dummy triggered a CoreUObject ensure, suppressed the report, and consumed a
full timeout. The fixture now uses a concrete transient object, and the automation runner rejects
missing reports or nonzero process exits even when earlier test events look successful.

The packaged 200 ms remote-owner run exposed two more bounded setup costs. Passing a network cvar
and a fixture command in one semicolon-delimited `-ExecCmds` value did not reliably dispatch the
fixture in the staged process, even though the host started normally. Separate `-ExecCmds` arguments
made both commands observable; the runner now records that argument shape. The first delayed
fixture also selected actors during Warmup, so a real combat event could kill a selected player
before the fixture reached its pass and score checks. The development command now waits for the
authoritative `Playing` phase before selecting or moving actors, with one bounded retry loop.
Candidate `LAN-20260915-13` passed Cook/Stage/Pak and the four-process `Net PktLag=200` remote-owner
damage/pass/goal sequence with all required score deltas and client Results exits. Both a bot-filled
and a four-human run retained one unclassified `GCNL_Death` Blueprint `Accessed None` warning; the
warning is preserved as a polish lead rather than classified away or hidden by the compact brief.

Use separate `-ExecCmds` arguments when a staged fixture combines cvars with a development command,
and make time-sensitive authority fixtures admit only after their required match phase. If the
compact brief still reports the same unclassified warning after one isolated configuration retry,
stop repeating the package and fix or inspect the owning asset instead. These rules shorten setup
and diagnosis while keeping marker requirements, error counts, and the physical/LAN proof gaps
unchanged.

## Provenance and startup-screen follow-up

The last 12+ hours still contained two avoidable context paths at the edges of otherwise compact
gates. Source provenance guidance required manually running `git diff HEAD --binary` and preserving
untracked inputs, which can flood the agent when the worktree contains large deletes or generated
assets. Startup screening also returned one complete row per log even when the decision was only
whether the candidate could proceed. `New-SourceSnapshot.ps1` now writes the complete tracked patch,
status, and untracked manifest to uniquely named evidence files and returns only source identity,
counts, sizes, and hashes. `New-AcceptanceSummary.ps1 -SummaryOnly` keeps the complete screen in
`-OutputJson` while returning only gate booleans, failed-gate names, and the next action.

These changes preserve candidate reviewability: a hash or count mismatch still requires opening the
persisted files, and a compact startup screen remains only a startup inventory. Neither helper
promotes source identity or log patterns into compile, runtime, LAN, or end-to-end acceptance.

The latest compile attempt exposed one more avoidable context path: the agent inspected the
requested `-OutputJson` file before UBT had finished writing the final record. `Tier1-CompileGate.ps1`
now publishes a valid `status: running` record immediately after the owned process starts and
atomically replaces it with the final compile record. A resume can also pass
`-SourceSnapshotPath` to `Get-ResumeBrief.ps1`; the stable source identity and individual file
hashes then arrive beside the selected gate without a second snapshot read. The measured combined
resume projection was 3,300 characters versus 4,552 characters for the separate resume plus source
snapshot projections (27.5% less routine context), while complete provenance remains on disk.

`Set-GatePhase.ps1` now publishes the updated gate contract through the same temporary-file rename.
That closes the remaining short race where a resume could parse a half-written contract and spend a
retry recovering the evidence file rather than evaluating the gate.

The per-gate resume pointer now uses a stable `source-snapshot-current` path. Numbered snapshots
remain available for immutable attempt history, while the deliberate `-Force` refresh of the
current pointer avoids editing the ledger and repeating path-discovery work after every small
workflow change.

## One-command resume packet

The last continuation still required separate resume and workflow-hygiene calls after a metadata
change. That repeated the same candidate, source identity, process counts, and proof-gap fields in
the conversation. `Scripts/Test/Get-NextIterationBrief.ps1` now runs the contract-aware resume
inventory and combines those fields with the selected phase, retry condition, and next action. It
keeps the detailed resume, workflow, contract, and source records on disk, so the packet reduces
context transfer without hiding evidence or changing gate scope.

Use `-RefreshSourceSnapshot` after deliberate source or ledger edits and `-RefreshWorkflow` after
contract, snapshot, ledger, or helper edits. Leave both switches off for an unchanged gate. A
missing or stale workflow record is reported as a warning and does not authorize artifact reuse.
The helper is an inventory only; compile, runtime, UI, LAN, and acceptance gates remain separate.

## Freshness guard follow-up — September 15, 2026

The next review found that identity checks alone could miss an in-place dirty-file edit: the
source snapshot and workflow record could still agree with each other while both were older than
the checkout. `Get-NextIterationBrief.ps1` now performs a cheap provenance comparison against the
current HEAD, tracked patch, worktree status, and untracked manifest. `Get-WorkflowValidationBrief.ps1`
also records a sorted hash of every `Scripts/Test/*.ps1` file, and the packet compares that hash on
routine resumes. A mismatch produces a refresh warning before an expensive gate; it never reuses or
invalidates compile, runtime, UI, LAN, or acceptance evidence by itself.

The guard was validated in both directions. The current packet reports `sourceSnapshotFreshness:
fresh` with all four comparisons true; its only current warning is the expected frozen-candidate
identity mismatch, which requires scope review before artifact reuse. Synthetic records with one
altered patch hash and one altered script-inventory hash each produced the expected warning and
remained inventory-only.
The records are `Saved/Logs/buildable-placement-20260914/workflow-freshness-guard-measurement.json`,
`next-iteration-source-stale-negative.json`, and `next-iteration-workflow-stale-negative.json`.

The packet also projects the selected candidate's package-time source identity when the gate
contract records it. A mismatch with the current snapshot is a bounded artifact-reuse warning, so
an older frozen binary is not accidentally treated as the current shipping source. The candidate
can still be reused for its explicitly recorded scope after review; a new candidate is required for
the latest source scope.

## Fixture admission and lifecycle projection — September 15, 2026

The rematch reset requirement previously had only broad economy automation coverage. Starting a
four-player lifecycle run without a purchased upgrade and nonzero match stats would spend an editor
process on setup failure and then require a raw log search to explain it. The development command
`bway.Test.MatchUpgradeRematchReset` now checks those prerequisites before requesting the production
rematch, waits for a fresh authoritative game state, and verifies every destination player has zero
owned ranks, cleared match stats, and a zero wallet. The production reset remains the code under test;
the fixture adds only bounded admission and observation.

This run also established a bridge capability detail: the Python `World` wrapper in this engine
does not expose `get_net_mode`. For the configured four-world listen topology, the first result from
`EditorLevelLibrary.get_pie_worlds(False)` is the authority world. Record that fact once in the gate
capability cache and dispatch with `SystemLibrary.execute_console_command`; probing the missing
method again adds failure output without changing the evidence.

`Get-MatchUpgradeLifecycleBrief.ps1` projects that run to marker counts, setup-blocked state, first
causal error, unknown-error count, critical count, and scope. A synthetic pass record and the new
fixture source compile passed. The helper is now part of the script-inventory freshness hash, so a
deliberate helper edit refreshes the single next-iteration packet. This removes repeated prerequisite
and raw-log context while keeping packaged, physical-input, and end-to-end acceptance requirements
unchanged.

## Operating contract for the next iteration

1. During active vertical-slice implementation, read `AI_Planning/SLICE_STATUS.md` once, then run `Scripts/Test/Get-NextIterationBrief.ps1` once with the selected contract phase. Add `-RefreshSourceSnapshot -RefreshWorkflow` after deliberate metadata edits. The helper invokes the contract-aware resume inventory and returns one bounded decision packet; use `Get-ResumeBrief.ps1` directly only for a narrower follow-up. After a pass or recorded blocker, continue to the next authorized gate while the goal is active. Do not reread either file unless the gate changes or a mutation updates it. Use the full default projection only when process ownership or dirty-file detail is the active diagnosis.
2. Write the gate contract in the first evidence record: expected observable result, required topology, selected source/assets, retry condition, and cleanup owner. Use `Scripts/Test/Get-GateBrief.ps1` for every follow-up inventory.
3. Batch independent, narrow reads in one tool call. Use `rg` with a path and a small match cap, then read only the surrounding source lines. Never send a whole historical document or full log to the model when a compact summary can answer the question.
4. Keep full logs and raw captures on disk. Return only: pass/fail, first causal error, counts, scope, evidence path, and next action. `Get-GateBrief.ps1` summarizes selected JSON and whole-file error counts without moving the file into context.
5. Treat each live Python observer as a single owned resource. Load it once, keep wrappers inside functions, register one callback, stop it in `finally` or the explicit cleanup step, and serialize results. A new hypothesis may justify a new callback; a repeated read does not.
6. For Windows UI, select one exact returned window, capture fresh state, perform one action, and capture fresh state again. Never reuse coordinates, screenshot IDs, or an activation result after another action. If the target is missing, stop and reselect instead of clicking again.
6a. Treat a compact brief's `hiddenEditorCount > 0` as a hard preflight failure. Resolve the exact process owner and close it normally; do not launch a duplicate editor or rely on a hidden process as UI evidence.
7. Reuse one editor and one build for a coherent gate. Record process IDs and log paths before launch, wait on those handles, and close the editor normally before rebuilding or changing reflected classes. Do not launch a second copy to compensate for an unclassified failure.
8. Stage only owned paths after the gate passes. Keep unrelated dirty files out of the commit and record their count, not their contents, in the ledger.
9. Update the ledger with one current status row and one next action. Put attempt history, retries, commands, and detailed observations in the gate document or JSON manifest; replace stale summaries instead of appending a transcript.
10. Before a long gate, write one preflight record with the source revision, editor/build paths, process inventory, requested topology, observed topology, bridge readiness, UI-channel availability, and the exact proof scope. Stop before launch when a required topology or channel cannot satisfy the gate.
11. Use `-ValidateOnly` or `Get-Help` once before running an unfamiliar helper. Record the accepted parameter set in the gate manifest so a long compile or package run never doubles as argument discovery.
12. Maintain a session capability cache beside the gate contract. Record each bridge or reflected operation as `supported`, `unsupported`, or `not attempted`; do not repeat an `unsupported` call unless the editor, plugin, or source revision changes.
13. After each phase, update the same gate manifest with `status`, `scope`, `evidence`, `proofGaps`, and `nextAction`. A partial result is useful evidence when its limits are explicit.
14. Treat computer-use availability as a preflight capability. If the service is unavailable or has no targetable window, record that fact once, skip only physical/UI assertions, and preserve the structured or packaged checks that remain valid.
15. Keep compact helper output bounded by default. When a helper writes a full snapshot, return a ProjectB-scoped sample and counts in the conversation; expand to all records only for a changed diagnosis.
16. For live polling, call `Get-LogDeltaBrief.ps1` after the initial baseline and pass only the delta result to the next decision. Do not call a whole-file search again unless the delta identifies a new error or marker transition.
17. For observer-based convergence, record the expected transition, timeout, and cleanup result in the gate manifest. A timeout is a partial result that changes the hypothesis; it is not permission to repeat an unchanged callback.
18. Prefer contract-driven evidence selection for follow-up briefs and `-QuietUnchanged` for a poll that has no expected transition. Persist compile/delta JSON records with the helper's output switches so the next decision can read one bounded result rather than re-deriving metadata from console text.
19. Persist packaged-flow inventories with `Get-PackagedFlowBrief.ps1 -OutputJson` when they are part of a gate. Keep `-LastEventCount` at zero until chronology is the actual question.
20. In separate-process PIE, copy the three reused `ProjectB*.log` files with `Scripts/Test/Capture-PIEClientLogs.ps1` before another launch. Record one readiness checkpoint for each child; if any remains on `Experience still loading`, preserve the captured logs and change the readiness hypothesis before retrying.
21. Pass phase evidence and proof gaps to `Set-GatePhase.ps1` with `-EvidenceCsv` and `-ProofGapsCsv` when the helper is launched through `pwsh -File`; this avoids shell-specific array binding without changing the contract schema.
22. For separate-process readiness, run `Wait-SeparateProcessReady.ps1` once with the target experience and the three child logs. Use its single result to gate the next authority command; do not hand-poll full logs or send a time-sensitive command after an unverified load state.
23. Add `-SummaryOnly` to routine `Get-GateBrief.ps1` follow-ups. Expand to the full console inventory only when the summary identifies a changed causal error or marker transition; the persisted JSON remains the detailed evidence source.
24. After a separate-process readiness timeout, run `Get-SeparateProcessTravelBrief.ps1` once on the captured child and host logs. Use its transition counts to choose the next hypothesis; only then inspect a narrow raw-log range. Do not relaunch until the travel or experience hypothesis changes.
25. For a staging-to-gameplay fixture, include `-HostLog` and `-HostExperienceMarker` in the readiness checkpoint, and add `-HostMapMarker` when the starting world is part of the fixture contract. If the authority command requires active gameplay, add `-HostRequiredMarker` with the exact round-phase marker. Use `-RequiredMarker` only when the child marker is an independent prerequisite that exists before the authority command; do not use it for `Local player locked hero` in the AutoLock-before-travel flow because that marker is emitted downstream of the command. If a required child/host marker, map marker, or phase marker is absent, withhold the authority command and record a setup/timing blocker; this avoids spending a full PIE lifecycle on a command issued against a still-loading, wrong-map, or warmup host world.
26. When a source-built editor gate uses a project-local Zen directory, call `Start-ProjectBEditor.ps1 -ValidateOnly -CreateZenDataPath` once with the final path, then reuse that path for the launch. Record `zenDataPathCreated` in the launch JSON; do not create a second editor to recover from a missing cache directory.
27. On resume, pass the gate contract to `Get-ResumeBrief.ps1` with the selected phase. Use the projected status, scope, proof-gap count, causal error, retry condition, and next action as the decision input; open the full manifest only when the summary identifies a changed diagnosis.
28. After a separate-process replication attempt, run `Get-ReplicationGateBrief.ps1` once with the host log, comma-separated child logs, readiness JSON files, and `-MinimumSamples`. Treat `unverified` as a stop/diagnose result, and use `-IncludeSamples` only when the bounded fields identify a sample or authority mismatch. For latency, issue `Net PktLag=100` exactly once after the phase checkpoint.
29. For a staging-to-destination readiness pair, pass the staging JSON to the second `Wait-SeparateProcessReady.ps1` call with `-BaselineJson`. This carries forward byte offsets and prevents an older destination marker in a reused child log from satisfying the new checkpoint.
30. Create the first-checkpoint baseline after explicit map load but before PIE starts. For replicated health that can regenerate, omit fixed `-ExpectedHealth`; compare each child sample to its paired authority sample within a bounded `-HealthTolerance` and require exact team equality. This avoids false failures without weakening sample identity or convergence scope.
31. For remote-owner combat fixtures, use `Get-TeamCombatFixtureBrief.ps1` and let its marker mode determine the required set; do not reuse authority-only marker names. Preserve child logs before teardown and require the remote `COMPLETE` marker before advancing the gate.
32. Under the intentional 100 ms lag profile, use a 20-point health tolerance for the dynamic paired comparison when regeneration is active. Keep three distinct samples per child and exact team equality; the tolerance handles timing, not missing or stale state.
33. If a server-only pass remains in flight or a selected receiver receives movement correction, change the fixture timing/ownership hypothesis once: use a bounded receive window and a cleanup-scoped receiver hold. Do not increase a shipping timeout or relax the peer sample requirement to make a fixture pass.
34. For remote score fixtures, configure multiprocess PIE before editor launch, archive reused child logs when a baseline could be invalidated by rotation, and dispatch the console command to the authority PIE world. The score fixture must require production damage, pass receipt, real opposing-goal overlap, exact team/player score deltas, `RoundComplete`, and cleanup; authoritative placement is fixture setup only and never a substitute for physical-input acceptance.
35. Collect replicated health/team samples before a scoring transition resets health. Use the bounded remote damage poll before pass setup, then keep the three-sample and exact-team checks unchanged. Preserve unknown startup errors and classify only the known Zen/PixelStreaming lines.
36. Before any `-skipbuild` package, compile the configured game target (`LyraGame`) and pass the successful `Tier1-CompileGate.ps1 -OutputJson` record to `Package-Preflight.ps1 -CompiledTargetRecord`. Require source-head and target equality plus an existing game binary; do not infer packaged game freshness from an editor compile or HEAD alone. Retain the package manifest hash and run the packaged smoke after staging.
37. If packaging runs in a restricted permission context, include `-RequireAutomationToolLogWrite` in the package preflight. Treat a failed engine-side `ErrorLog.txt` probe as a setup blocker and request the required permission before Cook/Stage/Pak; do not retry the unchanged package command.
38. Run editor automation filters through `Run-AutomationTests.ps1` with a fresh output prefix. Use `-ValidateOnly` for path/report admission and `-ForceMemoryDDC` when the source engine lacks a writable cache. Require report success, the framework exit marker, and process exit 0; preserve any process/report mismatch as a changed diagnosis.
39. Keep automation fixtures concrete and transient. Never instantiate abstract engine base classes as test dummies; an ensure or missing report invalidates the whole filter and requires a source fix before retrying.
40. Use `Scripts/Test/New-SourceSnapshot.ps1` once before an expensive gate when source provenance is part of the decision. Read only its bounded JSON projection; keep the complete patch, status, and untracked manifest on disk and use its `sourceIdentity` plus individual hashes to decide whether a candidate can be reused. The identity is a comparison index, not acceptance proof.
41. Use `New-AcceptanceSummary.ps1 -SummaryOnly -OutputJson <path>` for routine startup screening. Expand to the full per-log rows only when a required/forbidden pattern or candidate association changes the diagnosis.
42. When the bridge invokes a development console command without a PIE world, prefer a no-world delegate that resolves the intended authority or client PIE world internally. Record an unresolved world as setup failure and fix dispatch once instead of repeating the same editor-world command.
43. For short-lived four-world PIE fixtures, use one lifecycle sequence with explicit settling samples (for example 0.5 and 2.0 seconds). Keep the transient sample in the persisted evidence and require the settled all-peer sample for convergence; extract one bounded log range for the decision.
44. For buildable round-reset diagnostics, call production `ResetRoundState` and each `ABwayPlayerState::BeginRoundStatTracking` from a development-only fixture when orchestrator phase timing is unstable. Evaluate persistent actor count separately from per-round quota state and do not treat an ignored `StartRound` call as a reset.
45. For placement cleanup, let a cancel fixture reuse an already active placement spec before issuing `LocalInputCancel`. A no-hit confirm should remain active with quota unused until valid target data arrives; physical valid-confirm evidence remains a distinct acceptance requirement.
46. After a buildable fixture, run `Scripts/Test/Get-BuildableFixtureBrief.ps1` once against the saved editor log. Use its compact pass fields, peer counts, and first fixture error for routine decisions; expand `-IncludeSamples` or raw ranges only when the brief identifies a changed replication diagnosis.
47. Keep malformed placement payloads on the native fixture path. Parse empty, wrong-type, and multi-entry terminal markers in the same buildable brief, and use one bounded callback result instead of three broad log searches. When persisting a brief, use a bare filename beside the log or an explicit path containing a directory; the helper resolves both forms without a retry.
48. For a client-to-server buildable rejection check, let the fixture wait for the replicated Playing phase and the complete client/authority ownership chain. Submit through GAS once with `Net PktLag=100`, then use one authority callback/result projection; do not spend retries on a command issued during warmup, PostRound, or incomplete pawn initialization.
49. When physical input is unavailable, use the phase-aware synthetic valid-confirm fixture once to prove client-to-server target-data acceptance, authoritative spawn, four-world convergence, and cleanup. Keep that structured scope separate from physical cursor selection and packaged UI/LAN acceptance.
50. Treat a compile record with `status: running` as an in-progress gate. Wait for its recorded process before reading the compiler log; Tier1's final JSON is written atomically, so a transient read cannot create a redundant compile retry.
51. When a gate has a source snapshot, pass `-SourceSnapshotPath` with the contract-aware `Get-ResumeBrief.ps1 -SummaryOnly` call. Use its identity and hashes in the decision packet; open the complete patch/status/untracked files only when that summary or artifact provenance changes the diagnosis.
52. Treat the gate contract as a published decision record. Use `Set-GatePhase.ps1` for phase changes and wait for its command to finish before running a resume; the helper atomically replaces the JSON so a transient read cannot create a false parse failure.
53. Keep one stable `source-snapshot-current` path in the gate contract and resume command. Refresh it with `New-SourceSnapshot.ps1 -Force` only after intentional tracked edits; use numbered prefixes when an immutable historical snapshot is required.
54. In a packaged four-process transport fixture, let each client own its result-screen test exit and hold the listen host until all client result markers arrive. A host `PostMatch` test exit can create a false transport failure by closing the connection during the last client's replicated-results handoff; classify that as harness teardown, preserve the v1/v2 causal logs, and retry only with the changed host-lifetime fixture.
55. Treat explicit UBT failure signatures as a compile failure even when the bundled .NET process exits 0. `Tier1-CompileGate.ps1` records the raw `processExitCode`, effective `exitCode`, and bounded `failureSignals`; `Package-Preflight.ps1 -CompiledTargetRecord` requires that field to be present and empty, so a pre-change compile record is regenerated instead of being trusted. For packaged shop checks, use the delayed development probe after controller readiness and verify the catalog ID returned by data, rather than assuming a display label.
56. In `Get-ResumeBrief.ps1 -SummaryOnly`, keep the full ledger next-action line in the persisted full snapshot but bound its duplicate console projection. Use the contract's `gate.nextAction` for the complete action; expand `fullSnapshot` only when the ledger wording carries a changed diagnosis.
57. Treat UBT startup exceptions as authoritative compile failures even when the wrapper exits 0. The Tier1 signal set includes unhandled exceptions, `UnauthorizedAccessException`, access-denied writes, and failed generated-file renames. Check the editor/game binary timestamp after a compile and before launch; if engine ACLs block UBT, perform one authorized elevated retry after closing the known lock owner and preserve generated trace evidence.
58. Persist every packaged four-process harness verdict as a compact result JSON before returning. Keep host readiness, client exit status, and owned host teardown separate so a successful client run is not lost to a deliberately terminated listen host. Resume from that JSON plus one count-only flow brief instead of recovering a truncated console response from raw logs or relaunching an unchanged candidate.

## Quality guardrails

| Saving | Preserved proof |
|---|---|
| Compact evidence output | Full logs, screenshots, traces, and JSON stay under `Saved/Logs/` |
| One active gate at a time | All requirements in that gate's checklist and topology still run |
| Reusing a process | Only while its source, configuration, and unsaved-state contract remain unchanged |
| Fewer retries | Retry after a changed hypothesis, new evidence, or explicit reproducibility check; never hide an unknown result |
| Narrow reads | Expand only the specific file/range needed to explain a failure |
| Candidate reuse | Requires source patch, assets, configuration, and artifact identity review; HEAD equality alone is insufficient |
| Preflight admission | A cheap scope check prevents an expensive run whose topology or input channel cannot satisfy the gate |
| Capability cache | Unsupported inspection routes are recorded once; accepted routes still produce their required proof |
| Phase status | Partial compile/runtime/package results remain reviewable without claiming a pass |
| Contract-driven briefs and quiet polls | Full phase evidence and appended diagnostics remain on disk; unchanged polls are shortened only when no bytes changed |
| Persisted packaged inventories | Count-only console output and full JSON checkpoint preserve the same whole-file error/event scan |
| Gate-owned separate-process log copies | Child-process markers remain attributable to one attempt even though Unreal reuses `ProjectB*.log`; the helper does not interpret or discard evidence |
| Separate-process readiness helper | Appended-byte polling and one per-child result prevent late fixture commands; raw child logs and readiness JSON remain available |
| Gate brief summary projection | Routine console output returns bounded counts and first error while the complete phase inventory remains in the output JSON |
| Separate-process travel brief | One bounded per-process transition inventory replaces repeated broad log-tail scans; captured raw logs remain the source for changed diagnoses |
| Host-aware separate-process readiness | A server completion marker gates authority commands with child markers; a missing host marker remains a setup blocker rather than an ambiguous gameplay result |
| Optional host map marker | A wrong starting map fails preflight before a time-sensitive command; the full host log remains available for diagnosis |
| Source snapshots persist complete patch/status/untracked files and return hashes/counts | Large diffs and generated inputs do not enter routine context; provenance remains reviewable |
| Source snapshot identity index | Candidate provenance comparisons use one stable scalar before opening complete patch/status files; individual hashes and complete files remain authoritative |
| Startup screening supports SummaryOnly with full JSON persistence | Compact output does not remove per-log required/forbidden pattern evidence |
| Optional host phase marker | A warmup transition fails preflight before an authority fixture; the full host log remains available for diagnosis |
| Zen path provisioning | A missing project-local cache directory is created and write-probed during one preflight; strict validation remains the default for shared paths |
| Contract-aware resume projection | Candidate/process facts and the selected gate decision arrive in one bounded resume result; the complete contract remains on disk |
| Composite replication brief | Readiness, fixture, and peer comparison are evaluated once with detailed child briefs preserved; missing evidence remains `unverified` |
| Baseline-carrying readiness | Destination polling begins at the recorded staging offsets, preventing stale child markers from becoming a false ready result |
| Remote-owner marker mode | Authority and remote fixture markers are parsed separately; a valid remote run is not wasted on a false old-marker diagnosis |
| Dynamic peer error projection | Compact output includes per-child unknown-error counts and first unknown error while retaining complete logs and unchanged pass criteria |
| Latency-aware fixture timing | A bounded receiver hold and receive window address transport timing in a development fixture without widening shipping behavior or reducing the three-sample peer requirement |
| Remote authority dispatch | One cached PIE-world dispatch path avoids editor-world command rejection; the authority target and exact command remain in preflight evidence |
| Rotation-safe child logs | Archiving reused child logs before a fresh baseline prevents a shortened file from invalidating only one readiness child and forcing an unchanged lifecycle retry |
| Score-transition sampling | Pre-score replicated samples avoid round-reset timing noise while score ownership is proved independently through the real goal-overlap chain |
| Bounded remote damage poll | A production fire-interval race becomes one bounded wait with a causal timeout instead of a false pass or repeated full fixture runs |
| Game-target provenance preflight | A skip-build package cannot silently reuse a stale game executable; compile status, target, source head, and binary presence are checked before Cook/Stage/Pak |
| AutomationTool log ACL preflight | A restricted package run fails in a short permission check before spending a full Cook/Stage/Pak lifecycle; the complete error remains in the preflight JSON |
| Parameterized automation runner | One fresh editor lifecycle owns argument quoting, DDC setup, timeout cleanup, report parsing, and process/report agreement; full logs and HTML/JSON reports remain on disk |
| Concrete automation fixtures | A test ensure is fixed at the fixture boundary instead of being hidden by a partial report; full filter success still requires clean process/report evidence |
| Phase-aware development command dispatch | A bridge command resolves the requested PIE world internally; a missing world remains a setup failure and no editor-world retry is needed |
| Delayed four-world fixture samples | Transient and settled peer states remain visible; one lifecycle and one bounded log extraction prove the recorded convergence scope |
| Phase-independent buildable reset fixture | Persistent actors and per-round quota reset are checked independently through public production APIs; phase-race retries are avoided |
| Active placement cancel reuse | Invalid-confirm cleanup can target the existing active ability without a second activation attempt; the no-hit state remains an explicit invalid-target result |
| Buildable fixture brief | Spawn/quota/cancel/confirm/reset markers and settled peer counts are returned in one bounded JSON record; full editor log and optional samples remain available |
| Malformed payload projection | Empty, wrong-type, and multi-entry cleanup results are summarized beside the native fixture; full callback logs remain available and payload-shape rejection stays in the production path |
| Phase-aware replicated rejection | One fixture waits for Playing and ownership readiness before a lagged GAS target-data submission; activation, authority rejection, no-spawn, and cleanup remain required |
| Phase-aware replicated valid confirmation | One fixture waits for Playing and ownership readiness before a GAS target-data submission; authoritative spawn, peer convergence, ability cleanup, and test-actor cleanup remain required while physical input stays unclaimed |
| Atomic compile evidence | A valid running record and atomic final replacement prevent partial JSON reads and redundant compile retries; compiler logs remain available for changed failures |
| Combined resume and source projection | One bounded resume call carries gate/process facts plus source identity and hashes; complete provenance files remain authoritative on disk |
| Atomic gate contract publication | Phase updates are written to a temporary JSON and renamed into place; resumes never need to recover from a half-written contract |
| Stable current provenance pointer | A deliberate current snapshot refresh avoids ledger/path churn while numbered snapshots preserve immutable history |
| Client-owned packaged shutdown | Holding the host until every client reaches results removes a reproducible harness race without changing shipping code; v1/v2 logs remain available to prove the causal ordering and client error criteria stay strict |
| UBT failure-signal admission | Detecting `Result: Failed` and compiler signatures prevents a false process-zero compile from feeding Cook/Stage/Pak; package preflight also rejects records without the new signal field, while raw logs and the original process exit remain preserved |
| UBT startup exception admission | Unhandled/ACL/rename failures are promoted from stderr to bounded `failureSignals`, preventing stale editor launches and repeated lifecycle retries; generated trace backups remain preserved for diagnosis |
| Delayed packaged shop probe | One development-only timer drives the existing widget/RPC path after controller readiness; physical input and catalog-ID verification remain separate, so the structured check does not weaken UI acceptance |
| Bounded duplicate resume action | SummaryOnly shortens only the repeated ledger markdown line; the complete line remains in `codex-resume-brief.json` and the contract next action remains available |
| Canonical compile project path | Tier1 resolves relative project inputs against the configured root before UBT starts and records the original input plus `projectFileWasRelative`; a path ambiguity now fails or resolves during cheap admission instead of after an expensive build |
| Candidate consistency guard | `Get-ResumeBrief.ps1` compares the ledger and gate-contract candidate before artifact selection; a drift warning prevents stale log inspection without changing provenance or acceptance criteria |
| Compact workflow hygiene brief | `Get-WorkflowValidationBrief.ps1` performs the repeated script/JSON/diff/process/candidate checks in one bounded result; it never replaces compile, runtime, UI, LAN, or acceptance evidence |
| Source and helper freshness guard | `Get-NextIterationBrief.ps1` compares current checkout hashes and the test-script inventory to the recorded packet inputs; stale provenance becomes one bounded refresh warning while full evidence and every acceptance requirement remain intact |
| Candidate artifact identity guard | The selected gate contract carries the package-time source identity and frozen-scope flag; the packet warns when current dirty inputs differ, preventing stale artifact reuse without declaring a valid frozen scope invalid |
| Lifecycle fixture admission and projection | The rematch probe checks purchased-upgrade/stat prerequisites before travel and one compact brief reports reset markers and errors; an editor lifecycle is not spent on an impossible setup and raw log scans stay out of routine context |
| Packaged transport verdict persistence | A four-process run stores host readiness, client exits, and owned teardown in one compact JSON; truncated console output can be recovered without a second package lifecycle, while transport remains separate from UI and LAN acceptance |

The compact brief is an inventory, not an acceptance verdict. A gate passes only from evidence whose topology and artifact identity match the requirement. This contract therefore saves context without lowering the standard for final LAN, packaged, physical-input, or end-to-end proof.

These are qualitative workflow improvements. Account-level token usage is not exposed to the project, so this document does not claim a percentage reduction from build counts or log sizes.

## Packaged shop continuation — September 15, 2026

The remote-client shop gate turned the remaining repeat patterns into explicit workflow rules.
Candidate 04 was rejected before staging after its relative direct-UBT path produced a process-zero
false pass; the compile gate now scans its requested log and runner stdout/stderr for `Result:
Failed`, compiler errors, and build exceptions. Candidate 05 then proved that a client startup
`ExecCmds` command can run in the frontend world and lose its timer during travel. Development
fixtures that need the destination world now use a no-world delegate, one core-ticker resolver,
and a lasting gameplay-world timer. A missing world is a setup failure, not a reason to repeat the
same lifecycle.

The following attempts refined the evidence boundary without changing shipping code. Expected
wallet results are derived from the loaded catalog and an immediate baseline, with a bounded
five-point allowance for passive income. Post-`ProcessEvent` verification uses a weak widget and
core ticker instead of a raw `UWorld` timer, avoiding a teardown crash. Wrapper markers include
the emitted `LogTemp`/`LogExit` category so command-line arguments cannot satisfy a pass pattern.
Candidate 09 passed with `AttackStrength` rank 1, 500→425 gold, client exit 0, and zero host/client
error, unknown, or critical lines; host `-1` is recorded only as owned teardown.

The next iteration should follow this bounded sequence:

1. Read `SLICE_STATUS.md` once, then run `Get-ResumeBrief.ps1 -SummaryOnly` with the contract,
   selected phase, and current source snapshot. The helper writes the full snapshot; do not pass
   unsupported output-file switches or reread the full ledger for routine status.
2. Refresh the stable source snapshot only after deliberate source or ledger edits. Compare its
   `sourceIdentity` to candidate provenance before compiling or packaging.
3. Compile `LyraGame` with an absolute project path and wait on the atomic `running` record. Admit a
   skip-build package only when `failureSignals` is present and empty.
4. Reuse candidate 09 for the passed remote-shop scope. Do not recook because a wrapper regex or
   evidence projection changed; persist a new bounded JSON record from the existing logs.
5. Keep physical target confirmation, normal human selection, packaged buildable UI/LAN,
   second-PC LAN, and clean-device deployment as separate proof gaps. A structured shop/RPC pass
   cannot close any of those UI or topology requirements.

## Candidate consistency continuation — September 15, 2026

The next review found one cheap source of avoidable context: the gate contract had advanced to
candidate 10 while the compact ledger still named candidate 09. Resume screening therefore
inspected the older artifact even though the next action pointed at the newer package. The ledger
now names candidate 10, and `Get-ResumeBrief.ps1` compares the ledger candidate with the contract
candidate and emits a warning before any artifact is selected when they differ. This is a
metadata-only guard; it does not declare an artifact valid or bypass source, asset, configuration,
or staged-binary provenance checks.

The same review confirmed that candidate 10 is a frozen package scope: its source snapshot,
LyraGame compile, elevated AutomationTool ACL preflight, Cook/Stage/Pak output, hashes, and NullRHI
smoke remain attached to the candidate. Later documentation and ledger edits belong to the current
snapshot and do not retroactively change the packaged binary. The next iteration should therefore
use one current snapshot plus the contract-aware resume call, then open only the candidate-specific
brief for the selected proof gap. Do not read candidate 09 logs unless the remote-shop scope is
the selected frozen check.

The repeated end-of-turn hygiene command is now `Scripts/Test/Get-WorkflowValidationBrief.ps1`.
It returns one compact record for PowerShell syntax, selected JSON parsing, diff whitespace,
candidate consistency, source identity, gate gaps, and owned-process counts. This avoids rebuilding
the same inline validation pipeline in conversation context while retaining every input record and
the explicit acceptance boundary.
