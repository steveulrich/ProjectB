# Integrate and regenerate Dorado

The reconstructed environment is a native **ProjectB** map at `/BreakawayCore/Environments/Dorado/Maps/L_Dorado`, validated in **Unreal Engine 5.8.1-0+UE5** with the existing BreakawayCore dependencies. The editable source package was exported with **Blender 5.2.3 LTS**. Final save-close-reopen, scoped character/ball checks, six navigation paths, seven inspected captures, and sampled performance are recorded in the [validation report](ValidationReport.md), together with visual differences and unrun scopes.

This environment task does not resume the gameplay queue. [SLICE_STATUS.md](../../../AI_Planning/SLICE_STATUS.md) remains paused. The protected original map `/BreakawayCore/Maps/L_BW_Dorado` remains the gameplay baseline.

## Deliverable paths

| Item | Path |
| --- | --- |
| Environment entry map | `/BreakawayCore/Environments/Dorado/Maps/L_Dorado` |
| Owned native assets | `/BreakawayCore/Environments/Dorado/` |
| Native content on disk | `Plugins/GameFeatures/BreakawayCore/Content/Environments/Dorado/` |
| Recovered modular kit and assembled source | `SourceArt/Dorado/Dorado_Recovered.blend` |
| Reconstructed ring energy source | `SourceArt/Dorado/Dorado_RingEnergy_Reconstructed.blend` |
| FBX/DAE interchange | `SourceArt/Dorado/Exports/FBX/`, `Exports/DAE/` |
| Recovered texture sources | `SourceArt/Dorado/Textures/` |
| Placement and material manifests | `geometry_manifest.json`, `energy_manifest.json`, `Textures/texture_manifest.json` |
| Adjustable dimensions and saved views | `SourceArt/Dorado/dimensions.json` |
| Import and assembly module | `SourceArt/Dorado/Scripts/import_dorado.py` |
| Committed review evidence | [Evidence/README.md](Evidence/README.md) |
| Current full run output | `Saved/Logs/dorado-reconstruction-20261030/` (ignored local reports) |
| Reference and supplementary gallery | [ComparisonGallery.html](ComparisonGallery.html) |

The final native inventory contains **557 saved assets**: 103 Static Meshes, 133 Material Instances, seven master Materials, 313 Texture2Ds, and one map. The mesh count comprises 98 recovered meshes, four reconstructed energy modules, and one calibration mesh. [final-delivery-inventory.json](Evidence/final-delivery-inventory.json) records the counts and source/dependency audit.

## Open in ProjectB

1. Use the existing project editor and the [editor lifecycle procedure](../../../AI_Planning/Runbooks/editor-pie.md). Resolve unsaved-map conflicts before loading another level; do not launch a duplicate editor.
2. Open `/BreakawayCore/Environments/Dorado/Maps/L_Dorado`.
3. Inspect `CAM_Ref_01`, `CAM_Ref_02`, and `CAM_Ref_03`, then the supplementary top-down and player-height views recorded in `dimensions.json`.
4. Verify the gameplay actors and map World Settings against the protected baseline. The map copy retained the existing Experience, game mode, spawns, goals, base zones, and navigation volume. Recorded class/location comparisons cover 18 protected actors; they are not a complete gameplay property audit.
5. Use the actual project character and Relic for integration checks. The observed runtime capsule is radius 35 cm, half-height 96 cm, with 50 cm maximum step height. These runtime values take precedence over the base C++ constructor dimensions.

## Initialize a fresh checkout

Fetch Git LFS content before opening the map. Native assets and prepared PNG, DAE, and FBX authoring inputs are retained; recovered DDS cache fragments, duplicate recovery DAEs, Python caches, Blender backups, and pilot/calibration modeling experiments are omitted. These omissions do not remove required inputs from the staged native-import package.

Before using incremental assembly or validation scripts, run from the ProjectB root:

```powershell
python 'SourceArt/Dorado/Scripts/bootstrap_checkout.py' --dry-run
python 'SourceArt/Dorado/Scripts/bootstrap_checkout.py'
```

The helper verifies ten allowlisted [starting snapshots](../Config/Provenance/README.md) and copies only missing source observations, protected-map evidence, and native import/assembly registries into the normal ignored run directory. Existing reports are preserved, including newer results. These archived inputs describe the delivered starting state; copying them performs no new validation. No PIE user-settings baseline is distributed. Capture the current session's own preferences before running its traversal fixture.

The gallery and [selected evidence](Evidence/README.md) work directly from the checkout without bootstrapping. Full raw logs/traces and intermediate captures stay local. The bootstrap does not rebase absolute authoring paths; complete the relocation instructions below before rebuilding in another workspace.

