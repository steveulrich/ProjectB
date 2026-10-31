# Dorado validation report

**Status: validated native integration in the intended ProjectB workspace, within the checks below.** The final saved map reopened successfully in a fresh Unreal Engine 5.8.1 process on October 30, 2026. Structural validation reports no errors and six complete navigation paths. Seven final captures, scoped actual-character and Relic checks, native animation observations, and final performance evidence are recorded. Remaining visual differences and unrun checks are explicit; this is not a pixel-perfect match or a standalone packaged delivery.

## Scope and provenance

- Native map: `/BreakawayCore/Environments/Dorado/Maps/L_Dorado`.
- Owned content root: `/BreakawayCore/Environments/Dorado/`.
- Engine: **5.8.1-0+UE5**, recorded by [import-preflight.json](Evidence/import-preflight.json).
- Source exporter: **Blender 5.2.3 LTS**, build `e13b728c6880`; CryEngine Converter **1.6.0.0**.
- Editable source: `SourceArt/Dorado/Dorado_Recovered.blend` and `Dorado_RingEnergy_Reconstructed.blend`, with FBX/DAE, textures, manifests, and regeneration scripts.
- Committed evidence: [selected summaries and index](Evidence/README.md), [structural summary](Evidence/structural-summary.json), and [runtime summary](Evidence/runtime-validation-summary.json). Original full report filenames below identify local provenance under ignored `Saved/Logs/dorado-reconstruction-20261030/`; full traces and editor logs are not distributed.
- Integration target: existing ProjectB. No clean-project migration, standalone cook, packaged environment run, physical input acceptance, or LAN acceptance is claimed.

The gameplay queue remains **paused** in [SLICE_STATUS.md](../../../AI_Planning/SLICE_STATUS.md). This task does not resume any gameplay gate. The protected `/BreakawayCore/Maps/L_BW_Dorado` payload matches its Git HEAD LFS hash in `inherited-frontend-builddata.json`.

## Native assets and structural evidence

[final-delivery-inventory.json](Evidence/final-delivery-inventory.json) records the final native inventory, consistent with the fresh reopened `22:15:22 UTC` structural class counts:

| Class | Saved count |
| --- | ---: |
| World | 1 |
| StaticMesh | 103 |
| Material | 7 |
| MaterialInstanceConstant | 133 |
| Texture2D | 313 |
| Total | 557 |

The 103 meshes comprise 98 recovered source meshes, four reconstructed ring-energy meshes, and one calibration mesh. Glow cores use an engine primitive. The recovered manifest contains 288 placements and 171450 triangles; the four energy modules add four placements and 576 triangles. The final staged-input audit checked **888 required paths**, including 495 prepared texture-map paths and 30 art-override texture paths, with no missing or out-of-package paths. All 98 recorded source-FBX hashes match. The other five FBXs—four energy modules and calibration—were fingerprinted; their manifests had no pre-existing comparison hash. The protected original map still matches its Git HEAD LFS payload.

The asymmetric calibration check passed with maximum bounds error **0.0000134 cm**. This verifies numerical scale and axis conversion. It does not prove every mesh normal, UV, or material appearance. Eighteen protected gameplay actors matched recorded class and location, including player starts, goals, base zones, Relic spawn, and the navigation volume. No null-material, negative-scale, duplicate-owned-label, or changed-protected-actor error appears in that snapshot.

Earlier snapshots failed floor and navigation assertions, exposing disabled walking-fill collision and two rays aimed at intended cover tops. The floor remedies and runtime retests below preserve those failures. The cover fixture now requires the exact block-clump actors at `(25,±2000)`, Z=96±5 cm, and upward normals; it does not broadly ignore obstacles. Other ground assertions and centerline diagnostics remain. See [cover-sample-classification-adjustment.json](Evidence/cover-sample-classification-adjustment.json). The first full reopen passed sampled structural checks and all six complete navigation paths at 21:30 UTC, before later presentation changes.

