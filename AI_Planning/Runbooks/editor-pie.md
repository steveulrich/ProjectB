# Own editor and PIE lifecycles

Use this runbook for editor launch, Play in Editor (PIE), bridge inspection, and UI observation. Command paths are relative to the project root. Consult [Agent_Testing_Ladder.md](../../Plugins/GameFeatures/BreakawayCore/Docs/Agent_Testing_Ladder.md) when restarting the editor or collecting multiplayer evidence. The [gate runbook](gate-resume-evidence.md) owns preflight, evidence, and retry rules.

## Own the editor lifecycle

Create one process/evidence contract for the gate. Reuse its editor and build handles, recorded PID, and log. Wait on existing processes; do not launch a duplicate editor or compensate for an untargetable process by launching another.

Treat `hiddenEditorCount` as a stop signal. Identify the exact owner and close it normally before launching another editor. If shutdown reports a forced kill, verify the exact `UnrealEditor` PID before compiling. Close the known lock owner once, then retry the compile only after it exits. Never repeat an unchanged locked build.

When the bridge launcher cannot resolve the source-built engine, use `Scripts/Test/Start-ProjectBEditor.ps1`. For a project-local cache directory, pass `-CreateZenDataPath` so the single `-ValidateOnly`/launch preflight provisions a missing Zen path. Close that editor normally before rebuilding.

Restore temporary PIE flags, cheats, fixtures, and input overrides immediately after the run. Stop callbacks during cleanup and verify the closed-editor INI before the next gate.

## Choose supported observations

Prefer structured bridge or CLI inspection when it answers the question. Use fresh UI observations for visual polish, physical input, frontend flow, and security dialogs. Select, observe, act once, then refresh; do not reuse stale screenshot state.

If Computer Use is unavailable or has no targetable window, record `uiChannel: unavailable` once. Skip only physical/UI assertions. Continue structured checks whose scope remains valid, and retain the physical/UI proof gaps.

Cache bridge and reflection capabilities in the gate evidence. Record unsupported operations once and switch to a supported path. Do not repeat an unchanged unsupported call or treat a capability failure as gameplay evidence.

## Inspect PIE without retained wrappers

Put every Editor Python inspection inside a function that returns only serialized primitives (`dict`, `list`, `str`, `int`, or `float`). Release local references in `try/finally`, including exception paths, before seamless travel or stopping PIE. Never leave world, actor, widget, or component wrappers in interpreter globals. If a failed probe retained objects, clear its references or restart the editor before continuing travel testing.

Give each observer one declared timeout and cleanup owner. Do not redeclare a live observer. If the expected transition does not arrive, record the partial result and change the observation point or fixture before retrying. Do not repeat the same callback against unchanged source and evidence.

## Dispatch gameplay outside the Python guard

Do not execute gameplay RPCs, `TryActivateAbility`, or `CheatManager` mutations directly inside Editor Python while `FEditorScriptExecutionGuard` is active (`guard=1`). That context suppresses or alters normal RPC and replication paths.

Register a no-world console command with `FConsoleCommandWithArgsDelegate`. Resolve the intended authoritative or client PIE world internally, then queue gameplay mutations on a native game-thread ticker (`FTSTicker::GetCoreTicker()`, `guard=0`). A missing world is a setup failure: fix dispatch once instead of spending another lifecycle on an unchanged editor-world invocation.

For the configured four-world listen topology, `EditorLevelLibrary.get_pie_worlds(False)[0]` identifies the authority world. PIE `World` wrappers do not expose `get_net_mode()` in this bridge; record that limitation once rather than probing repeatedly. This ordering rule is scoped to that topology.

For remote-owner fixtures, apply multiprocess PIE settings before launching the editor. Dispatch time-sensitive commands to the authority PIE world. The native editor console route targets the editor world in this topology. Cache the `EditorLevelLibrary.get_pie_worlds(False)`/`SystemLibrary.execute_console_command` fallback once per editor lifecycle and record both source and target world in preflight. Preserve `uiChannel: unavailable` when applicable.

## Gate separate-process readiness

In listen-server PIE, explicitly load the staging map through the editor bridge; this engine ignores `ServerMapNameOverride`. Require the host map marker in readiness evidence. After that map load and before starting PIE, create a byte baseline with `Scripts/Test/New-LogOffsetBaseline.ps1`. Pass its JSON as `-BaselineJson` to the first readiness checkpoint. An offset baseline cannot itself satisfy readiness.

Before a time-sensitive authority fixture, run `Scripts/Test/Wait-SeparateProcessReady.ps1 -LogFiles <client1>,<client2>,<client3>` once with the target experience. Apply these conditions:

| Condition | Required arguments or behavior |
| --- | --- |
| Staging-to-gameplay command | `-HostLog <server-log> -HostExperienceMarker <staging-experience>` |
| Starting world matters | Also add `-HostMapMarker <map-name>` |
| Fixture needs active gameplay | Add `-HostRequiredMarker 'BwayRoundManagement: Started playing GAS phase'`, or the exact gate marker |
| Independent child prerequisite emitted before the authority command | Use `-RequiredMarker` |
| Second checkpoint in the same PIE attempt | Use `-BaselineJson <staging-readiness.json>` to start at prior byte offsets |

Do not gate AutoLock-before-travel on `Local player locked hero`; that marker occurs downstream of the command. Host completion, map selection, and active gameplay checks prevent warmup or stale `ProjectB*.log` markers from satisfying the wrong transition.

Use the compact per-child/host `ready` or `timeout` result. After a timeout, preserve the captured logs and run `Scripts/Test/Get-SeparateProcessTravelBrief.ps1` once on the child and host logs. Use its bounded transition counts to choose a new readiness hypothesis. Read narrow raw ranges only when the counts identify a changed diagnosis. Do not hand-poll or relaunch unchanged.

For short-lived four-world fixtures, prefer one command sequence with explicit settling samples, such as 0.5 and 2.0 seconds. Preserve the transient sample and require the settled all-peer result before declaring convergence. Use one bounded log extraction for the decision. Follow [replication and combat](replication-combat.md) for specialized sample comparisons and completion markers.
