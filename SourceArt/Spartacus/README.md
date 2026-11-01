# Spartacus (`Magnus` / `Argus`) — Breakaway Source Art & UE 5.8 Restoration Handoff

## 1. Archive Provenance, Naming Invariant & Selected Source Assets

- **Unpacked Archive Root**: `F:\BreakawayConverted\Unpacked`
- **Canonical ProjectB Plugin & HeroDataAsset** (`AI_Planning/HERO_CODENAME_MAP.md` & `BwayGoal5UIPolishTests.cpp`):
  - GameFeature Plugin: `Hero_Spartacus` (`/Hero_Spartacus/Characters/Heroes/Argus/`)
  - HeroDataAsset: `/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus`
  - **Preserved DisplayName Contract**: `DisplayName` on `DA_BW_HeroData_Argus` is intentionally preserved as **`"Argus"`** per `AI_Planning/HERO_CODENAME_MAP.md` (line 18) and `Breakaway.HeroSelection.RosterAndClasses` (`BwayGoal5UIPolishTests.cpp`), while wiring the restored original Breakaway Spartacus gladiator skeletal mesh, materials, animations, and dual swords.
- **Selected Lumberyard Character & Dual-Weapon Definitions**:
  - Character Definition: `Objects/Characters/Spartacus/spartacus.cdf` + `weapons_magnus.xml` (`WeaponDefinition="magCommSwords"`).
  - Body Skeletal Mesh: `Objects/Characters/Spartacus/spartacus_opti_mesh.skin` (`LOD0..LOD3`, `21,291` vertices, `216` bones, submaterials `spart_main` and `spart_skin`).
  - Dual Gladiator Swords (`magnus_skins.xml`):
    - **Right-Hand Primary Sword (`WeaponSpartacusSword2`)**: `Objects/Characters/Spartacus/sword2_mesh.skin` (`LOD0..LOD3`, `1,655` vertices) attached to `S_RAttachment` (`weapon_r`).
    - **Left-Hand Secondary Sword (`WeaponSpartacusSword1`)**: `Objects/Characters/Spartacus/spartacus_sword_mesh.skin` (`LOD0..LOD3`, `929` vertices) attached to `S_LAttachment` (`weapon_l`).

## 2. Recovered vs. Repaired vs. Reconstructed Assets

- **Recovered directly from archive**:
  - `spartacus_opti_mesh.skin` (`LOD0..LOD3`), `spartacus_sword_mesh.skin` (`LOD0..LOD3`), and `sword2_mesh.skin` (`LOD0..LOD3`), converted via `cgf-converter.exe` into `SourceArt/Spartacus/Converted/{DAE,GLB}/`.
  - Original in-engine Spartacus default UI portrait `UI/3/icons/skin_magnus_default.dds` preserving the authentic gladiator palette (bronze Corinthian crested helm, crimson lion-head left pauldron, segmented steel/bronze arm guard, dark leather belt/skirt, linen wraps, bronze greaves).
- **Missing Archive Textures & 3D Cycles UV-Baked PBR Reconstruction** (`SourceArt/Spartacus/Docs/texture_restoration_manifest.json`):
  - Exhaustive search across all `57,987` archive files confirmed `spartacus_{diff,ddna,spec}.tif`, `sparsword1_*`, and `sparsword2_*` were removed when `magnus_textures` migrated to `argus_*.dds` (which have incompatible UV layouts for `spartacus_opti_mesh.skin` and the dual swords).
  - Reconstructed complete `2048x2048` character PBR maps (`T_BW_Spartacus_{BC,N,ORM}.png`) and `1024x1024` dual-sword PBR maps (`T_BW_Spartacus_Sword1_{BC,N,ORM}.png`, `T_BW_Spartacus_Sword2_{BC,N,ORM}.png`) via 3D Blender Cycles Ambient Occlusion + Pointiness curvature + tangent normal baking (`SourceArt/Spartacus/Scripts/bake_spartacus_3d_maps.py` and `prepare_spartacus_textures.py`) directly onto the exact UV islands of `spartacus_opti_mesh.dae` (`14` classified material/anatomy triangle groups), `spartacus_sword_mesh.dae`, and `sword2_mesh.dae`, matched to `UI/3/icons/skin_magnus_default.dds` and Simon Loche's original gladiator design (`artstation.com/artwork/rLPmL`).

