# Render performance acceptance

## Current result

Candidate `LAN-20260927-44` has a measured rendering-contention problem when four packaged
instances render on the same GPU. This is a diagnostic result, not completed performance acceptance.
Disabling rendering in two background peers reduced the remaining peers' mean frame times by about
90%. All four peers stayed connected and completed the same ownership fixture.

Candidate49 now has a debugger-confirmed CSV worker shutdown defect and a verified capture workaround.
The CSV processing thread faults while clearing its allocator TLS cache after engine shutdown.
Adding `-csvNoProcessingThread` to the same CSV/timed-exit configuration completed 3,000 frames and
exited zero. This changes capture overhead; use a fresh baseline with that flag for future comparisons.
The engine defect itself is not repaired. Keep timing, fixture completion, and process-exit acceptance
separate. The original travel ASC GUID defect also remains open.

## Candidate49 shutdown diagnosis (September 30)

All 131 staged manifest hashes were rechecked. The current source differs from the recorded package
snapshot in two editor-only bridge handler sections; neither is part of the packaged game.
Five launches used the frozen Candidate49 executable without a build, cook, or editor launch:

| Run | Changed condition | Observed result |
|---|---|---|
| `perf-exit49-01` | CSV capture; remove `ExitAfterCsvProfiling`, allow normal `-seconds=65` exit | Capture completed, game continued about 48 seconds, exit 777003 |
| `shutdown-debug49-01` | No profiling, debugger, 25-second exit | No stack captured; debugger startup commands were skipped by `-g`. Exit accounting inconclusive; owned processes and settings cleanup subsequently verified |
| `shutdown-normal49-01` | No profiling or debugger, 25-second normal exit | Exit 0 and normal exit marker; settings restored |
| `shutdown-debug49-02` | Failing CSV configuration with debugger exception commands enabled | First and second chance access violation captured; halted debugger/game explicitly terminated afterward |
| `perf-exit49-02` | Same configuration as `perf-exit49-01`, add only `-csvNoProcessingThread` | 3,000-frame capture completed; normal timed exit 0; settings restored |

The captured stack is `FCsvProfilerProcessingThread::Run` (`CsvProfiler.cpp:3368`) →
`FPerThreadFreeBlockLists::ClearTLS` → `UnregisterThreadFreeBlockLists` → invalid critical-section
access in `RtlEnterCriticalSection` (write to address `0x24`). This identifies the worker teardown
as the primary failure; 777003 is the secondary crash-reporter failure code. Delaying shutdown after
capture completion did not resolve it. The worker-disabled run verifies a diagnostic workaround,
not a production engine patch or complete performance acceptance.

Evidence is in `Saved/Logs/render-performance-20260930/`: each run has arguments, PID, log, result,
and settings backup. `shutdown-debug49-02-debug.log` preserves the symbolized exception stack;
`shutdown-diagnosis.json` joins the verdicts. The unsuccessful first debugger setup and one blocked
launch while the second debugger was still held are preserved; neither is a successful exit claim.
No game was launched by the blocked preflight.

## Measurement contract

- Hardware: Ryzen 7 9800X3D, 32 GB RAM, RTX 4070 SUPER with 12,282 MiB reported GPU memory.
  The game selected the NVIDIA adapter. Driver: 610.88.
- Artifact: packaged Windows Development candidate44; executable SHA-256
  `C7EE5676435FE341C2E3D9D5BFC529A162A435EA58735101FC207289F32247D6`.
- Comparison workload: listen host, three separate local clients, four bots, four distinct human
  heroes, Dorado, 200 ms packet lag, the existing 23-stage relic ownership/carry fixture.
- Rendering: offscreen D3D12, requested 640 × 360, existing Epic scalability settings, fixed fixture cameras.
  The comparison did not save a dimension-verifying screenshot; preserve the requested-resolution qualifier.
  `t.MaxFPS 30` was requested at startup; timing proves it was not a persistent gameplay cap.
- Instrumentation: CPU, frame, GPU, and bookmark traces on the host and client1; GPU usage and
  owned-process memory/CPU counters approximately every three seconds.
- Comparison variable: only client2 and client3 switched from D3D12 rendering to NullRHI.
  Neither production visual settings nor gameplay source changed.
