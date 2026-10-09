# ProjectB Agentic Test Ladder

Fast, scriptable verification for Cursor agents and CI. Tiers are ordered by cost; run the lowest tier that matches your change.

| Tier | Script | When | Needs |
|------|--------|------|-------|
| **1** | `Tier1-CompileGate.ps1` | After a coherent set of C++ changes | Engine source or install |
| **2** | `Tier2-StandaloneSmoke.ps1` *(planned)* | Match-flow / packaging regressions | Packaged `LyraGame.exe` |
| **3** | `Tier3-CQTest.ps1` *(planned)* | Core-loop step checklists (11-1, …) | Editor build + `BreakawayCoreTests` |
| **4** | CI workflows *(planned)* | PR / nightly | Build agents |

## Quick start (agents)

```powershell
cd "E:\Unreal Projects\ProjectB\Scripts\Test"

# Validate paths without compiling
.\Run-Tier.ps1 -Tier 1 -ValidateOnly

# Tier 1 — compile gate (LyraEditor, not ProjectBEditor)
.\Run-Tier.ps1 -Tier 1
```

Exit code **0** = pass. Non-zero = fail; read the log path printed by the script.

The checked-in ProjectB config selects the direct bundled UBT runner for source-built engines. It uses the bundled .NET runtime, stores UBA state under `Saved/Logs/compile/UBARoot`, and captures the UBT log at the requested path. This avoids the engine dependency wrapper's unbounded MSBuild node reuse and protected system UBA storage. Pass `-DirectUbt` to force this runner when testing a different config; omit it to use the configured mode. Pass `-OutputJson <path>` to persist the source revision, runner, process ID, exit code, elapsed time, and log paths so a compile result can be added to the gate contract without reconstructing it from console output. The record is written as valid JSON with `status=running` immediately after launch, then atomically replaced by the final `passed` or `failed` record; wait for that owned process and inspect the status before reading the compiler log.

Agents may run the build, fix compiler errors, and restart the project editor. Preserve unsaved work and close the editor gracefully when a full rebuild is required. Wait for existing build processes; do not start duplicates. When affected behavior or the selected gate requires editor/PIE checks after success, reuse a ready editor with the current binaries or launch the resolved engine's `Engine/Binaries/Win64/UnrealEditor.exe` with the absolute `.uproject` path, verify bridge readiness, and run the relevant PIE checks. Follow sandbox permissions and report build results separately from gameplay results.

When specifying both `-LogFile` and `-OutputJson`, give them distinct base names, such as
`compile-editor-ubt.log` and `compile-editor.json`. UBT creates its own `.json` log beside the
text log; matching base names can overwrite the running compile record with JSON-lines output.
Keep waiting on the existing build handle if this happens. The final atomic record restores the
verdict; a temporarily unreadable record is not a reason to start another build.

### Build only the project targets

Use `LyraEditor` for editor/PIE work and `LyraGame` for packaged-game work. Select the target needed
for the current phase; do not build both automatically. From the project root, use the appropriate
command with a fresh evidence filename:

```powershell
# Editor/PIE phase
.\Scripts\Test\Tier1-CompileGate.ps1 -Target LyraEditor -OutputJson Saved\Logs\compile-editor.json

# Packaged-game phase
.\Scripts\Test\Tier1-CompileGate.ps1 -Target LyraGame -OutputJson Saved\Logs\compile-game.json
```

The configured defaults are Win64 Development. Pass the successful game compile record to
`Package-Preflight.ps1 -CompiledTargetRecord` before a `-skipbuild` package.

When reflected property layouts or native defaults used by cooked assets change, also compile
`LyraEditor` before cooking. The cooker loads editor DLLs; the staged game uses `LyraGame`.
Verify the affected editor runtime DLL advanced, then cook a fresh candidate with matching
layouts. Candidate42's relic component change exposed this mismatch as shifted properties and
an `AsyncLoading2` bad-export-index crash despite successful cooking. A game compile alone
does not establish cooker schema freshness.

Do not use Visual Studio **Build Solution** or **Rebuild Solution**, whole-solution MSBuild,
all-target builds, or standalone engine test/utility targets. UBT can still build dependencies
required by the selected project target. ProjectB automation tests continue to run through the
editor; they do not require rebuilding standalone engine test programs such as `PCGTests`.

The September 22, 2026 cleanup covers only the 40 directories listed in
`Saved/Logs/engine-storage-cleanup-proposal-20260922.json`. Its outcome is recorded in
`Saved/Logs/engine-storage-cleanup-result-20260922.json`. This is not recurring authorization to
clear other intermediates, caches, binaries, symbols, source, assets, or retained packages.

### Packaged LAN browser fixture

Pass `-DiagnosticExecCommands @('bway.Combat.DebugNumbersRouter 1','bway.Combat.DebugNumbers 1')`
when combat delivery is part of the gate. Each command gets its own recorded `-ExecCmds` argument;
the defaults remain unchanged. Commands must be single lines without embedded quotes.

For controlled listener delivery, also include `bway.Test.ListenerDelivery` in that array.
It applies small production damage/healing effects and a carrier-fumble scenario in both
gameplay lifetimes, then resets its relic setup. Use it only in an owned disposable match;
the resulting health/stat events are deliberate fixture actions. Require per-peer incoming,
outgoing, and healing number logs plus both fumble/cleanup markers. See
`AI_Planning/MESSAGE_LISTENER_LIFECYCLE.md` for the packaged evidence and verifier scope.

`Run-PackagedLANBrowserProbe.ps1` reuses the four-process launcher for a verified,
frozen candidate. `Connected` tests actual browser entry joins and shared return/rehost.
`Rediscovery` additionally tests full client disconnect, standalone frontend recovery,
fresh search requests, and different advertised session IDs before rejoining.
`SelectionEdges` exercises remote invalid and duplicate submissions, repeated locks,
selection-time disconnect/rejoin, and unique team rosters after travel. It temporarily
holds selection for 120 seconds; normal selection timing requires separate evidence.

Validate the selected artifact's provenance and hashes first. Then validate the launch
paths without starting processes:

```powershell
.\Scripts\Test\Run-PackagedLANBrowserProbe.ps1 -Candidate LAN-20260923-06 `
    -Variant Rediscovery -EvidenceDirectory Saved/Logs/lan-reconnect-20260923 `
    -Attempt r1 -ValidateOnly
```

Omit `-ValidateOnly` to launch. Choose a fresh attempt name; existing results and logs
are never overwritten. The runner renders all four processes, launches the three clients
concurrently before waiting on initial staging, uses `-PktLag=200`, and requires the
engine's packet-lag confirmation before advancing each initial join. Concurrent startup
prevents a slow rendered client from missing the host's first travel and leaving the
probe at its staging step.
An immediate `-ExecCmds="Net PktLag=200"` can run before a frontend network driver exists
and does not establish latency. The result preserves per-process exits and owned host
teardown separately. Run the gate's detailed evidence checker before accepting a result.
Physical input, visual polish, and second-PC LAN remain separate acceptance scopes.

The runner's transport verdict requires the selected fixture's detailed checker before
acceptance. Each fixture retains its own gameplay and cleanup requirements.

### Buildable PIE fixture

The development commands `bway.Test.AutoLockAllHeroes`, `bway.Test.BuildableCancel`,
`bway.Test.BuildableConfirm`, `bway.Test.BuildableSpawn`, and `bway.Test.BuildableResetRound`
resolve their intended PIE world internally when invoked through the bridge. Run them as one
bounded sequence after the playing-phase marker is present. `BuildableSpawn` records an early and
settled peer sample (0.5 and 2.0 seconds); require the settled four-world result for convergence
and keep the early sample in the saved log. `BuildableResetRound` preserves persistent actors and
resets per-round quota through the same public PlayerState boundary used at round start. A
no-hit `BuildableConfirm` remains active with quota unused by design; use `BuildableCancel` to
close that invalid-target probe, including when the placement spec is already active. These
fixtures prove native PIE scope only; physical valid-target input, latency rejection, packaged
UI/LAN, and second-PC checks remain separate acceptance gates.

