---
name: unreal-gas-attributes-effects
description: "Implement or diagnose Unreal 5.8 GAS AttributeSets, Gameplay Effects, calculations, stacking, replication, and attribute-change notifications."
---

# Unreal 5.8 GAS Attributes and Effects

## Select the representation

Consult [`references/attribute-model.md`](references/attribute-model.md) when changing attribute semantics, and
[`references/effect-selector.md`](references/effect-selector.md) when choosing or changing effect representation.

- Use an **Attribute** for replicated numeric gameplay state that participates in GAS
  calculations and temporary modifiers.
- Use an **Instant Gameplay Effect** for a permanent base-value change such as damage,
  healing, or initial values.
- Use a **Duration Gameplay Effect** for a timed modifier or periodic effect.
- Use an **Infinite Gameplay Effect** for a removable modifier with no automatic expiry.
- Use **SetByCaller** when runtime code supplies one named magnitude to an effect spec.
- Use a **Modifier Magnitude Calculation** for one reusable magnitude computation.
- Use a **Gameplay Effect Execution Calculation** for authoritative multi-attribute or
  multi-output calculations such as damage resolution.

## Execute

1. Define AttributeSets in native code and group attributes by coherent lifetime/ownership.
2. Initialize through an applied Gameplay Effect, not ad hoc direct writes spread across
   constructors and possession code.
3. Build a Gameplay Effect asset as immutable configuration; create a Gameplay Effect Spec
   for runtime level, context, captured values, and SetByCaller magnitudes.
4. Apply effects through an Ability System Component using correct source and target context.
5. Clamp invariants explicitly. The AttributeSet system has no automatic min/max clamp.
6. Replicate player-visible attributes and notify GAS correctly so prediction corrections
   and listeners remain valid.
7. Drive UI from ASC attribute-change delegates; do not poll every frame.
8. For affected behavior or the selected acceptance gate, test removal, expiry, stacking, respawn/reinitialization, prediction correction, and
   server/client values.

Read [`references/calculations-and-patterns.md`](references/calculations-and-patterns.md)
for damage, healing, regeneration, buff, debuff, and initialization patterns. Read
[`references/replication-ui.md`](references/replication-ui.md) for replication and UI.

## Reporting

For a full design, include the applicable fields below. For a targeted fix, report the change, relevant verification, and remaining limitations; omit unchanged design details.

1. **Attribute and AttributeSet layout**.
2. **Effect type and calculation method** with rationale.
3. **Ordered creation/application steps**, including source, target, context, level, and
   runtime magnitudes.
4. **Clamping and invariant locations**.
5. **Stacking, duration, periodic, and removal policy**.
6. **Authority, replication, UI notification, and verification**.

Do not invent Gameplay Effect settings, defaults, or prediction guarantees.

## Hard rules

- Gameplay Effect assets are definitions; do not mutate their shared defaults at runtime.
- Do not write temporary buffs directly into Attribute values.
- Do not rely on Data Table MinValue/MaxValue columns for clamping.
- Treat Instant effects as base-value changes; timed/infinite modifiers affect current
  values while active.
- Re-check critical invariants where final executed values are known; `PreAttributeChange`
  alone is not a universal final clamp.
- Keep gameplay authority on the server even when UI predicts or displays local changes.
- Do not poll attributes on Tick for UI.

Use [`references/troubleshooting.md`](references/troubleshooting.md) when values double,
fail to revert, exceed bounds, fail to replicate, or disagree across machines. See
[`references/sources.md`](references/sources.md) for the UE 5.8 source trail.
