"""
Idempotent UE 5.8 Editor Python pipeline to import, assemble, retarget, wire, and validate
the restored Breakaway heroes (Rawlins, Korryn, Spartacus) into their canonical ProjectB
per-hero GameFeature plugins (/Hero_Rawlins, /Hero_Morgan, /Hero_Spartacus).

Follows AI_Planning/HERO_CODENAME_MAP.md and AI_Planning/Runbooks/editor-pie.md:
- Self-contained per-plugin materials and IK rigs (zero cross-hero GFP asset dependencies).
- Returns only JSON-serializable primitives and clears local UObject references in finally.
"""

import gc
import json
import math
import os
import unreal


PROJECT_ROOT = "E:/Unreal Projects/ProjectB"
_TRACKED_TRANSFORMS_JSON = f"{PROJECT_ROOT}/Scripts/Editor/blender_weapon_transforms.json"
_LOG_TRANSFORMS_JSON = f"{PROJECT_ROOT}/Saved/Logs/hero-restoration-20261031/blender_weapon_transforms.json"
BLENDER_TRANSFORMS_JSON = _TRACKED_TRANSFORMS_JSON if os.path.exists(_TRACKED_TRANSFORMS_JSON) else _LOG_TRANSFORMS_JSON

MANNEQUIN_SKM_PATH = "/Game/Characters/Heroes/Mannequin/Meshes/SKM_Quinn"
ALONA_SKM_PATH = "/Hero_Alona/Characters/Heroes/Alona/Models/SKM_BW_alona"
ALONA_IK_PATH = "/Hero_Alona/Characters/Heroes/Alona/Animations/Rig/IK_BW_Alona"
ALONA_ABP_PATH = "/Hero_Alona/Characters/Heroes/Alona/Animations/ABP_BW_Alona"
ALONA_EXTRA_ANIMS = [
    "/Hero_Alona/Characters/Heroes/Alona/Animations/A_BW_Alona_Jump_Start",
    "/Hero_Alona/Characters/Heroes/Alona/Animations/A_BW_Alona_Jump_Fall_Land",
]

MANNEQUIN_CHAINS = [
    ("Spine", "spine_01", "spine_05"),
    ("Head", "neck_01", "Head"),
    ("LeftClavicle", "clavicle_l", "clavicle_l"),
    ("UpperArm_L", "UpperArm_L", "UpperArm_L"),
    ("LowerArm_L", "LowerArm_L", "LowerArm_L"),
    ("Hand_L", "Hand_L", "Hand_L"),
    ("RightClavicle", "clavicle_r", "clavicle_r"),
    ("UpperArm_R", "UpperArm_R", "UpperArm_R"),
    ("LowerArm_R", "LowerArm_R", "LowerArm_R"),
    ("Hand_R", "Hand_R", "Hand_R"),
    ("Thigh_L", "Thigh_L", "Thigh_L"),
    ("Calf_L", "Calf_L", "Calf_L"),
    ("Foot_L", "Foot_L", "Foot_L"),
    ("Ball_L", "Ball_L", "Ball_L"),
    ("Thigh_R", "Thigh_R", "Thigh_R"),
    ("Calf_R", "Calf_R", "Calf_R"),
    ("Foot_R", "Foot_R", "Foot_R"),
    ("Ball_R", "Ball_R", "Ball_R"),
    ("LeftThumb", "thumb_01_l", "thumb_03_l"),
    ("LeftIndex", "index_01_l", "index_03_l"),
    ("LeftMiddle", "middle_01_l", "middle_03_l"),
    ("LeftRing", "ring_01_l", "ring_03_l"),
    ("LeftPinkyMetacarpal", "pinky_metacarpal_l", "pinky_metacarpal_l"),
    ("LeftPinky", "pinky_01_l", "pinky_03_l"),
    ("RightThumb", "thumb_01_r", "thumb_03_r"),
    ("RightIndex", "index_01_r", "index_03_r"),
    ("RightMiddle", "middle_01_r", "middle_03_r"),
    ("RightRing", "ring_01_r", "ring_03_r"),
    ("RightPinkyMetacarpal", "pinky_metacarpal_r", "pinky_metacarpal_r"),
    ("RightPinky", "pinky_01_r", "pinky_03_r"),
]

HERO_BIND_CHAINS = [
    ("Spine", "bind_spine", "bind_spine2"),
    ("Head", "bind_neck", "bind_head"),
    ("LeftClavicle", "bind_left_clavicle", "bind_left_clavicle"),
    ("UpperArm_L", "bind_left_upperArm_A", "bind_left_upperArm_A"),
    ("LowerArm_L", "bind_left_elbow", "bind_left_elbow"),
    ("Hand_L", "bind_left_wrist", "bind_left_wrist"),
    ("RightClavicle", "bind_right_clavicle", "bind_right_clavicle"),
    ("UpperArm_R", "bind_right_upperArm_A", "bind_right_upperArm_A"),
    ("LowerArm_R", "bind_right_elbow", "bind_right_elbow"),
    ("Hand_R", "bind_right_wrist", "bind_right_wrist"),
    ("Thigh_L", "bind_left_thigh_A", "bind_left_thigh_A"),
    ("Calf_L", "bind_left_knee", "bind_left_knee"),
    ("Foot_L", "bind_left_ankle", "bind_left_ankle"),
    ("Ball_L", "bind_left_ball", "bind_left_ball"),
    ("Thigh_R", "bind_right_thigh_A", "bind_right_thigh_A"),
    ("Calf_R", "bind_right_knee", "bind_right_knee"),
    ("Foot_R", "bind_right_ankle", "bind_right_ankle"),
    ("Ball_R", "bind_right_ball", "bind_right_ball"),
    ("LeftThumb", "bind_left_thumb_1", "bind_left_thumb_3"),
    ("LeftIndex", "bind_left_index_1", "bind_left_index_3"),
    ("LeftMiddle", "bind_left_middle_1", "bind_left_middle_3"),
    ("LeftRing", "bind_left_ring_1", "bind_left_ring_3"),
    ("LeftPinkyMetacarpal", "bind_left_pinky_base", "bind_left_pinky_base"),
    ("LeftPinky", "bind_left_pinky_1", "bind_left_pinky_3"),
    ("RightThumb", "bind_right_thumb_1", "bind_right_thumb_3"),
    ("RightIndex", "bind_right_index_1", "bind_right_index_3"),
    ("RightMiddle", "bind_right_middle_1", "bind_right_middle_3"),
    ("RightRing", "bind_right_ring_1", "bind_right_ring_3"),
    ("RightPinkyMetacarpal", "bind_right_pinky_base", "bind_right_pinky_base"),
    ("RightPinky", "bind_right_pinky_1", "bind_right_pinky_3"),
]