After the run, call `Get-BuildableFixtureBrief.ps1 -LogFile <editor-log> -OutputJson
<brief-name>.json` once. It returns the spawn/quota/cancel/confirm/reset pass fields and settled
peer counts plus empty/wrong-type/multi-entry malformed-submission results without moving the full
editor log into context; add `-IncludeSamples` only for a changed replication diagnosis. A bare
filename is written beside the editor log; an absolute path or a relative path containing a
directory is written at that path, which avoids accidental nested evidence paths.

For a synthetic valid transport check, issue `bway.Test.BuildableReplicatedValidConfirm` after
`bway.Test.AutoLockAllHeroes`. It waits for Playing plus the ownership chain, sends one blocking
target through GAS, requires an authoritative spawn, four-world convergence, clean ability end,
and fixture-owned actor cleanup. This proves target-data transport and production placement logic;
it does not replace physical cursor selection or packaged UI/LAN proof.

For the authoritative rejection gate, issue `bway.Test.BuildableReplicatedQuotaReject` after
`bway.Test.AutoLockAllHeroes`. The command waits for the replicated Playing phase and the complete
client/authority ownership chain, activates the real placement ability on the owning client, sends
target data through GAS with `Net PktLag=100`, verifies the server rejects the used quota without a
second spawn, and restores the quota. Its `ReplicatedRejectSubmit`, authority target-data, and
`ReplicatedRejectResult` markers are included in the same compact brief. The valid transport command
uses `ReplicatedConfirmSubmit`, `ReplicatedConfirmResult`, `ReplicatedConfirm PeerConvergence`, and
`ReplicatedConfirmCleanup` markers, which the same brief parses into `pass.replicatedValidConfirm`.

## Editor Python and multiplayer evidence

Unreal's `AActor::GetFunctionCallspace` returns local execution while `GAllowActorScriptExecutionInEditor` is true. Editor Python sets this guard; it affects RPCs called indirectly by native widget handlers as well as direct RPC calls. A client-side Python call returning a success/rejection is therefore not proof of network transport.

For the upgrade shop, open the owning client's screen, then execute `bway.Test.ShopBuySelected` in that client's game world. The development-only command queues the normal widget purchase handler on a game timer and refuses execution if the guard remains active. Check its log for `authority=0 scriptguard=0`, then compare server and owning-client ranks, balances, effects, and response UI. Run fresh sessions after any direct authority test purchases. Same-process PIE still requires separate-process and packaged LAN follow-up.

## Engine resolution

For upgrade lifetime checks in four-world PIE, load `Verify-UpgradePersistence.py`
and call `start_upgrade_persistence(<fresh absolute JSON path>)` after Dorado's
Capture the Relic experience has four live human players and one relic per world.
The probe makes authority purchases, dispatches native delayed damage, and shortens
then restores the production round timer. It is not client RPC or physical-input
evidence. It owns a 100-second deadline, releases its callback, and saves full
samples; stop PIE and restore temporary play settings after the terminal result.
Use `Get-UpgradePersistenceBrief.ps1 -InputJson <record> -OutputJson <brief>` for
the independent rank/attribute, death, pawn replacement, map, and settlement checks.
Settlement resets on pawn/phase/round changes. The public GAS query functions can
be called through the `AbilitySystemBlueprintLibrary` CDO using each catalog
effect's modifier attribute; do not repeatedly probe protected `SpawnedAttributes`.

For the separate packaged-process lifetime gate, use the development commands
`bway.Test.UpgradeLifetimeAuthority` on a disposable four-human Dorado listen host
and `bway.Test.UpgradeLifetimeObserve` on its three clients. Give only clients
`-testexit="BwayUpgradeLifetimeTest: Client PASS"`; keep the host alive until all
clients finish. Extract `Get-PackagedUpgradeLifetimeBrief.ps1 -RunJson <runner JSON>
-OutputJson <brief>` from the saved logs. It checks eight catalog-priced purchases,
public attribute agreement/private ranks, the same remote victim and pre-death
pawn, replacement, later round, settled intervals, applied latency, and clean exits.
The native settle timer must rearm after readiness recovers even if its pawn/phase
key stays unchanged. Diagnostic `State` markers do not count as accepted samples.
The uncooked `UnrealEditor -game` shortcut failed CaptureTheRelic primary-experience
resolution in the September 21 run; do not replay it unchanged as packaged proof.

For death and respawn regression checks, execute `bway.Test.DamageRemotePlayer` in the listen server's game world while a remote human is alive. This development-only command queues lethal self-damage on a normal server tick and logs the script guard, granted abilities, health, and death state. Verify a replacement pawn, replicated death count, restored health, and enabled movement input on both server and owning client; repeat after respawn. Self-damage does not validate enemy kill credit. This command changes the running test match.

Set `UE_ENGINE_ROOT` if auto-detection fails:

```powershell
$env:UE_ENGINE_ROOT = 'E:\Github\UnrealEngine'   # parent of Engine/
.\Tier1-CompileGate.ps1
```

Otherwise the script reads `EngineAssociation` from `ProjectB.uproject` via the Epic Launcher registry (`HKCU\Software\Epic Games\Unreal Engine\Builds`).

## Target names

This Lyra fork uses **`LyraEditor`** / **`LyraGame`** UBT targets. The verified source-build package has a `Windows/LyraGame.exe` bootstrap and `Windows/ProjectB/Binaries/Win64/LyraGame.exe` executable.

Full tier design, CQTest mapping to Core Loop steps, and CI layout:  
`Plugins/GameFeatures/BreakawayCore/Docs/Agent_Testing_Ladder.md`

## Low-token packaged workflow

The repository's `AGENTS.md` directs vertical-slice work to this workflow. Start with `AI_Planning/SLICE_STATUS.md`, then use the helpers to consume a compact ledger and JSON result. Keep full evidence on disk:

```powershell
# From the project root. Inventory only: no build, editor launch, or playtest.
.\Scripts\Test\Get-ResumeBrief.ps1
```

The resume helper reads the ledger's `Current candidate` field, or an explicit `-Candidate` argument. It prints current HEAD, change count, candidate record availability, and a compact process inventory. ProjectB-owned processes are always included; unrelated Unreal process records are sampled to avoid flooding the agent context when a build host has spawned many `dotnet` workers. Full Git status and the complete process list remain in `Saved/Logs/codex-resume-brief.json`, replaced on each invocation. Pass `-IncludeAllProcesses` only when diagnosing process ownership. Add `-SummaryOnly` for routine gate resumes; it returns only the selected gate/process/count projection while preserving the same full snapshot. In this mode the repeated markdown next-action line is bounded; use `gate.nextAction` for the complete action and open `fullSnapshot` only when the ledger wording itself is diagnostic. Process discovery includes possible `dotnet` build hosts and is not exhaustive. Missing evidence produces a warning, never a gameplay pass. Matching HEAD alone does not establish package freshness: review the recorded project and engine patches, assets, untracked inputs, and configuration for the selected gate.

When resuming an active gate, pass `-GateContractPath` (and its phase when needed) to project the
current gate status, scope, proof-gap count, causal error, retry condition, and next action into
the same bounded resume result. If a source snapshot is part of the gate, add
`-SourceSnapshotPath` so its identity and file hashes arrive in the same decision packet; the
complete manifest remains on disk. The complete gate manifest remains at the returned `gate.path`:

```powershell
.\Get-ResumeBrief.ps1 -Candidate LAN-20260908-05 `
  -GateContractPath Saved\Logs\team-gameplay-20260912\gate-contract.json `
  -ContractPhase runtime `
  -SourceSnapshotPath Saved\Logs\team-gameplay-20260912\source-snapshot.json
```

Run package-specific helpers from `Scripts/Test`:

```powershell
.\Package-Preflight.ps1 -Candidate LAN-20260907-01 -OutputJson ..\..\Saved\Logs\preflight.json
# Build/cook/stage, then run TestProfiles.ShortMatch from BwayTestConfig.psd1.
.\Package-Preflight.ps1 -Candidate LAN-20260907-01 -RequireCandidate
$buildHead = (Get-Content '..\..\Saved\Logs\codex-packaged-LAN-20260907-01-project-head.txt').Trim()
.\New-AcceptanceSummary.ps1 -Candidate LAN-20260907-01 -SourceHead $buildHead `
  -Profile FrontendLAN -EvidenceLog 'E:\Unreal Projects\ProjectB\Saved\Logs\packaged-host.log' `
  -OutputJson ..\..\Saved\Logs\acceptance.json