The final pre-close [delivery-before-close.json](Evidence/delivery-before-close.json) inspection at 22:13:22 UTC returned `persisted_settings_pass=true`. PID 49472 exited normally at 22:13:49 UTC; fresh PID 58364 launched at 22:14:02 UTC and loaded the saved native map. [delivery-after-reopen.json](Evidence/delivery-after-reopen.json) confirms 340 actors, seven cameras, four stair components with WalkableSlopeIncrease=47 degrees, seven enabled Nanite backdrops with explicit tangents, seven master families, expected sun/sky RGB, no dirty packages, and no PIE world. The before/after map SHA-256 is identical: `04557811baddff4baab92a7fc07752dbe4552303c7d25147e1c803696f8dc89d`. The refreshed final inventory records this same on-disk hash.

Fresh `validation-structural.json` at 22:15:22 UTC returned **PASSED_SAMPLED_STRUCTURAL_SCOPE_ONLY**, with no errors and all six navigation paths valid, complete, and passed. Its three warnings are the inherited frontend BuiltData dependency and two explicitly retained centerline cover diagnostics. The script's `ready_to_integrate=false` means that this structural script alone cannot establish visual, traversal, ball, reopen, or migration acceptance; those scopes are reported separately here. `editor-delivery-reopen.log` contains no fatal, assertion, ensure, or shader-failure entry. It retains one inherited PixelStreaming2 startup `GEngine`-invalid error for a feature not used by this environment.

The native reopened REF01 capture was inspected and retains the scene, materials, and composition. This intermediate proof image remains in local `Renders/delivery-reopened/` run evidence; its first-frame temporal noise is visible. A floating Content Browser had left the level viewport inactive and delayed capture; closing it allowed the realtime viewport to finish. The seven inspected final PNGs are the committed gallery deliverables.

Post-validation cleanup found that the in-memory PIE settings restore had not persisted `AdditionalServerGameOptions`. The closed-editor `Restore-DoradoPIESettings.ps1` helper restored the four recorded keys from the session baseline, with only that options key requiring a change; [pie-settings-disk-restored.json](Evidence/pie-settings-disk-restored.json) records the exact edit. The second stop of PID 58364 exceeded the tool's graceful-shutdown timeout and the tool reported forced termination. Dirty packages were empty and the saved map hash was unchanged beforehand; no UnrealEditor process remained afterward. This later tool shutdown limitation does not replace the earlier successful normal close/reopen proof. Fresh PID 42844's [pie-settings-reopened-final.json](Evidence/pie-settings-reopened-final.json) readback at 22:21:36 UTC matches the original settings, including `?NumBots=4?Experience=B_BW_Experience_Dev`. [delivery-final-native.json](Evidence/delivery-final-native.json) confirms the unchanged map hash, 340 actors, seven cameras, four stair overrides, seven Nanite backdrops, and no dirty packages. The editor was left open on Dorado. Its log retains only the same PixelStreaming2 startup error; no new owned shader failure, ensure, or fatal entry was reported.

## Actual character movement

The following runs used the actual `/BreakawayCore/Characters/B_BW_Character_Humanoid` pawn in one local authority PIE world. Observed runtime settings were capsule radius **35 cm**, half-height **96 cm**, maximum step height **50 cm**, and maximum walk speed **600 cm/s**. Existing native Enhanced Input drove movement; no physical keyboard or gamepad acceptance is implied.

All six runs returned `PASS_SAMPLED_NATIVE_MOVEMENT`, reached their recorded destination, and reported no errors. Cleanup restored walking and collision state.

| Route | Recorded XY path, cm | Result | Evidence file |
| --- | --- | --- | --- |
| Floor bands | `(450,-650)` to `(450,650)` | Pass at sampled character scale | `dorado-floor-seams-01.json` |
| Cross centerline seam | `(-125,500)` to `(125,500)` | Pass at sampled character scale | `dorado-cross-center-seam-01.json` |
| East stairs up | `(1275,1025)` to `(1275,50)` | Pass; target floor Z approximately 209 cm | `dorado-east-stairs-up-01.json` |
| East stairs down | `(1275,50)` to `(1275,1025)` | Pass; target floor Z approximately 0 cm | `dorado-east-stairs-down-01.json` |
| West stairs up | `(-1275,1025)` to `(-1275,50)` | Pass; target floor Z approximately 209 cm | `dorado-west-stairs-up-01.json` |
| South approach | `(0,-3000)` to `(0,-3850)` | Pass; target floor Z approximately 242 cm | `dorado-south-approach-01.json` |

