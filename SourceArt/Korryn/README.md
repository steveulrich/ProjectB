# Korryn (`Morgan` / `Hexweaver`) — Breakaway Source Art & UE 5.8 Restoration Handoff

## 1. Archive Provenance, Base-Mesh Blocker & Selected Source Assets

- **Unpacked Archive Root**: `F:\BreakawayConverted\Unpacked`
- **Canonical ProjectB Plugin & HeroDataAsset** (`AI_Planning/HERO_CODENAME_MAP.md`):
  - GameFeature Plugin: `Hero_Morgan` (`/Hero_Morgan/Characters/Heroes/Hexweaver/`)
  - HeroDataAsset: `/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver` (`DisplayName = "Korryn"`)
- **Archive Identity & GFactory Base-Mesh Finding** (`SourceArt/Korryn/Docs/texture_restoration_manifest.json`):
  - `Unpacked/localization/en-us/text.xml` maps `ui_char_name_morganlefay -> "Korryn"`, and audio/VFX files reference `korryn` and `korryn_featherfall1.cgf`.
  - **Exact Archive Base-Mesh Blocker**: Exhaustive inspection of all `57,987` unpacked archive files across all 16 character `.pak` archives (including all 6 `.skin` meshes in `Objects/Characters/Morgan/`: `new_morgan_opti_mesh.skin`, `morgan_opti_mesh.skin`, `morgan_mesh.skin`, `morgan.skin`, `morgan_cin_mesh.skin`, `morgan_multimesh.skin`, `cs_morgan.lyr`, and UI portraits `dashboard_portrait_lg_morganlefay.dds` / `skin_morganlefay_default.dds`) confirmed that this Lumberyard archive build predates GFactory's late-production antler/forehead-skull Korryn 3D model swap (`artstation.com/projects/lQnR5`).
  - Following the project's archive-blocker contingency rule, all usable archive Korryn/Morgan character and weapon pieces are fully restored, assembled, and integrated:
    - Body Skeletal Mesh: `Objects/Characters/Morgan/new_morgan_opti_mesh.skin` (`LOD0..LOD4`, `332` bones, submaterials `morgan_skin`, `morgan_main`, `morgan_transp`).
    - Default Held Equipment: `WeaponStaffMLF` (`Objects/Characters/Morgan/weapons/staff_base/staff_base_mesh.skin`, `LOD0..LOD3`), attached **left-handed** to `S_LAttachment` (`Visible="true"` in `morganlefay_skins.xml`).
    - Transient / Effect Pieces Excluded from Permanent Attachments: `Raven_Idle` and `Raven_Fly` on `bind_raven_attach` (`Visible="false"` by default, toggled only during navigation fidgets in `morganlefay_seqnav.lua`), and right-hand spell orbs / `bind_wp_vfx1` staff wisps / `korryn_featherfall1.cgf` (spawned as runtime particle effects in `MorganLeFayEntityEffects.xml`).

## 2. Recovered vs. Repaired vs. Generated Assets

- **Recovered directly from archive**:
  - `new_morgan_opti_mesh.skin` (`LOD0..LOD4`) and `staff_base_mesh.skin` (`LOD0..LOD3`) converted via `cgf-converter.exe` into `SourceArt/Korryn/Converted/{DAE,GLB}/`.
  - Split CryEngine DDS mip chains (`morgan_{diff,ddna,spec}` at `2048x2048` and `morgan_staff_base_standard_{diff,ddna,spec,emis}` at `1024x1024`).
- **Repaired during texture & rig preparation**:
  1. Recombined cross-pak split DDS mip chains and `.dds.*a` gloss alpha streams (`DDS-Unsplitter.exe --safe`), preserving the `2048x2048` RGBA cutout alpha channel in `T_BW_Korryn_BC.png` for `BLEND_Masked` (`OpacityMaskClipValue = 0.76`) raven-feather and lace trim (`morgan_transp`).
  2. Calibrated the flat `0.987` staff `_ddna` gloss channel using MTL `Shininess = 121/255` and specular reflectance so the wooden/bone staff shaft reads satin (`~0.55` roughness) while metalwork reads polished (`~0.28` roughness).
  3. Applied the MTL violet `Emittance` tint `(0.091, 0.001, 0.231)` to `T_BW_Korryn_Staff_E.png`.
