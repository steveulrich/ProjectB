# Dorado Environment Reconstruction for Unreal Engine 5.8

## Role and goal

Act as a senior environment artist, technical artist, and Unreal Engine environment integrator.

Build an editable 3D reconstruction of the **Dorado environment from Amazon’s Breakaway**, matching the three attached screenshots as closely as the available evidence allows. Produce the environment assets, scene, lighting, collision, and integration package. This task is the environment only; preserve any existing gameplay systems.

Prioritize, in this order:

1. Arena layout, landmark silhouettes, and proportions.
2. Placement and shape of the distinctive architecture.
3. Materials, color distribution, and floor ornament.
4. Lighting, atmosphere, and background depth.
5. Smaller surface detail and secondary props.

The supplied images are the visual authority. Build the specific place they show. Generic jungle-temple assets are useful only where their actual shapes and materials fit the references.

Execute the work through modeling, assembly, visual comparison, and validation. Make routine implementation decisions independently and document uncertain assumptions. Continue beyond the initial blockout.

## 1. Inputs and workspace

Use the attached images in this order:

- **REF_01 — image(2).png:** Elevated oblique arena view. Primary evidence for floor layout, circular designs, elevated side platforms, stairs, low walls, masonry depth, the sun-emblem temple, and the foreground serpent head.
- **REF_02 — image(3).png:** View along the arena axis between massive foreground pylons. Primary evidence for arena depth, pylon framing, the broken ring silhouette, the narrow far shrine, jungle mountains, and the distant stepped pyramid.
- **REF_03 — image(4).png:** Exterior view emphasizing the sun temple, ring segment, floating islands, waterfalls, and bright atmospheric sky.

Resolve filenames if the attachments have been renamed. Inspect the images at full resolution and inspect useful crops.

First inspect the workspace, project instructions, installed Unreal version, available modeling tools, and existing import/editor automation. Use Blender or an equivalent scriptable modeling tool plus the actual Unreal Editor where available. Verify APIs against the installed version before relying on them.

If an existing project is provided, follow its content structure and conventions. If no project exists, create an isolated staging project with the requested engine version when available.

Do not silently substitute another engine version. Distinguish **“targets UE 5.8”** from **“validated in UE 5.8.”** If the target editor cannot run, complete the source assets and reproducible import package, state precisely what remains unverified, and describe that result as a source package awaiting Unreal validation.

## 2. Reference analysis and scale

Before detailed modeling, create a compact reference breakdown and provisional top-down layout. Identify visible structures, their relative positions, and the camera views that support each inference.

Separate:

- **Observed:** Directly visible in one or more screenshots.
- **Inferred:** Necessary to connect visible geometry.
- **Unknown:** Not determined by these screenshots.

Establish landmark correspondences across views before deciding that similar structures are the same object. REF_01 and REF_03 show a broad sun-emblem façade; REF_02 shows a tall, narrow far shrine. Preserve that visible difference while resolving their relationship. Do not automatically create two identical ends.

Infer scale from the target project’s actual character, stair proportions, cover heights, and floor modules. If no character is available, use a temporary 180 cm human proxy and label that as an assumption. Record estimated dimensions; do not describe them as original measurements.

Keep major dimensions adjustable in one configuration or parameter set: arena length and width, floor elevations, side-platform height and radius, pylon spacing, ring diameter and thickness, and temple dimensions.

Use centimeters consistently. Verify scale and orientation with a calibration object imported into Unreal. Use the main arena axis as X, width as Y, and up as Z unless the existing project requires another convention.

Build coherent geometry in all reachable areas. Where references leave gaps, extend the established forms conservatively and document the reconstruction. Additional references may resolve unknowns, but verify that they depict this Breakaway map and do not let them override the supplied visual target without explanation.

## 3. Arena and floor architecture

Reproduce a substantial elevated masonry court suspended over a deep jungle gorge.

Construct:

- A broad, elongated arena organized around a strong longitudinal axis.
- Deep structural retaining walls below the play surface, with visible block courses, corner treatments, gold trim, and planted lower ledges.
- A largely open floor of warm tan stone slabs with radial and polygonal seams.
- Pale stone borders, blue/slate insets, and crisp gold lines organizing the floor.
- The central circular medallion with concentric bands, a sun/gear-like center, and repeated stepped gold motifs.
- The larger foreground circular floor design with its tan center, emblem, blue-violet band, and gold geometric ornament.
- Two raised lateral platforms with rounded outer ends, thick masonry drums, luminous cyan/green circular tops, and substantial stairs.
- The stair directions, landings, gold-edged cheeks, and changes in height visible in the references.
- The stepped temple approach, including a decorated central strip and flanking stair runs.
- Low parapet sections, rectangular stone blocks, short barriers, and smaller ornamental heads in their observed locations.