```

Preflight reports the current checkout; it does not establish which source produced an existing package. `-RequireCandidate` additionally requires its actual executable. Inspect active processes and unsaved editor assets before building. Resolve `SourceHead` from that candidate's build evidence, retaining its source patch and artifact hashes separately. When a package uses `-skipbuild`, compile the **`LyraGame`** target first and pass its JSON record through `-CompiledTargetRecord`; preflight then requires a passing record at the current source head, a present game binary, and the hardened `failureSignals` field present and empty. A pre-change compile record is regenerated instead of trusted, and an editor-only compile is insufficient to prove the staged game binary matches the source.

For a new package, preflight also requires 10 GiB free on the project volume and records the
available and required bytes under `storage`. Current stages use about 4 GiB; the reserve also
allows packaging intermediates and logs, but does not guarantee capacity for larger cooks.
Increase `-MinimumFreeSpaceGiB` when the artifact grows. Resolve a failed capacity check before
retrying; do not lower the reserve to bypass a full disk. `-RequireCandidate` skips this check
because testing an existing artifact does not allocate a new stage.

Tier1 treats UBT startup failures as failures even when the wrapper process exits 0. Its bounded
`failureSignals` scan includes `Result: Failed`, compiler/fatal errors, unhandled exceptions,
`UnauthorizedAccessException`, access-denied path writes, and failed generated-file renames. This
prevents a stale binary or an ACL-blocked build from being admitted as a successful compile.

`Tier1-CompileGate.ps1` accepts the configured absolute project path by default. If a caller supplies
a relative `-ProjectFile`, the gate resolves it against `BwayTestConfig.psd1`'s `ProjectRoot`
before checking the file or starting UBT, and records `projectFileInput` plus
`projectFileWasRelative` in the running and final JSON records. This keeps path provenance visible
without spending a build on a working-directory mismatch; pass the absolute path when invoking
direct UBT from automation.

```powershell
.\Package-Preflight.ps1 -Candidate LAN-20260914-02 `
  -CompiledTargetRecord Saved\Logs\team-gameplay-20260912\compile-tier1-packaged-game-35a.json `
  -OutputJson Saved\Logs\package-preflight-compiled-target.json
```

When the build is running in a restricted permission context, add
`-RequireAutomationToolLogWrite`. The preflight opens the engine's existing
`Engine\Programs\AutomationTool\Saved\Logs\ErrorLog.txt` with read/write sharing and records a
compact failure before Cook/Stage/Pak starts. Use the recorded result to request the required build
permission once; do not spend a package lifecycle discovering the same ACL failure.

### Headless automation tests

Use `Run-AutomationTests.ps1` for editor automation filters. It resolves the configured source
editor, quotes the project/report/log arguments, refuses an existing report directory, owns one
editor PID with a bounded timeout, and writes a compact result that joins the process exit code
with the Automation report. Use `-ForceMemoryDDC` for this source engine when the default DDC has
no writable node. `-ValidateOnly` checks paths and report freshness without launching Unreal:

```powershell
.\Run-AutomationTests.ps1 -TestFilter Breakaway.Economy `
  -ForceMemoryDDC -ValidateOnly `
  -OutputPrefix ..\..\Saved\Logs\team-gameplay-20260912\automation-helper-validation
```

For a real run, choose a fresh prefix and omit `-ValidateOnly`. The result is `passed` only when
the report has no failed/not-run/warning tests, the log contains the framework exit-code-zero
marker, and the editor process exits zero. A test report with a nonzero process exit is retained as
`passed-test-report-process-anomaly` and remains a failed command for acceptance purposes.

The summary screens startup logs only. Every supplied log must exist, be nonempty, contain that profile's required patterns, and contain no forbidden patterns. Use `FrontendLAN` for a host that travels through the LAN playlist, or `ShortMatch` for direct DevMap startup. Client logs need their own relevant checks. A passing screen does not prove match completion, physical input, network transport, clean exit, or that a log belongs to the candidate.

For a startup-screen follow-up where the per-log rows are not needed in the conversation, add
`-SummaryOnly`. The complete evidence remains in `-OutputJson`; the compact projection returns
only the candidate, profile, gate booleans, failed-gate names, count, and next action. This keeps
screening evidence reviewable without copying every log row into context.

Before an expensive gate, use `New-SourceSnapshot.ps1` to persist the complete tracked patch and
untracked manifest without printing either to the agent context:

```powershell
.\New-SourceSnapshot.ps1 `
  -OutputPrefix ..\..\Saved\Logs\team-gameplay-20260912\source-snapshot-buildable
```

The JSON record reports the source HEAD, a stable `sourceIdentity` comparison index, dirty/tracked/
untracked counts, file sizes, and SHA-256 hashes. Compare `sourceIdentity` first; the `.patch`,
`.status`, and `.untracked` files plus individual hashes remain the provenance source of truth for
candidate reuse. Use a fresh prefix for immutable attempt history. For a gate's deliberate current
snapshot, keep one stable path (for example `source-snapshot-current`) and use `-Force` only after
the tracked ledger/configuration edits are complete; this keeps resume commands stable without
discarding earlier numbered snapshots.

`Get-NextIterationBrief.ps1` normalizes an optional UTF-8 BOM and line endings when checking those
files for freshness. A PowerShell encoding difference therefore cannot force a needless refresh,
while the original byte hashes and complete files remain available for provenance review.

On resuming active vertical-slice implementation: read the ledger, verify current HEAD/status, choose one pending gate at a time, then read only its code and evidence. Continue to the next authorized gate after a pass or recorded blocker while the goal remains active. Batch independent reads, keep full logs on disk, and print short results plus the first causal error. Retest passed gates when a relevant change invalidates their evidence. Update the ledger and candidate evidence after each verified result. A short match uses repeatable settings, but natural scoring and duration remain variable.

### Compact gate briefs

Use one selected evidence inventory after a gate starts. It records the current revision, dirty-file count, selected JSON summaries, whole-file log error counts, and project Unreal processes while keeping full evidence on disk:

```powershell
.\Get-GateBrief.ps1 -Gate marker-ui `
  -EvidenceDirectory ..\..\Saved\Logs\marker-ui-20260908 `
  -EvidenceFile verification-brief.json,screen-widget-runtime.json,ui-ownership.json `
  -LogFile Saved/Logs/codex-marker-ui-fixed-editor-20260908.log
```

Use `-ContractPath` when the gate contract is the primary record. It is an alias that adds the contract to the selected evidence files, and `-JsonFile` is accepted as a compatibility alias for `-EvidenceFile`, so a parameter-name typo does not turn a useful follow-up into a second invocation:

```powershell
.\Get-GateBrief.ps1 -Gate team-aware-combat-pass-score `
  -EvidenceDirectory ..\..\Saved\Logs\team-gameplay-20260912 `
  -ContractPath gate-contract.json `
  -LogFile editor-team-gate.log
```

When the contract already lists the phase evidence, let the brief select it instead of
retyping paths. `-ContractPhase runtime` (or `compile`, `package`, or `all`) reads that phase's
`evidence` array, routes `.log` entries through the bounded log scanner, and keeps the contract
itself in the JSON summary:

```powershell
.\Get-GateBrief.ps1 -Gate team-aware-combat-pass-score `
  -EvidenceDirectory ..\..\Saved\Logs\team-gameplay-20260912 `
  -ContractPath gate-contract.json -ContractPhase runtime
```

The console result is intentionally compact and the same record is written to `gate-brief.json`. Contract and evidence paths are deduplicated, and nested runtime/preflight records are projected to bounded scalar summaries; the full contract remains on disk for peer-level diagnosis. Add `-SummaryOnly` for routine follow-ups to return only phase, evidence/log counts, error totals, first causal error, process counts, scope, and output path while still writing the full JSON inventory. It is an inventory, not a pass. Do not include large JSON samples, screenshots, or full logs in the conversation unless the compact result identifies a failure that needs them. Use a new gate directory or a changed hypothesis before rerunning; do not overwrite a baseline or repeat an unchanged failed probe.

