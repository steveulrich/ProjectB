# Packaged LAN acceptance

## Current candidate

- Project commit: `e558167a` (September 6, 2026).
- Engine: local UE 5.8.1 source tree, `E:/Github/UnrealEngine`, HEAD `71fe36aac5a8df5ccd66c763ffc902b29b6a9c43`. Local engine modifications must also be captured before accepting the artifact; HEAD alone does not establish reproducibility.
- Platform/configuration: Win64 Development, `LyraGame`, listen server and clients.
- Pipeline: AutomationTool BuildCookRun, build, cook by the book, stage, Pak.
- Stage: `Saved/StagedBuilds/LAN-20260906`; no release archive or deployment yet.
- Logs: `Saved/Logs/codex-lan-package-20260906-closed-editor.log` and the referenced UBT log.
- Existing cook roots: frontend, DevMap, Dorado, and the configured BreakawayCore content directory. No new broad cook inclusion was added.
- Status: standalone build passed; cook running; no package or runtime acceptance claimed.

The first cook reported a missing `Alona` asset domain. Engine `AssetReferencingDomains.cpp` derives plugin domains from `IPlugin::GetName`; the registered plugin is `Hero_Alona`. The project reference rule has been corrected to that identifier. The running cook loaded the old rule, so a fresh cook must verify this correction. BF-069 records prevention. Engine tracked changes and status were captured in `Saved/Logs/codex-package-engine-tracked.patch` and `codex-package-engine-status.txt`; untracked engine content is not included in that patch.

Command issued with the editor closed:

```powershell
& 'E:/Github/UnrealEngine/Engine/Build/BatchFiles/RunUAT.bat' `
  '-ScriptsForProject=E:/Unreal Projects/ProjectB/ProjectB.uproject' `
  BuildCookRun '-project=E:/Unreal Projects/ProjectB/ProjectB.uproject' `
  -target=LyraGame -platform=Win64 -clientconfig=Development `
  -build -cook -stage -pak -utf8output -unattended `
  '-stagingdirectory=E:/Unreal Projects/ProjectB/Saved/StagedBuilds/LAN-20260906'
```

## Acceptance evidence required

## Cook repair evidence (September 6)

The first cook exited with code 25 after the standalone build passed. Its terminal errors were the stale Alona domain, incompatible TopDownArena movement override, and failed relic redirector import. The domain correction is committed separately. A full asset-registry referencer scan established that `B_Bway_Ball_Interactable` had no referencers and that `BP_Bway_Relic` was referenced only by that obsolete redirector; both packages contained only redirectors leading to a deleted asset. Those two dead packages were removed. The live relic remains `/BreakawayCore/GameModes/BallMode/Relic/BP_BW_RelicActor`, referenced by game state, spawn data, and the grant pad.

The shared hero parent now requires BwayCharacterMovementComponent. TopDownArenaMovementComponent derives from that class and declares its BreakawayCore module/plugin dependency, preserving its existing speed override. The editor build passed (`Saved/Logs/codex-cook-asset-repair-build.log`). After restart, B_Hero_Arena compiled, its movement default object had the correct class, the live relic loaded, and the removed redirectors were absent from the registry (`Saved/Logs/codex-cook-assets-editor-verified.log`). BF-072 records prevention. A fresh cook is still required; these editor checks do not prove packaging success.

## Remaining acceptance checks

1. Record terminal build/cook/stage results and first causal errors. Preserve manifests, executable/Pak hashes, effective configuration, and matching symbols for the exact tested output.
2. Launch the staged executable with the editor closed. Confirm the frontend opens without loose project content or a cook server. A source-machine smoke test does not prove clean-machine compatibility.
3. Host LAN through the frontend, discover it from three separate client processes, join, select heroes and teams, and enter Dorado. Verify four humans plus four bots, four players per team, and consistent selection on every peer. Direct-IP joining is supplementary evidence.
4. Exercise movement, attacks, abilities, relic pickup/pass/throw/fumble/score, buildable placement/damage/persistence, base healing, and shop transactions from owning clients. Compare authority and client state; include insufficient funds, duplicate requests, and forbidden purchase windows.
5. Exercise deaths and repeated respawns, upgrade persistence, disconnects, and joining during allowed phases. Verify input, camera, HUD, team counts, and bot replacement after each transition.
6. Complete a best-of-five match through gameplay, covering goal, elimination, and time/territory round outcomes across the test set. Verify round summaries, final results, and gold earned independently of spending.
7. Return each peer to a usable frontend. Host/join a second match and verify fresh match gold, upgrades, stats, scores, buildables, and session state.
8. Repeat the gameplay and transition checks on DevMap. Run a clean second-PC LAN test with required runtimes and firewall configuration, and record startup/dependency failures separately from gameplay failures.

All items remain open until backed by logs and visible behavior from the packaged candidate. Native probes may supplement gameplay but do not prove physical input or complete frontend navigation. Two same-process PIE worlds do not satisfy these gates.
