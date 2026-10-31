# Dorado visual iteration record

Status: all seven final stills inspected, final performance sampled, and the saved final state reopened successfully in a fresh UE5.8.1 editor. The supplied three references remain the visual authority. Source measurements inform the implementation but do not establish original reflectance or exact screenshot lighting.

## Changes and evidence

| Observed issue | Current change | Evidence and limit |
| --- | --- | --- |
| Foreground heads too small in REF01 comparison | Fit camera against eight court, temple, and near/far head landmarks; retain source head scale. | [camera-ref01-fit-heads.json](Evidence/camera-ref01-fit-heads.json), approximately 4.10 px selected-feature RMS; not a whole-image score. |
| Olive/brown masonry bias | Palette12 derives linear per-channel gains from measured source-texture medians and image-inferred slate/teal target colors. Original texture variation remains. | [material-linear-palette-audit.json](Evidence/material-linear-palette-audit.json) and [material-palette-trial12.json](Evidence/material-palette-trial12.json); 25 changed records, inferred target albedo rather than original measured reflectance. |
| Lateral pad colors and daylight-dependent glyphs | Seventh master family `Glow` is opaque and unlit. Both halves of the positive-X pad use cyan, while negative-X uses green, matching their REF01 projection. Separate stone-panel variants use the source metallic-region mask to preserve gold. | `Config/material_art_overrides.json` is authoritative for source instance IDs, material slots, colors, and emission. No gameplay role or actor transform is changed. |
| Repeated embossed patterns on broad floor/logo surfaces | Use full-atlas Normal and Roughness UV scale 1 for mainfloor01/02 and logo variants. The source metadata's 12×11.5 scale on mainfloor01/logo was repeating the atlas's emblem details. | Current override vectors and matching recovered diffuse/normal atlas inspection; mainfloor02 already records unit scale. Source PNGs are retained. |
| Gold highlights too sharp | Five pure-gold material overrides use RoughnessFloor 0.5, Metallic 1, and EmissionStrength 0. | Current override config; reflective gold stays separate from luminous glyphs. |
| Water moving across the fall rather than down it | Native Water panner U+0.12/V0; six waterfall overrides retain emission 2500 and opacity 0.65. | [water-source-axis-audit.json](Evidence/water-source-axis-audit.json) shows U/height correlation 0.902 and V/width 0.995. Flow direction is source-supported; appearance values are inferred and require motion review. |
| Opaque rectangular floor logos | Sixth master `MaskedLogo` uses source diffuse alpha and AlphaTest 0.98. | Importer and recovered material metadata; inspect the resulting edge after final shader compilation. |
| Cyan distance and inconsistent warm/cool balance | One shared lighting setup, with source-informed warm directional fog and cool sky fill. | Current [presentation.json](Evidence/presentation.json) plus importer; values below supersede historical trials. |

The current shared setup uses sun 85000 lux, RGB (255,244,228), pitch=-35/yaw=-20/roll=0; Sky Light multiplier 2.5 and RGB (160,200,255); EV100=11, white balance 6500 K, bloom 0.35; fog density 0.003, falloff 0.035, directional RGB (0.045,0.018,0.015), exponent 4; Rayleigh 0.5 and Mie 0.2. Camera-specific lighting substitutions are not used. The sky RGB is recorded in the importer; [presentation.json](Evidence/presentation.json) records the other values.

## Editor failures and recovery

Changing the loaded Power material parent from lit to unlit triggered an editor shader-cache failure involving `UniformExpressionCache`. This is separate from the earlier nonfatal viewport capture ensure. Recovery launched the source-built UE5.8.1 editor as PID 49472; see `editor-material-recovery-launch.json` and `editor-material-recovery.log`.

`build_materials()` now requires the loaded owned Dorado map to be saved and clean, then opens a blank map before changing material graphs or parents. This releases the live scene render proxies. Assembly reloads the saved Dorado map afterward. Dirty-map conflicts raise an error instead of discarding work. [material-scene-unload.json](Evidence/material-scene-unload.json) records the reason. Four subsequent `finish_visual_iteration` jobs completed in the recovery log, each approximately 18.8–20.2 seconds; that demonstrates the recorded authoring reruns, not final rendering acceptance.

