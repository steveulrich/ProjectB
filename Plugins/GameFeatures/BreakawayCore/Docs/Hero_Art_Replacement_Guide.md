# Hero Art Replacement Guide

This guide explains how to find, import, and swap Breakaway hero meshes so replacements stay **data-driven** and do not require C++ or ability rewires.

## Goals

- One stable slot per hero: mesh + AnimBP referenced only from `UBwayHeroDataAsset`
- Compatible stubs swap via **reimport** with zero gameplay reference changes
- Incompatible skeletons require retarget + updating that hero’s data asset only

## Slot naming convention

Use stable content names that never change when art iterates:

| Slot | Example | Purpose |
|------|---------|---------|
| Gameplay mesh | `SKM_Alona_Gameplay` | What `HeroMesh` points at |
| Gameplay AnimBP | `ABP_Alona_Gameplay` | What `AnimationBP` points at |
| Source import | `SKM_Alona_Import` (optional) | Raw FBX import before cleanup |
| Materials | `MI_Alona_*` | Per-hero material instances |

Folder layout (recommended):

```
/Hero_<Name>/Characters/Heroes/
  DA_<Name>_Hero.uasset          (UBwayHeroDataAsset)
  Mesh/SKM_<Name>_Gameplay
  Anim/ABP_<Name>_Gameplay
  Materials/...
```

## Finding licensed stub assets

Prefer assets that already match (or retarget cleanly to) the project’s humanoid skeleton:

1. **Epic / Fab marketplace** — search “Manny”, “Quinn”, “UE5 Mannequin”, “game ready character”
2. **Lyra / ShooterCore mannequin** — fine for early stubs; replace before marketing shots
3. **Mixamo** — usable after IK Retargeter to UE5 mannequin; avoid shipping Mixamo T-poses without retarget
4. **MetaHuman** — high quality but heavy; use LODs and a gameplay mesh proxy for multiplayer

Always check license for commercial / multiplayer use.

## Import settings (FBX)

In Content Browser → Import:

- Skeleton: **reuse** the shared Breakaway / UE5 mannequin skeleton when possible
- Import morph targets: as needed
- Import materials / textures: yes for stubs, then replace with project MI instances
- Create physics asset: yes for ragdoll / hit reactions
- Generate lightmap UVs: optional for static props; less critical for characters

After import:

1. Rename to `SKM_<Hero>_Gameplay` (or reimport into that asset)
2. Confirm sockets required by the hero (`RequiredMeshSockets` on the DA)
3. Assign physics asset and verify ≥2 LODs if `bRequireMultipleLODs` is enabled

## Compatible replacement (same skeleton)

1. Open `SKM_<Hero>_Gameplay`
2. Reimport → select the new FBX
3. Keep the same asset path / name
4. Save — **do not** change `UBwayHeroDataAsset`

Gameplay, replication, and AnimBP keep working.

## Incompatible skeleton

1. Import FBX with its own skeleton (or create one)
2. Use **IK Retargeter** (UE5) from source → Breakaway/Manny skeleton
3. Export / save retargeted animations and mesh onto `SKM_<Hero>_Gameplay` / AnimBP
4. Or point the hero DA at a new mesh **and** AnimBP that share a skeleton
5. Set `ExpectedSkeleton` on the DA so `IsDataValid` fails on mismatches

## AnimBP assignment

- `AnimationBP` on `UBwayHeroDataAsset` must use a Generated Class compatible with `HeroMesh`’s skeleton
- Prefer a thin per-hero AnimBP that parents a shared locomotion layer (slide, slide-jump via `IsSlideJumping()`)
- After swap: PIE → select hero → confirm no T-pose, slide, and jump blend

## Mesh relative transform

Use `HeroMeshRelativeTransform` on the DA for scale / Z-offset without forking the pawn Blueprint. Applied in `ABwayCharacterWithAbilities::ApplyHeroVisuals`.

## Optional movement feel

Assign `MovementFeelConfig` (`UBwayMovementFeelConfig`) on the DA for per-hero slide-jump tuning. Applied with visuals.

## Validation checklist (editor)

On the hero data asset, enable what you need:

- `ExpectedSkeleton`
- `RequiredMeshSockets` (e.g. weapon, hand_r, VFX)
- `bRequirePhysicsAsset`
- `bRequireMultipleLODs`

Run **Data Validation** on the asset / folder before check-in.

## Primary action animation replacement

Alona, Korryn, and Argus expose primary-action animation through a Gameplay Cue Blueprint
derived from `BwayGameplayCueNotify_PrimaryAction`. Open the cue's class defaults and change
`PrimaryActionMontage` to a compatible final montage. `PrimaryActionPlayRate` controls the
playback multiplier; it combines with the montage's RateScale. These properties change
presentation, not attack cadence, hit detection, damage, or the ability's lifetime.

| Hero | Cue under `/BreakawayCore/GameplayCues/` | Current montage under `/BreakawayCore/Animations/` |
| --- | --- | --- |
| Alona | `GCN_BW_Weapon_AlonaPrimary_Fire` | `AM_BW_Alona_Primary_Placeholder` |
| Korryn | `GCN_BW_Weapon_KorrynPrimary_Fire` | `AM_BW_Korryn_Primary_Placeholder` |
| Argus | `GCN_BW_Weapon_ArgusPrimary_Attack` | `AM_BW_Argus_Primary_Placeholder` |

Keep the cue's registered tag and native parent. Preserve any existing Blueprint burst
effects or audio when replacing only the montage. Rawlins's primary presentation is a
separate pending audit and is not covered by this interface table.

For a compatible montage replacement:

1. Retarget the sequence to a skeleton compatible with the hero's current mesh and AnimBP.
   The current placeholders use the shared mannequin `MM_Pistol_Melee` sequence.
2. Preserve the evaluated `UpperBody` slot and its group, so locomotion can continue underneath.
   Use a nonadditive sequence unless the evaluated animation graph is deliberately changed
   and revalidated for an additive replacement.
3. Keep positive finite duration and playback rates, automatic blend-out, finite section
   links, and no root motion. The cosmetic playback helper rejects root-motion montages.
4. Keep gameplay damage, costs, and authoritative events in the ability implementation.
   The placeholder montage needs no gameplay notifies or weapon socket to perform its action.
   Optional final VFX attachments must use sockets documented on the hero data asset.
5. Verify normal and rapid attacks, attacks while moving, death during playback, and an attack
   after respawn on the owner and observing peers. The pawn owns replacement and cleanup of
   the exact primary montage instance; do not add competing playback calls to the ability.

Same-slot foreign montages can reject a primary action while their group is occupied. Check
the intended action priority when adding other animation systems. Replaceable assets do not
make arbitrary skeletons, slot graphs, root-motion policies, or gameplay notifies compatible.

## Multiplayer validation

After any mesh swap:

1. Listen-server PIE with 2 clients
2. Confirm mesh replicates for remote pawns (not only locally)
3. Confirm nameplates / damage numbers still attach to capsule / mesh
4. Confirm projectile spawn sockets if abilities use mesh sockets
5. Profile LOD / skeletal mesh cost at target player count

## Do / Don’t

| Do | Don’t |
|----|-------|
| Keep `SKM_*_Gameplay` / `ABP_*_Gameplay` names stable | Hardcode mesh paths in C++ |
| Put presentation in DAs | Duplicate hero experiences for art variants |
| Retarget into the shared skeleton when possible | Ship Mixamo skeletons without retarget |
| Validate sockets before enabling VFX attach | Assume AnimBP works with any skeleton |
