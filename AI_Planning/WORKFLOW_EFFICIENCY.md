# Agent Workflow Efficiency & Operating Contract

**Last Updated:** October 13, 2026
**Scope:** Mandatory operational rules for AI coding agents working in `ProjectB` (UE 5.8.1).
**Historical Archive:** The 565-line chronological efficiency diary is archived at [Archive/Full_History_20261013/WORKFLOW_EFFICIENCY.md](./Archive/Full_History_20261013/WORKFLOW_EFFICIENCY.md). **Do NOT read the archived file during routine gates.**

---

## 1. Context & Resume Hygiene

1. **Single-Command Gate Resume:**
   - Begin every vertical-slice or `/goal` session by reading [SLICE_STATUS.md](./SLICE_STATUS.md) and running `Scripts/Test/Get-NextIterationBrief.ps1` **once**.
   - Add `-RefreshSourceSnapshot -RefreshWorkflow` only after deliberate source, ledger, contract, or helper edits.
   - Treat a `sourceSnapshotFreshness` warning or `candidateIdentityMismatch` between `SLICE_STATUS.md` (`**Current candidate:**`) and the active gate contract as a hard preflight stop until reconciled.
2. **Zero Raw-Log & Archive Ingestion:**
   - **NEVER** read raw `Saved/Logs/*.log` files, `Saved/Logs/*.json` full snapshots, or any file under `AI_Planning/Archive/` into LLM context.
   - Always query logs via bounded PowerShell helpers (`Get-GateBrief.ps1 -SummaryOnly`, `Get-LogDeltaBrief.ps1`, `Get-ReplicationGateBrief.ps1`, `Get-PackagedShopProbeBrief.ps1`, `Get-TeamCombatFixtureBrief.ps1`, `Get-RunFamilyBrief.ps1`).
   - Keep routine tool outputs under ~1,500 tokens per call.
3. **Gate Ledger & Acceptance Hygiene:**
   - Keep [SLICE_STATUS.md](./SLICE_STATUS.md) and active `*_ACCEPTANCE.md` / `*_ANIMATION.md` files **strictly <= 100 lines**.
   - Advance phase state atomically via `Scripts/Test/Set-GatePhase.ps1` (`compile`, `runtime`, `package`). When a phase has no current causal error, pass `firstCausalError: null` so `Get-GateBrief.ps1 -SummaryOnly` never resurfaces stale errors.

---

## 2. Build, Automation & Package Circuit Breakers

1. **Cheapest Verification First (Strict Tier Progression):**
   - **Tier 1 (Compile):** Build only the target required for the current phase via `Scripts/Test/Tier1-CompileGate.ps1 -Target <LyraEditor|LyraGame> -OutputJson <path>`. Never run Visual Studio Build/Rebuild Solution, whole-solution MSBuild, or standalone engine test targets.
   - **Tier 2 (Headless Automation):** Prove all C++ math, attribute scaling, ability lifecycles, and asset audits in `LyraEditor` using `Scripts/Test/Run-AutomationTests.ps1 -TestFilter <prefix> -OutputPrefix <fresh-base> -ForceMemoryDDC` before launching interactive PIE or compiling `LyraGame`.
   - **Tier 3 (4-World PIE):** Verify replication, GameplayCues, and authority/proxy convergence in 4-world PIE (`Wait-SeparateProcessReady.ps1` + `New-LogOffsetBaseline.ps1`) before packaging.
   - **Tier 4 (Packaged 4-Process Gate):** Compile `LyraGame`, verify `failureSignals` is empty, run `Scripts/Test/Package-Preflight.ps1 -CompiledTargetRecord <compile-game.json> -RequireAutomationToolLogWrite`, and stage/package **once** after Tiers 1–3 pass.
2. **Hard Per-Gate Circuit Breakers:**
   - **Max 5 `LyraEditor` builds** per gate.
   - **Max 2 `LyraGame` builds** per gate.
   - **Max 1 `BuildCookRun` package** per gate.
   - **Two-Strike Retry Stop:** After **2 attempts** with the same failure signature and no new causal evidence, **STOP** repeating that operation. Record the command, first causal error, attempted remedies, and retry condition in the gate contract, and yield or pivot to an independent authorized check.
3. **Process & Lock Ownership:**
   - Before compiling, verify no live or hidden `UnrealEditor.exe` / `LyraGame.exe` process holds binary locks (`hiddenEditorCount == 0`). Close owned processes cleanly before rebuilding; never launch duplicate editors.

---

## 3. Single-GPU 4-Process Topology Rule (RTX 4070 SUPER 12GB)

1. **Never Run 4 Concurrent D3D12 Rendered Peers:**
   - Running 4 simultaneous D3D12 processes on one 12GB GPU consumes `11,848 MiB` VRAM (`99%`) and causes `88.9–97.1ms` mean frame times (`Sync_RenderingThread` stalls) and `200ms+` authority frame spikes (see [RENDER_PERFORMANCE_ACCEPTANCE.md](./RENDER_PERFORMANCE_ACCEPTANCE.md)).
2. **Mandatory 2-Rendered + 2-NullRHI Split:**
   - For all 4-process packaged latency/transport gates on a single workstation, launch:
     - **2 Rendered D3D12 Processes:** Listen Host + Active Remote Test Owner at `-WinX=... -WinY=... -ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30`.
     - **2 `-NullRHI` Witness Processes:** Remaining 2 client peers launched with `-NullRHI -MaxFPS=30` to validate 4-peer replication, state convergence, and negative controls at `~7.2 GiB` VRAM and `<10ms` frame times.
3. **CSV Profiler Teardown Flag:**
   - Whenever `-csv` or `-csvGpuStats` is passed to a packaged process, always include `-csvNoProcessingThread` to prevent the UE 5.8 `FCsvProfilerProcessingThread::Run` TLS exit crash (`exit 777003`).
4. **Separate `-ExecCmds` Arguments:**
   - When combining a network cvar (`Net PktLag=100`) with a development fixture command, pass separate `-ExecCmds` arguments as supported by the runner and keep the listen host alive until all 3 clients hit their test-exit trigger.

---

## 4. Python Bridge & PIE Execution Safety

1. **Never Dispatch Gameplay RPCs or Ability Activations Under `FEditorScriptExecutionGuard` (`guard=1`):**
   - Executing gameplay RPCs, `TryActivateAbility`, or `CheatManager` mutations directly inside an Editor Python call (`guard=1`) suppresses or alters normal replication/RPC execution paths.
   - Always register a no-world console command (`FConsoleCommandWithArgsDelegate`) that resolves the target PIE world (`EditorLevelLibrary.get_pie_worlds(False)[0]` for authority) and queues gameplay mutations onto a native game-thread ticker (`FTSTicker::GetCoreTicker()`, where `guard=0`).
2. **Zero Retained PIE Wrapper References:**
   - Wrap all Editor Python inspections inside a function that returns only serialized primitives (`dict`/`list`/`str`/`int`/`float`) and cleans up local references in `try/finally` before seamless travel or stopping PIE.
3. **Unsupported Bridge Capabilities:**
   - PIE `UWorld` wrappers in this engine bridge do not expose `get_net_mode()`. In the 4-world listen topology, index `0` of `EditorLevelLibrary.get_pie_worlds(False)` is the authority world; do not probe `get_net_mode()` repeatedly.