A character capsule can bridge a gap that is unsafe for the smaller Relic. These passes do not validate all unsampled surfaces or substitute for the post-correction ball test.

After the center-band floor restoration, `dorado-repaired-center-seams-02.json` adds a seventh recorded character pass: `(25,-1000)` to `(25,1000)`, `PASS_SAMPLED_NATIVE_MOVEMENT`, destination reached, no errors, and walking/collision cleanup restored. This check addresses the repaired center lane; it does not validate the subsequent stair-foot correction.

After restoring the stair-foot tiles, `dorado-repaired-east-stairs-03.json` stopped at a recovered 45.55-degree bevel; the character's walkable floor angle is approximately 44.765 degrees. The scoped correction increases walkability to 47 degrees on only the four `rel_eldorado_mid_stair_c_group` components. Character settings, step height, source triangles, and ball collision remain unchanged; no ramp collider is added. `dorado-repaired-east-stairs-04.json` then returned `PASS_SAMPLED_NATIVE_MOVEMENT`, reached `(1275,50)`, reported no errors, and restored the pawn with position error 0.00000612 cm. This is an eighth recorded successful movement run, repeating the east stairs under the corrected collision setup. [stair-bevel-walkability-trial.json](Evidence/stair-bevel-walkability-trial.json) preserves the causal evidence. The assembler persists the rule, and the final reopen confirmed all four components retain it.

## Cover and exposed edge

The first cover run, `dorado-cover-block-01.json`, returned `FAIL_OBSTACLE_OR_CLEANUP`. Its straight world-Y approach met the angled cover obliquely, slid laterally by approximately **137 cm**, and progressed **482 cm**. Cleanup succeeded. The result is retained as a failed blocking test with an unsuitable approach angle, not silently relabeled as a pass.

The second run approached perpendicular to the visible face. `dorado-cover-block-02.json` returned `PASS_VISIBLE_OBSTACLE_BLOCKING`: the pawn moved then stopped after **212.7 cm**, within the **235 cm** permitted progress, and cleanup succeeded.

The first attempted edge route reached a platform boundary without establishing a fall. The second exposed-edge route observed a **406.826 cm** fall and the existing normal respawn behavior. Its automatic restoration targeted a narrow parapet and failed to verify a safe walking state, so `dorado-open-edge-02.json` remains `FAIL_EDGE_OR_CLEANUP`. The separate [open-edge-cleanup.json](Evidence/open-edge-cleanup.json) records the explicit recovery to `(450,1000)` on the court, walking mode, collision enabled, and normal respawn observed. This proves the recorded fall and cleanup only; it is not an all-edge acceptance pass.

## Actual Relic observations and floor remedy

Two runs used the actual `BP_BW_RelicActor` and existing throw path, rather than a generic test sphere. Both returned `OBSERVED_ACTUAL_RELIC_TRAJECTORY`, recorded 122 free-physics samples, reported no errors, and restored character state:

| Throw | Maximum sampled speed | Evidence |
| --- | ---: | --- |
| Open floor lane | 951.9 cm/s | `dorado-relic-floor-openlane-01.json` |
| Down the east stairs | 1088.8 cm/s | `dorado-relic-stairs-down-01.json` |

These observations demonstrate actual native trajectories. Sampled movement alone does not prove contact topology, every bounce, or safe travel across every floor band.

The source audit identified two original planar floor-fill meshes: `main_floor_tc` with 156 triangles and `main_floor_center_tc` with 148 triangles. At the integration scale their Z spans are approximately **0.00155 cm** and **0.0009 cm**. They are visible floor geometry. A broad decorative `NoCollision` exclusion incorrectly disabled their four instances, leaving gaps around the center bands and centerline. The correction restores collision on those original meshes; it does not add hidden box patches. See [planar-floor-collision-remedy.json](Evidence/planar-floor-collision-remedy.json), `runtime-floor-grid.json`, and `source-floor-gap-measurement.json`.