HERO_SPECS = {
    "Rawlins": {
        "plugin_mount": "/Hero_Rawlins",
        "base_dir": "/Hero_Rawlins/Characters/Heroes/Gunslinger",
        "hero_da": "/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger",
        "expected_display_name": "Rawlins",
        "source_art_dir": f"{PROJECT_ROOT}/SourceArt/Rawlins",
        "skm_name": "SKM_BW_Rawlins",
        "skm_lods": ["SKM_BW_Rawlins_LOD0.fbx", "SKM_BW_Rawlins_LOD1.fbx", "SKM_BW_Rawlins_LOD2.fbx", "SKM_BW_Rawlins_LOD3.fbx"],
        "mesh_z_offset": -96.0,
        "needs_masked_master": False,
        "textures": [
            ("T_BW_Rawlins_BC.png", "BC"),
            ("T_BW_Rawlins_N.png", "N"),
            ("T_BW_Rawlins_ORM.png", "ORM"),
            ("T_BW_Rawlins_E.png", "E"),
            ("T_BW_Rawlins_Gun_BC.png", "BC"),
            ("T_BW_Rawlins_Gun_N.png", "N"),
            ("T_BW_Rawlins_Gun_ORM.png", "ORM"),
            ("T_BW_Rawlins_Gun_E.png", "E"),
        ],
        "material_instances": {
            "MI_BW_Rawlins_Main": {
                "master": "opaque",
                "bc": "T_BW_Rawlins_BC",
                "n": "T_BW_Rawlins_N",
                "orm": "T_BW_Rawlins_ORM",
                "e": "T_BW_Rawlins_E",
                "emissive_strength": 4.5,
                "specular": 0.45,
                "roughness_scale": 1.0,
            },
            "MI_BW_Rawlins_Cape": {
                "master": "twosided",
                "bc": "T_BW_Rawlins_BC",
                "n": "T_BW_Rawlins_N",
                "orm": "T_BW_Rawlins_ORM",
                "e": "T_BW_Rawlins_E",
                "emissive_strength": 3.5,
                "specular": 0.40,
                "roughness_scale": 1.0,
            },
            "MI_BW_Rawlins_Ammo": {
                "master": "opaque",
                "bc": "T_BW_Rawlins_BC",
                "n": "T_BW_Rawlins_N",
                "orm": "T_BW_Rawlins_ORM",
                "e": None,
                "emissive_strength": 0.0,
                "specular": 0.50,
                "roughness_scale": 1.0,
            },
            "MI_BW_Rawlins_TombBreaker": {
                "master": "opaque",
                "bc": "T_BW_Rawlins_Gun_BC",
                "n": "T_BW_Rawlins_Gun_N",
                "orm": "T_BW_Rawlins_Gun_ORM",
                "e": "T_BW_Rawlins_Gun_E",
                "emissive_strength": 5.0,
                "specular": 0.50,
                "roughness_scale": 1.0,
            },
        },
        "skm_slot_map": {
            "main_mat": "MI_BW_Rawlins_Main",
            "cape_mat": "MI_BW_Rawlins_Cape",
            "ammo_mat": "MI_BW_Rawlins_Ammo",
        },
        "weapons": [
            {
                "sm_name": "SM_BW_Rawlins_TombBreaker",
                "lods": [
                    "SM_BW_Rawlins_TombBreaker_LOD0.fbx",
                    "SM_BW_Rawlins_TombBreaker_LOD1.fbx",
                    "SM_BW_Rawlins_TombBreaker_LOD2.fbx",
                    "SM_BW_Rawlins_TombBreaker_LOD3.fbx",
                ],
                "mi": "MI_BW_Rawlins_TombBreaker",
                "socket_name": "weapon_r",
                "parent_bone": "S_RAttachment",
                "blender_preview_name": "SM_BW_Rawlins_TombBreaker_HandPreview",
                "slot": "primary",
            }
        ],
        "required_sockets": ["relic_r", "weapon_r"],
        "source_primary_montage": "/BreakawayCore/Animations/AM_BW_Rawlins_Primary_Placeholder",
        "primary_montage_name": "AM_BW_Rawlins_Primary",
        "primary_seq_name": "A_BW_Rawlins_Primary",
    },
    "Korryn": {
        "plugin_mount": "/Hero_Morgan",
        "base_dir": "/Hero_Morgan/Characters/Heroes/Hexweaver",
        "hero_da": "/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver",
        "expected_display_name": "Korryn",
        "source_art_dir": f"{PROJECT_ROOT}/SourceArt/Korryn",
        "skm_name": "SKM_BW_Korryn",
        "skm_lods": [
            "SKM_BW_Korryn_LOD0.fbx",
            "SKM_BW_Korryn_LOD1.fbx",
            "SKM_BW_Korryn_LOD2.fbx",
            "SKM_BW_Korryn_LOD3.fbx",
            "SKM_BW_Korryn_LOD4.fbx",
        ],
        "mesh_z_offset": -106.5,
        "needs_masked_master": True,
        "textures": [
            ("T_BW_Korryn_BC.png", "BC"),
            ("T_BW_Korryn_N.png", "N"),
            ("T_BW_Korryn_ORM.png", "ORM"),
            ("T_BW_Korryn_E.png", "E"),
            ("T_BW_Korryn_Staff_BC.png", "BC"),
            ("T_BW_Korryn_Staff_N.png", "N"),
            ("T_BW_Korryn_Staff_ORM.png", "ORM"),
            ("T_BW_Korryn_Staff_E.png", "E"),
        ],
        "material_instances": {
            "MI_BW_Korryn_Skin": {
                "master": "opaque",
                "bc": "T_BW_Korryn_BC",
                "n": "T_BW_Korryn_N",
                "orm": "T_BW_Korryn_ORM",
                "e": "T_BW_Korryn_E",
                "emissive_strength": 5.5,
                "specular": 0.35,
                "roughness_scale": 1.03,
            },
            "MI_BW_Korryn_Main": {
                "master": "twosided",
                "bc": "T_BW_Korryn_BC",
                "n": "T_BW_Korryn_N",
                "orm": "T_BW_Korryn_ORM",
                "e": None,
                "emissive_strength": 0.0,
                "specular": 0.45,
                "roughness_scale": 1.0,
            },
            "MI_BW_Korryn_Feathers": {
                "master": "masked",
                "bc": "T_BW_Korryn_BC",
                "n": "T_BW_Korryn_N",
                "orm": "T_BW_Korryn_ORM",
                "e": None,
                "emissive_strength": 0.0,
                "specular": 0.40,
                "roughness_scale": 1.0,
            },
            "MI_BW_Korryn_Staff": {
                "master": "opaque",
                "bc": "T_BW_Korryn_Staff_BC",
                "n": "T_BW_Korryn_Staff_N",
                "orm": "T_BW_Korryn_Staff_ORM",
                "e": "T_BW_Korryn_Staff_E",
                "emissive_strength": 5.5,
                "specular": 0.50,
                "roughness_scale": 1.0,
            },
        },
        "skm_slot_map": {
            "morgan_skin": "MI_BW_Korryn_Skin",
            "morgan_main": "MI_BW_Korryn_Main",
            "morgan_transp": "MI_BW_Korryn_Feathers",
        },
        "weapons": [
            {
                "sm_name": "SM_BW_Korryn_Staff",
                "lods": [
                    "SM_BW_Korryn_Staff_LOD0.fbx",
                    "SM_BW_Korryn_Staff_LOD1.fbx",
                    "SM_BW_Korryn_Staff_LOD2.fbx",
                    "SM_BW_Korryn_Staff_LOD3.fbx",
                ],
                "mi": "MI_BW_Korryn_Staff",
                "socket_name": "weapon_l",
                "parent_bone": "S_LAttachment",
                "blender_preview_name": "SM_BW_Korryn_Staff_HandPreview",
                "slot": "primary",
            }
        ],
        "required_sockets": ["relic_r", "weapon_l"],
        "source_primary_montage": "/BreakawayCore/Animations/AM_BW_Korryn_Primary_Placeholder",
        "primary_montage_name": "AM_BW_Korryn_Primary",
        "primary_seq_name": "A_BW_Korryn_Primary",
    },
    "Spartacus": {
        "plugin_mount": "/Hero_Spartacus",
        "base_dir": "/Hero_Spartacus/Characters/Heroes/Argus",
        "hero_da": "/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus",
        "expected_display_name": "Argus",
        "source_art_dir": f"{PROJECT_ROOT}/SourceArt/Spartacus",
        "skm_name": "SKM_BW_Spartacus",
        "skm_lods": [
            "SKM_BW_Spartacus_LOD0.fbx",
            "SKM_BW_Spartacus_LOD1.fbx",
            "SKM_BW_Spartacus_LOD2.fbx",
            "SKM_BW_Spartacus_LOD3.fbx",
        ],
        "mesh_z_offset": -96.0,
        "needs_masked_master": False,
        "textures": [
            ("T_BW_Spartacus_BC.png", "BC"),
            ("T_BW_Spartacus_N.png", "N"),
            ("T_BW_Spartacus_ORM.png", "ORM"),
            ("T_BW_Spartacus_Sword1_BC.png", "BC"),
            ("T_BW_Spartacus_Sword1_N.png", "N"),
            ("T_BW_Spartacus_Sword1_ORM.png", "ORM"),
            ("T_BW_Spartacus_Sword2_BC.png", "BC"),
            ("T_BW_Spartacus_Sword2_N.png", "N"),
            ("T_BW_Spartacus_Sword2_ORM.png", "ORM"),
        ],
        "material_instances": {
            "MI_BW_Spartacus_Main": {
                "master": "twosided",
                "bc": "T_BW_Spartacus_BC",
                "n": "T_BW_Spartacus_N",
                "orm": "T_BW_Spartacus_ORM",
                "e": None,
                "emissive_strength": 0.0,
                "specular": 0.50,
                "roughness_scale": 1.0,
            },
            "MI_BW_Spartacus_Skin": {
                "master": "opaque",
                "bc": "T_BW_Spartacus_BC",
                "n": "T_BW_Spartacus_N",
                "orm": "T_BW_Spartacus_ORM",
                "e": None,
                "emissive_strength": 0.0,
                "specular": 0.35,
                "roughness_scale": 1.05,
            },
            "MI_BW_Spartacus_SwordL": {
                "master": "opaque",
                "bc": "T_BW_Spartacus_Sword1_BC",
                "n": "T_BW_Spartacus_Sword1_N",
                "orm": "T_BW_Spartacus_Sword1_ORM",
                "e": None,
                "emissive_strength": 0.0,
                "specular": 0.50,
                "roughness_scale": 1.0,
            },
            "MI_BW_Spartacus_SwordR": {
                "master": "opaque",
                "bc": "T_BW_Spartacus_Sword2_BC",
                "n": "T_BW_Spartacus_Sword2_N",
                "orm": "T_BW_Spartacus_Sword2_ORM",
                "e": None,
                "emissive_strength": 0.0,
                "specular": 0.50,
                "roughness_scale": 1.0,
            },
        },
        "skm_slot_map": {
            "spart_main": "MI_BW_Spartacus_Main",
            "spart_skin": "MI_BW_Spartacus_Skin",
        },
        "weapons": [
            {
                "sm_name": "SM_BW_Spartacus_SwordR",
                "lods": [
                    "SM_BW_Spartacus_SwordR_LOD0.fbx",
                    "SM_BW_Spartacus_SwordR_LOD1.fbx",
                    "SM_BW_Spartacus_SwordR_LOD2.fbx",
                    "SM_BW_Spartacus_SwordR_LOD3.fbx",
                ],
                "mi": "MI_BW_Spartacus_SwordR",
                "socket_name": "weapon_r",
                "parent_bone": "S_RAttachment",
                "blender_preview_name": "SM_BW_Spartacus_SwordR_HandPreview",
                "slot": "primary",
            },
            {
                "sm_name": "SM_BW_Spartacus_SwordL",
                "lods": [
                    "SM_BW_Spartacus_SwordL_LOD0.fbx",
                    "SM_BW_Spartacus_SwordL_LOD1.fbx",
                    "SM_BW_Spartacus_SwordL_LOD2.fbx",
                    "SM_BW_Spartacus_SwordL_LOD3.fbx",
                ],
                "mi": "MI_BW_Spartacus_SwordL",
                "socket_name": "weapon_l",
                "parent_bone": "S_LAttachment",
                "blender_preview_name": "SM_BW_Spartacus_SwordL_HandPreview",
                "slot": "secondary",
            },
        ],
        "required_sockets": ["relic_r", "weapon_r", "weapon_l"],
        "source_primary_montage": "/BreakawayCore/Animations/AM_BW_Argus_Primary_Placeholder",
        "primary_montage_name": "AM_BW_Spartacus_Primary",
        "primary_seq_name": "A_BW_Spartacus_Primary",
    },
}