- **Generated / Reconstructed**:
  - `T_BW_Korryn_E.png` (`2048x2048` luminous violet iris emissive map strictly masked to the 360 eyeball UV triangles at `U=[0.0009, 0.0481]`, `V=[0.8635, 0.9113]`).

## 3. Directory Layout & Deliverables

- `SourceArt/Korryn/Korryn_Restored.blend` — Clean, editable Blender 5.2 rest-pose scene (`1.0` scale, Z-up, meters) containing the `SKM_BW_Korryn` armature + `LOD0..LOD4` meshes, origin-pivoted `SM_BW_Korryn_Staff` `LOD0..LOD3` static meshes, and `SM_BW_Korryn_Staff_HandPreview` aligned to her left-hand grip.
- `SourceArt/Korryn/Exports/`:
  - `SKM_BW_Korryn_LOD0.fbx` .. `SKM_BW_Korryn_LOD4.fbx` (and `SKM_BW_Korryn.fbx`)
  - `SM_BW_Korryn_Staff_LOD0.fbx` .. `SM_BW_Korryn_Staff_LOD3.fbx`
- `SourceArt/Korryn/Textures/Prepared/`:
  - Character (`2048x2048`): `T_BW_Korryn_BC.png` (sRGB RGBA cutout), `T_BW_Korryn_N.png` (Linear `TC_Normalmap`), `T_BW_Korryn_ORM.png` (Linear `TC_Masks`), `T_BW_Korryn_E.png` (sRGB)
  - Staff (`1024x1024`): `T_BW_Korryn_Staff_BC.png` (sRGB), `T_BW_Korryn_Staff_N.png` (Linear `TC_Normalmap`), `T_BW_Korryn_Staff_ORM.png` (Linear `TC_Masks`), `T_BW_Korryn_Staff_E.png` (sRGB)
- `SourceArt/Korryn/Renders/`:
  - `korryn_front.png`, `korryn_back.png`, `korryn_equipment_closeup.png`, `korryn_casting_pose_check.png`, `korryn_gameplay_distance_check.png`

## 4. UE 5.8 Asset Mapping, Sockets & Floor-Contact Calibration

- **Plugin Content Root**: `/Hero_Morgan/Characters/Heroes/Hexweaver/`
  - Skeletal Mesh & Skeleton: `Models/SKM_BW_Korryn` (`5` LODs), `Models/SKM_BW_Korryn_Skeleton`, `Models/SKM_BW_Korryn_PhysicsAsset`
  - Equipment Static Mesh: `Models/SM_BW_Korryn_Staff` (`4` LODs)
  - Self-Contained Materials:
    - Master Materials (`bUsedWithSkeletalMesh = true`, `ORMTexture` default bound to `TC_Masks` `T_BW_Korryn_ORM`):
      - `Materials/M_BW_Korryn_PBR` (`BLEND_Opaque`)
      - `Materials/M_BW_Korryn_TwoSided_PBR` (`BLEND_Opaque`, `TwoSided`)
      - `Materials/M_BW_Korryn_Masked_PBR` (`BLEND_Masked`, `TwoSided`, `OpacityMaskClipValue = 0.76`)
    - Material Instances & Slot Mapping:
      - Slot `morgan_skin` -> `Materials/MI_BW_Korryn_Skin`
      - Slot `morgan_main` -> `Materials/MI_BW_Korryn_Main` (`TwoSided`)
      - Slot `morgan_transp` -> `Materials/MI_BW_Korryn_Feathers` (`Masked`, `TwoSided`)
      - Weapon `SM_BW_Korryn_Staff` Slot `0` -> `Materials/MI_BW_Korryn_Staff`
  - Animation & Retargeting:
    - `Animations/Rig/IK_BW_Korryn`, `Animations/Rig/IK_Mannequin_For_Korryn`, `Animations/Rig/RTG_Mannequin_To_Korryn`, `Animations/Rig/RTG_Alona_To_Korryn`
    - `Animations/ABP_BW_Korryn` (retargeted from `ABP_BW_Alona` locomotion + `UpperBody` slot graph)
    - `Animations/AM_BW_Korryn_Primary` (`Animations/A_BW_Korryn_Primary`, retargeted from `AM_BW_Korryn_Primary_Placeholder`)