For the team-aware combat/pass fixture, use the dedicated marker brief instead of several ad hoc log searches:

```powershell
.\Get-TeamCombatFixtureBrief.ps1 `
  -LogFile ..\..\Saved\Logs\team-gameplay-20260912\editor-team-combat-pass-12.log `
  -OutputJson ..\..\Saved\Logs\team-gameplay-20260912\team-combat-fixture-brief.json
```

The helper reports the fixture markers, optional four-world peer-convergence markers, natural goal/round/results counts, error totals, and first actionable error in one compact result. In this authority-only scope it also reports recurring Zen and Pixel Streaming startup messages separately, so they do not displace a real fixture failure. Generic and packaged briefs keep Pixel Streaming errors actionable because they can affect rendered/UI evidence. The helper records `passed-scope-limited` only for the authority fixture; separate-process client transport, latency, physical input, and packaged LAN still require their own gates. When a native bridge command cannot target the authority PIE world, dispatch all independent authority commands in one `execute_python` call and retain the capability-cache entry instead of repeating unsupported reflection or editor-world calls.

The same helper recognizes `BwayTeamCombatRemote` markers from the separate-process remote-owner fixture and switches to its narrower required set (`Setup`, changed remote damage, pass setup/receive, and cleanup). The `remote-owner-score` mode additionally requires `RemoteScoreSetup`, real opposing-goal overlap, exact team/player score deltas, `RoundComplete`, and `RemoteScore COMPLETE`. The composite replication brief can therefore reuse the helper without misclassifying a valid remote-owner or score run as an older authority fixture.

When launching a packaged fixture with a network cvar, pass the cvar and the fixture as separate
`-ExecCmds` arguments and retain the argument array in the harness result JSON. A single
semicolon-delimited `-ExecCmds` value can start the process while dropping the time-sensitive
fixture command. The fixture command itself must wait for the authoritative `Playing` marker before
selecting or moving actors; a setup during Warmup can create a real death or respawn event before
the intended assertion sequence. Keep unclassified Blueprint warnings in the brief and change the
fixture or owning asset before adding any warning classification.

For the development-only rematch lifecycle probe, use the dedicated marker brief once after the
editor run:

```powershell
.\Get-MatchUpgradeLifecycleBrief.ps1 `
  -LogFile ..\..\Saved\Logs\buildable-placement-20260914\editor-match-upgrade-lifecycle.log `
  -OutputJson ..\..\Saved\Logs\buildable-placement-20260914\match-upgrade-lifecycle-brief.json
```

The helper reports setup, production rematch request, destination verification, PASS/FAIL/timeout,
setup-blocked state, unknown errors, and critical lines in one bounded record. A
`passed-scope-limited` result proves that a completed PostMatch rematch clears purchased upgrades,
match stats, and wallet state in a same-process authority fixture. It does not prove packaged LAN,
second-PC transport, or physical input. Keep the full editor log beside the compact JSON and inspect
raw lines only when the brief identifies a changed lifecycle failure.

For the packaged four-player variant, launch the host with
`bway.Test.MatchUpgradeLifecycleSeed` and each client with
`bway.Test.MatchUpgradeLifecycleObserve`. The native startup ticker waits for a game world and
four ready human PlayerStates. Setup buys a catalog-backed rank for every human, verifies the
exact debit, seeds nonzero stats, and invokes production round/rematch APIs. Start with a match
target above one point and a planning window long enough for client replication (the saved
runner uses five points and eight seconds). Keep the host alive until every client emits its
`ClientReset PASS` test-exit marker.

```powershell
.\Get-PackagedRematchBrief.ps1 `
  -RunJson ..\..\Saved\Logs\packaged-rematch-20260920\packaged-LAN-20260920-02-rematch-economy-result.json `
  -OutputJson ..\..\Saved\Logs\packaged-rematch-20260920\rematch-brief.json
```

This historical single-upgrade brief requires four distinct authority purchase records, a server reset verdict, and three
clients whose replicated purchase precedes a fresh-world reset in the requested destination.
Owner-only upgrades are checked on each owning client; each client also checks the replicated
roster's stats and wallets over a two-second settled interval. Purchase price assertions are
catalog-derived. The cross-process wallet sample allows up to 20 additional gold for passive
income between transaction and observation; the authority transaction itself requires the exact
debit. Full peer details remain in JSON. This is structured packaged travel/reset evidence;
physical shop/rematch input, LAN discovery, rendered presentation, and second-PC acceptance
remain separate gates.

For the current all-four-upgrade fixture, use `Get-AllUpgradeRematchBrief.ps1` with the frozen
catalog and terminal runner result. The fixture buys all four current IDs for each human and
requires public modifiers to clear in the next playable match after normal selection timeout.
The saved runner requires all four peers alive before normal client exits; it does not exit a
client at the earlier reset marker.

```powershell
.\Get-AllUpgradeRematchBrief.ps1 `
  -RunJson ..\..\Saved\Logs\all-upgrade-rematch-20260922\packaged-LAN-20260922-03-all-upgrade-rematch-result.json `
  -CatalogJson ..\..\Saved\Logs\all-upgrade-rematch-20260922\catalog-frozen.json `
  -OutputJson ..\..\Saved\Logs\all-upgrade-rematch-20260922\rematch-brief.json
```

This checker requires 16 price/debit records, 32 correlated attribute rows, owner before/reset
state, normal selection ordering, D3D12 initialization, observed 200 ms latency, and three normal
client exits. Catalogs and ranks replicate owner-only: resolve the local catalog when checking
remote public attributes. Requiring remote catalogs incorrectly blocks the observer. This scope
does not establish physical input, visual presentation, purchase RPC transport, or second-PC LAN.

Periodic native ticker callbacks receive the current frame delta, not the elapsed interval since
the previous callback. Use `FPlatformTime::Seconds()` for observer deadlines and settled windows.
Start a settled window at the first qualifying sample and reset it when its state or world changes;
do not count loading time as a settled sample. Candidate 02 of the packaged rematch gate preserves
the frame-delta failure; the corrected fixture also bounds destination diagnostics to once every
five seconds.

For separate-process PIE, preserve `Saved/Logs/ProjectB.log`, `ProjectB_2.log`, and `ProjectB_3.log` before the next launch because Unreal reuses those filenames. If a file rotates or becomes shorter than the baseline, archive the three files and create a new baseline instead of retrying against stale offsets. Use `Scripts/Test/Capture-PIEClientLogs.ps1 -Gate <gate> -Label <attempt> -OutputJson <path>` once after the run; it copies only the three client logs and records their paths and byte sizes. Read the copied files with a bounded marker query. If a client remains on `Experience still loading`, record that readiness result and stop the gate rather than repeating the same launch timing.

Use `Scripts/Test/Wait-SeparateProcessReady.ps1 -LogFiles <client1>,<client2>,<client3> -ExperienceMarker <experience> -TimeoutSeconds <n> -OutputJson <path>` once during each separate-process attempt. The comma-separated log list avoids PowerShell’s repeated-parameter binding failure. The helper watches only appended log bytes and returns one compact readiness record per child. For a staging-to-gameplay command, also pass `-HostLog <server-log> -HostExperienceMarker <staging-experience>` and, when the starting world is mandatory, `-HostMapMarker <map-name>`; the host marker/map marker become required readiness conditions, so an authority command cannot arrive while the server is still loading or on the wrong map. When gameplay commands require a live round, add `-HostRequiredMarker 'BwayRoundManagement: Started playing GAS phase'` (or the gate’s exact phase marker) so warmup transitions are gated too. Use `-RequiredMarker` only when the child marker is an independent prerequisite emitted before the authority command; do not use `Local player locked hero` to gate the AutoLock-before-travel flow because that marker is downstream of the command. Dispatch a time-sensitive authority fixture only when every child reports the expected `OnExperienceLoadComplete(..., Client)` marker and, when supplied, the child required marker, host `... Server` marker, map marker, and required phase marker. A `timeout` changes the readiness hypothesis; it is not evidence of combat or replication failure and should not trigger an unchanged relaunch.

For a second checkpoint after staging, pass `-BaselineJson <staging-readiness.json>`. The helper
starts each file at the byte offset recorded by that first checkpoint, so reused child logs cannot
satisfy the destination experience from an older attempt. The output preserves the final offsets
for the next checkpoint.

After the explicit staging-map load and before `play_in_editor start`, run
`Scripts/Test/New-LogOffsetBaseline.ps1` and pass its JSON as `-BaselineJson` to the first
readiness checkpoint. This records current byte lengths without claiming readiness and prevents
stale markers in reused `ProjectB*.log` files from satisfying staging. Taking the baseline after
PIE starts can exclude a host marker already emitted during startup.

After readiness and fixture collection, use `Get-ReplicationGateBrief.ps1` once to join the
readiness records, authority fixture, and multi-sample peer comparison. It persists the detailed
fixture and peer briefs beside one bounded composite record; add `-IncludeSamples` only when a
changed diagnosis needs marker rows:

```powershell
.\Get-ReplicationGateBrief.ps1 `
  -HostLog Saved\Logs\team-gameplay-20260912\editor-team-peer-multisample-latency.log `
  -ChildLogs 'Saved\Logs\ProjectB.log,Saved\Logs\ProjectB_2.log,Saved\Logs\ProjectB_3.log' `
  -ReadinessJson 'Saved\Logs\team-gameplay-20260912\separate-process-ready-peer-staging.json,Saved\Logs\team-gameplay-20260912\separate-process-ready-peer-capture.json' `
  -MinimumSamples 3 -OutputJson Saved\Logs\team-gameplay-20260912\replication-gate-brief.json