def _blender_matrix_to_ue_component_transform(mw):
    """
    Convert a Blender 4x4 world matrix (meters, Z-up, right-handed) to UE 5.8
    SkeletalMesh component-space Transform (centimeters, Y-reflected S=diag(1,-1,1)).
    """
    ax_x = unreal.Vector(mw[0][0], -mw[1][0], mw[2][0])
    ax_y = unreal.Vector(-mw[0][1], mw[1][1], -mw[2][1])
    ax_z = unreal.Vector(mw[0][2], -mw[1][2], mw[2][2])
    rot = unreal.MathLibrary.make_rotation_from_axes(ax_x, ax_y, ax_z)
    loc = unreal.Vector(100.0 * mw[0][3], -100.0 * mw[1][3], 100.0 * mw[2][3])
    return unreal.Transform(loc, rot, unreal.Vector(1.0, 1.0, 1.0))


def _ensure_master_material(pkg_dir, mat_name, default_bc, default_n, default_orm, two_sided=False, masked=False):
    asset_path = f"{pkg_dir}/{mat_name}"
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat_lib = unreal.MaterialEditingLibrary

    mat = unreal.load_asset(asset_path)
    if not mat:
        mat = asset_tools.create_asset(mat_name, pkg_dir, unreal.Material, unreal.MaterialFactoryNew())
    else:
        mat_lib.delete_all_material_expressions(mat)

    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED if masked else unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("two_sided", bool(two_sided))
    mat.set_editor_property("used_with_skeletal_mesh", True)
    if masked:
        mat.set_editor_property("opacity_mask_clip_value", 0.76)

    default_black = unreal.load_asset("/Engine/EngineResources/Black")

    # BaseColorTexture * BaseColorTint -> MP_BASE_COLOR
    bc_tex = mat_lib.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -520, -260)
    bc_tex.set_editor_property("parameter_name", "BaseColorTexture")
    bc_tex.set_editor_property("texture", default_bc)
    bc_tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

    bc_tint = mat_lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -520, -60)
    bc_tint.set_editor_property("parameter_name", "BaseColorTint")
    bc_tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))

    bc_mul = mat_lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, -200)
    mat_lib.connect_material_expressions(bc_tex, "RGB", bc_mul, "A")
    mat_lib.connect_material_expressions(bc_tint, "RGB", bc_mul, "B")
    mat_lib.connect_material_property(bc_mul, "", unreal.MaterialProperty.MP_BASE_COLOR)

    if masked:
        mat_lib.connect_material_property(bc_tex, "A", unreal.MaterialProperty.MP_OPACITY_MASK)

    # NormalTexture -> MP_NORMAL
    n_tex = mat_lib.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -520, 120)
    n_tex.set_editor_property("parameter_name", "NormalTexture")
    n_tex.set_editor_property("texture", default_n)
    n_tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    mat_lib.connect_material_property(n_tex, "RGB", unreal.MaterialProperty.MP_NORMAL)

    # ORMTexture -> R: AO, G*RoughnessScale: Roughness, B*MetallicScale: Metallic
    # NOTE: ORMTexture MUST reference a non-sRGB TC_MASKS texture on the master material so PCD3D_SM6 compiles cleanly.
    orm_tex = mat_lib.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -520, 360)
    orm_tex.set_editor_property("parameter_name", "ORMTexture")
    orm_tex.set_editor_property("texture", default_orm)
    orm_tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    mat_lib.connect_material_property(orm_tex, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    rough_scale = mat_lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -520, 580)
    rough_scale.set_editor_property("parameter_name", "RoughnessScale")
    rough_scale.set_editor_property("default_value", 1.0)
    rough_mul = mat_lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, 440)
    mat_lib.connect_material_expressions(orm_tex, "G", rough_mul, "A")
    mat_lib.connect_material_expressions(rough_scale, "", rough_mul, "B")
    mat_lib.connect_material_property(rough_mul, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal_scale = mat_lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -520, 680)
    metal_scale.set_editor_property("parameter_name", "MetallicScale")
    metal_scale.set_editor_property("default_value", 1.0)
    metal_mul = mat_lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, 560)
    mat_lib.connect_material_expressions(orm_tex, "B", metal_mul, "A")
    mat_lib.connect_material_expressions(metal_scale, "", metal_mul, "B")
    mat_lib.connect_material_property(metal_mul, "", unreal.MaterialProperty.MP_METALLIC)

    spec_val = mat_lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -220, 680)
    spec_val.set_editor_property("parameter_name", "SpecularValue")
    spec_val.set_editor_property("default_value", 0.5)
    mat_lib.connect_material_property(spec_val, "", unreal.MaterialProperty.MP_SPECULAR)

    # EmissiveTexture * EmissiveStrength -> MP_EMISSIVE_COLOR
    em_tex = mat_lib.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -520, 820)
    em_tex.set_editor_property("parameter_name", "EmissiveTexture")
    em_tex.set_editor_property("texture", default_black)
    em_tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)

    em_str = mat_lib.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -520, 1020)
    em_str.set_editor_property("parameter_name", "EmissiveStrength")
    em_str.set_editor_property("default_value", 0.0)

    em_mul = mat_lib.create_material_expression(mat, unreal.MaterialExpressionMultiply, -220, 880)
    mat_lib.connect_material_expressions(em_tex, "RGB", em_mul, "A")
    mat_lib.connect_material_expressions(em_str, "", em_mul, "B")
    mat_lib.connect_material_property(em_mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mat_lib.layout_material_expressions(mat)
    mat_lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, False)
    return mat