Keep circular patterns embedded in the larger court. Preserve the open lanes and sparse arrangement of obstacles. Maintain exposed edges where the screenshots show open space.

Treat visible blocks and luminous pads as visual features until the project establishes their gameplay role. Their appearance alone does not establish destructibility, healing, spawning, scoring, or launch behavior.

Use geometry for prominent raised borders and relief that affect the silhouette or close-view lighting. Use textures, masks, or decals for finer carvings. Keep surface detail controlled enough for the court to remain readable.

## 4. Signature architecture

### A. Monumental broken ring

Create a bespoke, vertically oriented ring/arch assembly with substantial depth. Its silhouette must follow the references.

Required visual features:

- Large, shaped stone segments with angular, stepped edges.
- Visible interruptions, including the upper gap shown in REF_02.
- Translucent violet energy spanning the appropriate gaps.
- Layered gold borders, inset panels, shallow reliefs, and small violet luminous inserts.
- Animal-head details at visible terminals, including the circular eye/profile at the upper-right terminal in REF_02.
- Portions extending below the arena floor into the surrounding void.

Preserve the gaps and uneven segmentation. Determine placement, orientation, and the number of assemblies from landmark correspondence. Avoid adding duplicate rings merely to fill unseen space.

### B. Giant serpent-head pylons

Model the oversized architectural heads and supports as major custom assets.

Match:

- Long projecting snouts and strongly shaped open jaws.
- Thick gold lips, jaw borders, angular upper planes, and dark mouth recesses.
- Small visible teeth and the long curled gold elements.
- Dark teal/slate stone bodies with layered gold ornament.
- Tall supports with recessed vertical channels and scroll-like relief.
- The scale of the heads relative to the arena and their framing in REF_02.

These forms must read correctly in profile as well as from the reference cameras. Use enough geometric detail to reproduce their silhouette.

### C. Temple façades and ornaments

Reproduce the visible sun temple with:

- Broad sloping/triangular architecture and long gold-lined edges.
- Large inset panels combined with smaller masonry courses.
- A circular recess containing concentric rings and a pointed sun surround.
- A bright cream/yellow sun core.
- A horizontal lower terrace, broad approach, and slender gold-trimmed ornaments.

Reproduce the narrow shrine visible in REF_02 with its distinct proportions, elongated gold ribs, and vertical violet accent.

Add the narrow violet-lit pedestals, small architectural head motifs, and repeated trim elements where supported by the images. Keep speculative connections separate from confidently reconstructed features.

## 5. Cliffs, islands, foliage, and distant architecture

The surrounding world is a major part of the visual target. Build depth around the arena.

Include:

- Tall jungle-covered mountains and steep, layered cliff faces.
- Open chasms and a visible drop beneath the arena.
- Floating islands at multiple distances and elevations.
- Broad, vegetated island tops and broken, tapered rock undersides.
- Hanging roots and smaller detached fragments where visible.
- Cyan waterfalls descending from cliffs and islands into mist.
- The prominent floating island and waterfall composition in REF_03.
- Distant stepped pyramids, including the open gold summit symbol visible at the left of REF_02.
- Progressively softer, paler mountain and temple silhouettes toward the horizon.

Use mesh-based landforms for undercuts, floating undersides, and overhangs. A terrain heightfield alone cannot describe these shapes.

Model nearby and middle-distance silhouettes in 3D so they hold up from player viewpoints. Reserve cards or simplified background layers for sufficiently distant scenery.

Build trees as dense, rounded, spreading canopy masses with readable light and dark volumes. Match the stylized jungle appearance. Concentrate vegetation on islands, cliffs, outer ledges, and sparse floor seams while keeping the court clear.

Use deliberate clusters and variations. Repetition should support the reference composition rather than create visibly identical islands or mechanically uniform forests.

## 6. Materials and surface style

Target stylized game art with substantial forms, clean bevels, broad color variation, restrained weathering, and clear material separation.