## 3. Directory Layout & Deliverables

- `SourceArt/Spartacus/Spartacus_Restored.blend` — Clean, editable Blender 5.2 rest-pose scene (`1.0` scale, Z-up, meters) containing the `SKM_BW_Spartacus` armature + `LOD0..LOD3` meshes, origin-pivoted `SM_BW_Spartacus_SwordR` (`LOD0..LOD3`) and `SM_BW_Spartacus_SwordL` (`LOD0..LOD3`) static meshes, and `SM_BW_Spartacus_SwordR_HandPreview` + `SM_BW_Spartacus_SwordL_HandPreview` aligned to both hand grips.
- `SourceArt/Spartacus/Exports/`:
  - `SKM_BW_Spartacus_LOD0.fbx` .. `SKM_BW_Spartacus_LOD3.fbx` (and `SKM_BW_Spartacus.fbx`)
  - `SM_BW_Spartacus_SwordR_LOD0.fbx` .. `SM_BW_Spartacus_SwordR_LOD3.fbx`
  - `SM_BW_Spartacus_SwordL_LOD0.fbx` .. `SM_BW_Spartacus_SwordL_LOD3.fbx`
- `SourceArt/Spartacus/Textures/Prepared/`:
  - Character (`2048x2048`): `T_BW_Spartacus_BC.png` (sRGB), `T_BW_Spartacus_N.png` (Linear `TC_Normalmap`), `T_BW_Spartacus_ORM.png` (Linear `TC_Masks`)
  - Left Sword (`1024x1024`): `T_BW_Spartacus_Sword1_BC.png` (sRGB), `T_BW_Spartacus_Sword1_N.png` (Linear `TC_Normalmap`), `T_BW_Spartacus_Sword1_ORM.png` (Linear `TC_Masks`)
  - Right Sword (`1024x1024`): `T_BW_Spartacus_Sword2_BC.png` (sRGB), `T_BW_Spartacus_Sword2_N.png` (Linear `TC_Normalmap`), `T_BW_Spartacus_Sword2_ORM.png` (Linear `TC_Masks`)
- `SourceArt/Spartacus/Renders/`:
  - `spartacus_front.png`, `spartacus_back.png`, `spartacus_equipment_closeup.png`, `spartacus_attack_pose_check.png`, `spartacus_recovery_pose_check.png`

## 4. UE 5.8 Asset Mapping, Dual-Equipment C++ Support & Sockets

- **C++ Dual-Equipment & Relic-Carry Support (`BreakawayCoreRuntime`)**:
  - Extended `UBwayHeroDataAsset`, `ABwayCharacterWithAbilities`, `ABwayHeroPreviewActor`, and `ARelicActor` with `HeroSecondaryEquipmentMesh`, `HeroSecondaryEquipmentSocket`, `HeroSecondaryEquipmentRelativeTransform`, and `SetEquipmentHiddenForRelicCarry(bool)` so both `HeroEquipmentComponent` (`SM_BW_Spartacus_SwordR`) and `HeroSecondaryEquipmentComponent` (`SM_BW_Spartacus_SwordL`) spawn in Hero Select previews and gameplay pawns, clear cleanly when switching heroes, hide simultaneously on authority and replicated clients during Relic carry, and restore immediately on Relic drop/pass/throw/score.
- **Plugin Content Root**: `/Hero_Spartacus/Characters/Heroes/Argus/`
  - Skeletal Mesh & Skeleton: `Models/SKM_BW_Spartacus` (`4` LODs), `Models/SKM_BW_Spartacus_Skeleton`, `Models/SKM_BW_Spartacus_PhysicsAsset`
  - Equipment Static Meshes: `Models/SM_BW_Spartacus_SwordR` (`4` LODs), `Models/SM_BW_Spartacus_SwordL` (`4` LODs)
  - Self-Contained Materials:
    - Master Materials (`bUsedWithSkeletalMesh = true`, `ORMTexture` default bound to `TC_Masks` `T_BW_Spartacus_ORM`): `Materials/M_BW_Spartacus_PBR` (`BLEND_Opaque`), `Materials/M_BW_Spartacus_TwoSided_PBR` (`BLEND_Opaque`, `TwoSided`)
    - Material Instances & Slot Mapping:
      - Slot `spart_main` -> `Materials/MI_BW_Spartacus_Main` (`TwoSided`)
      - Slot `spart_skin` -> `Materials/MI_BW_Spartacus_Skin`
      - Weapon `SM_BW_Spartacus_SwordR` Slot `0` -> `Materials/MI_BW_Spartacus_SwordR`
      - Weapon `SM_BW_Spartacus_SwordL` Slot `0` -> `Materials/MI_BW_Spartacus_SwordL`
  - Animation & Retargeting:
    - `Animations/Rig/IK_BW_Spartacus`, `Animations/Rig/IK_Mannequin_For_Spartacus`, `Animations/Rig/RTG_Mannequin_To_Spartacus`, `Animations/Rig/RTG_Alona_To_Spartacus`
    - `Animations/ABP_BW_Spartacus` (retargeted from `ABP_BW_Alona` locomotion + `UpperBody` slot graph)
    - `Animations/AM_BW_Spartacus_Primary` (`Animations/A_BW_Spartacus_Primary`, retargeted from `AM_BW_Argus_Primary_Placeholder`)