`dorado-relic-repaired-center-02.json` records the actual Relic after center-band restoration: 217 free-physics samples, maximum sampled speed 903.6 cm/s, no errors, and successful cleanup. It settled with zero velocity at `(2.245509,-643.211931,15.335434)` cm, inside the previously unsupported band. [repaired-center-floor-support.json](Evidence/repaired-center-floor-support.json) confirms floor support at that XY and across ten probes around Y±650. With recorded ball radius 15.334581 cm, the calculated bottom is within 0.000004 cm of the floor hit at Z=0.0008568 cm. This is specific evidence of support at the former hole, not an arena-wide contact guarantee.

The second stair throw, `dorado-relic-stairs-down-02.json`, recorded 132 free-physics samples but fell through a disabled `center_point` tile at the stair foot. Its generic trajectory-observed result is **not** a stair acceptance pass. The source audit identified four such 171-triangle tiles and two 486-triangle `groud_design` walking strips whose collision was also excluded. The scoped remedy restores their exact source triangle collision; stairs and exposed edges remain unchanged. See [stair-foot-collision-remedy.json](Evidence/stair-foot-collision-remedy.json).

The post-fix third throw is successful within the sampled movement-and-rest scope. [relic-repaired-stairs-analysis.json](Evidence/relic-repaired-stairs-analysis.json) records 234 actual free-physics samples, approximately 910.7 cm sampled travel, maximum speed 1095.3 cm/s, and a five-second stationary tail of 81 samples. The Relic settled at `(1272.285397,852.065057,15.429090)` cm. [stairs-repaired-rest-support.json](Evidence/stairs-repaired-rest-support.json) measures its collision-body bottom only 0.00001064 cm above the floor hit. This establishes support after traversing the repaired stair-foot region; it does not identify every contact between samples.

All 152 queries in `restored-tile-native-probes.json` found a hit. Eight additional `ground-fill-low-origin-probes.json` queries start at Z20 cm beneath the surrounding cover and confirm the restored visible walking fill at floor Z7.45–9.14cm. These scoped native measurements complement the actual ball and character runs; they are not exhaustive proof of every arena surface.

## Visual reconstruction

Three supplied reference images were inspected at native resolution. The recovered scene preserves separate broad sun and narrow-shrine ends, two giant serpent-wall pairs, four smaller edge-brace pairs, the complementary broken ring, court platforms, retaining walls, trees, floating islands, and waterfalls.

Manual landmark fits use recovered source geometry and exact surface picks. Their residuals are approximately 4.10 px for CAM_Ref_01, 2.68 px for CAM_Ref_02, and 4.13 px for CAM_Ref_03. These figures measure selected feature correspondence; they are not whole-image similarity scores or a visual acceptance threshold. The near serpent heads in REF01 fit through camera perspective without rescaling their source geometry.

The source package also includes four explicitly reconstructed ring-energy modules and a source-backed manifest for six pedestal orbs plus eight ring-face glow cores. The small primitive dimensions and native glow materials are reconstructions, not recovered original particle simulations. See [energy reconstruction](EnergyReconstruction.md).

The final three reference and four supplementary PNGs are inspected in the [comparison gallery](ComparisonGallery.html). [VisualIteration.md](VisualIteration.md) records the major changes and remaining differences. Earlier cyan-atmosphere trials remain documented in [LightingFit.md](LightingFit.md). An initial capture-path failure involved a missing viewport realtime override; the corrected False/True override pair produced the final captures. This nonfatal ensure is separate from the material-edit crash below.