| Surface | Target appearance |
|---|---|
| Floor | Warm tan/sandstone, dark joints, limited cracks |
| Main masonry | Muted teal, green-grey, olive-grey, and slate-blue |
| Insets and shaded panels | Desaturated blue and violet |
| Gold | Saturated yellow-gold, warm highlights, amber recesses |
| Vegetation | Yellow-green, emerald, and cool teal canopy masses |
| Waterfalls | Luminous turquoise/cyan |
| Energy | Violet/lavender with pale cores and transparent edges |
| Temple sun | Cream-white/yellow glow within shaped gold ornament |

Keep gold primarily reflective/gilded. Confine strong emission to the luminous features supported by the screenshots.

Maintain relatively complete, ornate architecture. Use restrained chips, moss, cracks, and grime. Avoid heavy rubble or pervasive erosion that changes the original visual hierarchy.

Create native Unreal master materials and instances for stone, gold/trim, foliage, emissive features, and waterfalls. Expose useful color, roughness, variation, moss, emission, and animation controls.

Rebuild procedural source materials in Unreal or bake their needed maps. Verify imported color-space, normal orientation, roughness, metallic, and opacity settings.

Use sensible UVs, shared trim sheets, tileable surfaces, and unique detail where it materially improves a landmark. Provide texture sources and material assignments. Do not bake screenshot lighting into every surface or use screenshot projections as the finished arena.

## 7. Lighting and effects

Match the warm, bright, open-air lighting while keeping shaded architecture readable.

Establish:

- Strong warm sunlight from a direction consistent with the scene references.
- Cool blue/violet ambient fill in shaded stone.
- Readable gold reflections and warm sandstone highlights.
- Strong atmospheric perspective across the gorge.
- Mist in waterfalls and distant low areas.
- Restrained bloom around the sun emblem, pads, and violet energy.

Keep the gameplay surface clear of dense fog. Preserve the bright sky and distant haze without washing out nearby material detail.

Use a coherent lighting setup across the scene. The references may have different exposures or capture conditions; document any camera-specific exposure settings rather than silently changing geometry or light direction for each comparison.

Create subtle looping waterfalls and violet-energy effects using appropriate native materials/effects. Preserve their silhouettes and transparency. Keep them visually distinct: cyan falling water and violet energy between architectural segments.

Separate presentation lighting, sky, fog, and post-processing into an identifiable group, sublevel, or project-compatible layer so the environment can integrate with an existing world lighting setup.

Use the target project’s renderer. Evaluate Nanite, Lumen, shadow, foliage, and translucency options against actual engine support and performance; do not treat enabling a feature as validation.

## 8. Mesh and scene organization

Deliver a practical modular kit:

- Floors, borders, stairs, walls, parapets, and blocks.
- Circular platforms and floor-medallion pieces.
- Ring segments and terminal details.
- Serpent heads, support columns, and architectural ornaments.
- Temple components.
- Cliff and island forms, rocks, roots, trees, and vegetation.
- Waterfall and energy components.

Use shared modules where repetition is visible and custom meshes where silhouette demands it.

Require consistent naming, intentional pivots, applied transforms, correct normals/tangents, appropriate UVs, and stable material slots. Verify the export/import axis conversion. Avoid accidental negative scale, inverted faces, duplicate coplanar surfaces, and visible seams.

Use instancing and distance simplification where appropriate. Optimize repeated and distant assets while retaining the defining shapes. Choose budgets from the target project and report measured results where tools permit.

Keep scene geometry editable and grouped logically. Source-generation and import scripts should be repeatable with stable names and deterministic seeds. Rerunning them must update owned assets without duplicating the whole scene or deleting unrelated project content.

## 9. Collision and gameplay integration

Preserve the project’s existing movement, input, camera, game modes, collision profiles, and gameplay logic. If the project uses Lyra, GAS, Experiences, or Game Features, follow that architecture without replacing it.

Author collision deliberately:

- Continuous walkable floor collision across module seams.
- Floor ornament that does not snag characters or cause unintended ball bounces.
- Correct blocking on structural walls, stairs, platforms, and visible obstacles.
- Open boundaries where the arena is visibly exposed.
- No unnecessary collision on distant scenery, energy ribbons, or purely decorative details.

Prefer efficient simple collision where suitable. Use more detailed static collision only where its behavior and cost are justified.