- Sample scope: fixture setup plus two seconds through completion minus two seconds. Both roles
  use the same wall-clock interval within each run. First dated engine logs approximate trace
  origins; the guards exclude setup and shutdown. Raw traces remain authoritative.
- Diagnostic threshold: 33.33 ms. Provisional final single-client target: 16.67 ms at 1920 × 1080.
  The final target question is pending; this assumption does not establish acceptance.

## Controlled comparison

| Measurement | Four rendered peers | Two rendered, two NullRHI peers |
|---|---:|---:|
| Host mean game-thread frame | 97.08 ms | 9.42 ms |
| Host p95 / p99 | 119.01 / 196.05 ms | 10.77 / 11.48 ms |
| Client1 mean game-thread frame | 88.92 ms | 9.70 ms |
| Client1 p95 / p99 | 105.95 / 114.25 ms | 10.96 / 12.29 ms |
| Host / client1 sample counts | 267 / 292 | 2,106 / 2,046 |
| Host / client1 time in rendering synchronization | 95.81% / 96.48% | 64.86% / 70.71% |
| Mean GPU memory used | 11,768.75 MiB | 7,231.50 MiB |
| Peak GPU memory used | 11,848 MiB | 7,277 MiB |
| Mean GPU busy | 97.38% | 93.00% |
| Fixture PASS markers | Four peers | Four peers |
| Client1 / client2 / client3 OS exits | 777003 / 0 / 0 | 777003 / 0 / 0 |

The baseline's game thread waits on render synchronization; the render thread waits for visibility
tasks. GPU queue durations also expand under contention. Near-capacity GPU memory and the controlled
render-count change make shared rendering resources the leading cause. These measurements do not
isolate GPU residency/paging from scheduling and execution contention. Do not label a specific GPU
pass or gameplay replication code as the root cause from this evidence.

The earlier one-process trace at requested 640 × 360 provides context only: its 30–45 second window averaged
3.85 ms with p95 4.76 ms across 3,892 frames. Its seven-bot standalone workload differs from the
comparison, so it is not an A/B result.

## Capture limitations and failures

1. `perf-solo01` captured 900 startup CSV frames and finished before Playing. Its CSV is valid
   startup evidence only. The process exited 777003.
2. `perf-solo02` reached gameplay, but the `LoadingScreen/Hide` CSV trigger never started capture.
   Its delayed cap and screenshot commands did not execute. Reused its completed trace instead of
   retrying the same trigger. Its process also exited 777003.
3. `perf-four01` and `perf-four02` completed all four fixture markers, but client1, which was traced,
   exited 777003. Untraced clients2/3 exited zero. The host was terminated by owned fixture cleanup
   after client completion; host teardown is not a normal gameplay-exit claim.
4. This engine names 777003 `CrashReporterCrashed`. The Windows crash-reporting thread's exception
   handler terminates the process with that value. Normal `LogExit: Exiting` text does not override
   the observed OS failure. The later no-trace failure rules out tracing as a necessary condition.
5. Insights reported pending RHI command lists, interleaved/negative GPU event durations, and
   `MemAlloc` tag-tracker errors. Preserve those analyzer logs. This capture does not prove memory
   allocation correctness; GPU queue durations include waiting and are not isolated execution time.
6. Timer-statistics exports include GPU rows even with a CPU thread filter. Analysis uses uniquely
   named CPU timers and explicit thread IDs/depth for frame events; it does not sum overlapping
   CPU/GPU rows. GPU timing warnings limit per-pass conclusions.
7. The first four-process export escaped Windows backslashes and wrote incorrect output paths.
   Re-exported the existing traces with forward slashes and verified every expected file. No game
   relaunch was needed. Exit zero alone is insufficient export validation.
8. Candidate logs retain a Virtual Shadow Map non-Nanite queue-overflow warning. Its visual impact
   and recurrence under target conditions require inspection; do not silently classify it away.

## Actual 1080p rendering baseline and presentation gaps

`perf-target01` requested 1920 × 1080 but produced an 888 × 500 screenshot during Warmup.
Unreal's window-resolution validation clamps sizes to the available desktop unless `-ForceRes` is
present. This invalidates its requested-resolution claim; it is not 1080p acceptance evidence.