The capture helper now calls `editor_set_viewport_realtime(False)` followed by `True`. Installed `LevelEditorSubsystem.cpp` shows that False adds the named override and True removes it, preventing the missing-override ensure seen when True was called alone. This changes the project helper only; no engine source was changed. A successful request still requires a saved PNG and visual inspection.

After the final reopen, an inactive level viewport behind a floating Content Browser delayed the REF01 screenshot. Closing the child window let capture complete. The reopened native image confirms the scene remains intact, with visible first-frame temporal noise. Keep the level viewport active and realtime rendering enabled for animated sky/effect preview, and allow temporal rendering to settle before comparison. The final seven-image gallery uses the already inspected settled captures.

## Final image review

The [comparison gallery](ComparisonGallery.html) presents the three supplied references beside the final native PNGs and includes all four supplementary views. It links the existing files without image edits, crops, projections, or color corrections. The rendered references retain their supplied aspect ratios. This is a visual review, not a whole-image numerical similarity score.

| View | Closest correspondences | Remaining differences |
| --- | --- | --- |
| REF01 | Court slab layout, central and foreground circles, paired side platforms and stairs, broad sun façade, near/far serpent profiles, and ring depth | Masonry and cover faces are paler; tan slabs lean more yellow/olive. Gold has some orange highlights, especially stair cheeks. Cyan/green pad glow is weaker. Ring energy is more saturated blue-violet. |
| REF02 | Pylon framing, distinct narrow shrine, ring's upper break and animal-head terminal, circles, and distant stepped pyramid | Warm haze reduces the cooler green forest layers and dark teal pyramid contrast. Foreground shadows lack the reference's purple bounce. Small framing offsets remain, including the pedestal orb and pyramid. |
| REF03 | Sun center, ring edge, island/cliff silhouettes, hanging roots, waterfall location, and distant temple silhouette | Yellow/cream haze replaces much of the peach sky and blue-violet cliff shading. Water shows strong repeated streaks and less soft cyan translucency. Ring energy reads as continuous bright strips in some stone-adjacent areas. |

Across the views, foliage preserves the rounded source canopy masses but is more lime green and has sharper light/dark patches than the supplied images. Architecture remains readable in the near field; the distant contrast is less accurate. Small violet cores are visible, while the sun and pad bloom differ in strength. The final result therefore preserves the specific architecture and composition more closely than the original atmosphere and effects treatment.

The player-center and side-platform images show continuous visible court surfaces, sparse cover, floor ornament, and coherent scenery between the reference cameras. They also reveal the remaining bright gold stair highlights and pale cover panels. The initial shrine-end capture sat only 2.7 cm above its raised floor. A native floor query measured Z162.3028 cm at `(450,-3000)`; the saved camera now sits at Z327.303 cm, 165 cm above that floor. [shrine-camera-floor-adjustment.json](Evidence/shrine-camera-floor-adjustment.json) records the correction; geometry and lighting were unchanged.

The supplementary top-down image uses a perspective camera at Z6000 cm with horizontal FOV82°. The original orthographic attempt washed out under the atmospheric projection. The corrected view covers the main court, with the vertical ring seen edge-on across its center; the end-temple extents are partly cropped. It is a layout presentation, not a metric orthographic drawing. All seven final images are committed under `SourceArt/Dorado/Docs/Renders/`; the original run copies remain under ignored `Saved/Logs/dorado-reconstruction-20261030/Renders/final/`.

## Final validation and limits

The final 600-frame WindowsEditor PIE sample and its actual readback settings are recorded in the [validation report](ValidationReport.md). It is not a controlled comparison to the earlier material/lighting state. An independent review of two native CAM_Ref_03 captures approximately 108 seconds apart found broad changes in water streaks and violet bands beyond the measured static-region temporal noise. [effects-final-review.json](Evidence/effects-final-review.json) records this native animation observation and connected Time/Panner source wiring. Full-loop continuity, direction, speed, and smoothness remain unproven; selected views do not cover every reachable sightline.

Final native inventory is 557 assets. The final close/reopen from PID 49472 to PID 58364 preserved the exact map hash, camera transforms, four stair overrides, seven Nanite backdrops, material families, and shared lighting; dirty packages remained empty. Fresh structural validation reported no errors and six complete navigation paths. The [validation report](ValidationReport.md) records the inherited dependency/startup messages and unrun scopes. The [readiness checklist](ReadinessChecklist.md) summarizes this ProjectB-specific coverage without claiming a perfect image match or independent standalone distribution.
