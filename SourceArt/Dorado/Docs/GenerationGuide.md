# Regenerate the Dorado source package

The source pipeline recovers the supplied Dorado archive into editable geometry and native-import inputs. Unreal validation remains a separate stage.

The executed tools are Blender **5.2.3 LTS**, build `e13b728c6880`, and CryEngine Converter **1.6.0.0**. The target project uses Unreal Engine **5.8.1**.

## Files and coordinates

| Output | Purpose |
| --- | --- |
| `Dorado_Recovered.blend` | Editable modular kit and assembled scene with recovered PBR material previews. |
| `geometry_manifest.json` | Stable mesh IDs, material slots, instance transforms, source hashes, and export status. |
| `Config/geometry_stage.json` | Full source placement and material records used by Blender assembly. |
| `Sources/CGF/`, `Sources/MTL/`, `Sources/Layers/` | Copied original geometry, materials, and layer placement XML. |
| `Exports/DAE/`, `Exports/FBX/` | Converted source meshes and Unreal import meshes. |
| `Textures/texture_manifest.json` | Texture sources, conversion notes, material IDs, and prepared PNG paths. |
| `dimensions.json` | Source-to-project scale, dimensions, anchors, and comparison cameras. |

Source placements use meters around `[513.97998, 666.29779, 73.674896]`. Exported mesh vertices use centimeters. ProjectB applies uniform integration scale `0.5` and preserves its existing Y-axis court orientation:

```text
UE position = 50 * [sourceX - centerX, -(sourceY - centerY), sourceZ - centerZ]
UE scale = sourceScale * 0.5
UE quaternion XYZW = [-sourceQX, sourceQY, -sourceQZ, sourceQW]
```

Apply the mesh import transform and scene placement once each. The asymmetric calibration mesh has source dimensions `[100, 200, 300]` cm and an offset center. Its expected Unreal bounds are recorded in `Config/calibration.json`.

## Rebuild from the archived conversion

From the ProjectB root, run Blender in a separate background process:

```powershell
& 'F:\Blender Foundation\Blender 5.2\blender.exe' -b --factory-startup --python 'E:\Unreal Projects\ProjectB\SourceArt\Dorado\Scripts\prepare_geometry.py' -- --blender
```

This command reads the staged configuration and DAE files, exports the FBXs, saves the assembled `.blend`, and binds the adjacent recovered PNGs. It does not modify the open Blender scene or Unreal assets.

The current stage and texture manifests retain absolute paths for this workstation. If you relocate the package, update those input and output paths before regenerating. The saved Blender image paths are relative to `SourceArt/Dorado/`.

Inspect `geometry_manifest.json` after the command. A successful process exit alone is insufficient: `failures` must be empty, and each required asset must have `exported: true`.

## Recover again from the original archive

This step requires the original archive at `F:/BreakawayConverted/Unpacked` and the installed converter. It copies source data into the owned package before conversion.

1. Update the original material and placement inventory when source files change:

   ```powershell
   python 'SourceArt\Dorado\Scripts\inspect_material_sources.py'
   python 'SourceArt\Dorado\Scripts\audit_source_scope.py'
   ```

2. Stage and convert the scoped geometry:

   ```powershell
   python 'SourceArt\Dorado\Scripts\prepare_geometry.py' --stage --convert
   ```

3. Run the Blender assembly command from the preceding procedure.

4. Review source, conversion, and export failures before importing into Unreal.

Conversion signatures include the owned CGF and derived material hashes. A changed signature invalidates cached DAE reuse. The converter operates on copied files. Its temporary material copies use a local placeholder texture to avoid broad archive searches; original material XML remains in `Sources/MTL/`.

The installed converter does not accept its advertised `-outputfile` option. The pipeline converts beside each owned CGF and copies the generated DAE to `Exports/DAE/`.

## Filter non-rendering geometry

