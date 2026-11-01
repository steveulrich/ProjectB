# Rawlins (`Gunslinger`) — Breakaway Source Art & UE 5.8 Restoration Handoff

## 1. Archive Provenance & Selected Source Assets

- **Unpacked Archive Root**: `F:\BreakawayConverted\Unpacked`
- **Canonical ProjectB Plugin & HeroDataAsset** (`AI_Planning/HERO_CODENAME_MAP.md`):
  - GameFeature Plugin: `Hero_Rawlins` (`/Hero_Rawlins/Characters/Heroes/Gunslinger/`)
  - HeroDataAsset: `/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger` (`DisplayName = "Rawlins"`)
- **Selected Lumberyard Character & Weapon Definitions**:
  - Character Definition: `Objects/Characters/Rawlins/new_rawlins.cdf` (`rawlins_skins.xml`, `Skin="Default"`).
  - Body Skeletal Mesh: `Objects/Characters/Rawlins/new_rawlins_opti_mesh.skin` (`LOD0..LOD3`, `20,919` vertices, `223` bones, submaterials `main_mat`, `cape_mat`, `ammo_mat`).
  - Default Held Equipment: `Weapon_RWL_TombBreaker` (`Objects/Characters/Rawlins/weapons/new_rawlins_base_weapon_mesh.skin`, `LOD0..LOD3`, `2,123` vertices), attached right-handed to `S_RAttachment` (`Pistol_Left` on `S_LAttachment` is `Visible="false"` by default in `rawlins_skins.xml` and only toggled transiently during dual-pistol abilities).
  - *Why late-alpha `new_rawlins.cdf` was selected over early `rawlins.cdf`*: `rawlins_skins.xml` routes all playable skins to `new_rawlins.cdf` + `Weapon_RWL_TombBreaker` (matching GFactory's Breakaway Rawlins weapon portfolio `wJowLV`) and retains complete `2048x2048` / `1024x1024` split DDS texture chains on disk, whereas early `rawlins_diff.tif` and `python_diff.tif` were stripped from the shipped archive.

## 2. Recovered vs. Repaired vs. Generated Assets

- **Recovered directly from archive**:
  - `new_rawlins_opti_mesh.skin` (`LOD0..LOD3`) and `new_rawlins_base_weapon_mesh.skin` (`LOD0..LOD3`) converted via `cgf-converter.exe` into `SourceArt/Rawlins/Converted/{DAE,GLB}/`.
  - Split CryEngine DDS mip chains (`_diff`, `_ddna` + `.dds.*a` gloss alpha streams, `_spec`, `_emis`) for both Rawlins (`2048x2048`) and Tomb Breaker (`1024x1024`).
- **Repaired during texture & rig preparation** (`SourceArt/Rawlins/Docs/texture_restoration_manifest.json`):
  1. Recombined cross-pak split CryEngine DDS mip chains and `.dds.*a` gloss alpha streams (`DDS-Unsplitter.exe --safe`), patched DX10 typeless `BC1`/`BC3` headers, and reconstructed unit-length DirectX `Y-` tangent-space normal maps from `BC5` RG channels.
  2. Repaired `rawlins_default_matgroup.mtl`'s `cape_mat Diffuse="white.dds"` oversight by binding `MI_BW_Rawlins_Cape` (`TwoSided`) to `T_BW_Rawlins_BC` (matching `c1`/`c2`/`efox` skin MTLs whose poncho UVs share the main character atlas).
  3. Converted CryEngine Specular/Glossiness maps into UE 5.8 Metallic/Roughness PBR (`T_BW_Rawlins_ORM.png`, `T_BW_Rawlins_Gun_ORM.png`, `R=AO, G=Roughness, B=Metallic`, linear/non-sRGB) and masked out the unused green UV-filler quadrant on the Tomb Breaker revolver textures.
- **Generated / Reconstructed**: None required (`0` generated texture maps; 100% archive texture recovery).

## 3. Directory Layout & Deliverables

- `SourceArt/Rawlins/Rawlins_Restored.blend` — Clean, editable Blender 5.2 rest-pose scene (`1.0` scale, Z-up, meters) containing the `SKM_BW_Rawlins` armature + `LOD0..LOD3` meshes, origin-pivoted `SM_BW_Rawlins_TombBreaker` `LOD0..LOD3` static meshes, and `SM_BW_Rawlins_TombBreaker_HandPreview` aligned to the right-hand grip.
- `SourceArt/Rawlins/Exports/`:
  - `SKM_BW_Rawlins_LOD0.fbx` .. `SKM_BW_Rawlins_LOD3.fbx` (and `SKM_BW_Rawlins.fbx`)
  - `SM_BW_Rawlins_TombBreaker_LOD0.fbx` .. `SM_BW_Rawlins_TombBreaker_LOD3.fbx`
- `SourceArt/Rawlins/Textures/Prepared/`:
  - Character (`2048x2048`): `T_BW_Rawlins_BC.png` (sRGB), `T_BW_Rawlins_N.png` (Linear `TC_Normalmap`), `T_BW_Rawlins_ORM.png` (Linear `TC_Masks`), `T_BW_Rawlins_E.png` (sRGB)
  - Tomb Breaker (`1024x1024`): `T_BW_Rawlins_Gun_BC.png` (sRGB), `T_BW_Rawlins_Gun_N.png` (Linear `TC_Normalmap`), `T_BW_Rawlins_Gun_ORM.png` (Linear `TC_Masks`), `T_BW_Rawlins_Gun_E.png` (sRGB)
- `SourceArt/Rawlins/Renders/`:
  - `rawlins_front.png`, `rawlins_back.png`, `rawlins_equipment_closeup.png`, `rawlins_aiming_pose_check.png`, `rawlins_aiming_grip_closeup.png`

## 4. UE 5.8 Asset Mapping, Sockets & Transforms

- **Plugin Content Root**: `/Hero_Rawlins/Characters/Heroes/Gunslinger/`
  - Skeletal Mesh & Skeleton: `Models/SKM_BW_Rawlins` (`4` LODs), `Models/SKM_BW_Rawlins_Skeleton`, `Models/SKM_BW_Rawlins_PhysicsAsset`
  - Equipment Static Mesh: `Models/SM_BW_Rawlins_TombBreaker` (`4` LODs)
  - Self-Contained Materials:
    - Master Materials (`bUsedWithSkeletalMesh = true`, `ORMTexture` default bound to `TC_Masks` `T_BW_Rawlins_ORM`): `Materials/M_BW_Rawlins_PBR` (`BLEND_Opaque`), `Materials/M_BW_Rawlins_TwoSided_PBR` (`BLEND_Opaque`, `TwoSided`)
    - Material Instances & Slot Mapping:
      - Slot `main_mat` -> `Materials/MI_BW_Rawlins_Main`
      - Slot `cape_mat` -> `Materials/MI_BW_Rawlins_Cape` (`TwoSided`)
      - Slot `ammo_mat` -> `Materials/MI_BW_Rawlins_Ammo`
      - Weapon `SM_BW_Rawlins_TombBreaker` Slot `0` -> `Materials/MI_BW_Rawlins_TombBreaker`
  - Animation & Retargeting:
    - `Animations/Rig/IK_BW_Rawlins`, `Animations/Rig/IK_Mannequin_For_Rawlins`, `Animations/Rig/RTG_Mannequin_To_Rawlins`, `Animations/Rig/RTG_Alona_To_Rawlins`
    - `Animations/ABP_BW_Rawlins` (retargeted from `ABP_BW_Alona` locomotion + `UpperBody` slot graph)
    - `Animations/AM_BW_Rawlins_Primary` (`Animations/A_BW_Rawlins_Primary`, retargeted from `AM_BW_Rawlins_Primary_Placeholder`)
- **Capsule & Socket Calibration**:
  - `HeroMeshRelativeTransform`: Translation `(0.0, 0.0, -96.0)`, Rotation `(Pitch=0.0, Yaw=-90.0, Roll=0.0)`, Scale `(1.0, 1.0, 1.0)` (verified animated Idle foot height in `L_BW_Dorado`: `bind_left_ball = 4.42 cm`, `bind_right_ball = 4.79 cm` above floor).
  - Sockets on `SKM_BW_Rawlins_Skeleton` (with `RelativeScale = (0.01, 0.01, 0.01)` compensating CryEngine's `100x` `S_XForm` root bone scale so world attachment scale is exact `(1.0, 1.0, 1.0)`):
    - `weapon_r` (parent bone `S_RAttachment`): Primary equipment socket for `SM_BW_Rawlins_TombBreaker` (`HeroEquipmentRelativeTransform = Identity`).
    - `relic_r` (parent bone `bind_right_wrist`): Relic carry attachment socket; `ABwayCharacterWithAbilities::SetEquipmentHiddenForRelicCarry(true)` hides `SM_BW_Rawlins_TombBreaker` while carrying the Relic and restores it on drop/pass/throw/score.

## 5. Repeatable Rebuild & Reimport Commands

```powershell
# 1. Prepare PBR PNG textures from unpacked DDS streams
python "E:\Unreal Projects\ProjectB\SourceArt\Rawlins\Scripts\prepare_rawlins_textures.py"

# 2. Rebuild Rawlins_Restored.blend, export LOD FBXs, and render studio/action PNGs
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" --background --python "E:\Unreal Projects\ProjectB\SourceArt\Rawlins\Scripts\assemble_and_export_rawlins.py"

# 3. Inside ProjectB UnrealEditor Python (idempotent import, material build, IK retarget, socket calibration & HeroData validation)
import importlib, Scripts.Editor.ImportRestoredHeroes as H
importlib.reload(H)
H.run_hero("Rawlins")
```

## 6. Verification Evidence

- **DataValidation & Cold-Load Audit**: `Saved/Logs/hero-restoration-20261031/cold_load_validation.json` (`DataValidationResult.VALID`, `0` errors, `0` warnings, `0` cross-hero GFP references).
- **4-Player Listen-Server PIE (`L_BW_HeroSelect_Staging` -> `L_BW_Dorado`)**:
  - Consolidated Report: `Saved/Logs/hero-restoration-20261031/pie_e2e_verification.json`
  - Screenshots:
    - `Saved/Logs/PresentationCaptures/hero-restore-select04/player261-select-rawlins.png`
    - `Saved/Logs/PresentationCaptures/hero-restore-dorado04/player261-dorado-idle-rawlins.png`
    - `Saved/Logs/PresentationCaptures/hero-restore-action03/player261-action-rawlins.png`
    - `Saved/Logs/PresentationCaptures/hero-restore-relic03/player261-relic-carry-rawlins.png`
- **Headless Automation & Windows Cook**:
  - `Saved/Logs/hero-restoration-20261031/auto-hero-relic01.json` (`7/7` passed, `0` errors/warnings)
  - `Saved/Logs/hero-restoration-20261031/cook_check01.json` (`exitCode: 0`, `5,097` packages cooked, `0` hero warnings/errors)
