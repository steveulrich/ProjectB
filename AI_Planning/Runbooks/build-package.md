# Build, automate, and package

Use this runbook when the selected gate needs compilation, headless automation, or packaging. Command paths are relative to the project root. Consult [Agent_Testing_Ladder.md](../../Plugins/GameFeatures/BreakawayCore/Docs/Agent_Testing_Ladder.md) for applicable build, restart, and multiplayer requirements, and [Scripts/Test/README.md](../../Scripts/Test/README.md) for shared profiles and helper arguments. Apply only the tiers relevant to the change and selected gate; preserve every selected acceptance proof and final end-to-end requirement.

## Choose the cheapest applicable verification

Compile a coherent set of related C++ fixes, then run affected tests. Relevant headless proofs can include C++ math, attribute scaling, ability lifecycles, and asset audits. Relevant four-world PIE proofs can include replication, GameplayCues, and authority/proxy convergence. Complete required cheaper prerequisites before interactive or packaged checks. A targeted fix does not require unrelated full-system tiers, and headless or same-process evidence does not replace required physical UI or packaged LAN proof.

Build only these top-level Unreal Build Tool (UBT) targets:

| Selected phase | Target |
| --- | --- |
| Editor or PIE | `LyraEditor` |
| Packaged game | `LyraGame` |

Use `Scripts/Test/Tier1-CompileGate.ps1 -Target <LyraEditor|LyraGame> -OutputJson <path>`. Compile only the target needed for the phase. Do not use Visual Studio Build/Rebuild Solution, whole-solution MSBuild, all-target builds, or standalone engine test/utility targets. UBT may build dependencies required by the selected project target. Tier1 defaults to the direct bundled UBT runner, with a project-local UBA root and preflight write probe.

## Respect per-gate caps

These are hard limits for each gate:

| Operation | Maximum |
| --- | ---: |
| `LyraEditor` builds | 5 |
| `LyraGame` builds | 2 |
| `BuildCookRun` packages | 1 |

Record build/package counts and retry reasons with the gate evidence. At a cap, stop that operation and record the blocker and condition for further work. Do not silently reset counts, relabel the same gate, or relax the cap. Continue an independent authorized check when possible. The [two-attempt failure stop](gate-resume-evidence.md#poll-and-classify-failures) also applies, even before a cap is reached.

## Compile and verify the result

1. Check the owned process inventory before compiling. No live or hidden `UnrealEditor.exe` or `LyraGame.exe` process may hold the target binaries. Require `hiddenEditorCount == 0`, close owned lock holders cleanly, and reuse existing build handles. Follow [editor ownership and shutdown](editor-pie.md#own-the-editor-lifecycle) for forced shutdown or hidden-process findings.
2. Pass the absolute project path where possible. Tier1 canonicalizes relative inputs against the configured project root and records `projectFileInput` and `projectFileWasRelative`.
3. Preserve the `-OutputJson` result immediately after the build. It records source revision, runner, process, exit, elapsed time, and log metadata. A record with `status: running` is an owned in-progress gate: wait for its recorded process before reading compiler output. Tier1 publishes the final record atomically; a partial read must not trigger another compile.
4. Check both process exit and `failureSignals`. Bundled .NET/UBT can return exit 0 after `Result: Failed`, a C++ error, `Unhandled exception`, `UnauthorizedAccessException`, an access-denied write, or a generated-file rename failure. Treat these failure signals as authoritative, preserve the raw compile log, and fix the source or causal environment issue before retrying.
5. After a successful editor-target compile, confirm that the linked runtime DLL timestamp advanced before launching the editor.

## Run affected headless automation

Use `Scripts/Test/Run-AutomationTests.ps1 -TestFilter <prefix> -OutputPrefix <fresh-base>`. On this source engine, add `-ForceMemoryDDC` when the default derived data cache (DDC) has no writable node. The helper owns one editor PID and refuses stale report directories. Success requires report success, the framework exit marker, and process exit 0. Preserve the complete evidence and report only the selected scope.

## Prove package readiness

Package only after the applicable compile and affected-test prerequisites pass and packaged evidence is required. Reuse an existing candidate when its recorded source, patch, assets, configuration, and artifact identity cover the tested change; evidence extraction alone does not justify a recook.

For a `-skipbuild` package, first compile `LyraGame` and pass its JSON record to `Scripts/Test/Package-Preflight.ps1 -CompiledTargetRecord <compile-game.json>`. An editor-only build cannot establish current game binaries or newly added runtime classes. The compile record must contain an empty `failureSignals` field; regenerate older records that lack it. Never package a record with nonempty explicit UBT failure signatures.

When packaging under restricted permissions, add `-RequireAutomationToolLogWrite` to prove the engine-side AutomationTool log ACL before Cook/Stage/Pak. Preserve the source/candidate preflight and artifact identity defined in [gate provenance](gate-resume-evidence.md#establish-preflight-and-provenance). Use the [packaged transport and shop runbook](packaged-transport-shop.md) for launch topology, result handling, and probe scope.
