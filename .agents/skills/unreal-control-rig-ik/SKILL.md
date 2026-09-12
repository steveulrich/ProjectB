---
name: unreal-control-rig-ik
description: "Retarget, solve, author, or repair Unreal 5.8 skeletal poses with IK Rig, IK Retargeter, or Control Rig; diagnose transfer and contact defects."
---

# Unreal 5.8 Control Rig and IK

## Ownership boundary

This skill owns skeleton transfer, IK Retargeter setup, retarget poses/operations, procedural solves,
contact correction, and animation baking. If a retargeted preview/export is correct but the final pose
is wrong, route to [`unreal-animation-blending`](../unreal-animation-blending/SKILL.md). Route capsule
movement, root-motion authority, and network prediction to
[`unreal-character-movement`](../unreal-character-movement/SKILL.md).

## Route before editing

Consult [`references/system-selector.md`](references/system-selector.md) when choosing or changing the mechanism.

- Transfer animation between skeletons -> IK Rig + IK Retargeter.
- Retarget live from another component -> Retarget Pose From Mesh.
- Procedurally solve a connected body -> Full Body IK.
- Adjust a few limbs at runtime -> IK Rig node or focused IK nodes.
- Author or repair poses/animation -> Control Rig or FK Control Rig in Sequencer.
- Pin contacts after the base transfer is correct -> goals, Speed Planting, or foot placement.

Do not add IK until the source animation, skeleton assignment, chain map, retarget pose,
pelvis, and root behavior have been checked.

## Diagnose first when the request is a bug

Read [`references/symptom-diagnostics.md`](references/symptom-diagnostics.md), then reproduce
with one source sequence and one target mesh. Record whether the defect appears in the IK
Retargeter preview, an exported sequence, runtime retargeting, or only the final AnimGraph.

## Execute

1. Classify the job and isolate the first stage where the pose becomes wrong.
2. For affected behavior or the selected acceptance gate, verify source animation, source/target meshes, skeletons, reference poses, and root bones.
3. Define matching retarget roots and chains; inspect every chain mapping.
4. Create and align a dedicated retarget pose.
5. For affected behavior or the selected acceptance gate, validate the operation stack from top to bottom, enabling one correction at a time.
6. Add goals/solvers only for contacts or procedural controls that need them.
7. For affected behavior or the selected acceptance gate, verify root motion, curves, notifies, runtime component order, LOD, and hidden-mesh ticking.
8. For affected behavior or the selected acceptance gate, test representative clips at speed; profile and bake only after the live result is correct.

Load only what applies:

- [`references/ik-retargeter-setup.md`](references/ik-retargeter-setup.md)
- [`references/poses-chains-and-operations.md`](references/poses-chains-and-operations.md)
- [`references/root-pelvis-and-contact.md`](references/root-pelvis-and-contact.md)
- [`references/runtime-retargeting.md`](references/runtime-retargeting.md)
- [`references/control-rig-and-fbik.md`](references/control-rig-and-fbik.md)
- [`references/runtime-foot-placement.md`](references/runtime-foot-placement.md)
- [`references/sequencer-and-baking.md`](references/sequencer-and-baking.md)

## Reporting

For a full design, include the applicable fields below. For a targeted fix, report the change, relevant verification, and remaining limitations; omit unchanged design details.

1. **First broken stage** and evidence.
2. **Chosen system** and rejected alternatives.
3. **Required assets, roots, chains, goals, solvers, and pose assumptions**.
4. **Exact editor/runtime node order and settings to inspect or change**.
5. **One-change-at-a-time diagnostic procedure**.
6. **Root-motion, curve, notify, networking, LOD, and performance consequences**.
7. **Acceptance clips and debug checks**.

## Hard rules

- Set a retarget root on both IK Rigs, normally the pelvis/hips for bipeds.
- Chain names may differ, but source and target chain intent and boundaries must match.
- Create a new retarget pose for alignment; preserve the default pose as a baseline.
- A Retarget Pose operation belongs first in the operation stack.
- Prefer no FK translation for most chains; change translation mode only for a diagnosed need.
- An IK operation cannot move a chain unless the target chain has an IK goal.
- Fix chains, pose, pelvis, root, and speed before masking foot slide with Speed Planting.
- `Ignore Root Lock in Preview` is preview-only; export behavior uses export root-lock settings.
- A hidden runtime source mesh must still tick pose and refresh bones.
- Inertialization normally belongs before final IK; runtime IK belongs after the base pose exists.
- Treat Modular Control Rig according to its Experimental UE 5.8 maturity; do not silently make
  it a shipping dependency.
- Test multiple clips, not only the editor reference pose or one idle.

See [`references/sources.md`](references/sources.md) for the UE 5.8 source trail.
