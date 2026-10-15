# Render Performance & Single-GPU Isolation Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED — VERTICAL SLICE 1.0 SIGNED OFF** (Full-flow `1920x1080` Frontend -> LAN Host -> Hero Selection -> Dorado match HUD layout and CSV render performance verified at **`6.33ms` mean frame time / `158.08 FPS`**, `6.85ms` p95, `0` VSM overflow warnings, and `0` widget overlap/clipping/focus-loss events across all match states; Single-GPU 4-process D3D12 contention isolated and solved via 2-Rendered/2-NullRHI topology).
- **Frozen Candidates:**
  - `LAN-20261015-01`: Exe SHA-256 `A0D887766ED66CC9A0C63F434302D272EBCE28AF3B3BB029B3899DE304D3AEB6` (Goal 4 full-flow `1920x1080` HUD layout audit + CSV render performance at `6.3258ms` mean / `158.08 FPS` + `0` VSM overflows — `Saved/Logs/goal4-20261015/goal4-fullflow-1080p-summary.json`).
  - `LAN-20261014-02`: Exe SHA-256 `996E8F7DEBC4B7D1D9F333E423EF5D9844F4FF60C27E2BF3A72CCFF2DB0C347E` (Goal 3 Dorado VSM Non-Nanite job queue tuning + 4-process 200ms run with `0` VSM overflows).
  - `LAN-20260927-44`: Exe SHA-256 `C7EE5676435FE341C2E3D9D5BFC529A162A435EA58735101FC207289F32247D6` (4-process contention matrix + 1080p baseline).
  - `LAN-20260930-49`: CSV profiler worker thread TLS exit diagnosis (`-csvNoProcessingThread`).

## 2. Production Architecture & Invariants

- **Single-GPU 4-Process Topology Rule (RTX 4070 SUPER 12GB):**
  - Running 4 concurrent D3D12 rendered processes on one 12GB GPU exhausts `11,848 MiB` VRAM (`99%` util) and causes `88.9–97.1ms` mean frame times dominated by `Sync_RenderingThread` stalls (`72.5ms`).
  - **Mandatory Gate Topology:** Always run **2 Rendered D3D12 peers** (Host + Active Remote Test Owner at `-ResX=640 -ResY=360 -ForceRes -MaxFPS=30`) + **2 `-NullRHI` witness peers**, holding VRAM to `7,277 MiB` and mean frame time to `9.42–9.70ms`.
- **Dorado Virtual Shadow Map (VSM) Non-Nanite Marking Invariant (`Config/DefaultEngine.ini`):**
  - Configures `r.Shadow.Virtual.NonNanite.IncludeInCoarsePages=0` and `r.Shadow.Virtual.MaxPhysicalPages=4096` to eliminate `Virtual Shadow Map Page Management: Non-Nanite Marking Job Queue overflow` warnings on `L_BW_Dorado` (`0` warnings in 4-world PIE, 4-process packaged runs, and 1080p full-flow captures).
- **CSV Profiler Shutdown Invariant:**
  - In UE 5.8, `FCsvProfilerProcessingThread::Run` can access freed TLS memory if the worker thread outlives `RequestEngineExit`, returning exit code `777003`. Pass `-csvNoProcessingThread` whenever `-csv` or `-csvGpuStats` is enabled on packaged test harnesses.

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| 4-Rendered D3D12 Contention Diagnosis | `Candidate44` (`4x D3D12 640x360`) | **ISOLATED** — `11,848 MiB` VRAM, `88.9–97.1ms` mean frame (`Saved/Logs/render-perf-20260927/perf-4d3d12.json`) |
| 2-Rendered D3D12 + 2-NullRHI Topology | `Candidate44` (`2x D3D12 + 2x NullRHI`) | **PASS** — `7,277 MiB` VRAM, `9.42–9.70ms` mean frame, `100%` < 16.6ms (`Saved/Logs/render-perf-20260927/perf-2d3d12-2null.json`) |
| Single-Process 1080p Dorado Baseline | `Candidate44` (`1920x1080 -ForceRes`) | **PASS** — `5.56ms` mean frame (`179.8 FPS`), `6.42ms` p95, `2.36ms` GPU (`Saved/Logs/render-perf-20260927/perf-1080p-single.json`) |
| CSV Profiler Clean Exit Verification | `Candidate49` (`-csv -csvNoProcessingThread`) | **PASS** — Process exits `0` with valid CSV output (`Saved/Logs/render-perf-20260930/csv-exit-check.json`) |
| Dorado VSM Non-Nanite Queue Optimization | `LAN-20261014-02` (4-world PIE + 4-proc packaged) | **PASS** — `0` VSM overflow warnings across all peers (`Saved/Logs/goal3-packaged-20261014/packaged-LAN-20261014-02-200ms-result.json`) |
| Full-Flow 1080p HUD Layout & CSV Render Perf | `LAN-20261015-01` (`1920x1080 -ForceRes -NoVSync -csvGpuStats -csvNoProcessingThread`) | **PASS** — `6.3258ms` mean frame (`158.08 FPS` > 120 FPS budget), `6.8500ms` p95, `0` VSM overflows, and `0` overlap/clipping/focus loss across `Frontend`, `HeroSelection`, `Prematch`, `Playing`, `CallForRelicMarker`, `UpgradeShop`, `Dead/Respawning`, `PostRound`, and `PostMatch` (`Saved/Logs/goal4-20261015/goal4-fullflow-1080p-summary.json`) |

## 4. Remaining Open Proof Gaps

- None for single-machine 1080p Vertical Slice 1.0 acceptance; multi-machine physical hardware LAN playtest remains optional post-slice validation.

## 5. Archive Pointer

- Full chronological gate diary: [RENDER_PERFORMANCE_ACCEPTANCE.md](./Archive/Full_History_20261013/RENDER_PERFORMANCE_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
