# Dorado environment validation

This is a reusable checklist for later rebuilds, with boxes intentionally reset. It does not track the current delivery's pass/fail state. Record new results and evidence under `Saved/Logs/`; source inspection alone cannot satisfy editor or runtime checks.

For the October 30 delivery, use the [validation report](ValidationReport.md) and [readiness coverage](ReadinessChecklist.md). They record completed checks, failed attempts and remedies, evidence limits, and unrun scopes. A partial recorded result does not automatically complete a broader checklist item.

## Source and native assets

- [ ] Record exact editor build, Blender/exporter version, source hashes, source seed, and configuration used.
- [ ] Verify the asymmetric calibration object's dimensions, axes, face orientation, and UVs in Unreal. Confirm 100 cm per meter and ProjectB scale 0.5.
- [ ] Save real native meshes, textures, materials, effects, and the entry map through Unreal.
- [ ] Check material slots, normals/tangents, UV channels, import colorspaces, normal-map orientation, roughness, metallic, and opacity.
- [ ] Check all imports and shader compilations for failures. Audit missing dependencies and required plugins.
- [ ] Rerun the owned assembly and confirm stable names, asset reuse, and no duplicated actors or unrelated changes.
- [ ] Save, close, and reopen the map and assets in the exact target editor. Recheck materials and references.

## Visual comparison

- [ ] Save and render `CAM_Ref_01` at 2048×918 and `CAM_Ref_02`/`CAM_Ref_03` at 1920×862. Record transforms, horizontal FOV, quality, and exposure.
- [ ] Compare each render against its supplied reference. Record the largest remaining discrepancies and the actual changes made between iterations.
- [ ] Confirm distinct broad sun and narrow-shrine ends, two giant serpent-wall pairs, four smaller edge-brace pairs, complementary ring segments, visible gaps, and violet bridging.
- [ ] Confirm central and end floor patterns remain embedded in continuous court geometry, with sparse cover and open lanes.
- [ ] Confirm lateral pad colors, stair directions, landings, retaining depth, exposed edges, and gold trim.
- [ ] Confirm the REF_03 island's overhang, tapered underside, subsidiary shelf, roots, cyan fall, and atmospheric depth.
- [ ] Inspect top-down and player-height views beyond the three reference cameras. Check hidden sides and reachable corners.
- [ ] Confirm waterfalls and violet energy loop visibly and remain distinct. Inspect translucency and bloom against bright sky.

## Gameplay preservation and traversal

- [ ] Compare the protected gameplay baseline and asset dependencies. Confirm no changes to movement, input, cameras, game modes, Experiences, collision profiles, or gameplay logic.
- [ ] Traverse every accessible floor seam, stair run, platform transition, obstacle route, and exposed boundary using the actual project character.
- [ ] Test the actual ball over floor ornament, stairs, landings, walls, and open edges. Record contact behavior and any changed collision approximation.
- [ ] If a test sphere substitutes for the ball, record radius, mass, collision, damping, restitution, and provisional scope explicitly.
- [ ] Confirm decorative trim and relief do not snag characters or change ball bounce. Distant scenery, water, and energy should have no unnecessary collision.
- [ ] Verify project navigation across intended surfaces and stairs while excluding scenery and void regions.
- [ ] Confirm integration anchors are editable and labeled; their positions must not silently assign gameplay roles.

## Delivery and measured cost

- [ ] Separate presentation lights, sky, fog, and post-processing so they can integrate with an existing world.
- [ ] Audit the full native dependency graph. Include each dependency or identify its existing project source explicitly.
- [ ] Validate the intended ProjectB integration. If claiming a standalone package, also test Unreal migration into a clean matching-version project.
- [ ] Record hardware, renderer, resolution, quality settings, viewpoint, frame time, and memory where tools permit. Distinguish measured results from estimates.
- [ ] Deliver editable modeling sources, interchange files, texture sources, stable scripts, central dimensions, all comparison renders, a top-down view, and player-height views.
- [ ] State remaining visual discrepancies and every unrun validation step. Do not upgrade source-only or structured evidence into UI, traversal, or target-editor acceptance.
