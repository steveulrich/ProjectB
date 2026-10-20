# Render Performance & Single-GPU Isolation Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** Earlier performance/topology checks retain their scope. October 15 full-flow results are diagnostic observations, not final slice acceptance: the probe hardcoded zero overlap/VSM events, accepted widget activation as focus, and clamped frame samples to 50 ms.
- **Frozen Candidates:**
  - `LAN-20261019-01`: Exe SHA-256 `64FAB3BD1EC1F0C7FCA61CFFE0EC8F7DCC0D45F531F5F8AE2672E50E9064C338` (Goal 3 Dorado VSM Nanite mesh + `M_TeamColorBasic` `bUsedWithNanite=True` fix with reverted Epic shadow cvars: `0` CSV `VSM/OVERFLOW_FLAGS` frames and `0` `[VSM]` log warnings across 2,400-frame 1080p cooked captures and 4-process 200ms packaged run).
  - `LAN-20261015-01`: Exe SHA-256 `A0D887766ED66CC9A0C63F434302D272EBCE28AF3B3BB029B3899DE304D3AEB6`. The old summary at `Saved/Logs/goal4-20261015/goal4-fullflow-1080p-summary.json` is retained as historical evidence; its blanket pass is superseded by this review.
  - `LAN-20261014-02`: Exe SHA-256 `996E8F7DEBC4B7D1D9F333E423EF5D9844F4FF60C27E2BF3A72CCFF2DB0C347E` (earlier Goal 3 Dorado VSM config-only run).
  - `LAN-20260927-44`: Exe SHA-256 `C7EE5676435FE341C2E3D9D5BFC529A162A435EA58735101FC207289F32247D6` (4-process contention matrix + 1080p baseline).
  - `LAN-20260930-49`: CSV profiler worker thread TLS exit diagnosis (`-csvNoProcessingThread`).

## 2. Production Architecture & Invariants

- **Single-GPU 4-Process Topology Rule (RTX 4070 SUPER 12GB):**
  - Running 4 concurrent D3D12 rendered processes on one 12GB GPU exhausts `11,848 MiB` VRAM (`99%` util) and causes `88.9–97.1ms` mean frame times dominated by `Sync_RenderingThread` stalls (`72.5ms`).
  - **Mandatory Gate Topology:** Always run **2 Rendered D3D12 peers** (Host + Active Remote Test Owner at `-ResX=640 -ResY=360 -ForceRes -MaxFPS=30`) + **2 `-NullRHI` witness peers**, holding VRAM to `7,277 MiB` and mean frame time to `9.42–9.70ms`.
- **Dorado Virtual Shadow Map (VSM) Non-Nanite Marking Invariant (`L_BW_Dorado` & `/Game/Characters/Cosmetics/M_TeamColorBasic`):**
  - All 16 large `el_dorado_fbx_Scene_*` arena static meshes have Nanite enabled and their shared Masked material `M_TeamColorBasic` sets `bUsedWithNanite=True`, routing those large shadow receivers/casters through Nanite VSM while preserving Lyra/engine Epic shadow defaults (`ResolutionLodBiasDirectional=-1.5`, `Clipmap.FirstLevel=6`, `LastLevel=22`, `RadiusThreshold=0.01`).
- **CSV Profiler Shutdown Invariant:**
  - In UE 5.8, `FCsvProfilerProcessingThread::Run` can access freed TLS memory if the worker thread outlives `RequestEngineExit`, returning exit code `777003`. Pass `-csvNoProcessingThread` whenever `-csv` or `-csvGpuStats` is enabled on packaged test harnesses.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| 4-Rendered D3D12 Contention Diagnosis | `Candidate44` (`4x D3D12 640x360`) | **ISOLATED** — `11,848 MiB` VRAM, `88.9–97.1ms` mean frame (`Saved/Logs/render-perf-20260927/perf-4d3d12.json`) |
| 2-Rendered D3D12 + 2-NullRHI Topology | `Candidate44` (`2x D3D12 + 2x NullRHI`) | **PASS** — `7,277 MiB` VRAM, `9.42–9.70ms` mean frame, `100%` < 16.6ms (`Saved/Logs/render-perf-20260927/perf-2d3d12-2null.json`) |
| Single-Process 1080p Dorado Baseline | `Candidate44` (`1920x1080 -ForceRes`) | **PASS** — `5.56ms` mean frame (`179.8 FPS`), `6.42ms` p95, `2.36ms` GPU (`Saved/Logs/render-perf-20260927/perf-1080p-single.json`) |
| CSV Profiler Clean Exit Verification | `Candidate49` (`-csv -csvNoProcessingThread`) | **PASS** — Process exits `0` with valid CSV output (`Saved/Logs/render-perf-20260930/csv-exit-check.json`) |
| Dorado VSM Nanite + Epic Shadow Verification (Cooked 1080p + 4-Proc) | `LAN-20261019-01` (`Measure-DoradoVsm.ps1` `E`/`E2` + 4-proc 200ms) | **PASS** — `0` CSV `VSM/OVERFLOW_FLAGS` frames (`2,400` frames/run), `0` `[VSM]` warnings, `310.0` mean requested pages, `25.8` non-Nanite pre-cull, `11.67ms` mean frame, 1080p `HighResShot` captures (`Saved/Logs/goal3-respawn-travel-20261019/vsm/vsm-summary.json`) |
| October 15 full-flow observations | `LAN-20261015-01` (`1920x1080`) | **PARTIAL** — reported 360 samples, 6.3258 ms mean and 6.85 ms p95; independently validate CSV data and warning counts. Widget presence does not prove layout/focus or physical input. |

## 4. Remaining Open Proof Gaps

- Independently validate unclamped 1080p timings and VSM warning counts; observe layout, focus, and physical KBM/gamepad behavior in the final integrated flow.
- Current queue Goals 4–8 remain separate polish work. Goal 9 owns human sign-off before roster expansion; the older Goal 4 label does not satisfy that gate.
- Review source removes manufactured PASS markers, uses actual viewport size, preserves unclamped frame times, and labels unmeasured visual/input/VSM checks explicitly. The frozen candidate does not contain these changes.

## 5. Archive Pointer

- Full chronological gate diary: [RENDER_PERFORMANCE_ACCEPTANCE.md](./Archive/Full_History_20261013/RENDER_PERFORMANCE_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
