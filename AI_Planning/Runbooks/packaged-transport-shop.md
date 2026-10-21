# Verify packaged transport and shop flows

Use this runbook after the [build and package preflight](build-package.md#prove-package-readiness). Command paths are relative to the project root. Preserve the candidate's recorded source, patch, assets, configuration, snapshot, and artifact identity. A matching candidate can be reused; reading evidence does not justify another launch or recook.

## Use the single-workstation transport topology

For four-process packaged latency and transport gates on one workstation, use two rendered peers and two NullRHI witnesses. On the RTX 4070 SUPER 12 GB, do not run four concurrent D3D12 rendered peers.

| Role | Launch settings |
| --- | --- |
| Listen host and active remote test owner | D3D12, with `-WinX=... -WinY=... -ResX=640 -ResY=360 -ForceRes -Windowed -MaxFPS=30` |
| Remaining two client witnesses | `-NullRHI -MaxFPS=30` |

The witnesses prove four-peer replication, state convergence, and negative controls. They do not prove rendered UI or physical input. Record requested and observed topology and retain any separate rendered/UI and final LAN requirements.

The recorded four-rendered baseline consumed 11,848 MiB VRAM (99%), with 88.9–97.1 ms mean frame times, `Sync_RenderingThread` stalls, and authority spikes above 200 ms. The split topology recorded about 7.2 GiB VRAM and frame times below 10 ms. These are historical measurements, not current-run assertions; see [RENDER_PERFORMANCE_ACCEPTANCE.md](../RENDER_PERFORMANCE_ACCEPTANCE.md).

Whenever a packaged process receives `-csv` or `-csvGpuStats`, also pass `-csvNoProcessingThread`. This avoids the UE 5.8 `FCsvProfilerProcessingThread::Run` TLS exit crash (`exit 777003`).

## Dispatch and finish transport fixtures

When combining a network cvar such as `Net PktLag=100` with a development fixture command, pass separate `-ExecCmds` arguments as supported by the runner. Preserve both in its recorded argument list. A semicolon-delimited value can launch the process but drop the time-sensitive fixture command.

For an authority fixture issued during startup, wait for the exact `Playing` phase before selecting or moving actors. Keep retries bounded. Preserve an unclassified asset warning in the brief instead of relabeling it as startup noise.

Give clients the result-screen test-exit trigger. Keep the listen host alive until all three clients reach that trigger. A host `PostMatch` test-exit can disconnect a slower client before it processes replicated results. Terminate the host during owned fixture cleanup and record host teardown separately from client gameplay and error status.

Match wrapper markers against emitted log categories such as `LogTemp: .*` or `LogExit: .*`. Command-line arguments can contain the same test-exit text and must not satisfy a pass pattern.

Before returning, persist a compact JSON harness verdict with separate fields for host readiness, client exit status, and owned host teardown. On resume, read that verdict plus one count-only packaged-flow brief. Do not reconstruct a truncated console verdict from raw logs or relaunch an unchanged candidate. Request a small event tail only for an ordering question; use `-OutputJson` when preserving the flow inventory as evidence.

## Probe the packaged shop

Do not count an immediate `-ExecCmds=ToggleUpgradeShop` as UI evidence; startup commands can run before the local controller exists. Use the development-only delayed `bway.Test.ShopOpenAndBuy` probe for structured widget/RPC coverage. Verify the catalog's actual stable ID. Keep physical B-key and gamepad input as separate acceptance proof.

For a packaged client probe launched through startup `ExecCmds`, use a no-world delegate and one bounded ticker to resolve the post-travel gameplay world before starting timers. Derive expected wallet assertions from the loaded catalog and an immediate baseline. Document a bounded passive-income tolerance.

After `ProcessEvent`, verify through weak widget references or a freshly resolved world. Never schedule a post-action callback through a raw world pointer that could be tearing down.

After the owned run, use `Scripts/Test/Get-PackagedShopProbeBrief.ps1` with host/client logs and the candidate snapshot. Persist its compact, data-derived result. This extracts evidence; it does not justify relaunching or recooking a candidate whose artifact identity still matches. Preserve physical shop input, rendered flow, and packaged LAN proof gaps that the structured probe cannot establish.