def _import_textures(spec):
    tex_dir = f"{spec['base_dir']}/Textures"
    prep_dir = f"{spec['source_art_dir']}/Textures/Prepared"
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

    tasks = []
    kinds = {}
    for fname, kind in spec["textures"]:
        src_file = f"{prep_dir}/{fname}"
        if not os.path.exists(src_file):
            raise RuntimeError(f"Missing prepared texture: {src_file}")
        stem = os.path.splitext(fname)[0]
        kinds[stem] = kind
        t = unreal.AssetImportTask()
        t.filename = src_file
        t.destination_path = tex_dir
        t.destination_name = stem
        t.replace_existing = True
        t.replace_existing_settings = False
        t.automated = True
        t.save = False
        tasks.append(t)

    asset_tools.import_asset_tasks(tasks)

    imported = {}
    for stem, kind in kinds.items():
        tex_path = f"{tex_dir}/{stem}"
        tex = unreal.load_asset(tex_path)
        if not tex:
            raise RuntimeError(f"Failed to import texture: {tex_path}")
        tex.modify()
        if kind in ("BC", "E"):
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
            tex.set_editor_property("srgb", True)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
        elif kind == "N":
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER_NORMAL_MAP)
        elif kind == "ORM":
            tex.set_editor_property("srgb", False)
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER_SPECULAR)
        unreal.EditorAssetLibrary.save_loaded_asset(tex, False)
        imported[stem] = tex
    return imported