## Regenerate editable sources

Follow the [generation guide](GenerationGuide.md) for archive recovery, original `Nodraw`/`Helper` exclusion, source-normal preservation, FBX options, and dimension controls. The original archive under `F:/BreakawayConverted/Unpacked` is required to repeat source extraction. Existing owned DAE/PNG inputs support regeneration without extracting the archive again, after updating any relocated absolute paths in the manifests.

From the ProjectB root, the implemented source entry points are:

```powershell
python 'SourceArt/Dorado/Scripts/inspect_material_sources.py'
python 'SourceArt/Dorado/Scripts/audit_source_scope.py'
python 'SourceArt/Dorado/Scripts/prepare_geometry.py' --stage --convert
python 'SourceArt/Dorado/Scripts/prepare_textures.py'
```

Run Blender in a separate background process using `prepare_geometry.py -- --blender`, as documented in the generation guide. The source conversion tool is CryEngine Converter 1.6.0.0. Inspect manifest failures and hashes after export; process success alone is insufficient.

For the four separate energy meshes, run `analyze_ring_gaps.py` with system Python and `build_ring_energy.py` with Blender background mode. For the small source-backed violet cores, run `prepare_glow_points.py`; it produces `Config/inferred_glow_points.json` and reads the working `source-effects-scope.json`, generated by the source audit or seeded by the checkout bootstrap. Native rebuilding uses the already generated glow configuration. See [energy and glow reconstruction](EnergyReconstruction.md) for which parts are reconstructed rather than original meshes.

`Scripts/audit_package_paths.py` checks staged native-rebuild inputs without opening an editor. The final October 30 audit checked 888 required paths, including 495 prepared texture-map paths and 30 art-override texture paths. All existed inside the source package. All 98 recorded source-FBX hashes matched; the four energy FBXs and calibration FBX were fingerprinted but had no pre-existing manifest hash to compare. See [final-delivery-inventory.json](Evidence/final-delivery-inventory.json). This is file/provenance evidence, not an executed full rebuild or relocation test.

## Regenerate native assets

Run this stage inside the owned UE editor, with PIE stopped. The script's direct `__main__` entry only runs `preflight()`; it does not perform a full import automatically.

Initialize the module in the Unreal Python console or the established editor Python bridge:

```python
import sys
import importlib
from pathlib import Path
import unreal
script_dir = str(Path(unreal.Paths.project_dir()) / 'SourceArt/Dorado/Scripts')
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)
import import_dorado as dorado
dorado = importlib.reload(dorado)
plan = dorado.preflight()
```

Reload the module after editing its Python source. The importer also reloads `dorado_dimensions` on module load; assembly reads and validates the current `dimensions.json` on each invocation. Inspect the returned engine, owned paths, and dirty packages before proceeding.

`dorado.rebuild_environment()` is the implemented full native rebuild from exported inputs. It builds materials, imports both mesh manifests and calibration, assembles both scenes, creates glows/presentation/cameras, applies backdrop settings, configures the seven Nanite meshes, and runs structural validation. It is intended for a monitored editor job with enough lifetime for synchronous mesh builds. Do not dispatch the entire rebuild through a short-lived bridge call. The equivalent staged functions are:

1. `dorado.build_materials()` imports prepared textures and builds native master Materials and instances. It supports `start`, `limit`, `only_ids`, and `rebuild_masters` for scoped reruns. Save the owned Dorado map first: the function refuses dirty-map conflicts and unloads a clean Dorado scene to a blank map before material edits to avoid live shader-cache invalidation. Later assembly reloads the saved map.
2. `dorado.import_geometry()` imports the recovered mesh manifest. Then call `dorado.import_geometry(manifest_name='energy_manifest.json')` and `dorado.import_calibration()`. Use `force_ids` only for changed meshes that require reimport.
3. `dorado.prepare_map()` loads the owned map or creates it from the protected baseline. It refuses unsaved-map conflicts. Placeholder removal applies to the new copy.
4. `dorado.assemble_geometry()` and `dorado.assemble_geometry(manifest_name='energy_manifest.json')` place assets using source transforms and the dimension adapter.
5. `dorado.setup_presentation()`, `dorado.setup_cameras()`, and `dorado.setup_glow_points()` create the shared presentation, named cameras, and 14 small violet cores.
6. Apply the recorded backdrop scope with `dorado.apply_backdrop_budget(shadow=True)`. Configure the seven selected Nanite backdrops individually with `dorado.configure_nanite(index)` for indices 0 through 6, waiting for each owned editor operation to finish.
7. Save and run `dorado.structural_validate()`. Review the report's errors and warnings; a saved map is not equivalent to a passed validation report.