The current authoritative [presentation.json](Evidence/presentation.json) records one shared setup: Directional Light 85000 lux, RGB `(255,244,228)`, pitch=-35/yaw=-20/roll=0, fixed EV100=11, Sky Light multiplier 2.5, bloom 0.35, white balance 6500 K, fog density 0.003/falloff 0.035, directional fog RGB `(0.045,0.018,0.015)` with exponent 4, Rayleigh 0.5, and Mie 0.2. The importer sets Sky Light RGB `(160,200,255)`. Explicit RGB keywords fix the native positional BGRA constructor mismatch. These settings supersede earlier trials but do not by themselves establish a visual pass.

The source UV audit shows U follows waterfall height (correlation 0.902) while V follows width (0.995). The native Water panner now uses U+0.12/V0; six waterfall overrides use emission 2500 and opacity 0.65. The new sixth master, MaskedLogo, uses original diffuse alpha with the source AlphaTest 0.98 threshold to remove the opaque logo quad. Source-axis evidence is in [water-source-axis-audit.json](Evidence/water-source-axis-audit.json); the final stills show patterned, streakier water than the reference.

[effects-final-review.json](Evidence/effects-final-review.json) independently compares two native CAM_Ref_03 captures taken about 108 seconds apart. Water streaks and violet energy bands change broadly while stone/gold controls remain aligned. Effect-region differences remain approximately 8.6–18.5 RGB levels after a noise-suppressing comparison, versus 0.16–0.43 in static controls. Importer inspection confirms Time × AnimationRate → Panner → sampled UVs. This supports native time-varying effects; two captures do not prove motion direction, speed, smoothness, seamless wrapping, or a complete observed repeat period.

The subsequent seventh master, Glow, makes the opaque Power glyphs unlit, with paired per-pad cyan/green variants selected by reference projection. Palette12 uses measured linear source-texture medians and inferred target colors; full-atlas normal/roughness scales and pure-gold roughness were also corrected. [VisualIteration.md](VisualIteration.md) records these changes and their evidence. A live lit-to-unlit parent change caused a separate editor shader-cache crash. Recovery PID 49472 completed four subsequent visual-iteration jobs after the importer began unloading the saved clean map before material edits. All seven final stills are inspected in the [comparison gallery](ComparisonGallery.html); final performance and a subsequent clean reopen passed their recorded scopes. Later stable runs do not erase the crash history.

The final images closely preserve the recovered architecture, court patterns, and selected camera landmarks. Remaining differences include warmer and paler distance, reduced blue/violet shadow fill, some orange gold highlights, pale cover faces, weak pad glow, streakier waterfalls, and more saturated energy. The top-down view is a perspective layout capture at Z6000 cm/FOV82°, not a metric orthographic view. Its main court is complete; end-temple extents are partly cropped. The shrine-end camera was corrected to sit 165 cm above its measured local raised floor before its final capture.

## Performance evidence and limits

[performance-csv-comparison-summary.json](Evidence/performance-csv-comparison-summary.json) records three fixed-camera WindowsEditor PIE captures of 600 frames each. Baseline median GPU time was **4.294 ms**; backdrop GI and shadow variants were **4.296 ms** and **4.291 ms**. Median draw calls changed from **713** to **708** and **706**; shadow-depth draw calls changed from **115** to **115** and **111**. GPU memory median remained approximately **5611 MB**.

The A/B samples used RTX 4070 SUPER / Ryzen 7 9800X3D hardware, a 1280×720 PIE client, scalability values 3, VSM enabled, and a fixed pawn position `(450,1000,98.15)` cm facing yaw 90 degrees. `editor-recovery01.log` records dynamic resolution disabled (`r.DynamicRes.OperationMode 0`) and screen percentage fixed at 100 before the first fixed capture at 20:45 UTC. The same session then captured the GI and shadow variants, and restored the original values 1/0 at 20:48:13 UTC. `perf-baseline-metadata.json` records the earlier 20:43 setup values 1/0; those are not the fixed A/B capture settings. The CSV itself does not record primary-resolution CVars.

This small A/B scope does not demonstrate a frame-rate improvement: median frame time increased from 13.160 ms to 13.981/14.088 ms, and there was only one capture per condition. The reports lack individual VSM/Lumen pass timings.