Pipeline version **2** checks the original material's `Shader` field before exporting beauty geometry. Materials with `Nodraw` or `Helper` shaders are excluded even when their names contain only `Col`, `COL`, or `Arena_COL_MAT`. Name-based filters also exclude proxy, collision, and invisible materials. Higher LOD and shadow-proxy nodes are excluded from the LOD0 beauty mesh.

This distinction matters: the original collision colors are bright green or red. Exporting these faces as visible surfaces obscured the textured covers, ramps, statue, and temple wall. The correction removed **774 non-rendering triangles from nine meshes**. The original CGF and DAE data remain available.

The affected mesh families are:

- `rel_ed_block_clump_a_group` through `rel_ed_block_clump_e_group`.
- `rel_eldorado_mid_circle_top_ramp_a_group` and `rel_eldorado_mid_circle_top_ramp_b_group`.
- `rel_el_arenastatue_a_group`.
- `rel_el_arenastartareawall_b_group`.

Exact changed IDs, material slots, FBX paths, and hashes are recorded in [geometry-shader-filter-changes.json](Evidence/geometry-shader-filter-changes.json) relative to the project root. Reimport these meshes and rebind their slots by stable names; removing a collision material changes the remaining numeric slot indices.

Beauty filtering does not establish collision acceptance. Preserve or reconstruct intended collision separately and test the actual character and Relic. Do not render `Nodraw` surfaces to compensate for missing collision.

## Import and verify

The calibrated FBX export uses `axis_forward='-Y'`, `axis_up='Z'`, `apply_unit_scale=True`, and `apply_scale_options='FBX_SCALE_NONE'`. Source UVs and per-corner normals are retained. Unreal mirrors source Y during import; verify the calibration bounds before placing the scene.

Material matching uses the source material ID in `geometry_manifest.json`, compared case-insensitively with the texture manifest. The FBX slot names use stable `DOR_` hashes. Source instance material overrides, including floor logos and warm cloud variants, remain explicit in the manifest.

For native import, dependency checks, comparison cameras, and reopen validation, see the [integration guide](IntegrationGuide.md). Run the [traversal plan](../Config/traversal_plan.json) only in an owned disposable PIE session. Source mesh profiles and successful exports do not prove native collision, rendered appearance, or runtime performance.

## Apply the dimension controls

Edit the `parameters` object in [dimensions.json](../dimensions.json). The adapter reads camera fields without modifying them. Its immutable source baseline is in [dorado_dimensions.py](../Scripts/dorado_dimensions.py); do not change that baseline when adjusting the central configuration.

Call the adapter on each original manifest instance before spawning or resetting its actor:

```python
import dorado_dimensions as dimensions

config = dimensions.load_config()
dimensions.validate(config)
location_cm, quaternion_xyzw, scale = dimensions.apply(source_record, config)
```

Apply the resulting transform once. Reapplying it to an already modified record compounds the adjustment. The original manifest remains the reset source. At the shipped defaults, all 288 recovered instance transforms and four reconstructed energy transforms remain exactly unchanged.