Test stairs with the actual project character and ball, when available. A smooth ramp collider changes rolling and bouncing behavior; assess that consequence explicitly. Use separate character/ball collision only if the existing project supports it.

If the actual ball is unavailable, use a clearly labeled test sphere and record its radius and physics settings. Its results are provisional.

Verify navigation if the project uses AI traversal. Keep navigation confined to intended reachable surfaces and confirm relevant stairs and connections.

Provide clearly named, editable arena-center and end-of-court reference anchors where useful for integration. Label inferred positions. Leave scoring, spawning, healing, destruction, and out-of-bounds responses to established gameplay systems unless explicitly requested.

## 10. Unreal delivery

Use the existing project’s content convention. Otherwise keep authored content under:

**/Game/Environments/Dorado/**

Organize maps, meshes, materials, textures, effects, and optional assembly assets beneath that root. Use a clear entry map such as **L_Dorado**. A content plugin is acceptable when it fits the project’s architecture; preserve the same self-contained dependency principle.

Deliver:

- A real `.umap` saved through Unreal Editor.
- Real `.uasset` meshes, textures, materials, and required effects.
- Editable modeling sources, such as `.blend` files.
- Exported interchange assets and texture sources.
- Import and assembly automation with required settings.
- The dimension/configuration file.
- A short integration guide, dependency list, and validation report.

Keep runtime operation independent of modeling tools and editor-only Python. Prefer simple static scene assembly unless a more complex system provides a clear benefit.

Include every required non-engine dependency or identify an existing project dependency explicitly. Keep presentation-only elements separable. Provide exact instructions for migrating or integrating the map and dependencies.

## 11. Visual matching and iteration

Create three saved Unreal comparison cameras:

- **CAM_Ref_01:** Elevated oblique arena view.
- **CAM_Ref_02:** Long-axis view framed by the foreground pylons.
- **CAM_Ref_03:** Exterior temple/island/sky view.

Match the corresponding image aspect ratio, framing, perspective, landmark placement, and approximate exposure. Record camera transforms, field of view, and render settings. Estimate lens parameters from the images; do not assume an original FOV.

Use one coherent environment across all three cameras. Also inspect ordinary player-height views throughout the arena.

Work in these stages:

1. Analyze references and record the provisional layout and assumptions.
2. Block out the floor, elevation changes, temples, pylons, ring, and major background masses.
3. Fit comparison cameras and correct large silhouette/proportion errors.
4. Replace blockout forms with finished modular and custom assets.
5. Add materials, ornament, vegetation, lighting, and effects.
6. Render the three reference views in Unreal and compare them side by side, using overlays where useful.
7. Fix the highest-impact remaining differences, then repeat the comparisons.
8. Validate traversal, dependencies, integration, and runtime cost.

Compare geometry and composition before spending time on microdetail. For each review, identify the largest remaining differences and the actual changes made to address them.

A finished scene should hold up between the reference cameras. Do not hide missing surfaces, incomplete geometry, or incorrect scale behind one carefully chosen view.

## 12. Completion and handoff

Before describing the result as ready to integrate:

- Save the map and assets, close them, and reopen them in the exact target editor.
- Confirm correct scale, materials, lighting, collision, and effects.
- Check for missing references, failed imports, shader failures, and required-plugin problems.
- Traverse the accessible arena at player height.
- Test floor seams, stairs, platform transitions, obstacles, and open edges.
- Test the actual ball where available and label approximations.
- Validate navigation if required.
- Test migration into a clean matching-version project for a standalone package, or the intended target project for a project-specific integration.
- Confirm dependencies are included or explicitly identified.
- Record performance with hardware, resolution, quality settings, and viewpoint where measurement is possible.
- Identify all validation steps that could not be run.

Deliver comparison renders from all three saved Unreal cameras, a top-down layout view, and representative player-height views.

The final report should state:

1. What was built and where to open it.
2. Exact tool and engine versions used.
3. How to integrate the environment.
4. Which areas closely match the screenshots.
5. Which dimensions or unseen areas were inferred.
6. Remaining visual differences and technical limitations.
7. Validation performed and any unresolved steps.

Describe completion accurately. A blockout, source-only model, unexecuted import script, or non-Unreal render is an intermediate result. Native Unreal assets and successful target-editor checks are required for the validated integration deliverable.

**Begin by inspecting the three references and available tools, then proceed through reconstruction and validation.**