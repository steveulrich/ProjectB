---
name: unreal-splines
description: "Build or diagnose Unreal 5.8 spline queries, spline meshes, placement, path motion, and coordinate-space or deformation errors."
---

# Unreal 5.8 Splines

## Ownership boundary

This skill owns path math, spline queries, spline-mesh deformation, distance travel, and rigid
placement along a path. Route terrain deformation/paint to
[`unreal-landscape`](../unreal-landscape/SKILL.md), procedural population to
[`unreal-pcg`](../unreal-pcg/SKILL.md), and streamed-cell/Data Layer/HLOD ownership to
[`unreal-world-partition`](../unreal-world-partition/SKILL.md).

## Select the system

Consult [`references/use-cases.md`](references/use-cases.md) when choosing a spline implementation pattern.

- Use `USplineComponent` for path data, transforms, distance queries, and movement.
- Use one `USplineMeshComponent` per segment to deform a mesh between two endpoints.
- Use Landscape Splines when the path must deform or paint Landscape terrain.
- Use PCG spline tools when the spline is an authoring input for procedural placement.
- Use ISM/HISM placement instead of spline meshes when repeated objects must remain rigid.

Do not substitute one system for another merely because each contains “Spline” in its
name.

## Execute

1. Classify the job: query path, move actor, deform continuous mesh, place rigid
   instances, edit Landscape, or drive PCG.
2. Declare coordinate space for every query. Prefer World for actor placement and
   Local when feeding start/end data into a Spline Mesh Component owned by the same
   actor.
3. Use **distance along spline** for constant-speed travel and uniform spacing. Do not
   assume spline point index or input key is proportional to physical distance.
4. Put editor-generated geometry in the Construction Script. Put runtime mutation in
   explicit runtime functions. Make both paths idempotent.
5. Batch point edits, then update the spline once. Avoid rebuilding components every
   tick.
6. For affected behavior or the selected acceptance gate, validate start, midpoint, end, tightest bend, closed-loop seam, reverse travel,
   collision, and cooked build behavior.

Consult [`references/action-patterns.md`](references/action-patterns.md) when an operation needs an exact implementation pattern.
Read [`references/troubleshooting.md`](references/troubleshooting.md) when output twists,
stretches, bunches, duplicates, or moves at inconsistent speed.

## Reporting

For a full design, include the applicable fields below. For a targeted fix, report the change, relevant verification, and remaining limitations; omit unchanged design details.

1. **System choice** and why.
2. **Actor/component layout**.
3. **Ordered Blueprint nodes or C++ calls**, including coordinate spaces.
4. **Variables to expose**, with units and safe bounds where known.
5. **Runtime/editor boundary**.
6. **Failure checks**.
7. **Verification procedure**.

Prefer node and property names used by UE 5.8. Mark experimental PCG features as
experimental. Do not invent editor paths, node names, defaults, or performance claims.

## Hard rules

- Treat a Spline Component as a path, not rendered geometry.
- Treat a Spline Mesh Component as a two-point deformation, not a multi-point path.
- Derive repeated counts and spacing from `Get Spline Length`.
- Scale motion by delta time.
- Preserve an explicit mesh forward axis and up-vector convention.
- Clean up previously generated components before regenerating them.
- Keep spline mutation authoritative in networked gameplay; replicate state or results,
  not divergent per-client construction.
- Profile dense spline meshes and collision. Prefer instancing for large rigid repeats.

See [`references/sources.md`](references/sources.md) for the UE 5.8 source trail.