| Control | Effect and coupling |
| --- | --- |
| `arena_length_cm`, `arena_width_cm` | Apply a shared Y/X affine to the connected court structural kit. Source structural rotations are cardinal, so floor and stair shared points follow the same transform. Loose cover and banner shapes remain rigid while their positions follow the court. Temple front anchors move with the court; their shapes use the separate temple controls. |
| `main_floor_elevation_cm` | Raise or lower the court, temples, main ring, and energy together. Distant terrain, vegetation, clouds, waterfalls, and vista rings retain their source transforms. |
| `side_platform_height_cm` | Scale the complete side-platform and stair assembly vertically about floor Z=0. Existing tread-to-pad contacts follow one affine; the main floor plane remains fixed before the elevation offset. |
| `side_platform_radius_cm` | Apply an additional shared X/Y ratio to the connected court footprint, including stairs and end approaches. It also changes effective arena length and width. Shared centerline and step meshes span both platforms, so independent pad-only resizing needs mesh deformation. The value 300 is a provisional ratio baseline, not a verified physical radius. |
| `pylon_spacing_cm` | Move the giant framing serpent-wall pairs apart or together by symmetrical X shifts. Each wall body and its matching clump use the same shift; small edge/support snake heads remain unchanged by this control. The shipped value 1900 is a nominal baseline label, inherited from an earlier small-head measurement, so edits specify a spacing delta from the recovered giant pair rather than an absolute measured center spacing. |
| `end_approach_rise_cm` | Scale the start ramp, attached start floors, end stairs, and serpent-wall groups vertically about the main floor plane. Raise the narrow shrine's front/floor anchor by the same approach-height delta. |
| `ring_outer_diameter_cm`, `ring_axial_depth_cm` | Scale main-ring local X/Z and Y respectively, including the recovered magnetic mesh and reconstructed energy. Reinspect gap alignment after nondefault values. Vista rings remain unchanged. |
| `ring_radial_thickness_cm` | Requires the separate source regeneration command below. Changes radial offsets around a fixed source centerline rather than uniformly enlarging the ring. |
| `sun_temple_dimensions_cm`, `narrow_shrine_dimensions_cm` | Width/depth/height ratios about fixed front/floor anchors. Both narrow-shrine meshes move together. These are provisional envelope baselines; they are not exact measured final bounds. |
| `main_floor_core_width_cm` | Informational source-envelope value. An independent edit is rejected; use `arena_width_cm` for the connected court width. |

These controls adjust environment geometry. Existing goals, player starts, gameplay volumes, comparison cameras, navigation, and lighting are not manifest instances and are not repositioned by this module. Changes to playable dimensions require those actors to be reconciled and actual-character/Relic traversal to be repeated. Large changes also need a fresh visual check against the unchanged surrounding terrain. Positive finite dimensions are required; a new noncardinal structural mesh that would need shear is rejected instead of silently approximated.

### Regenerate ring radial thickness

After changing `ring_radial_thickness_cm`, run this command from the project root:

```powershell
& 'F:\Blender Foundation\Blender 5.2\blender.exe' -b --factory-startup --python 'E:\Unreal Projects\ProjectB\SourceArt\Dorado\Scripts\regenerate_ring_dimensions.py' -- --config 'E:\Unreal Projects\ProjectB\SourceArt\Dorado\dimensions.json'
```

The command reads `Dorado_Recovered.blend` afresh and writes the two main-ring FBXs plus an editable variant blend under `Exports/DimensionVariants/`. It preserves source UVs, material slots, and angles, and transforms authored corner normals with the deformation's inverse-transpose Jacobian. The measured source median centerline radius is 4537.19877 cm before actor integration scale. Inner vertices cannot cross the radial axis. Original exports and the recovered source blend remain unchanged.

After both exports succeed, the command writes `Config/ring_dimension_bake.json`. `load_config()` reads this metadata and verifies source/export file hashes; `asset_overrides(config)` returns the variant FBX records keyed by the original mesh IDs. Import these variants into the same native mesh assets before applying the instance transforms. A changed thickness without matching, current bake metadata is rejected. Resetting the parameter to 370 uses the original FBX paths and the command becomes a no-op. Because the radial centerline stays fixed, changing thickness also changes the outer radius; the diameter parameter remains an additional actor-scale ratio. Reinspect energy joins and collision after changed geometry.

Run the bounded source checks with:

```powershell
python SourceArt/Dorado/Scripts/test_dorado_dimensions.py
```

Eight checks cover exact default identity, independent control effects, positive scales, unchanged unrelated scenery, structural shared-point transforms, the side assembly's fixed ground plane, giant-pylon spacing with matching clump seams, stale source/export rejection, invalid input rejection, and radial deformation invariants. They do not replace Unreal rendering or gameplay acceptance.
