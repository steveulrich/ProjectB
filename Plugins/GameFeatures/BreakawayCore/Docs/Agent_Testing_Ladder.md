# Agentic Testing Ladder — ProjectB / BreakawayCore

Automated verification for Cursor agents, mapped to [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md). Tiers increase in fidelity and runtime cost.

```
Tier 1  Compile gate          ~2–15 min   agents run after C++ changes
Tier 2  Standalone log smoke   ~30–90 s    packaging / experience regressions
Tier 3  CQTest (editor)        ~2–5 min    per core-loop sub-step (11-1, …)
Tier 4  CI                     scheduled   PR compile + nightly smoke + step tests
```

Scripts live in `Scripts/Test/`. Config: `Scripts/Test/BwayTestConfig.psd1`.

---

## Tier 1 — Compile gate ✅ (implemented)

**Goal:** Remove “did it build?” friction. No human, no editor.

| Item | Value |
|------|-------|
| Script | `Scripts/Test/Tier1-CompileGate.ps1` |
| UBT target | **`LyraEditor`** Win64 Development |
| Project | `ProjectB.uproject` |
| Pass | `Build.bat` exit code **0** |

```powershell
cd "E:\Unreal Projects\ProjectB\Scripts\Test"
.\Run-Tier.ps1 -Tier 1 -ValidateOnly   # path check only
.\Run-Tier.ps1 -Tier 1                  # full compile
```

**Agent rule (updated September 5, 2026):** Agents may compile, fix compiler errors, and restart the project editor as part of authorized development. Run Tier 1 after C++ changes and inspect its exit code and logs. Fix relevant errors and rerun until the build succeeds or a concrete external blocker remains. A successful compile does not replace the relevant PIE or multiplayer checks.

Before building, inspect running editor/build processes. Preserve unsaved work, stop PIE, and close this project's editor gracefully when a full rebuild/restart is needed, especially for reflected header/class changes. Do not force-kill editors or discard unsaved changes. Wait for an existing build instead of starting duplicates. Use asynchronous process handles and report meaningful progress during long builds.

After a successful build, launch the resolved engine's `Engine/Binaries/Win64/UnrealEditor.exe` with the absolute ProjectB.uproject path and verify editor/bridge readiness before testing. The MCP launcher may not discover a source-built engine; use the engine resolver and launch the verified executable directly when needed. Respect sandbox escalation requirements. Ask the user only for missing decisions, unsaved-work conflicts, or permissions the available tools cannot satisfy.

**Unreal permission prompts (user authorization, September 6, 2026):** Agents may handle security permission requests directly related to this project's verified Unreal engine and build executables, including Windows Firewall prompts needed for LAN testing. Verify the requesting executable and use the normal permission dialog with the narrowest offered network scope sufficient for the test. This authorization persists across candidate builds. It does not authorize disabling security software, adding broad exclusions, changing unrelated permissions, or handling credentials. The installed computer-use guidance was updated with this exception; this project note preserves the user's authorization if a plugin update replaces its cached documentation.

**Notes:**
- Target is `LyraEditor`, not `ProjectBEditor` — Lyra keeps native target names.
- Set `UE_ENGINE_ROOT` when registry lookup fails.
- Logs: `%TEMP%\bway_tier1_compile_<timestamp>.log`

---

## Tier 2 — Standalone log smoke (planned)

**Goal:** Confirm `B_BW_Experience_Dev` loads in a **packaged** build, match-flow config resolves, and Lyra default experience is **not** used.

| Item | Value |
|------|-------|
| Script | `Scripts/Test/Tier2-StandaloneSmoke.ps1` *(stub — implement when packaged builds are routine)* |
| Binary | `Binaries\Win64\ProjectB.exe` (or staged package output) |
| Map URL | `L_BW_DevMap?Experience=B_BW_Experience_Dev&SkipHeroSelection=1&NumBots=3&PointsToWin=1` |
| Pass | Required log lines present; forbidden lines absent; clean exit |

**Required log patterns:**
- `B_BW_Experience_Dev`
- `BwayMatchFlow: Resolved`

**Forbidden:**
- `B_LyraDefaultExperience` (fallback experience — indicates asset-manager / registration failure)

**Prerequisites:**
1. Game target built: `LyraGame` Win64 Development (or shipping package).
2. `/BreakawayCore` content cooked (`MapsToCook`, `DirectoriesToAlwaysCook` in `DefaultGame.ini`).

**Implementation sketch:**

```powershell
$exe = "E:\Unreal Projects\ProjectB\Binaries\Win64\ProjectB.exe"
$log = "$env:TEMP\bway_smoke.log"
& $exe "$SmokeMapUrl" -log -stdout -FullStdOutLogOutput -ExecCmds="quit" 2>&1 | Tee-Object $log
# Assert required/forbidden patterns via BwayTestConfig.psd1
```

**When agents run Tier 2:** After match-flow, experience registration, `DefaultGame.ini` asset-scan, or `.uproject` plugin changes that affect standalone startup.

---

## Tier 3 — CQTest functional tests (planned)

**Goal:** Editor automation for core-loop checklists without human PIE — the sweet spot for step **11-1** and beyond.