def _build_materials_and_instances(hero_name, spec, textures):
    mat_dir = f"{spec['base_dir']}/Materials"
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat_lib = unreal.MaterialEditingLibrary

    first_mi = next(iter(spec["material_instances"].values()))
    default_bc = textures[first_mi["bc"]]
    default_n = textures[first_mi["n"]]
    default_orm = textures[first_mi["orm"]]

    masters = {
        "opaque": _ensure_master_material(
            mat_dir, f"M_BW_{hero_name}_PBR", default_bc, default_n, default_orm, two_sided=False, masked=False
        ),
        "twosided": _ensure_master_material(
            mat_dir, f"M_BW_{hero_name}_TwoSided_PBR", default_bc, default_n, default_orm, two_sided=True, masked=False
        ),
    }
    if spec["needs_masked_master"]:
        masters["masked"] = _ensure_master_material(
            mat_dir, f"M_BW_{hero_name}_Masked_PBR", default_bc, default_n, default_orm, two_sided=True, masked=True
        )

    instances = {}
    default_black = unreal.load_asset("/Engine/EngineResources/Black")
    for mi_name, mcfg in spec["material_instances"].items():
        mi_path = f"{mat_dir}/{mi_name}"
        mi = unreal.load_asset(mi_path)
        if not mi:
            mi = asset_tools.create_asset(mi_name, mat_dir, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        parent_mat = masters[mcfg["master"]]
        mat_lib.set_material_instance_parent(mi, parent_mat)
        mat_lib.set_material_instance_texture_parameter_value(mi, "BaseColorTexture", textures[mcfg["bc"]])
        mat_lib.set_material_instance_texture_parameter_value(mi, "NormalTexture", textures[mcfg["n"]])
        mat_lib.set_material_instance_texture_parameter_value(mi, "ORMTexture", textures[mcfg["orm"]])
        mat_lib.set_material_instance_texture_parameter_value(
            mi, "EmissiveTexture", textures[mcfg["e"]] if mcfg["e"] else default_black
        )
        mat_lib.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", float(mcfg["emissive_strength"]))
        mat_lib.set_material_instance_scalar_parameter_value(mi, "SpecularValue", float(mcfg["specular"]))
        mat_lib.set_material_instance_scalar_parameter_value(mi, "RoughnessScale", float(mcfg["roughness_scale"]))
        mat_lib.set_material_instance_scalar_parameter_value(mi, "MetallicScale", 1.0)
        mat_lib.update_material_instance(mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
        instances[mi_name] = mi
    return instances


def _import_skeletal_and_weapon_meshes(spec, mi_map):
    models_dir = f"{spec['base_dir']}/Models"
    weap_dir = f"{models_dir}/Weapon"
    exports_dir = f"{spec['source_art_dir']}/Exports"
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    skm_sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)

    prev_interchange = unreal.SystemLibrary.get_console_variable_bool_value("Interchange.FeatureFlags.Import.FBX")
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
    try:
        # 1. SkeletalMesh LOD0 + LOD1..N
        skm_name = spec["skm_name"]
        skm_path = f"{models_dir}/{skm_name}"
        existing_skm = unreal.load_asset(skm_path)
        existing_skel = existing_skm.get_editor_property("skeleton") if existing_skm else unreal.load_asset(f"{models_dir}/{skm_name}_Skeleton")
        existing_phys = existing_skm.get_editor_property("physics_asset") if existing_skm else unreal.load_asset(f"{models_dir}/{skm_name}_PhysicsAsset")

        fbx_ui = unreal.FbxImportUI()
        fbx_ui.automated_import_should_detect_type = False
        fbx_ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
        fbx_ui.import_as_skeletal = True
        fbx_ui.import_mesh = True
        fbx_ui.import_animations = False
        fbx_ui.import_materials = False
        fbx_ui.import_textures = False
        fbx_ui.create_physics_asset = existing_phys is None
        if existing_skel:
            fbx_ui.skeleton = existing_skel
        if existing_phys:
            fbx_ui.physics_asset = existing_phys
        fbx_ui.skeletal_mesh_import_data.set_editor_property("import_morph_targets", True)
        fbx_ui.skeletal_mesh_import_data.set_editor_property("convert_scene", True)
        fbx_ui.skeletal_mesh_import_data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)

        t_skm = unreal.AssetImportTask()
        t_skm.filename = f"{exports_dir}/{spec['skm_lods'][0]}"
        t_skm.destination_path = models_dir
        t_skm.destination_name = skm_name
        t_skm.replace_existing = True
        t_skm.automated = True
        t_skm.save = False
        t_skm.factory = unreal.FbxFactory()
        t_skm.options = fbx_ui
        asset_tools.import_asset_tasks([t_skm])

        skm = unreal.load_asset(skm_path)
        if not skm:
            raise RuntimeError(f"Failed to import SkeletalMesh: {skm_path}")

        for lod_idx in range(1, len(spec["skm_lods"])):
            lod_file = f"{exports_dir}/{spec['skm_lods'][lod_idx]}"
            res_lod = skm_sub.import_lod(skm, lod_idx, lod_file)
            if res_lod < 0:
                raise RuntimeError(f"Failed SkeletalMesh LOD{lod_idx} import from {lod_file} (code={res_lod})")

        # Bind SkeletalMesh materials across slots
        skm_mats = list(skm.get_editor_property("materials"))
        fallback_mi = next(iter(mi_map.values()))
        for idx, smat in enumerate(skm_mats):
            imp_name = str(smat.get_editor_property("imported_material_slot_name"))
            slot_name = str(smat.get_editor_property("material_slot_name"))
            target_mi_name = spec["skm_slot_map"].get(imp_name) or spec["skm_slot_map"].get(slot_name)
            smat.set_editor_property("material_interface", mi_map[target_mi_name] if target_mi_name else fallback_mi)
            skm_mats[idx] = smat
        skm.set_editor_property("materials", skm_mats)

        # 2. Weapon StaticMeshes LOD0 + LOD1..N
        weapons_out = {}
        for wspec in spec["weapons"]:
            sm_name = wspec["sm_name"]
            sm_path = f"{weap_dir}/{sm_name}"
            sm_ui = unreal.FbxImportUI()
            sm_ui.automated_import_should_detect_type = False
            sm_ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
            sm_ui.import_as_skeletal = False
            sm_ui.import_mesh = True
            sm_ui.import_animations = False
            sm_ui.import_materials = False
            sm_ui.import_textures = False
            sm_ui.static_mesh_import_data.set_editor_property("combine_meshes", True)
            sm_ui.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
            sm_ui.static_mesh_import_data.set_editor_property("convert_scene", True)
            sm_ui.static_mesh_import_data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)

            t_sm = unreal.AssetImportTask()
            t_sm.filename = f"{exports_dir}/{wspec['lods'][0]}"
            t_sm.destination_path = weap_dir
            t_sm.destination_name = sm_name
            t_sm.replace_existing = True
            t_sm.automated = True
            t_sm.save = False
            t_sm.factory = unreal.FbxFactory()
            t_sm.options = sm_ui
            asset_tools.import_asset_tasks([t_sm])

            sm = unreal.load_asset(sm_path)
            if not sm:
                raise RuntimeError(f"Failed to import weapon StaticMesh: {sm_path}")
            for lod_idx in range(1, len(wspec["lods"])):
                lod_file = f"{exports_dir}/{wspec['lods'][lod_idx]}"
                ok = sm_sub.import_lod(sm, lod_idx, lod_file)
                if ok < 0:
                    raise RuntimeError(f"Failed StaticMesh LOD{lod_idx} import from {lod_file}")
            sm.set_material(0, mi_map[wspec["mi"]])
            unreal.EditorAssetLibrary.save_loaded_asset(sm, False)
            weapons_out[sm_name] = sm
    finally:
        cvar_val = "1" if prev_interchange else "0"
        unreal.SystemLibrary.execute_console_command(None, f"Interchange.FeatureFlags.Import.FBX {cvar_val}")

    return skm, weapons_out


def _upsert_mesh_socket(skm, socket_name, bone_name, local_xf):
    sock = skm.find_socket(socket_name)
    if not sock:
        sock = unreal.SkeletalMeshSocket(skm)
        skm.add_socket(sock, False)
        skm.rename_socket(sock.socket_name, socket_name)
        sock = skm.find_socket(socket_name)
    if not sock:
        raise RuntimeError(f"Failed to create/find socket '{socket_name}' on {skm.get_path_name()}")
    sock.set_socket_parent(skm, bone_name)
    sock.set_socket_local_transform(local_xf)
    if str(sock.bone_name) != bone_name:
        raise RuntimeError(f"Socket '{socket_name}' parent mismatch: {sock.bone_name} != {bone_name}")
    return sock