- **Capsule Floor-Contact Offset & Sockets**:
  - Because `new_morgan_opti_mesh.skin`'s rest-pose boot soles sit `+11.01 cm` above armature origin `Z=0` (`char_z_min_m = 0.1101 m`), `HeroMeshRelativeTransform` on `DA_BW_HeroData_Hexweaver` uses Translation **`(0.0, 0.0, -106.5)`** (`-96.0 cm` capsule half-height `- 10.5 cm` sole offset), Rotation `(Pitch=0.0, Yaw=-90.0, Roll=0.0)`, Scale `(1.0, 1.0, 1.0)`. Verified animated Idle foot height in `L_BW_Dorado`: `bind_left_ball = 2.09 cm`, `bind_right_ball = 3.28 cm` above floor (matching Alona's `1.16 / 2.96 cm`).
  - Sockets on `SKM_BW_Korryn_Skeleton` (`0.01` local scale compensating CryEngine's `100x` `S_XForm` root bone scale -> exact `(1.0, 1.0, 1.0)` world scale):
    - `weapon_l` (parent bone `S_LAttachment`): Left-hand primary equipment socket for `SM_BW_Korryn_Staff`.
    - `relic_r` (parent bone `bind_right_wrist`): Relic carry socket; `SetEquipmentHiddenForRelicCarry(true)` hides `SM_BW_Korryn_Staff` while carrying the Relic and restores it on drop/pass/throw/score.

## 5. Repeatable Rebuild & Reimport Commands

```powershell
# 1. Prepare PBR PNG textures from unpacked DDS streams
python "E:\Unreal Projects\ProjectB\SourceArt\Korryn\Scripts\prepare_korryn_textures.py"

# 2. Rebuild Korryn_Restored.blend, export LOD FBXs, and render studio/action PNGs
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --python "E:\Unreal Projects\ProjectB\SourceArt\Korryn\Scripts\assemble_and_export_korryn.py"

# 3. Inside ProjectB UnrealEditor Python (idempotent import, material build, IK retarget, socket calibration & HeroData validation)
import importlib, Scripts.Editor.ImportRestoredHeroes as H
importlib.reload(H)
H.run_hero("Korryn")
```

## 6. Verification Evidence

- **DataValidation & Cold-Load Audit**: `Saved/Logs/hero-restoration-20261031/cold_load_validation.json` (`DataValidationResult.VALID`, `0` errors, `0` warnings, `0` cross-hero GFP references).
- **4-Player Listen-Server PIE (`L_BW_HeroSelect_Staging` -> `L_BW_Dorado`)**:
  - Consolidated Report: `Saved/Logs/hero-restoration-20261031/pie_e2e_verification.json`
  - Screenshots:
    - `Saved/Logs/PresentationCaptures/hero-restore-select04/player262-select-korryn.png`
    - `Saved/Logs/PresentationCaptures/hero-restore-dorado04/player262-dorado-idle-korryn.png`
    - `Saved/Logs/PresentationCaptures/hero-restore-action03/player262-action-korryn.png`
    - `Saved/Logs/PresentationCaptures/hero-restore-relic03/player262-relic-carry-korryn.png`
- **Headless Automation & Windows Cook**:
  - `Saved/Logs/hero-restoration-20261031/auto-hero-relic01.json` (`7/7` passed, `0` errors/warnings)
  - `Saved/Logs/hero-restoration-20261031/cook_check01.json` (`exitCode: 0`, `5,097` packages cooked, `0` hero warnings/errors)