```

The composite status is `unverified` when any readiness record, fixture marker, sample-count
requirement, or authority comparison fails. It never upgrades partial transport evidence to full
gameplay or LAN acceptance. For adverse latency, use Unreal's exact `Net PktLag=100` syntax; the
space form is rejected and should not consume another runtime attempt. When the probed health can
regenerate during transport, omit `-ExpectedHealth` and pass a bounded `-HealthTolerance` (20 for
the remote-owner lag fixture) so paired authority samples account for timing without accepting a
missing sample or team mismatch. For the remote score fixture, collect those three samples before
the goal ends the round and resets health; the score markers independently prove the later state
transition. The development fixture polls for production damage before starting pass setup, so a
hero fire-interval race is recorded as a timeout rather than a false pass.

After a timeout, run `Scripts/Test/Get-SeparateProcessTravelBrief.ps1 -LogFiles <captured-child1>,<captured-child2>,<captured-child3> -HostLog <host-log> -OutputJson <path>`. It returns bounded per-process counts for map load, seamless travel, experience start/complete, loading-screen repetition, travel failures, and errors, plus a small recent-marker tail. Use those counts to choose the next hypothesis before reading raw log ranges. The helper is read-only and does not convert a transition timeout into a gameplay verdict.

When invoking `Set-GatePhase.ps1` through `pwsh -File`, use `-EvidenceCsv` and `-ProofGapsCsv` for comma-separated lists. The CSV forms avoid PowerShell array binding differences while writing the same evidence and proof-gap arrays.

For `Get-GateBrief.ps1`, a relative `-OutputJson` name is placed under `-EvidenceDirectory`; pass a filename (for example, `gate-brief-runtime.json`) or an absolute path. This keeps the persisted brief from being nested accidentally under the evidence directory.

For a live log that is polled more than once, use `Get-LogDeltaBrief.ps1` with a state file. The first call records the current byte offset; later calls inspect only appended lines and return new errors, critical lines, and any requested marker counts. This keeps repeated polling out of the conversation without changing the raw log or the acceptance scope:

```powershell
.\Get-LogDeltaBrief.ps1 `
  -LogFile ..\..\Saved\Logs\team-gameplay-20260912\editor-team-combat-pass-12.log `
  -StatePath ..\..\Saved\Logs\team-gameplay-20260912\team-combat-log-delta.json `
  -Pattern 'BwayTeamCombatTest:', 'Results screen shown'
```

Use `-Reset` only for a new evidence file or after log rotation. A delta result is an inventory, not a pass; use the full-log fixture or gate brief for the acceptance record.

For a poll that is expected to be unchanged, add `-QuietUnchanged`. It returns only the
zero-delta paths and totals while still advancing the state file; a poll with appended data
returns the normal error and marker details. Add `-OutputJson` when the delta itself is part of
the gate evidence so the complete result is retained on disk:

```powershell
.\Get-LogDeltaBrief.ps1 `
  -LogFile ..\..\Saved\Logs\team-gameplay-20260912\editor-team-combat-pass-12.log `
  -StatePath ..\..\Saved\Logs\team-gameplay-20260912\team-combat-log-delta.json `
  -QuietUnchanged `
  -OutputJson ..\..\Saved\Logs\team-gameplay-20260912\team-combat-log-delta-latest.json
```

When a gate has several attempts, summarize the family once instead of loading every log into context:

```powershell
.\Get-RunFamilyBrief.ps1 `
  -EvidenceDirectory ..\..\Saved\Logs\team-gameplay-20260912 `
  -FilePattern 'editor-team-combat-pass*.log' `
  -OutputJson ..\..\Saved\Logs\team-gameplay-20260912\team-combat-run-family.json
```

The family brief reports attempt count, byte and line totals, repeated error signatures, and the latest attempt. It does not replace the selected gate evidence or authorize a retry; retry only after a changed hypothesis.

If `hiddenEditorCount` is greater than zero, stop the gate preflight. Resolve and close the exact hidden editor normally before launching another instance; an untargetable process cannot prove UI or physical-input behavior.

### Gate preflight and editor launch

Before an expensive editor or multiplayer run, create one compact preflight record beside the gate contract. Include the source revision, editor and project paths, process inventory, requested and observed topology, bridge readiness, UI-channel availability, and the proof scope the run can satisfy. Stop before launch if a required topology or input channel is unavailable.

When the bridge launcher cannot resolve the source-built engine, use the parameterized direct-launch helper. It refuses a duplicate ProjectB editor and records the process ID, log path, and exact arguments:

```powershell
.\Scripts\Test\Start-ProjectBEditor.ps1 `
  -EngineRoot 'E:\Github\UnrealEngine' `
  -ZenDataPath 'E:\Unreal Projects\ProjectB\Saved\Logs\<gate>\ZenData' `
  -CreateZenDataPath `
  -LogFile 'E:\Unreal Projects\ProjectB\Saved\Logs\<gate>\editor.log' `
  -OutputJson 'E:\Unreal Projects\ProjectB\Saved\Logs\<gate>\editor-launch.json'
```

Use `-ValidateOnly` to check the resolved paths, duplicate-editor guard, log directory, Zen write access, source revision, dirty-file count, and command line without starting an editor. Pass `-CreateZenDataPath` for a project-local `Saved/Logs/<gate>/ZenData` directory when it may not exist; the launch record reports `zenDataPathCreated` and the helper still probes write access. Without the switch, a missing path remains a preflight error. The helper records the source snapshot, path, and write probe result in `editor-launch.json` and removes only its uniquely named temporary probe file.

If startup is blocked by Unreal's platform validation or Turnkey worker fan-out, use `-SkipPlatformValidation -SkipTurnkeyValidation` only after the Win64 SDK preflight has passed. These switches add Unreal's `-Multiprocess -Unattended` flags, which skip the editor-only startup checks; do not use them as evidence for a platform or packaging gate.

If the editor reports that the default Derived Data Cache graph has no writable nodes, add `-ForceMemoryDDC` for the runtime probe. This maps to Unreal's `-DDC-ForceMemoryCache` recovery flag and does not establish cache, build, or package acceptance.

Record bridge and reflection probes in a session capability cache. Mark each operation `supported`, `unsupported`, or `not attempted`; do not call an unchanged unsupported operation again. If Computer Use is unavailable or exposes no targetable window, record `uiChannel: unavailable` once and exclude only physical/UI assertions from that run.

Update the gate contract after each phase. `Set-GatePhase.ps1` writes the phase status, scope, evidence, proof gaps, and next action in one operation, publishing the complete JSON through a temporary-file rename. Pass `-OverallStatus` only when you are explicitly changing the gate's overall state; the helper never promotes a partial phase to a gameplay pass:

```powershell
.\Set-GatePhase.ps1 -ContractPath Saved/Logs/team-gameplay-20260912/gate-contract.json `
  -Phase runtime -PhaseStatus partial -Scope 'four-player PIE authority snapshot' `
  -Evidence editor-team-gate-humanflow.log `
  -ProofGaps 'human hero selection','damage and pass transport' `
  -NextAction 'Resolve hero selection, then run team-aware damage/pass/score.' `
  -OverallStatus runtime-pass-unverified
```

A compile result, runtime result, or package result can be partial evidence when its topology and proof gaps are explicit; it is not a pass until the required evidence is complete.

Use this order for each acceptance gate:

1. Define the expected observable result and identify the last relevant evidence. Verify the candidate's source record before reusing it.
   For tracked source snapshots, capture `git diff HEAD --binary` so both staged and unstaged edits are recorded. Plain `git diff` omits staged changes. Preserve untracked build inputs separately; neither command captures them.
2. Run the cheapest applicable check. Group related fixes before compiling; package once those checks pass when the gate requires it.
3. Record the process handle and log path. Wait for completion; inspect short log excerpts when new output or a failure changes the next action.
4. Diagnose a failure before rerunning. Record the hypothesis and what changed, or identify the repeat as a reproducibility check.
5. Update the compact ledger after verification. Keep detailed history in the acceptance document and retain full logs. Record test scope and remaining gaps.

For future comparisons, note build/package counts and repeated test attempts in the gate's evidence record. These measure workflow overhead; token savings remain unmeasured until comparable usage data exists. Keep the final multiplayer and end-to-end acceptance requirements.

Keep routine tool responses near 1,500 tokens: return the result, first causal error, and full evidence path. Expand a diagnostic excerpt when it cannot explain the failure. After two attempts with the same failure and no new evidence, record the retry condition and move to an independent gate while implementation is active. Preserve security approval boundaries.

Give each live observer one declared timeout and cleanup owner. If the expected transition is not observed, record the timeout and cleanup result in the gate contract, then change the observation point or use a native fixture before retrying. Do not repeat the same callback against unchanged source and evidence.

For physical separate-process tests, launch the verified candidate visibly and confirm its window in Computer Use before sending input. `-WindowStyle Normal` does not guarantee that a sandbox-launched process appears on the interactive desktop. If the process/log confirms startup but inventory exposes no client window, diagnose the execution context before another launch. Use an approved interactive launch when required, retain a distinct log for the retry, and record disposal of the unused test process. Do not count an untargetable frontend process as an input or LAN pass.

Keep `SLICE_STATUS.md` under about 100 lines by replacing stale state and linking detailed history. Workflow preparation during a user-requested pause does not authorize builds or playtests. These instructions guide future runs; they do not change account usage limits or establish a measured savings percentage.

### Record a gate without repeating history

Add this compact record to the relevant acceptance document after a meaningful result. Keep only its status, evidence link, and next action in the ledger. Use `unknown` for unavailable counts.

```text
Gate / expected observable result:
Candidate / source revision / patch and configuration evidence:
Result / scope actually verified / remaining gaps:
Evidence paths:
Builds / packages / test attempts / elapsed time:
Failure signature / attempted remedies / condition for retry:
Changes that invalidate this evidence:
Current process and log handles, observed at:
Next action after resume:
```

Count retries, including failed launches, and explain what changed. Compare overhead only across similar gates. Do not infer token savings from build counts or account-wide usage. Preserve the final four-player LAN and physical-input acceptance requirements.

## Results identity fixture

In an isolated development listen-server session with eight players and active hero selection, `bway.Test.ResultsIdentity negative-tie` sets duplicate names and negative scores. `bway.Test.ResultsIdentity unique-winner` resets that fixture and gives the highest player ID ten kills. The command uses native setters, refuses client worlds, and holds selection open. Start a fresh session afterward: it changes names, hero selection, timing, and stats.

Compare summaries on the authority and owning client after replication converges. Require eight unique IDs, one local-player column per peer, one matching MVP, preserved identity after team remapping, and portraits matching selected hero assets. Generic `set` console commands are rejected in editor PIE. These fixtures do not establish natural gameplay, final-stat arrival order, or physical input; see `AI_Planning/PACKAGED_LAN_ACCEPTANCE.md` for recorded evidence and outstanding checks.

## Verify final results independently of live stats

In a fresh development PIE listen-server session, configure four local peers with eight total players and remain in hero selection:

1. Run `bway.Test.ResultsIdentity negative-tie` in the server world and allow its names, hero choices, and stats to replicate.
2. Run `bway.Test.ResultsSnapshot` in the server world. It queues the production results RPC on a normal game tick, captures ten kills, and immediately resets that player's live stats.
3. Require the log marker `BwayResultsSnapshotFixture: sent authority=1 scriptguard=0` with `capturedKills=10 liveKills=0`.
4. In editor Python, execute `Scripts/Test/Verify-ResultsSnapshot.py`, then call `verify_results_snapshot(expected_peers=4)`. It checks the actual results widgets, every stat field, team aggregates, MVP, local identity, and portrait bindings across all peers.
5. Keep the generated `Saved/Logs/codex-results-snapshot-4-peers.json` with the runtime log. Stop PIE and restore the original client count.

The live player counter must be zero while the captured results retain ten. The fixture is excluded from Shipping, rejects client worlds, and requires the prepared eight-player selection state. Start a fresh session afterward. This verifies snapshot transport and widget data; natural match completion, separate processes, input, late arrival during PostMatch, and packaged LAN require their own checks.

For natural-match verification, start a fresh four-peer DevMap session and let the configured match finish without synthetic stats or forced goals. Execute the same Python verifier file, then call `verify_natural_results(expected_peers=4, run_name="natural-match-1")` after the breakdown appears. It compares every results column and aggregate to live authority, checks scores, rounds, portraits, and team display, and requires a credited relic goal. Keep the gameplay log to establish scoring provenance.

After recording that result, `verify_relic_scorer_identity()` checks authority-side identity retention through drop/throw/pass, reset cleanup, and rejection of post-match personal credit. This probe mutates the finished session; stop PIE afterward. It does not verify client release input or projectile goal collision.

## Selection presentation through round transitions

Load `Scripts/Test/Verify-SelectionPresentation.py` in editor Python after starting a fresh four-peer listen-server PIE session. `selection_frontend_snapshot("timed")` records the actual widgets and checks countdown visibility against remaining time; require four active selection rows to establish timed-selection coverage. The helper prints only pass/scope/peer-count/evidence by default; pass `verbose=True` when the peer rows themselves are needed in the conversation. The complete report is always written to `Saved/Logs`.

`probe = start_selection_overlap_probe()` waits for Warmup, opens local selection on the three existing remote peers, and lets gameplay score naturally. A passing probe requires a PostRound summary on the host, no overlapping summary and no countdown on the three selecting clients, then restoration of the still-active summary when one client closes selection. Inspect `probe["stage"] == "passed"` and the `postround-overlap` / `postround-restored` JSON files under `Saved/Logs`. It unregisters its callback on success or failure. If stopping PIE early, unregister `probe["handle"]` first; retain only scalar state, not world references.

This is a presentation fixture, not a new network arrival. Keep packaged admission and frontend discovery/return evidence separately. The observer begins during Warmup because a fast goal can make Playing shorter than its 0.25-second sampling interval. Check Blueprint validator error/warning counts after native header changes; the compile action's success field alone can miss graph errors.

## Compact packaged flow inventory

Use `Get-PackagedFlowBrief.ps1 -LogFiles <host-log>,<client-log>` to summarize the repeated selection, travel, results, and runtime-error searches. Counts are returned without an event tail by default; pass `-LastEventCount 8` (or another small value) only when chronology is needed. Pass `-OutputJson <path>` to preserve the complete inventory without a shell redirection step.

For the development-only remote shop transport probe, use `Get-PackagedShopProbeBrief.ps1` after
the owned host/client processes have exited:

```powershell
.\Get-PackagedShopProbeBrief.ps1 `
  -HostLog ..\..\Saved\Logs\packaged-LAN-20260915-09-remote-shop-host.log `
  -ClientLog ..\..\Saved\Logs\packaged-LAN-20260915-09-remote-shop-client.log `
  -Candidate LAN-20260915-09 `
  -SourceSnapshotPath ..\..\Saved\Logs\buildable-placement-20260914\source-snapshot-candidate09.json `
  -OutputJson ..\..\Saved\Logs\packaged-LAN-20260915-09-remote-shop-brief.json
```