def _normalize_cryengine_100x_anim_sequence(anim_seq):
    """
    UE 5.8 IKRetargetBatchOperation writes S_XForm track scale as [1,1,1] and bakes
    bind_root translations in cm (~114) rather than S_XForm's 100x meter parent space (~1.14).
    Restore S_XForm track scale to [100,100,100] and scale bind_root positional keys by 0.01,
    matching A_BW_Alona_Idle / A_BW_Alona_Primary_Staff 1:1. Idempotent if already normalized.
    """
    if not isinstance(anim_seq, unreal.AnimSequence):
        return
    num_frames = unreal.AnimationLibrary.get_num_frames(anim_seq)
    if num_frames <= 0:
        return
    sx0 = unreal.AnimationLibrary.get_bone_pose_for_frame(anim_seq, "S_XForm", 0, False)
    if abs(float(sx0.scale3d.x) - 100.0) < 1.0:
        return

    ctrl = anim_seq.controller
    ctrl.open_bracket(unreal.Text("Normalize CryEngine 100x Root Scale"), False)
    try:
        s100 = unreal.Vector(100.0, 100.0, 100.0)
        sx_pos, sx_rot, sx_scl = [], [], []
        for f in range(num_frames):
            t = unreal.AnimationLibrary.get_bone_pose_for_frame(anim_seq, "S_XForm", f, False)
            sx_pos.append(t.translation)
            sx_rot.append(t.rotation)
            sx_scl.append(s100)
        ctrl.set_bone_track_keys("S_XForm", sx_pos, sx_rot, sx_scl, False)

        br_pos, br_rot, br_scl = [], [], []
        for f in range(num_frames):
            t = unreal.AnimationLibrary.get_bone_pose_for_frame(anim_seq, "bind_root", f, False)
            br_pos.append(t.translation * 0.01)
            br_rot.append(t.rotation)
            br_scl.append(t.scale3d)
        ctrl.set_bone_track_keys("bind_root", br_pos, br_rot, br_scl, False)
    finally:
        ctrl.close_bracket(False)


def _configure_cryengine_retarget_ops(rctrl, match_alona_pelvis=True):
    """
    Disable the 'Root Motion' op on CryEngine target skeletons whose root bone ('S_XForm')
    carries a 100x meter-to-centimeter scale. Leaving 'Root Motion' enabled overwrites
    S_XForm's animated track scale to [1,1,1] and bakes bind_root local translations in cm
    (~114) instead of meters (~1.14), which multiplies by S_XForm's 100x ref scale at runtime.
    """
    for op_i in range(rctrl.get_num_retarget_ops()):
        op_name = str(rctrl.get_op_name(op_i))
        if op_name == "Root Motion":
            rctrl.set_retarget_op_enabled(op_i, False)
        elif op_name == "Pelvis Motion" and match_alona_pelvis:
            op_ctrl = rctrl.get_op_controller(op_i)
            s = op_ctrl.get_settings()
            s.set_editor_property("scale_horizontal", 0.0)
            s.set_editor_property("scale_vertical", 1.0)
            s.set_editor_property("floor_constraint_weight", 0.0)
            op_ctrl.set_settings(s)