For an already imported scene, `finish_visual_iteration()` reapplies material art overrides, both manifests, glow points, backdrop flags, presentation, and cameras. It does not rebuild the entire source archive or perform final runtime validation.

Material reruns clear existing instance parameters before restoring source defaults and current art overrides. Removing an override causes that previously overridden material to be rebuilt during the next scoped override pass. Scene assembly clears old component material overrides, then binds source overrides and variants by their stable hashed `DOR_` slot names. It does not assume numeric slot positions survive mesh reimport. A missing expected native slot raises an assembly failure. Stable actor labels plus the `DoradoReconstruction` ownership tag support repeatable assembly; the final native structural audit found no duplicate-owned-label error.

The current importer builds seven master families: Surface, Foliage, Water, Energy, Cloud, MaskedLogo, and Glow. MaskedLogo preserves the original floor-logo diffuse alpha and AlphaTest=0.98 instead of drawing an opaque rectangular plane. Glow is opaque and unlit for the Power glyphs; per-instance variants make the positive-X pad cyan and negative-X pad green. Water pans U at +0.12 and V at 0 because recovered U follows the vertical stream and V crosses its width. Six waterfall overrides use emission 2500 and opacity 0.65. Use [presentation.json](Evidence/presentation.json) and the [visual iteration record](VisualIteration.md) for current shared lighting and palette settings, not earlier suggestions. Native colors and rotations use explicit constructor keywords; positional `Color` construction had incorrectly interpreted RGB in native BGRA order.

The coordinate transform is `50 * [sourceX-centerX, -(sourceY-centerY), sourceZ-centerZ]` cm, with source center `[513.97998, 666.29779, 73.674896]` meters. The 0.5 integration scale is already included. Do not apply it twice. Camera rotations use explicit `pitch`, `yaw`, and `roll` keyword arguments.

## Collision and runtime verification

The planar `main_floor_tc` and `main_floor_center_tc` meshes are visible source-authored floor fills. They must retain collision on all four instances. Their near-zero thickness does not make them decorative. A broad decoration exclusion caused ball-sized holes around the center bands; the remedy restores the original floor mesh collision instead of adding hidden patch boxes. A repaired-center character pass and an actual Relic settling on the former hole are now recorded in the validation report.

The same rule applies to the four `center_point` tiles at the stair feet and two `groud_design` floor strips. They contain visible walking fill and must retain their original triangle collision. The second stair-ball run fell through a disabled stair-foot tile before this correction. The third run recorded 234 actual-ball physics samples and a five-second stationary tail supported by the restored surface. [stair-foot-collision-remedy.json](Evidence/stair-foot-collision-remedy.json) records the exact source instances and measurements; [relic-repaired-stairs-analysis.json](Evidence/relic-repaired-stairs-analysis.json) and [stairs-repaired-rest-support.json](Evidence/stairs-repaired-rest-support.json) record the successful scoped retest. Keep backed floor-logo planes and elevated banners noncolliding; a name containing ornament terminology is not enough to decide floor collision.

Four `rel_eldorado_mid_stair_c_group` components require an increased walkable-slope override of 47 degrees for their recovered 45.55-degree beveled steps. The actual character's approximately 44.765-degree walkable-floor setting and 50 cm step height remain unchanged. The importer applies this override only to those four components. It does not add a smooth ramp, change the source triangles, or alter the ball's collision surface. The repaired east-stair character run passed after this adjustment; the final fresh reopen confirmed all four saved overrides.

Decorative energy, glow cores, clouds, and background foliage do not need traversal collision. Preserve actual stair profiles, visible cover blocking, and exposed edges. Do not infer ball safety from a capsule crossing a seam or from sparse vertical traces.

`validate_traversal.py` exposes `start_walk`, `start_ball_throw`, `start_blocking`, `start_edge_fall`, `status`, and `cancel`. It drives existing native Enhanced Input in one disposable local PIE world and observes the actual project pawn and Relic. Use `configure_traversal_pie()` only for that owned session and call `restore_traversal_pie()` afterward. These checks do not establish physical input, remote replication, or four-player LAN behavior.

The in-memory PIE restore did not durably restore `AdditionalServerGameOptions` after restarting the editor; native Python does not expose the required `SaveConfig` operation here. With the owned ProjectB editor closed, run the task-specific [cleanup helper](../Scripts/Restore-DoradoPIESettings.ps1) from the project root:

```powershell
& '.\SourceArt\Dorado\Scripts\Restore-DoradoPIESettings.ps1' -ProjectRoot 'E:\Unreal Projects\ProjectB'
```

The helper reads this session's `Saved/Logs/dorado-reconstruction-20261030/pie-settings-baseline.json`, checks that no owned editor process is running, backs up the generated user INI when needed, and restores only `PlayNumberOfClients`, `RunUnderOneProcess`, `AdditionalServerGameOptions`, and `bLaunchSeparateServer`. It requires each key to be unambiguous and rejects unrecognized additional-server options. It preserves other INI text and verifies the write. The recorded run changed only `AdditionalServerGameOptions`, restoring `?NumBots=4?Experience=B_BW_Experience_Dev`; see [pie-settings-disk-restored.json](Evidence/pie-settings-disk-restored.json). Use the baseline captured for the actual test session, not a copied user's preferences. Verify the native settings after reopening. [pie-settings-reopened-final.json](Evidence/pie-settings-reopened-final.json) confirms all four original values in fresh PID 42844; the final editor was left open on Dorado with clean assets.

## Captures, reopening, and dependencies

Use `dorado.capture('CAM_Ref_01', iteration='final')` and the corresponding named cameras after final changes. REF01 is 2048 by 918; REF02 and REF03 are 1920 by 862. Capture requests are asynchronous: verify the PNG exists and inspect it before recording a pass. The helper uses a False/True realtime-override pair, matching the installed subsystem's add/remove behavior. The final full close/reopen from PID 49472 to fresh PID 58364 preserved the exact saved map hash, seven cameras, four stair overrides, seven Nanite backdrops, materials, and lighting. The fresh structural report has no errors and six complete paths. Earlier capture and material-edit failures and their remedies are recorded in [VisualIteration.md](VisualIteration.md).

Keep the level viewport active with realtime rendering enabled when previewing the animated sky/effects or requesting a capture. A floating Content Browser left the viewport inactive after reopening and stalled one capture; closing that child window allowed the PNG to complete. Let temporal rendering settle before judging the preview: the first reopened REF01 frame has visible noise. The inspected final gallery remains unchanged.

The final three reference PNGs and four supplementary views are presented in the [comparison gallery](ComparisonGallery.html). Keep the gallery, reference images, and evidence render folder at their project-relative locations. `CAM_TopDown` is a perspective plan view at Z6000 cm/FOV82°, not a metric orthographic drawing. `CAM_Player_ShrineEnd` uses its measured local floor plus 165 cm eye height; the raised-floor correction is recorded in [shrine-camera-floor-adjustment.json](Evidence/shrine-camera-floor-adjustment.json). Final stills retain visible atmosphere/material differences documented in the visual review.

The native map depends on existing ProjectB gameplay assets outside the owned environment root. The dependency audit also reaches the missing `/Game/System/FrontEnd/Maps/L_LyraFrontEnd_BuiltData` through the inherited game mode/game state/frontend chain. Git LFS payload hashes prove this reference exists in unchanged baseline content; see `inherited-frontend-builddata.json`. The environment task retains that external reference rather than changing gameplay/frontend assets.

This delivery targets ProjectB. No standalone clean-project migration or cook has been demonstrated. For another already compatible ProjectB/Lyra workspace, select the owned map in Unreal's Content Browser and use **Asset Actions > Migrate**. Review the complete dependency closure in the native Asset Report, including `/BreakawayCore/` and `/Game/` content; preserve the required Game Feature/plugin mounts and engine/plugin versions in the destination. Complete the migration through the native destination dialog, then open the map there and verify references. Copying only the owned environment folder does not include its gameplay dependencies. The inherited missing frontend BuiltData package must remain explicitly accounted for.

The source manifests are workstation-specific: their current project prefix is `E:/Unreal Projects/ProjectB` (also serialized with Windows backslashes), and archive extraction uses `F:/BreakawayConverted/Unpacked`. No relocation helper has been validated. To relocate authoring inputs, copy `SourceArt/Dorado` to the matching directory in the destination project, work on the copied JSON files, and rebase local input/output path fields that point into the old project prefix. Include geometry-stage/geometry/energy/calibration manifests, texture-map paths, and art-override texture paths. Preserve source archive provenance paths and recorded hashes; source re-extraction still requires the archive or a separately updated archive-root setting. Also rebase any local input/output paths in the bootstrapped working copies under Saved/Logs, while retaining the immutable Config/Provenance snapshots and their recorded hashes. Run `audit_package_paths.py` from the copied package and require no missing paths or paths outside that package before native import. This path audit does not replace an executed destination rebuild and reopen test.