`perf-target02` adds `-ForceRes`. Its screenshot verifies 1920 × 1080 during Playing, with the HUD
included through `Shot SHOWUI`. The CSV contains 3,000 frames; 1,834 first-round gameplay samples
remain after excluding the first 60 Playing frames, the final 30 frames, and screenshot-adjacent frames.

| CSV timing | Mean | p95 | p99 |
|---|---:|---:|---:|
| Frame | 5.56 ms | 6.42 ms | 6.91 ms |
| Game thread | 1.96 ms | 2.55 ms | 2.92 ms |
| Render thread | 5.56 ms | 6.41 ms | 6.89 ms |
| RHI thread | 1.50 ms | 1.90 ms | 2.26 ms |
| GPU | 4.96 ms | 5.43 ms | 5.62 ms |

None of those selected frame samples exceeded 16.67 ms. These counters can include waiting and
overlap; do not sum them. CSV parsing uses the final header because startup can add counters. Phase
frame numbers use the logged CSV command anchors to disambiguate the log's modulo-1000 frame index.

This is a limited direct-map rendering baseline. The screenshot shows 100 health, six empty ability
slots, overlapping gamepad input labels, and a camera looking across the arena from a raised platform.
It does not establish a correctly selected local hero, an appropriate gameplay camera, or readable
controls. Recheck those observations through the normal frontend and hero-selection flow before
attributing them to production gameplay. The VSM overflow message is visible and remains actionable.
The CSV frame overlay is instrumentation, not a shipping HUD feature.

Both historical Candidate44 target captures completed CSV output and restored the prior packaged user-settings file byte
for byte, but exited 777003. `perf-target01` had CPU/frame tracing; `perf-target02` had no trace
argument. Both logs contain zero ordinary runtime error/critical lines; that does not override the
failed process exits. The Candidate49 worker teardown diagnosis and verified capture workaround are recorded above.

## Evidence and continuation

Evidence directory: `Saved/Logs/render-performance-20260927/`.

- `perf-four01-analysis.json` and `perf-four02-analysis.json`: scoped timing, sensor, and exit results.
- `perf-four01-result.json` and `perf-four02-result.json`: arguments, owned PIDs, markers, cleanup.
- Per-run `*-preflight.json`, `*-sensors.jsonl`, logs, and raw `.utrace` files retain the measurement
  contract and observations. `*-export3*` contains the comparable gameplay exports and analyzer logs.
- `solo02-analysis.json`: separate standalone context and failed CSV-trigger/shutdown accounting.
- `perf-target02-analysis.json`, `perf-target02.png`, and the referenced CSV: actual dimensions,
  phase-scoped rendering timing, visible presentation issues, and failed exit accounting.
- `perf-target01-result.json`: rejected resolution/setup attempt; its screenshot remains in the
  candidate's `Saved/Screenshots/Windows/` directory.
- `Saved/run-perf-four44.ps1`: preserved ownership workload with tracing/sensors and an explicit
  `-HeadlessBackgroundPeers` diagnostic switch. This switch cannot satisfy four-rendered acceptance.
- `Saved/export-perf44.ps1` and `Saved/analyze-render-perf44.py`: export and bounded result extraction.

This gate used six packaged attempts: two initial standalone captures, two four-process comparisons,
and two target-resolution captures. It required zero builds, cooks, packages, or editor launches.
Raw trace exports were reused after path/interval corrections. CSV extraction was corrected for the
final header and modulo frame anchors without relaunching either target capture.

Next: capture a correctly selected human hero through the normal frontend/LAN flow at verified 1080p,
then inspect the camera, six ability slots, labels, carry/release, and VSM warning. Use one rendered
peer with headless network peers only for this scoped diagnostic; retain four-rendered presentation
as a separate gate. Do not lower production visual quality from the same-PC stress result alone.
Moving cameras, combat/effects bursts, physical controls, second-PC LAN, and physical menu quit remain
unverified within final integrated acceptance. Candidate49's unprofiled timed shutdown and worker-disabled
CSV shutdown pass their scoped exit checks.

For the independent travel defect, see [Travel GUID investigation](./TRAVEL_GUID_INVESTIGATION.md).
