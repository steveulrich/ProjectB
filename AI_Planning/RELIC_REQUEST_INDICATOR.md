# Relic request indicator: material fix and remaining visibility gate

September 8, 2026. The renderer material fix passed. Request activation and automatic marker visibility have **not** passed together. Candidate `LAN-20260908-05` contains neither this fix nor the newer connected-frontend host fix.

## Renderer defect and change

`ULyraTeamDisplayAsset::ApplyToActor` visits mesh components, including `UWidgetComponent`. The old generic mesh path created an MID and installed it through the widget's virtual `SetMaterial`. `UWidgetComponent` then created its own renderer MID from that override, producing an invalid MID parent and disrupting the widget's render texture. The old warning is recorded in `Saved/Logs/codex-frontend-travel-client-20260908.log`.

`Source/LyraGame/Teams/LyraTeamDisplayAsset.cpp` now recognizes widget components, calls `UpdateWidget`, and applies team parameters to their existing renderer MID through `ApplyToMaterial`. Normal mesh behavior stays in the existing path. No assets, RPCs, input mappings, or material content changed.

Keep world-space widgets' base material slots as a Material or Material Instance Constant. Apply runtime scalar, color, and texture parameters through the renderer MID. Verify the `SlateUI` texture and rendered output in addition to checking warning counts. The placeholder widget currently uses `Widget3DPassThrough`, a soft-glow control-ring material, and the hand icon from ControlRigModules. The plugin descriptor supports content and has no editor-only module restriction; the icon's final cooked/drop-in-art acceptance remains open.

## Verification

One `LyraEditor` build passed in 50.08 seconds. A fresh editor ran three same-process PIE peers, three humans and zero bots, on DevMap. The long round was an explicit test override, not a gameplay tuning change.

| Check | Result |
|---|---|
| Initial replicated widgets | Nine widgets; every base slot and renderer parent was a Material; every renderer MID existed |
| Parameter fixture | Five style applications per widget; scalar, color, and texture values reached the existing MID on all nine |
| Normal team callback | Three calls per pawn; renderer MID and base slot stayed stable on all nine |
| Render texture | With fixture visibility enabled, all nine `SlateUI` parameters matched their actual non-null render targets |
| Visual output | Hand markers rendered above the remote characters in the observed game view |
| Cleanup | Original material parameters restored; all nine widgets hidden; observer unregistered; no dirty content assets |
| Invalid dynamic-parent warning | Zero in the fixed runtime log |
| Critical failures | Zero fatal/assertion/ensure/Accessed None lines |

This is a renderer and team-style fixture pass. Forced visibility does not prove request gameplay, team filtering, duration, respawn cleanup, packaged operation, or second-PC LAN behavior.

## Remaining request visibility failure

Physical RMB activation produced successful predicted/server request log pairs, including 18:41:30.965 and 18:41:31.031 UTC. An extended observer covered 18:41:07–18:45:56 UTC: 1,770 samples, nine widgets per sample, zero observer errors, and **zero visible markers**. This was before the visibility fixture. The first shorter observer ended too early to establish the request result; retain its evidence without calling it a pass.

The existing base-character Blueprint has several concrete lifecycle problems to address:

- `Event Possessed` → authority branch → experience-ready wait → `WaitGameplayTagAddToActor(State.RequestingRelic)` → visible=true. Remote proxies do not run this possession path.
- No tag-removal listener clears visibility after the request ends.
- A separate `HandleRelicRequestVisibilityChange` event is not connected to the above listener. Its team comparison reads the same pawn's PlayerState on both sides, so it does not establish viewer-team eligibility.

These findings do not yet explain every authority-side observation. Inspect the actual request effect CDO's granted tag/duration and the pawn ASC initialization order before replacing the binding. `ALyraCharacter::PossessedBy` calls the parent before the pawn-extension controller-change handler, so experience readiness alone is not evidence of ASC readiness. The cue Blueprint has empty application/removal event graphs.

**Next gate:** on a real remote request, an eligible teammate sees the marker; ineligible viewers do not; it disappears when the tag is removed and after pawn replacement. Bind only after the current pawn's ASC is ready, initialize from the current tag state, and release old listeners. Define the exact viewer rule from the intended gameplay/reference before claiming team-filter acceptance. Verify physical input and normal replication without Python-issued client RPCs.

## Evidence and attempt accounting

- Detailed JSON, baseline asset exports and source patch: `Saved/Logs/request-widget-20260908/`.
- Compact verdict: `verification-brief.json`; material fixture: `team-style-fixture.json`; rendered textures: `renderer-fixture.json`; restored visibility: `fixture-restored.json`.
- Request observations: `physical-request-observation.json` (short/incomplete) and `physical-request-observation-extended.json` (failure reproduced).
- Reusable local diagnostic functions: `Saved/verify-request-widget-20260908.py`. They retain no PIE object wrappers in globals and do not drive gameplay RPCs. Do not rerun configuration over the existing baseline file.
- Build: `Saved/Logs/codex-request-widget-build-20260908.log`.
- Read-only audit editor: `Saved/Logs/codex-request-widget-editor-20260908.log`, PID 51292, actual exit 0.
- Fixed editor: `Saved/Logs/codex-request-widget-fixed-editor-20260908.log`, PID 38208, actual exit 0; monitor 83264 complete. Both PIDs were absent after shutdown.

Overhead: one build, two editor launches (audit and fixed), one three-peer PIE run, no new package. One material probe initially omitted the Blueprint team callback's required arguments; it restored its temporary material parameters in `finally` and was corrected. Its four Python traceback lines remain in the log. Setting transient style properties with normal editor notifications also emitted 12 null-world warnings; the corrected fixture uses `PropertyAccessChangeNotifyMode.NEVER`. Two unrelated PixelStreaming2 startup error lines remain. No invalid-material-parent warning appeared in either fixture attempt.

The reflected preference restore did not persist client count or extra server options. After normal editor exit, those exact INI fields were restored to four clients and empty options; listen-server mode, one-process mode, and full-flow=false were retained. Preserve the 37 unrelated dirty workspace entries. BF-116 records prevention in the local bug-fixer catalog. Repackage only after coherent affected fixes are ready; keep the final four-player, packaged-host and second-PC gates open.