def _build_ik_rigs_sockets_and_animations(hero_name, spec, skm, blender_transforms):
    rig_dir = f"{spec['base_dir']}/Animations/Rig"
    anim_dir = f"{spec['base_dir']}/Animations"
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    ar = unreal.AssetRegistryHelpers.get_asset_registry()

    # 1. Create IK_BW_<Hero> first so we can query exact ref-pose bone transforms for socket authoring
    ik_hero_name = f"IK_BW_{hero_name}"
    ik_hero_path = f"{rig_dir}/{ik_hero_name}"
    ik_hero = unreal.load_asset(ik_hero_path)
    if not ik_hero:
        ik_hero = asset_tools.create_asset(ik_hero_name, rig_dir, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl_h = unreal.IKRigController.get_controller(ik_hero)
    ctrl_h.set_skeletal_mesh(skm)
    for c in list(ctrl_h.get_retarget_chains()):
        ctrl_h.remove_retarget_chain(c.get_editor_property("chain_name"))
    ctrl_h.set_retarget_root("bind_root")
    for cname, sbone, ebone in HERO_BIND_CHAINS:
        ctrl_h.add_retarget_chain(cname, sbone, ebone, "None")
    unreal.EditorAssetLibrary.save_loaded_asset(ik_hero, False)

    # 2. Author mathematically exact weapon sockets & relic_r socket on SKM_BW_<Hero>
    socket_checks = {}
    hero_bt = blender_transforms[hero_name]["previews"]
    for wspec in spec["weapons"]:
        mw = hero_bt[wspec["blender_preview_name"]]["matrix_world"]
        t_weap_ue = _blender_matrix_to_ue_component_transform(mw)
        t_bone_ue = ctrl_h.get_ref_pose_transform_of_bone(wspec["parent_bone"])
        t_local = unreal.MathLibrary.make_relative_transform(t_weap_ue, t_bone_ue)
        _upsert_mesh_socket(skm, wspec["socket_name"], wspec["parent_bone"], t_local)
        t_composed = unreal.MathLibrary.compose_transforms(t_local, t_bone_ue)
        socket_checks[wspec["socket_name"]] = {
            "parent_bone": wspec["parent_bone"],
            "composed_loc_cm": [t_composed.translation.x, t_composed.translation.y, t_composed.translation.z],
            "composed_scale": [t_composed.scale3d.x, t_composed.scale3d.y, t_composed.scale3d.z],
        }

    # Author relic_r on bind_right_wrist with scale normalized to [1,1,1] matching Alona
    t_rwrist = ctrl_h.get_ref_pose_transform_of_bone("bind_right_wrist")
    t_relic_target = unreal.Transform(
        t_rwrist.translation + unreal.Vector(0.0, 4.0, 0.0),
        unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0),
        unreal.Vector(1.0, 1.0, 1.0),
    )
    t_relic_local = unreal.MathLibrary.make_relative_transform(t_relic_target, t_rwrist)
    _upsert_mesh_socket(skm, "relic_r", "bind_right_wrist", t_relic_local)
    t_relic_comp = unreal.MathLibrary.compose_transforms(t_relic_local, t_rwrist)
    socket_checks["relic_r"] = {
        "parent_bone": "bind_right_wrist",
        "composed_loc_cm": [t_relic_comp.translation.x, t_relic_comp.translation.y, t_relic_comp.translation.z],
        "composed_scale": [t_relic_comp.scale3d.x, t_relic_comp.scale3d.y, t_relic_comp.scale3d.z],
    }

    skel = skm.get_editor_property("skeleton")
    phys = skm.get_editor_property("physics_asset")
    unreal.EditorAssetLibrary.save_loaded_asset(skm, False)
    if skel:
        unreal.EditorAssetLibrary.save_loaded_asset(skel, False)
    if phys:
        unreal.EditorAssetLibrary.save_loaded_asset(phys, False)

    # 3. Create self-contained IK_Mannequin_For<Hero> and RTG_Mannequin_To_<Hero>
    manny_skm = unreal.load_asset(MANNEQUIN_SKM_PATH)
    ik_manny_name = f"IK_Mannequin_For{hero_name}"
    ik_manny_path = f"{rig_dir}/{ik_manny_name}"
    ik_manny = unreal.load_asset(ik_manny_path)
    if not ik_manny:
        ik_manny = asset_tools.create_asset(ik_manny_name, rig_dir, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl_m = unreal.IKRigController.get_controller(ik_manny)
    ctrl_m.set_skeletal_mesh(manny_skm)
    for c in list(ctrl_m.get_retarget_chains()):
        ctrl_m.remove_retarget_chain(c.get_editor_property("chain_name"))
    ctrl_m.set_retarget_root("pelvis")
    for cname, sbone, ebone in MANNEQUIN_CHAINS:
        ctrl_m.add_retarget_chain(cname, sbone, ebone, "None")
    unreal.EditorAssetLibrary.save_loaded_asset(ik_manny, False)

    rtg_name = f"RTG_Mannequin_To_{hero_name}"
    rtg_path = f"{rig_dir}/{rtg_name}"
    rtg = unreal.load_asset(rtg_path)
    if not rtg:
        rtg = asset_tools.create_asset(rtg_name, rig_dir, unreal.IKRetargeter, unreal.IKRetargetFactory())
    rctrl = unreal.IKRetargeterController.get_controller(rtg)
    rctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, ik_manny)
    rctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, ik_hero)
    if rctrl.get_num_retarget_ops() == 0:
        rctrl.add_default_ops()
    rctrl.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, ik_manny)
    rctrl.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, ik_hero)
    for op_i in range(rctrl.get_num_retarget_ops()):
        rctrl.run_op_initial_setup(op_i)
    rctrl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    _configure_cryengine_retarget_ops(rctrl, match_alona_pelvis=True)

    target_poses = [str(k) for k in rctrl.get_retarget_poses(unreal.RetargetSourceOrTarget.TARGET).keys()]
    if "Aligned_A_Pose" not in target_poses:
        rctrl.create_retarget_pose("Aligned_A_Pose", unreal.RetargetSourceOrTarget.TARGET)
    rctrl.set_current_retarget_pose("Aligned_A_Pose", unreal.RetargetSourceOrTarget.TARGET)
    rctrl.reset_retarget_pose("Aligned_A_Pose", [], unreal.RetargetSourceOrTarget.TARGET)
    rctrl.set_root_offset_in_retarget_pose(unreal.Vector(0.0, 0.0, 0.0), unreal.RetargetSourceOrTarget.TARGET)
    unreal.EditorAssetLibrary.save_loaded_asset(rtg, False)

    # 4. Retarget Locomotion AnimBP (ABP_BW_Alona -> ABP_BW_<Hero>) via transient CryEngine-to-CryEngine RTG
    alona_skm = unreal.load_asset(ALONA_SKM_PATH)
    alona_ik = unreal.load_asset(ALONA_IK_PATH)
    temp_rtg_name = f"RTG_TempAlona_To_{hero_name}"
    temp_rtg_path = f"{rig_dir}/{temp_rtg_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(temp_rtg_path):
        unreal.EditorAssetLibrary.delete_asset(temp_rtg_path)
    temp_rtg = asset_tools.create_asset(temp_rtg_name, rig_dir, unreal.IKRetargeter, unreal.IKRetargetFactory())
    tctrl = unreal.IKRetargeterController.get_controller(temp_rtg)
    tctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, alona_ik)
    tctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, ik_hero)
    tctrl.add_default_ops()
    tctrl.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.SOURCE, alona_ik)
    tctrl.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, ik_hero)
    for op_i in range(tctrl.get_num_retarget_ops()):
        tctrl.run_op_initial_setup(op_i)
    tctrl.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
    _configure_cryengine_retarget_ops(tctrl, match_alona_pelvis=True)

    loco_assets = [ar.get_asset_by_object_path(f"{ALONA_ABP_PATH}.ABP_BW_Alona")]
    for ea in ALONA_EXTRA_ANIMS:
        ename = ea.rsplit("/", 1)[-1]
        ad = ar.get_asset_by_object_path(f"{ea}.{ename}")
        if ad.is_valid():
            loco_assets.append(ad)

    loco_inputs = unreal.IKRetargetBatchOperationInputs()
    loco_inputs.assets_to_retarget = loco_assets
    loco_inputs.source_mesh = alona_skm
    loco_inputs.target_mesh = skm
    loco_inputs.ik_retarget_asset = temp_rtg
    loco_inputs.target_path = anim_dir
    loco_inputs.search = "Alona"
    loco_inputs.replace = hero_name
    loco_inputs.include_referenced_assets = True
    loco_inputs.overwrite_existing_files = True
    created_loco = unreal.IKRetargetBatchOperation.run_batch_retarget(loco_inputs)

    # Delete temporary Alona->Hero retargeter immediately so zero /Hero_Alona references persist
    unreal.EditorAssetLibrary.delete_asset(temp_rtg_path)

    abp_path = f"{anim_dir}/ABP_BW_{hero_name}"
    abp = unreal.load_asset(abp_path)
    if not abp:
        raise RuntimeError(f"Expected retargeted AnimBlueprint not found at {abp_path}")
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)

    # 5. Retarget Hero-Specific Primary Attack Montage from Mannequin via RTG_Mannequin_To_<Hero>
    src_am_path = spec["source_primary_montage"]
    src_am_name = src_am_path.rsplit("/", 1)[-1]
    src_am_ad = ar.get_asset_by_object_path(f"{src_am_path}.{src_am_name}")

    # Clean old primary montage/sequence before overwrite if present
    target_am_path = f"{anim_dir}/{spec['primary_montage_name']}"
    target_seq_path = f"{anim_dir}/{spec['primary_seq_name']}"

    prim_inputs = unreal.IKRetargetBatchOperationInputs()
    prim_inputs.assets_to_retarget = [src_am_ad]
    prim_inputs.source_mesh = manny_skm
    prim_inputs.target_mesh = skm
    prim_inputs.ik_retarget_asset = rtg
    prim_inputs.target_path = anim_dir
    prim_inputs.prefix = f"RTG_{hero_name}_"
    prim_inputs.include_referenced_assets = True
    prim_inputs.overwrite_existing_files = True
    created_prim = unreal.IKRetargetBatchOperation.run_batch_retarget(prim_inputs)

    retargeted_am = None
    retargeted_seq = None
    for ad in created_prim:
        obj = ad.get_asset()
        if isinstance(obj, unreal.AnimMontage):
            retargeted_am = obj
        elif isinstance(obj, unreal.AnimSequence):
            retargeted_seq = obj

    if not retargeted_am:
        raise RuntimeError(f"Failed to retarget primary montage from {src_am_path}")

    if retargeted_seq:
        retargeted_seq.set_editor_property("enable_root_motion", False)
        _normalize_cryengine_100x_anim_sequence(retargeted_seq)
        seq_cur_path = retargeted_seq.get_outer().get_name()
        if seq_cur_path != target_seq_path:
            if unreal.EditorAssetLibrary.does_asset_exist(target_seq_path):
                unreal.EditorAssetLibrary.delete_asset(target_seq_path)
            unreal.EditorAssetLibrary.rename_asset(seq_cur_path, target_seq_path)
            retargeted_seq = unreal.load_asset(target_seq_path)

    retargeted_am.set_editor_property("enable_auto_blend_out", True)
    am_cur_path = retargeted_am.get_outer().get_name()
    if am_cur_path != target_am_path:
        if unreal.EditorAssetLibrary.does_asset_exist(target_am_path):
            unreal.EditorAssetLibrary.delete_asset(target_am_path)
        unreal.EditorAssetLibrary.rename_asset(am_cur_path, target_am_path)
        retargeted_am = unreal.load_asset(target_am_path)

    for ad in created_loco:
        aobj = ad.get_asset()
        if aobj:
            _normalize_cryengine_100x_anim_sequence(aobj)
            unreal.EditorAssetLibrary.save_loaded_asset(aobj, False)
    for ad in ar.get_assets_by_path(anim_dir, recursive=False):
        aobj = ad.get_asset()
        if isinstance(aobj, unreal.AnimSequence):
            _normalize_cryengine_100x_anim_sequence(aobj)
            unreal.EditorAssetLibrary.save_loaded_asset(aobj, False)
    if retargeted_seq:
        _normalize_cryengine_100x_anim_sequence(retargeted_seq)
        unreal.EditorAssetLibrary.save_loaded_asset(retargeted_seq, False)
    unreal.BlueprintEditorLibrary.compile_blueprint(abp)
    unreal.EditorAssetLibrary.save_loaded_asset(retargeted_am, False)
    unreal.EditorAssetLibrary.save_loaded_asset(abp, False)

    return abp, retargeted_am, socket_checks