Mirror [ShooterTests](../../../ShooterTests/README.md): new Game Feature plugin **`BreakawayCoreTests`**.

### Module layout

```
Plugins/GameFeatures/BreakawayCoreTests/
  BreakawayCoreTests.uplugin
  Source/BreakawayCoreTestsRuntime/
    BreakawayCoreTestsRuntime.Build.cs   # CQTest, BreakawayCoreRuntime, LyraGame
    Private/
      BwayCoreLoopTestBase.h/.cpp        # map spawner + experience wait helpers
      BwayCoreLoop_11_1_MatchFlowRules.cpp
      BwayCoreLoop_11_2_HeroSkip.cpp     # later
      ...
```

### Test naming (maps to plan checklists)

| Automation filter | Core-loop step | Assert |
|-------------------|----------------|--------|
| `Breakaway.CoreLoop.11-1` | 11-1 Config + rules | Experience == `B_BW_Experience_Dev`; log `BwayMatchFlow: Resolved`; `PointsToWin == 1`; bot count == 3 |
| `Breakaway.CoreLoop.11-2` | 11-2 Hero skip | No duplicate Warmup; Step 10 bot score still works |
| `Breakaway.CoreLoop.11-3` | 11-3 Prematch | `EBwayMatchPhase::Prematch`; no pawns |
| … | 11-4 → 11-8 | Per sub-step table in Core Loop plan |

### 11-1 reference test (first to implement)

```cpp
TEST_CLASS_WITH_FLAGS(BwayCoreLoop_11_1_MatchFlowRules, "Project.Functional Tests.Breakaway.CoreLoop",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
{
    // FMapTestSpawner loads L_BW_DevMap with URL options from BwayTestConfig
    // StartWhen: experience loaded == B_BW_Experience_Dev (LyraExperienceManagerComponent)
    // Until: log contains "BwayMatchFlow: Resolved"
    // Then: UBwayRoundManagementComponent::PointsToWin == 1
    // Then: UBwayBotCreationComponent::GetTargetBotCount() == 3
};
```

**Run command:**

```powershell
UnrealEditor-Cmd.exe "E:\Unreal Projects\ProjectB\ProjectB.uproject" `
  -ExecCmds="Automation RunTests Breakaway.CoreLoop.11-1; Quit" `
  -unattended -nopause -log
```

Wrapped by `Scripts/Test/Tier3-CQTest.ps1` with pass/fail parsing.

**World settings prerequisite:** `L_BW_DevMap` may need `Force Standalone Net Mode` disabled for future listen-server tests (see ShooterTests README). For 11-1, standalone map load is sufficient.

**Agent rule:** Tier 3 is optional unless the user requests it. When run, use the filter matching the core-loop sub-step you touched (e.g. `11-1` only).

---

## Tier 4 — CI (later)

| Trigger | Tier | Branch / agent |
|---------|------|----------------|
| PR → `project-b` | 1 | Compile `LyraEditor` Win64 Development |
| Nightly | 2 | Package Win64 + standalone smoke |
| Per merge / label | 3 | `Automation RunTests Breakaway.CoreLoop.<step>` |

### Suggested GitHub Actions layout

```yaml
# .github/workflows/tier1-compile.yml
# - checkout, cache DerivedDataCache
# - UE_ENGINE_ROOT from secret or self-hosted runner
# - Scripts/Test/Run-Tier.ps1 -Tier 1

# .github/workflows/tier2-nightly-smoke.yml  (self-hosted or UE-capable runner)
# - Build LyraGame + cook /BreakawayCore
# - Scripts/Test/Run-Tier.ps1 -Tier 2

# .github/workflows/tier3-coreloop.yml  (optional per-step matrix)
# - Run-Tier.ps1 -Tier 3 -Filter Breakaway.CoreLoop.11-1
```

Self-hosted Windows runners are typical for UE CI (long compile, large disk).

---

## Decision matrix — which tier when?

| Change type | Suggested verification |
|-------------|----------------------|
| C++ only (BreakawayCoreRuntime, LyraGame) | Agent compile gate + editor/PIE smoke |
| `.ini` asset manager / cook paths | Recompile + **2** when package exists |
| Experience / match-flow assets | PIE + optional **3** (`11-1` filter) |
| Core-loop sub-step claim (“11-2 passes”) | PIE checklist + optional **3** (that step’s filter) |
| Pre-release / standalone validation | **2** + relevant **3** |

---

## Implementation order

1. ✅ **Tier 1** — `Tier1-CompileGate.ps1` (done)
2. **Tier 3 / 11-1** — `BreakawayCoreTests` plugin + first CQTest (highest agent value before packaging)
3. **Tier 2** — smoke script once `ProjectB.exe` is built routinely
4. **Tier 4** — PR compile workflow, then nightly smoke

---

## Related docs

- [CoreLoop_Implementation_Plan.md](./CoreLoop_Implementation_Plan.md) — step checklists and PIE URLs
- [ShooterTests README](../../../ShooterTests/README.md) — CQTest patterns
- `Scripts/Test/README.md` — script entry points
- `.cursor/rules/agent-testing.mdc` — agent instructions
