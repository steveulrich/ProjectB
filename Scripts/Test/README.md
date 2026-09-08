# ProjectB Agentic Test Ladder

Fast, scriptable verification for Cursor agents and CI. Tiers are ordered by cost; run the lowest tier that matches your change.

| Tier | Script | When | Needs |
|------|--------|------|-------|
| **1** | `Tier1-CompileGate.ps1` | After any C++ edit | Engine source or install |
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

Agents may run the build, fix compiler errors, and restart the project editor. Preserve unsaved work and close the editor gracefully when a full rebuild is required. Wait for existing build processes; do not start duplicates. After success, launch the resolved engine's `Engine/Binaries/Win64/UnrealEditor.exe` with the absolute `.uproject` path, verify bridge readiness, and run the relevant PIE checks. Follow sandbox permissions and report build results separately from gameplay results.

## Editor Python and multiplayer evidence

Unreal's `AActor::GetFunctionCallspace` returns local execution while `GAllowActorScriptExecutionInEditor` is true. Editor Python sets this guard; it affects RPCs called indirectly by native widget handlers as well as direct RPC calls. A client-side Python call returning a success/rejection is therefore not proof of network transport.

For the upgrade shop, open the owning client's screen, then execute `bway.Test.ShopBuySelected` in that client's game world. The development-only command queues the normal widget purchase handler on a game timer and refuses execution if the guard remains active. Check its log for `authority=0 scriptguard=0`, then compare server and owning-client ranks, balances, effects, and response UI. Run fresh sessions after any direct authority test purchases. Same-process PIE still requires separate-process and packaged LAN follow-up.

## Engine resolution

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
.\Package-Preflight.ps1 -Candidate LAN-20260907-01 -OutputJson ..\..\Saved\Logs\preflight.json
# Build/cook/stage, then run TestProfiles.ShortMatch from BwayTestConfig.psd1.
.\Package-Preflight.ps1 -Candidate LAN-20260907-01 -RequireCandidate
$buildHead = (Get-Content '..\..\Saved\Logs\codex-packaged-LAN-20260907-01-project-head.txt').Trim()
.\New-AcceptanceSummary.ps1 -Candidate LAN-20260907-01 -SourceHead $buildHead `
  -Profile FrontendLAN -EvidenceLog 'E:\Unreal Projects\ProjectB\Saved\Logs\packaged-host.log' `
  -OutputJson ..\..\Saved\Logs\acceptance.json
```

Preflight reports the current checkout; it does not establish which source produced an existing package. `-RequireCandidate` additionally requires its actual executable. Inspect active processes and unsaved editor assets before building. Resolve `SourceHead` from that candidate's build evidence, retaining its source patch and artifact hashes separately.

The summary screens startup logs only. Every supplied log must exist, be nonempty, contain that profile's required patterns, and contain no forbidden patterns. Use `FrontendLAN` for a host that travels through the LAN playlist, or `ShortMatch` for direct DevMap startup. Client logs need their own relevant checks. A passing screen does not prove match completion, physical input, network transport, clean exit, or that a log belongs to the candidate.

On resumption: read the ledger, verify current HEAD/status, choose one pending gate, then read only its code and evidence. Batch independent reads, keep full logs on disk, and print short results plus the first causal error. Retest passed gates when a relevant change invalidates their evidence. Update the ledger and candidate evidence after each verified result. A short match uses repeatable settings, but natural scoring and duration remain variable.

Use this order for each acceptance gate:

1. Define the expected observable result and identify the last relevant evidence. Verify the candidate's source record before reusing it.
2. Run the cheapest applicable check. Group related fixes before compiling; package once those checks pass when the gate requires it.
3. Record the process handle and log path. Wait for completion; inspect short log excerpts when new output or a failure changes the next action.
4. Diagnose a failure before rerunning. Record the hypothesis and what changed, or identify the repeat as a reproducibility check.
5. Update the compact ledger after verification. Keep detailed history in the acceptance document and retain full logs. Record test scope and remaining gaps.

For future comparisons, note build/package counts and repeated test attempts in the gate's evidence record. These measure workflow overhead; token savings remain unmeasured until comparable usage data exists. Keep the final multiplayer and end-to-end acceptance requirements.

Keep routine tool responses near 1,500 tokens: return the result, first causal error, and full evidence path. Expand a diagnostic excerpt when it cannot explain the failure. After two attempts with the same failure and no new evidence, record the retry condition and move to an independent gate while implementation is active. Preserve security approval boundaries.

Keep `SLICE_STATUS.md` under about 100 lines by replacing stale state and linking detailed history. Workflow preparation during a user-requested pause does not authorize builds or playtests. These instructions guide future runs; they do not change account usage limits or establish a measured savings percentage.

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