The helper matches emitted `LogTemp`/`LogExit` categories, parses the data-derived catalog ID,
rank, cost, and before/after gold, and returns only a compact status line while persisting the
full marker and error inventory. It does not launch a process or recook an artifact. A pass proves
only the packaged listen-server widget/RPC scope; physical input, normal selection, buildable UI,
second-PC LAN, and clean-device checks remain separate.

Counts span each complete file and are not per-match assertions. The brief preserves total errors while separating only the known Zen startup message; Pixel Streaming errors remain actionable for rendered/UI checks. Duplicate starts require inspecting the relevant sequence and expected worlds; zero critical lines does not prove gameplay acceptance. Keep physical-input and rendered-UI observations alongside these logs. This helper is read-only and can inspect a live log without launching or restarting anything.

For a four-process packaged listen-server harness, persist the launcher verdict as a compact result
JSON before returning. Keep client exit status, host listen readiness, and owned host teardown in
separate fields; the host may be intentionally terminated after every client reaches Results. Read
that result plus one `Get-PackagedFlowBrief` inventory on resume. Do not recover a truncated console
verdict by rescanning full logs or relaunching a candidate whose source and artifact evidence are
unchanged.

## Compact workflow hygiene check

Use `Get-WorkflowValidationBrief.ps1` once after updating a gate contract, source snapshot, or
candidate record. It parses the test scripts and selected JSON records, checks `git diff --check`,
reconciles the ledger and contract candidates, and reports owned-process counts plus the selected
phase's proof-gap count. The result is a bounded JSON projection; it does not compile, launch,
inspect UI, or establish gameplay acceptance. It also records a sorted content hash for every
test PowerShell helper so the next-iteration packet can detect stale hygiene after a helper edit:

```powershell
.\Get-WorkflowValidationBrief.ps1 `
  -JsonPath Saved/Logs/buildable-placement-20260914/source-snapshot-current.json,Saved/Logs/buildable-placement-20260914/source-snapshot-candidate10.json,Saved/Logs/buildable-placement-20260914/compile-hero-reselect-20260915.json,Saved/Logs/buildable-placement-20260914/codex-packaged-LAN-20260915-10-preflight.json,Saved/Logs/packaged-LAN-20260915-10-directmatch-brief.json,Saved/Logs/buildable-placement-20260914/gate-contract.json,Saved/Logs/codex-resume-brief.json `
  -OutputJson Saved/Logs/buildable-placement-20260914/workflow-validation-brief.json
```

Paths are resolved against the project root when relative. Keep the full evidence files on disk and
open a raw record only when this hygiene result identifies a changed diagnosis.

## Movement upgrade evidence

`Get-MovementUpgradeBrief.ps1 -RunJson <runner-result.json> -CatalogJson <cold-catalog.json>
-OutputJson <brief.json>` derives the expected movement bonus and cost from the
saved catalog. It requires one listen host and three distinct clients at 200 ms,
all 16 peer speed-limit observations, owner-private ranks, bounded wallet deltas,
injected walking, a four-live-process checkpoint, normal client exits, and no
runtime errors. Exact debit/rollback, slow composition, replacement-avatar and
reset behavior are separate `Breakaway.Economy.MovementUpgrade` automation proofs.
It does not establish physical input, slide physics, final-alpha tuning or
second-PC LAN. Preserve the loaded catalog report with the candidate.

`Configure-MovementUpgrade.py` previews the one known catalog by default. The
explicit `-MovementUpgradeApply` flag adds the provisional movement entry through
Unreal editor APIs, validates only that asset, and saves only that catalog. A
separate cold commandlet verifies persistence. It refuses a dirty target or a
conflicting existing entry; it synchronously discovers only the Economy directory.

The same authoring helper supports `-UpgradeKind=CooldownReduction` with
`-CooldownUpgradeReport=<preview|apply|cold>-rN` and explicit
`-CooldownUpgradeApply` for saving. It preserves the other catalog entries.
For life leech, use `-UpgradeKind=LifeLeech`,
`-LifeLeechUpgradeReport=<preview|apply|cold>-rN`, and `-LifeLeechUpgradeApply`.
Reports are written under `Saved/Logs/life-leech-20260922/`. This adds a fifth
available choice while preserving four equipped slots. The helper records the
slot count and refuses an unexpected change to it.

`Breakaway.Economy.LifeLeech` tests the production damage listener and real
damage/healing effects, including fractional and capped healing, exclusions,
rank replacement, alternate four-item loadouts, reset, and listener teardown.
Run it through `Run-AutomationTests.ps1` with a fresh output prefix. This headless
scope does not establish multiplayer, packaged, physical, or visual acceptance.

For packaged life-leech purchase and health evidence, run the development commands
`bway.Test.LifeLeechHost` on authority and `bway.Test.LifeLeechClient` on each client.
The fixture requires exactly four human players on Dorado, funds an ordinary purchase window,
temporarily disables base healing, and observes a remote owner's fractional damage and healing.
It restores its health, wallet, upgrade, and base-healing changes. Preserve the harness's
four-process live checkpoint, three normal client exits, and separate owned host teardown.
Summarize the saved harness result with `Get-LifeLeechBrief.ps1 -RunJson <result.json>
-CatalogJson <frozen-catalog.json> -OutputJson <brief.json>`. The checker independently evaluates
damage mitigation, healing, owner privacy, and 32 peer rows. Its ten-gold purchase allowance
covers passive income while wallet/rank/attribute updates settle; exact authority debit remains
an automation assertion. See `AI_Planning/LIFE_LEECH_ACCEPTANCE.md` for scope and evidence.
These checks do not replace actual death/rematch, physical input, all-kit, or second-PC gates.

The alternate four-item lifecycle fixture uses
`bway.Test.UpgradeLifeLeechLifetimeAuthority` / `bway.Test.UpgradeLifeLeechLifetimeObserve`
for actual death, respawn, and rounds, and
`bway.Test.MatchUpgradeLifeLeechSeed` / `bway.Test.MatchUpgradeLifeLeechObserve`
for production rematch, normal selection timeout, and the next playable match. These equip
attack, armor, movement, and life leech; cooldown remains unequipped. Add `-LifeLeechLoadout`
to `Get-PackagedUpgradeLifetimeBrief.ps1` or `Get-AllUpgradeRematchBrief.ps1` when extracting
that variant. The default commands and extractor options retain their existing four-upgrade scope.

For actual primary attack provenance, launch a fresh four-human candidate in hero-selection
staging with `bway.Test.AllKitLeechHost` on the listen host and
`bway.Test.AllKitLeechClient` on each client. The host assigns one of each current hero through
the production selection manager; each owner activates its existing granted primary after
travel. Use separate `-ExecCmds` arguments for the fixture and `Net PktLag=200`. Preserve the
four-live checkpoint, three normal client exits, and separate owned host teardown.

Extract the result with `Get-AllKitLeechBrief.ps1 -RunJson <harness-result.json>
-CatalogJson <frozen-catalog.json> -OutputJson <brief.json>`. It requires four owning activations,
exact kit-derived damage/heal event counts and attribution, 128 peer health rows, private ranks,
seven owner/authority primary termination markers, and fixture cleanup. The fixture snapshots
live avatars after the round replaces pawns, and restores transforms, movement, view, health,
wallets, upgrades, new effects, base-healing rates, and listeners. It intentionally advances
match state and belongs in a disposable test session. This proves synthetic primary activation
and replication; physical controls, other abilities, presentation, and second-PC LAN remain
separate gates. Evidence and verifier controls are in `Saved/Logs/all-kit-leech-20260922/`.

For replicated leech boundaries, use `bway.Test.LeechLimitsHost` and
`bway.Test.LeechLimitsClient` in a disposable four-human Dorado session. The development probe
publishes complete numbered baseline/result frames; observers require settled production ASC
health and reject skipped frames. Its seven cases cover a fractional missing-health cap, full
health, immunity, friendly damage, self damage, overkill with a second postmortem hit, and delayed
damage attributed to a dead attacker. The last case calls the native damage path with persistent
PlayerState provenance; it does not prove an in-flight projectile's lifetime.