def _wire_and_validate_hero_da(hero_name, spec, skm, weapons_map, abp, primary_am):
    da = unreal.load_asset(spec["hero_da"])
    if not da:
        raise RuntimeError(f"Missing Hero Data Asset: {spec['hero_da']}")

    # Preserve canonical DisplayName (e.g., 'Argus' on DA_BW_HeroData_Argus per HERO_CODENAME_MAP.md)
    cur_display = str(da.get_editor_property("DisplayName"))
    if cur_display != spec["expected_display_name"]:
        raise RuntimeError(f"Unexpected DisplayName '{cur_display}' on {spec['hero_da']} (expected '{spec['expected_display_name']}')")

    da.modify()
    da.set_editor_property("HeroMesh", skm)
    da.set_editor_property("ExpectedSkeleton", skm.get_editor_property("skeleton"))
    da.set_editor_property("AnimationClass", abp.generated_class())
    da.set_editor_property("AnimationLayerClass", None)
    da.set_editor_property(
        "HeroMeshRelativeTransform",
        unreal.Transform(
            unreal.Vector(0.0, 0.0, float(spec["mesh_z_offset"])),
            unreal.Rotator(roll=0.0, pitch=0.0, yaw=-90.0),
            unreal.Vector(1.0, 1.0, 1.0),
        ),
    )

    primary_w = next((w for w in spec["weapons"] if w["slot"] == "primary"), None)
    secondary_w = next((w for w in spec["weapons"] if w["slot"] == "secondary"), None)

    if primary_w:
        da.set_editor_property("HeroEquipmentMesh", weapons_map[primary_w["sm_name"]])
        da.set_editor_property("HeroEquipmentSocket", primary_w["socket_name"])
        da.set_editor_property("HeroEquipmentRelativeTransform", unreal.Transform())
    else:
        da.set_editor_property("HeroEquipmentMesh", None)
        da.set_editor_property("HeroEquipmentSocket", "None")

    if secondary_w:
        da.set_editor_property("HeroSecondaryEquipmentMesh", weapons_map[secondary_w["sm_name"]])
        da.set_editor_property("HeroSecondaryEquipmentSocket", secondary_w["socket_name"])
        da.set_editor_property("HeroSecondaryEquipmentRelativeTransform", unreal.Transform())
    else:
        da.set_editor_property("HeroSecondaryEquipmentMesh", None)
        da.set_editor_property("HeroSecondaryEquipmentSocket", "None")

    da.set_editor_property("PrimaryActionMontageOverride", primary_am)
    da.set_editor_property("RequiredMeshSockets", list(spec["required_sockets"]))
    da.set_editor_property("bRequirePhysicsAsset", True)
    da.set_editor_property("bRequireMultipleLODs", True)

    unreal.EditorAssetLibrary.save_loaded_asset(da, False)
    unreal.EditorAssetLibrary.save_directory(spec["base_dir"], only_if_is_dirty=True, recursive=True)

    # Validate DataAsset
    val_sub = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem)
    val_res, val_errs, val_warns = val_sub.is_object_valid(da, unreal.DataValidationUsecase.SAVE)

    # Audit cross-hero GFP dependencies across all packages in spec["plugin_mount"]
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    forbidden_mounts = {"/Hero_Alona", "/Hero_Rawlins", "/Hero_Morgan", "/Hero_Spartacus"} - {spec["plugin_mount"]}
    cross_refs = []
    for ad in ar.get_assets_by_path(spec["plugin_mount"], recursive=True):
        pkg = str(ad.package_name)
        for dep in (ar.get_dependencies(pkg, unreal.AssetRegistryDependencyOptions()) or []):
            sdep = str(dep)
            for fm in forbidden_mounts:
                if sdep.startswith(fm + "/"):
                    cross_refs.append({"from": pkg, "to": sdep})

    skm_sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    return {
        "hero_da": spec["hero_da"],
        "display_name": cur_display,
        "validation_result": str(val_res),
        "validation_errors": [str(e) for e in val_errs],
        "validation_warnings": [str(w) for w in val_warns],
        "skm_lods": skm_sub.get_lod_count(skm),
        "weapon_lods": {k: sm_sub.get_lod_count(v) for k, v in weapons_map.items()},
        "cross_hero_gfp_refs": cross_refs,
    }


def import_restored_heroes(hero_names=None, reimport_meshes_and_textures=True):
    if hero_names is None:
        hero_names = ["Rawlins", "Korryn", "Spartacus"]

    with open(BLENDER_TRANSFORMS_JSON, "r", encoding="utf-8") as f:
        blender_transforms = json.load(f)

    results = {}
    for hname in hero_names:
        spec = HERO_SPECS[hname]
        if reimport_meshes_and_textures:
            textures = _import_textures(spec)
            mi_map = _build_materials_and_instances(hname, spec, textures)
            skm, weapons_map = _import_skeletal_and_weapon_meshes(spec, mi_map)
        else:
            textures = None
            mi_map = None
            models_dir = f"{spec['base_dir']}/Models"
            skm = unreal.load_asset(f"{models_dir}/{spec['skm_name']}")
            weapons_map = {
                w["sm_name"]: unreal.load_asset(f"{models_dir}/Weapon/{w['sm_name']}")
                for w in spec["weapons"]
            }
        abp, primary_am, socket_checks = _build_ik_rigs_sockets_and_animations(hname, spec, skm, blender_transforms)
        val_info = _wire_and_validate_hero_da(hname, spec, skm, weapons_map, abp, primary_am)
        val_info["sockets"] = socket_checks

        for check_key, anim_name in [
            ("idle_pose_check", f"A_BW_{hname}_Idle"),
            ("primary_pose_check", spec["primary_seq_name"]),
        ]:
            anim_obj = unreal.load_asset(f"{spec['base_dir']}/Animations/{anim_name}")
            if anim_obj:
                sx = unreal.AnimationLibrary.get_bone_pose_for_frame(anim_obj, "S_XForm", 0, False)
                br = unreal.AnimationLibrary.get_bone_pose_for_frame(anim_obj, "bind_root", 0, False)
                val_info[check_key] = {
                    "S_XForm_scale": [round(sx.scale3d.x, 3), round(sx.scale3d.y, 3), round(sx.scale3d.z, 3)],
                    "bind_root_loc": [round(br.translation.x, 4), round(br.translation.y, 4), round(br.translation.z, 4)],
                }
        results[hname] = val_info

        # Release local wrappers between heroes
        del textures, mi_map, skm, weapons_map, abp, primary_am
        gc.collect()

    return results