- **Capsule & Socket Calibration**:
  - `HeroMeshRelativeTransform`: Translation `(0.0, 0.0, -96.0)`, Rotation `(Pitch=0.0, Yaw=-90.0, Roll=0.0)`, Scale `(1.0, 1.0, 1.0)` (verified animated Idle foot height in `L_BW_Dorado`: `bind_left_ball = 5.47 cm`, `bind_right_ball = 5.71 cm` above floor).
  - Sockets on `SKM_BW_Spartacus_Skeleton` (`0.01` local scale compensating CryEngine's `100x` `S_XForm` root bone scale -> exact `(1.0, 1.0, 1.0)` world scale):
    - `weapon_r` (parent bone `S_RAttachment`): Right-hand primary sword socket (`SM_BW_Spartacus_SwordR`).
    - `weapon_l` (parent bone `S_LAttachment`): Left-hand secondary sword socket (`SM_BW_Spartacus_SwordL`).
    - `relic_r` (parent bone `bind_right_wrist`): Relic carry socket (hides both swords during Relic carry and restores both on drop).

## 5. Repeatable Rebuild & Reimport Commands

```powershell
# 1. Bake 3D Cycles AO/curvature/normal maps and prepare PBR PNG textures
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --python "E:\Unreal Projects\ProjectB\SourceArt\Spartacus\Scripts\bake_spartacus_3d_maps.py"
python "E:\Unreal Projects\ProjectB\SourceArt\Spartacus\Scripts\prepare_spartacus_textures.py"

# 2. Rebuild Spartacus_Restored.blend, export LOD FBXs, and render studio/action PNGs
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --python "E:\Unreal Projects\ProjectB\SourceArt\Spartacus\Scripts\assemble_and_export_spartacus.py"

# 3. Inside ProjectB UnrealEditor Python (idempotent import, material build, IK retarget, socket calibration & HeroData validation)
import importlib, Scripts.Editor.ImportRestoredHeroes as H
importlib.reload(H)
H.run_hero("Spartacus")
```

## 6. Verification Evidence

- **DataValidation & Cold-Load Audit**: `Saved/Logs/hero-restoration-20261031/cold_load_validation.json` (`DataValidationResult.VALID`, `0` errors, `0` warnings, `0` cross-hero GFP references).
- **4-Player Listen-Server PIE (`L_BW_HeroSelect_Staging` -> `L_BW_Dorado`)**:
  - Consolidated Report: `Saved/Logs/hero-restoration-20261031/pie_e2e_verification.json`
  - Screenshots:
    - `Saved/Logs/PresentationCaptures/hero-restore-select04/player260-select-spartacus.png`
    - `Saved/Logs/PresentationCaptures/hero-restore-dorado04/player260-dorado-idle-spartacus.png`
    - `Saved/Logs/PresentationCaptures/hero-restore-action03/player260-action-spartacus.png`
    - `Saved/Logs/PresentationCaptures/hero-restore-relic03/player260-relic-carry-spartacus.png`
- **Headless Automation & Windows Cook**:
  - `Saved/Logs/hero-restoration-20261031/auto-hero-relic01.json` (`7/7` passed, `0` errors/warnings, including `Breakaway.HeroSelection.RosterAndClasses`)
  - `Saved/Logs/hero-restoration-20261031/cook_check01.json` (`exitCode: 0`, `5,097` packages cooked, `0` hero warnings/errors)
