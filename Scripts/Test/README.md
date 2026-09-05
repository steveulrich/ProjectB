# ProjectB Agentic Test Ladder

Fast, scriptable verification for Cursor agents and CI. Tiers are ordered by cost; run the lowest tier that matches your change.

| Tier | Script | When | Needs |
|------|--------|------|-------|
| **1** | `Tier1-CompileGate.ps1` | After any C++ edit | Engine source or install |
| **2** | `Tier2-StandaloneSmoke.ps1` *(planned)* | Match-flow / packaging regressions | Pre-built `ProjectB.exe` |
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

## Engine resolution

Set `UE_ENGINE_ROOT` if auto-detection fails:

```powershell
$env:UE_ENGINE_ROOT = 'E:\Github\UnrealEngine'   # parent of Engine/
.\Tier1-CompileGate.ps1
```

Otherwise the script reads `EngineAssociation` from `ProjectB.uproject` via the Epic Launcher registry (`HKCU\Software\Epic Games\Unreal Engine\Builds`).

## Target names

This Lyra fork uses **`LyraEditor`** / **`LyraGame`** UBT targets. The packaged game binary is **`ProjectB.exe`** (from `ProjectB.uproject`).

Full tier design, CQTest mapping to Core Loop steps, and CI layout:  
`Plugins/GameFeatures/BreakawayCore/Docs/Agent_Testing_Ladder.md`