The fixture waits for production immunity to clear after the deferred GAS Playing transition.
It temporarily sets the respawn delay to 20 seconds for lethal observations, then waits for
the scheduled replacement pawns and restores the original delay. Death tags may clear when an
old pawn uninitializes: zero health must coincide with a death tag or missing pawn, and every
peer must subsequently observe a different living pawn. Cleanup restores health, wallets,
upgrades, base healing and the respawn setting, unregisters listeners, and destroys the probe.
Round progression and kill statistics belong to the disposable test session.

Use `Get-LeechLimitsBrief.ps1 -RunJson <harness-result.json>
-CatalogJson <frozen-catalog.json> -OutputJson <brief.json>` for the final extraction. The full
scope requires 256 peer rows, independent per-case damage/heal calculations and attribution,
natural replacements, cleanup, actor teardown, four-live proof, and three normal client exits.
Raw evidence and the copied-log checker controls are under
`Saved/Logs/life-leech-limits-20260922/`. This tests the current eligibility policy; final-alpha
reference tuning, other abilities, physical/visual and second-PC acceptance remain separate.

`Breakaway.Economy.CooldownUpgrade` exercises all four loaded hero ability sets,
their configured cooldowns and an effect-default fallback. Multiplayer prediction,
actual gameplay activation and HUD timing require separate runtime evidence.

`Breakaway.Abilities.Korryn.FlockLifecycle` activates the production Flock grant in an
isolated authority world. Four cases cover cancellation, timer completion, repeated use,
and restoration of distinct movement/collision values and ability tags. It advances only
that world's timers and clears cooldowns between cases; it does not prove network movement
or natural cooldown expiry.

For the all-16 cooldown gate, use `bway.Test.AllKitCooldownHost` and
`bway.Test.AllKitCooldownClient` in a fresh four-process hero-selection session. The host
selects one of each current hero, purchases the loaded cooldown upgrade between rounds,
and prepares sequential real-owner activations. Each ability completes two natural cooldown
cycles. Keep a long round (1800 seconds), separate `-ExecCmds` arguments for the probe and
`Net PktLag=200`, and the four-live checkpoint. Clients exit normally after observing
restoration and replicated probe destruction; the runner closes the host afterward.

The fixture validates floor, a 1300-unit dash route, target placement, and relic clearance
before each case. The spawn platforms are not assumed to be clear combat lanes. It uses
temporary 5000 maximum/4000 current health to keep cooldown observations independent of death
and restores health, maximum health, wallets, upgrades, transforms, control rotations, and
base healing. Match progression belongs to the disposable session. These controlled health
values cannot establish damage balance or final-alpha combat parity.

`Get-AllKitCooldownBrief.ps1 -HostLog <host> -ClientLogs <client1>,<client2>,<client3>
-TransportResult <runner.json> -ArtifactVerification <hash-check.json>
-SourceAudit <source-audit.json> -OutputJson <brief.json>` requires the complete 16-family
audit, 32 activation cycles, 64 owner HUD samples, server/owner natural expiries, rejection
during cooldown, 512 peer rows, restoration/teardown, artifact identity, and process exits.
Records are in `Saved/Logs/all-kit-cooldown-20260922/`; command implementation or a passing
compile alone does not establish acceptance. Physical input, visual presentation, full combat
effects, LAN discovery, and second-PC LAN remain separate requirements.

`Get-CooldownUpgradeBrief.ps1 -RunJson <runner-result.json> -CatalogJson <cold-catalog.json>
-KitJson <kit-reference.json> -OutputJson <brief.json>` requires four rendered
Argus processes at 200 ms. It compares the bought reduction and real ForGlory
cooldown against separately exported catalog/kit data, including eight authority
cycles, owner cooldown rejection, three HUD samples per owner, expiry and
reactivation. The probe checks the authored timer, progress bar and dimmed icon;
the separate overlay is optional. Effect-removal observers capture authority
expiry even when immediate reactivation occurs between polling samples, and are
removed during fixture cleanup. Physical input and visual inspection remain
separate acceptance requirements.

## Shop input and selector evidence

`Get-ShopInputBrief.ps1 -RunJson <runner-result.json> -OutputJson <brief.json>`
requires one listen host and three clients at 200 ms, a four-live-process checkpoint,
native opening/restart/close assertions, clean client exits, and no runtime errors.
Host teardown is recorded separately. Add `-RequireNavigation` for Slate gamepad
Accept and committed Down/Up selection with restored selector focus. The default
mode supports candidate11's earlier input scope; it does not imply navigation.

Add `-RequirePurchase` to require navigation plus actual Slate Buy activation,
remote pending/disabled focus, replicated rank/wallet, four independent exact
authority debits at the loaded catalog price, and gamepad Back. This mode checks
wallet/debit arithmetic in addition to native pass markers. Authority funding is
development-only and belongs to the disposable fixture match. For staggered
startup, wait on an independent possession/HUD marker; purchase readiness itself
depends on all four peers and cannot gate launching the next client.

`-RequireRespawn` additionally requires each player to observe death and possess a
different living pawn while retaining the same shop, focus, move/look locks,
purchase rank, and exactly one shop input binding. It also requires post-respawn
Slate navigation, four matching authority damage/respawn records, and exact death
counter increments. `bway.Test.ShopRespawnFocus` kills players sequentially through
production damage to avoid substituting a team wipe/round reset for respawn. The
fixture is scoped to four separate packaged processes and has a 180-second timeout;
its runner gives clients a 210-second normal engine exit deadline. A prior purchase
pass alone cannot satisfy this mode.

Controller-level test key injection bypasses CommonInput's Slate preprocessor.
Mirror its input-method update before dispatch and restore the original method in
cleanup; otherwise the first gamepad Accept can change modes before the virtual
cursor has reached its target. Keep default cursor policy and all focus assertions.
These synthetic checks do not replace physical devices. Purchase-button navigation
is covered only when the purchase mode is explicitly required.
For hidden packaged fixtures, engine `-seconds=<time>` can provide normal client
exit after a proved live checkpoint; keep the host alive until clients exit and
record owned host teardown separately. Do not retry an unavailable Windows close
handle or relaunch an unchanged artifact to reconstruct a saved verdict.

## One-command next-iteration packet

Use `Get-NextIterationBrief.ps1` when resuming a vertical-slice gate. It runs the
contract-aware resume inventory and combines the selected phase, candidate,
source identity, process counts, proof gaps, retry condition, and next action in
one bounded result. It also verifies that the source snapshot hashes still match the
checkout and that the workflow-helper inventory has not changed since the last hygiene
record. The full resume snapshot and workflow record remain on disk; the packet is an
inventory and does not compile, launch, inspect UI, or establish acceptance.

When only `-GateContractPath` is supplied, the source snapshot, workflow validation,
and output default to `source-snapshot-current.json`, `workflow-validation-brief.json`,
and `next-iteration-brief.json` beside that contract. Explicit path parameters override
those defaults, including when inspecting a frozen candidate snapshot.

```powershell
.\Get-NextIterationBrief.ps1 `
  -GateContractPath ..\..\Saved\Logs\buildable-placement-20260914\gate-contract.json `
  -ContractPhase package `
  -SourceSnapshotPath ..\..\Saved\Logs\buildable-placement-20260914\source-snapshot-current.json `
  -WorkflowValidationPath ..\..\Saved\Logs\buildable-placement-20260914\workflow-validation-brief.json `
  -OutputJson ..\..\Saved\Logs\buildable-placement-20260914\next-iteration-brief.json
```

After deliberate source or ledger edits, add `-RefreshSourceSnapshot`; after
contract, snapshot, ledger, or helper edits, add `-RefreshWorkflow`. These
switches are explicit because refreshing provenance or parsing every helper is
unnecessary for an unchanged gate. A stale or missing source snapshot or workflow
record is reported as a warning and should be refreshed before an expensive gate,
so a compact packet cannot silently reuse outdated provenance or hygiene. If the
contract records `candidateSourceIdentity`, the packet also warns when the selected
artifact is older than the current source snapshot; reuse then remains limited to the
candidate's recorded frozen scope.