The final state has a separate **600-frame** WindowsEditor PIE sample, `dorado-final.csv`, with final materials, lighting, and seven Nanite backdrops. [performance-final-metadata.json](Evidence/performance-final-metadata.json) records actual CVar readback: dynamic resolution 0, screen percentage 100, all ten recorded scalability values 3, and VSM 1. Hardware, 1280×720 client area, fixed pawn location, and yaw match the setup above; this is one local authority with zero bots. [performance-final-summary.json](Evidence/performance-final-summary.json) uses R-7 interpolated percentiles without outlier trimming:

| Metric | Median | 95th percentile | 99th percentile |
| --- | ---: | ---: | ---: |
| Frame time | 8.340 ms | 10.800 ms | 12.314 ms |
| GPU time | 4.705 ms | 5.750 ms | 6.014 ms |

Median reported GPU local memory was **5400 MB**, with **654 draw calls** and **81 shadow-depth draw calls**. One frame exceeded 16.67 ms; none exceeded 33.33 ms. `runtime-final-player-ui.jpg` provides the separate fresh native UI observation after camera motion. [performance-final-restored.json](Evidence/performance-final-restored.json) confirms the original dynamic-resolution/screen-percentage values 1/0 were restored and no traversal job remained active.

The final scene differs from the earlier A/B state, so these figures do not isolate an optimization's benefit. One short fixed-view editor sample does not establish sustained, packaged, shipping, or four-player performance, and does not replace per-pass profiling.

## External dependency warning

The recursive asset audit reached 2272 packages and reported one missing package: `/Game/System/FrontEnd/Maps/L_LyraFrontEnd_BuiltData`. The inherited chain is:

```text
L_BW_Dorado
  -> B_BWayGameMode_Default
  -> B_BWayGameMode
  -> BP_BW_GameState
  -> L_LyraFrontEnd
  -> L_LyraFrontEnd_BuiltData
```

`inherited-frontend-builddata.json` proves the current frontend map and original Dorado map match their Git HEAD LFS payload hashes. This is an inherited external frontend lighting-data reference, not a newly missing owned Dorado mesh or texture. It remains a warning and a distribution concern. Repairing unrelated gameplay/frontend content is outside this environment change; migration or packaging must still account for it.

## Final coverage and remaining limits

| Check | Current status | Evidence or limit |
| --- | --- | --- |
| Center-band floor and Relic | Repaired-lane evidence recorded | Seventh character pass, 217 actual-ball samples, settling at former hole, support probes and cleanup; retain limited scope |
| Stair-foot and ground-strip collision | Scoped native evidence passed | 152 restored-region hits, eight isolated ground-fill hits, actual stair-ball traversal/rest, corrected character stair pass; four saved component overrides confirmed after final reopen |
| Final presentation | Seven stills inspected; time-varying effects observed | Shared lighting and native materials visible; full effect repeat period, direction, speed, and smoothness were not proved |
| Seven Nanite backdrops | Final sample recorded | `nanite-backdrop-configuration.json` and final 600-frame performance evidence; no isolated speedup claim |
| Final structural/navigation report | Final reopened snapshot passed | No errors and six complete paths at 22:15:22 UTC; sampled scope only |
| Final reference and supplementary views | Seven PNGs inspected | Three reference comparisons, perspective top-down, and three player views in `ComparisonGallery.html` |
| Save-close-reopen | Final state passed | PID 49472 closed normally; PID 58364 reopened unchanged map hash, settings, and clean assets |
| Final asset/source manifest | Refreshed and audited | 557 native assets, 888 staged paths, 103 FBX fingerprints, 98 recorded hashes matched; protected original map unchanged |
| Standalone migration/cook | Not performed | Outside the current ProjectB integration claim |

Unrun scopes include a fresh end-to-end source regeneration into an empty destination, relocation, standalone migration/cook, packaged performance, physical input, and remote/network/LAN acceptance. Native movement and contact checks sample the stated routes and surfaces; they do not exhaust every accessible seam, bounce, hidden side, or exposed edge. Effect loop continuity was not observed over a full repeat. The inherited frontend dependency remains explicit. These limits and the recorded visual differences travel with the ProjectB integration handoff.
