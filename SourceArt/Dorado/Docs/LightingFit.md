# Dorado lighting fit

Status: source audit and visual recommendations, October 30, 2026. These observations use `Renders/iteration04/CAM_Ref_03.png`; the recommendations are not validated. This historical audit used a shared lighting setup at fixed EV100=12; the later applied settings below take precedence.

The later [presentation.json](Evidence/presentation.json) is authoritative for applied settings. The latest values and palette changes are summarized in [VisualIteration.md](VisualIteration.md). Explicit RGB keyword arguments corrected the native positional BGRA color-constructor mismatch. The search ranges below document earlier reasoning and must not overwrite the current report automatically.

## Source evidence

The original `mission_mission0.xml` fixes time of day at 7.5. Its SHA-256 and extracted values are in [source-lighting-observation.json](Evidence/source-lighting-observation.json).

| Source control | Recorded value | Interpretation |
| --- | --- | --- |
| Sun intensity | 85000.102 | Retain the native Directional Light at 85000 lux while fitting atmosphere. |
| Sun RGB | `(0.84958, 0.43241, 0.07422)` | Strongly warm source color; renderer and tone mapping differ. |
| Base fog RGB, multiplier | `(0.17884, 0.30375, 0.35282)`, 3.42857 | Cool base fog. |
| Top fog RGB, multiplier | `(0.10163, 0.17884, 0.51319)`, 2.14286 | Cool upper atmosphere. |
| Radial fog RGB, multiplier | `(0.54650, 0.27366, 0.09331)`, 3.57143 | Strong warm fog toward the sun; absent from the initial native setup. |
| Radial fog size, lobe | 0.74286, 1.0 | Broad directional haze, not a global orange grade. |
| Fog ramp start/end | 27.4286/438.8576 source meters | At the 0.5 integration scale, 1371/21943 cm are distance cues, not a direct UE fog equation. |
| Mie/Rayleigh scattering | 4.8/2.0 | Hazy source atmosphere; coefficients cannot be copied between renderers. |

`Environment/Lighting/SunVector` is approximately `(0, 0.6, -0.8)`. Interpreting it as incoming rays produced native pitch=-53.13, yaw=-90. That places the sun toward the upper-left of CAM_Ref_03, while the reference glow lies above the right half. The source vector may describe static terrain lighting rather than the dynamic time-of-day sun. Its semantic interpretation is unresolved.

The reference glow direction, inferred from the fitted camera and approximate sky pixel `(1100, 50)`, points toward azimuth 138 degrees and elevation 30 degrees. Native incoming-light rotation near pitch=-30, yaw=-42, roll=0 is an image-derived candidate, not a recovered original rotation. Compare floor shadow direction in REF_01 and opposite-facing REF_02 before accepting it.

## Observed mismatch

Iteration04 has cyan sky, blue distant cliffs, white waterfall cards, and a dark near temple. REF_03 has cream sky and a peach distant temple silhouette; nearby stone and foliage remain cooler and readable. The missing directional haze matters more than overall white balance. REF_02 remains green and cool away from the sun, so globally tinting fog or textures orange would reduce the match.

## Bounded native tuning procedure

1. Preserve fixed EV100=12, 85000-lux Movable Directional Light, and shared cameras. Compare the image-derived sun orientation with the current source-vector interpretation before changing intensity.
2. Reduce the Sky Light capture multiplier from 2.8 toward 1.0 in two steps. Check near shadowed stone after each step. Keep the capture approximately neutral/cool so useful warm/cool separation remains.
3. Reduce `SkyAtmosphereComponent.rayleigh_scattering_scale` within 0.25-0.7 of default. Increase `mie_scattering_scale` within 1-4 and inspect the pale sky and distant silhouette. Search `mie_anisotropy` within 0.75-0.9; increasing it narrows the sun halo. These are art-direction search bounds, not source conversions.
4. Add warm directional height-fog inscattering. Search `directional_inscattering_exponent` between 2 and 4 for a broad cone and start distance between 1500 and 4000 cm. The source radial hue normalized by red is approximately `(1, 0.501, 0.171)`; tune luminance at fixed exposure rather than assuming the source multiplier has native units. Preserve a neutral/cool non-directional base.
5. The current fog actor at Z=-3500 with falloff 0.12 concentrates mist beneath the arena. To haze far cliffs, raise it toward Z=0 and compare falloff 0.02-0.05, density 0.008-0.03. Keep a clear near range. Assess the island at approximately 20000 cm and the far temple separately; stop when the far temple becomes a pale silhouette without flattening the ring.
6. Use post-process white balance after spatial haze matches. Raising post-process `white_temp` toward 7500-8500 warms the display. Raising a light's temperature to those values makes that light cooler; the controls have different meanings.

Use native atmosphere/fog controls before a custom sky gradient. If a native unlit sky material is needed, record it as reconstructed art direction and retain the same spatial sun direction in all views.

## Installed API caveats

Installed UE headers verify `SkyLuminanceFactor`, `SkyAndAerialPerspectiveLuminanceFactor`, `MieScatteringScale`, `MieAnisotropy`, and `RayleighScatteringScale`. The aerial-distance property's native spelling is **`AerialPespectiveViewDistanceScale`**, without the second `r`; inspect the generated Python name before using it.

`ExponentialHeightFogComponent` exposes `FogInscatteringLuminance` and `DirectionalInscatteringLuminance`. With `r.SupportExpFogMatchesVolumetricFog=1`, separate legacy directional color/exponent/start controls are ignored in favor of volumetric fog controls. Check that setting before diagnosing an ineffective edit. A fog inscattering cubemap also disables the directional term.

Always use keyword arguments for `unreal.Rotator(pitch=..., yaw=..., roll=...)`; its positional Python constructor order is not pitch/yaw/roll.

## Acceptance limits

Inspect all three saved beauty views, a player-height view, and lighting/exposure debug views. This fit does not establish GPU performance, target-platform quality, or physical traversal. Preserve final parameters and actual render evidence in the parent validation report.
