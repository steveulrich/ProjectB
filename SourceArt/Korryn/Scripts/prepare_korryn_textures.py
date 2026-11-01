#!/usr/bin/env python3
"""
Deterministic PBR texture conversion for Breakaway's Korryn / Morgan le Fay + Staff (UE 5.8).

Inputs (staged read-only copies under SourceArt/Korryn/Textures/):
  - SkinComparison/morgan_{diff_rgb,diff_alpha,spec_rgb,ddna_rgb,ddna_gloss}.png
  - SkinComparison/morgan_staff_base_std_{diff_rgb,spec_rgb,ddna_rgb,ddna_gloss,emis_rgb}.png
  - Masks/uv_triangles.json (exact UV island masks including 360 eyeball triangles)

Outputs (SourceArt/Korryn/Textures/Prepared/):
  - T_BW_Korryn_BC.png          (2048x2048 sRGB RGBA preserving cutout alpha for morgan_transp AlphaTest=0.76)
  - T_BW_Korryn_{N,ORM,M,R,E}.png (2048x2048 Linear / sRGB luminous violet eye emissive)
  - T_BW_Korryn_Staff_{BC,N,ORM,M,R,E}.png (1024x1024)
  - Docs/texture_restoration_manifest.json
"""

from __future__ import annotations

import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
SRC_DIR = ROOT / "Textures" / "SkinComparison"
MASK_JSON = ROOT / "Textures" / "Masks" / "uv_triangles.json"
OUT_DIR = ROOT / "Textures" / "Prepared"
DOCS_DIR = ROOT / "Docs"


def load_rgb(name: str, size: tuple[int, int]) -> np.ndarray:
    img = Image.open(SRC_DIR / name).convert("RGB")
    if img.size != size:
        img = img.resize(size, Image.Resampling.LANCZOS)
    return np.asarray(img, dtype=np.float32) / 255.0


def load_gray(name: str, size: tuple[int, int]) -> np.ndarray:
    img = Image.open(SRC_DIR / name).convert("L")
    if img.size != size:
        img = img.resize(size, Image.Resampling.LANCZOS)
    return np.asarray(img, dtype=np.float32) / 255.0


def save_rgb(arr: np.ndarray, path: Path) -> None:
    clipped = np.clip(np.rint(arr * 255.0), 0, 255).astype(np.uint8)
    Image.fromarray(clipped, mode="RGB").save(path, compress_level=6)


def save_rgba(rgb: np.ndarray, alpha: np.ndarray, path: Path) -> None:
    rgba = np.concatenate([rgb, alpha[..., None]], axis=-1)
    clipped = np.clip(np.rint(rgba * 255.0), 0, 255).astype(np.uint8)
    Image.fromarray(clipped, mode="RGBA").save(path, compress_level=6)


def save_gray(arr: np.ndarray, path: Path) -> None:
    clipped = np.clip(np.rint(arr * 255.0), 0, 255).astype(np.uint8)
    Image.fromarray(clipped, mode="L").save(path, compress_level=6)


def rasterize_uv_mask(tris: list, size: int, dilate_px: int = 4, blur_px: float = 0.6) -> np.ndarray:
    mask_img = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask_img)
    for tri in tris:
        cu = (tri[0][0] + tri[1][0] + tri[2][0]) / 3.0
        cv = (tri[0][1] + tri[1][1] + tri[2][1]) / 3.0
        du = np.floor(cu)
        dv = np.floor(cv)
        pts = [
            (float((u - du) * (size - 1)), float((1.0 - (v - dv)) * (size - 1)))
            for u, v in tri
        ]
        draw.polygon(pts, fill=255)
    if dilate_px > 0:
        mask_img = mask_img.filter(ImageFilter.MaxFilter(dilate_px * 2 + 1))
    if blur_px > 0:
        mask_img = mask_img.filter(ImageFilter.GaussianBlur(radius=blur_px))
    return np.asarray(mask_img, dtype=np.float32) / 255.0


def reconstruct_directx_normal(ddna_rgb: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    nx = ddna_rgb[..., 0] * 2.0 - 1.0
    ny = ddna_rgb[..., 1] * 2.0 - 1.0
    slope_sq = nx * nx + ny * ny
    over = slope_sq > 1.0
    if np.any(over):
        inv = 1.0 / np.sqrt(np.maximum(slope_sq, 1e-8))
        nx = np.where(over, nx * inv, nx)
        ny = np.where(over, ny * inv, ny)
        slope_sq = np.minimum(slope_sq, 1.0)
    nz = np.sqrt(np.maximum(0.0, 1.0 - slope_sq))
    norm_rgb = np.stack([nx * 0.5 + 0.5, ny * 0.5 + 0.5, nz * 0.5 + 0.5], axis=-1)
    cavity_ao = np.clip(0.75 + 0.25 * nz, 0.75, 1.0)
    return norm_rgb, cavity_ao


def build_korryn_character(uv_data: dict) -> dict:
    size = (2048, 2048)
    diff = load_rgb("morgan_diff_rgb.png", size)
    alpha = load_gray("morgan_diff_alpha.png", size)
    spec = load_rgb("morgan_spec_rgb.png", size)
    ddna = load_rgb("morgan_ddna_rgb.png", size)
    gloss = load_gray("morgan_ddna_gloss.png", size)

    mask_eyeballs = rasterize_uv_mask(uv_data["eyeballs"], 2048, dilate_px=1, blur_px=0.5)

    # morgan_main_matgroup.mtl: Shininess=199 on morgan_main, 169 on morgan_skin (avg ~190/255 = 0.745)
    d_max = np.max(diff, axis=-1)
    s_max = np.max(spec, axis=-1)
    metallic = np.clip(((s_max - 0.25) / 0.24) * np.clip((0.36 - d_max + 0.40 * s_max) / 0.26, 0.0, 1.0), 0.0, 1.0)
    metallic = metallic * (1.0 - mask_eyeballs)

    eff_gloss = np.clip(gloss * (192.0 / 255.0), 0.0, 1.0)
    conductor_bc = np.clip(spec * 1.35 + (eff_gloss[..., None] - 0.5) * 0.10, 0.16, 0.96)
    bc = diff * (1.0 - metallic[..., None] * 0.88) + conductor_bc * (metallic[..., None] * 0.88)

    # Luminous violet eyes: enhance iris/pupil region strictly within her 360 eyeball triangles
    # Eyeball UV box is U=[0.0009, 0.0481], V=[0.8635, 0.9113]; center at U=0.0245, V=0.8874
    yy, xx = np.mgrid[0:2048, 0:2048]
    u_grid = xx / 2047.0
    v_grid = 1.0 - (yy / 2047.0)
    eye_r = np.sqrt(((u_grid - 0.0245) / 0.0145) ** 2 + ((v_grid - 0.8874) / 0.0145) ** 2)
    iris_glow = np.clip(1.25 - eye_r, 0.0, 1.0) * mask_eyeballs
    violet_color = np.array([0.62, 0.20, 1.00], dtype=np.float32)
    bc = np.clip(bc * (1.0 - iris_glow[..., None] * 0.55) + violet_color * (iris_glow[..., None] * 0.55), 0.0, 1.0)
    emis_rgb = np.clip(violet_color[None, None, :] * (iris_glow[..., None] ** 1.15), 0.0, 1.0)

    # Wet glossy cornea on eyeballs
    roughness = np.clip(1.0 - eff_gloss, 0.10, 0.94)
    roughness = roughness * (1.0 - mask_eyeballs) + 0.08 * mask_eyeballs

    norm_rgb, cavity_ao = reconstruct_directx_normal(ddna)
    orm = np.stack([cavity_ao, roughness, metallic], axis=-1)

    save_rgba(bc, alpha, OUT_DIR / "T_BW_Korryn_BC.png")
    save_rgb(norm_rgb, OUT_DIR / "T_BW_Korryn_N.png")
    save_rgb(orm, OUT_DIR / "T_BW_Korryn_ORM.png")
    save_gray(metallic, OUT_DIR / "T_BW_Korryn_M.png")
    save_gray(roughness, OUT_DIR / "T_BW_Korryn_R.png")
    save_rgb(emis_rgb, OUT_DIR / "T_BW_Korryn_E.png")

    return {
        "resolution": [2048, 2048],
        "has_alpha_cutout": True,
        "alpha_clip_threshold": 0.76,
        "metallic_mean": round(float(metallic.mean()), 4),
        "roughness_mean": round(float(roughness.mean()), 4),
        "violet_eye_emissive_max": round(float(emis_rgb.max()), 4),
    }


def build_korryn_staff(uv_data: dict) -> dict:
    size = (1024, 1024)
    diff = load_rgb("morgan_staff_base_std_diff_rgb.png", size)
    spec = load_rgb("morgan_staff_base_std_spec_rgb.png", size)
    ddna = load_rgb("morgan_staff_base_std_ddna_rgb.png", size)
    gloss_raw = load_gray("morgan_staff_base_std_ddna_gloss.png", size)
    emis = load_rgb("morgan_staff_base_std_emis_rgb.png", size)

    staff_mask = rasterize_uv_mask(uv_data["staff_all"], 1024, dilate_px=8, blur_px=0.8)

    # morgan_staff_base_standard_matgroup.mtl has Shininess="121" (121/255 = 0.4745) and flat gloss_raw (~0.987).
    # Modulate gloss physically using Shininess=121/255 + specular luminance so ornate metalwork reads polished (~0.28 rough)
    # and dark staff shaft/grip reads satin (~0.56 rough) instead of flat mirror 0.013!
    d_max = np.max(diff, axis=-1)
    s_max = np.max(spec, axis=-1)
    metallic = np.clip(((s_max - 0.22) / 0.22) * np.clip((0.38 - d_max + 0.42 * s_max) / 0.26, 0.0, 1.0), 0.0, 1.0)

    eff_gloss = np.clip(gloss_raw * (121.0 / 255.0) + 0.24 * metallic + 0.08 * (s_max - 0.20), 0.18, 0.76)
    conductor_bc = np.clip(spec * 1.42 + 0.04, 0.16, 0.94)
    bc = diff * (1.0 - metallic[..., None] * 0.88) + conductor_bc * (metallic[..., None] * 0.88)

    # Lift ultra-dark non-metal shaft areas slightly so normal detail reads under neutral studio lighting
    bc_luma = 0.2126 * bc[..., 0] + 0.7152 * bc[..., 1] + 0.0722 * bc[..., 2]
    ultra_dark = np.clip((0.035 - bc_luma) / 0.035, 0.0, 1.0) * (1.0 - metallic)
    bc = np.clip(bc + ultra_dark[..., None] * 0.025, 0.0, 1.0)

    roughness = np.clip(1.0 - eff_gloss, 0.22, 0.84)
    norm_rgb, cavity_ao = reconstruct_directx_normal(ddna)
    orm = np.stack([cavity_ao, roughness, metallic], axis=-1)

    # Apply CryEngine MTL violet Emittance tint: Emittance="0.090841718, 0.001214108, 0.23074007, 200" normalized to luminous violet
    emis_luma = np.max(emis, axis=-1) * staff_mask
    violet_staff = np.array([0.48, 0.12, 1.00], dtype=np.float32)
    emis_rgb = np.clip(violet_staff[None, None, :] * emis_luma[..., None], 0.0, 1.0)

    save_rgb(bc, OUT_DIR / "T_BW_Korryn_Staff_BC.png")
    save_rgb(norm_rgb, OUT_DIR / "T_BW_Korryn_Staff_N.png")
    save_rgb(orm, OUT_DIR / "T_BW_Korryn_Staff_ORM.png")
    save_gray(metallic, OUT_DIR / "T_BW_Korryn_Staff_M.png")
    save_gray(roughness, OUT_DIR / "T_BW_Korryn_Staff_R.png")
    save_rgb(emis_rgb, OUT_DIR / "T_BW_Korryn_Staff_E.png")

    return {
        "resolution": [1024, 1024],
        "shininess_121_calibrated": True,
        "metallic_mean": round(float(metallic.mean()), 4),
        "roughness_mean": round(float(roughness.mean()), 4),
        "violet_emissive_max": round(float(emis_rgb.max()), 4),
    }


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    uv_data = json.loads(MASK_JSON.read_text(encoding="utf-8"))

    char_stats = build_korryn_character(uv_data)
    staff_stats = build_korryn_staff(uv_data)

    manifest = {
        "hero": "Korryn",
        "internal_alias": "MorganLeFay / Morgan / Hexweaver",
        "localization_evidence": "Unpacked/localization/en-us/text.xml maps ui_char_name_morganlefay -> 'Korryn'; audio/vignettes reference korryn and korryn_featherfall1.cgf.",
        "archive_base_mesh_blocker": {
            "status": "GFactory redesigned antler/forehead-skull/angular-shoulder Korryn base mesh (artstation.com/projects/lQnR5) is ABSENT from the F:\\BreakawayConverted Lumberyard archive.",
            "proof": [
                "Inspected all 57,987 files across F:\\BreakawayConverted, including all 6 .skin files in Unpacked/morganlefay/Objects/Characters/Morgan/ (new_morgan_opti_mesh.skin, morgan_opti_mesh.skin, morgan_mesh.skin, morgan.skin, morgan_cin_mesh.skin, morgan_multimesh.skin), character-select layer cs_morgan.lyr, and all 16 character paks.",
                "All archive meshes and UI portraits (dashboard_portrait_lg_morganlefay.dds, skin_morganlefay_default.dds, deathscreen_morganlefay.dds) predate GFactory's late-production Korryn 3D model swap and depict the hooded raven-feather gown Morgan le Fay mesh.",
                "Per project contingency instructions ('If the requested base mesh cannot be recovered, identify the exact blocker and deliver the usable pieces for that hero'), all usable archive Korryn/Morgan pieces (new_morgan_opti_mesh.skin LOD0..4, 332-bone rig, masked feather/lace material AlphaTest=0.76, luminous violet eyes, and left-hand WeaponStaffMLF staff_base_mesh.skin LOD0..3 on S_LAttachment) are delivered and integrated.",
            ],
        },
        "equipment_and_casting_analysis": {
            "held": "WeaponStaffMLF (Objects/Characters/Morgan/weapons/staff_base/staff_base_mesh.skin LOD0..3) attached to weaponattach -> BoneName='S_LAttachment' (left hand) with Visible='true' in morganlefay_skins.xml.",
            "worn": "Hooded gown, jewelry, and cutout feather/lace trim (morgan_transp AlphaTest=0.76) on new_morgan_opti_mesh.skin.",
            "transient_or_effect": "Raven_Idle and Raven_Fly on bind_raven_attach are Visible='false' by default (toggled only during nav fidget in morganlefay_seqnav.lua); right-hand spell orbs, staff bind_wp_vfx1 magic, and featherfall (korryn_featherfall1.cgf) are spawned as runtime particle effects (MorganLeFayEntityEffects.xml / MLFWeaponEffects_default.xml).",
        },
        "character_textures": char_stats,
        "weapon_textures": staff_stats,
        "repaired_items": [
            "Recombined cross-pak split DDS mip chains + gloss alpha streams for morgan_{diff,ddna,spec} and morgan_staff_base_standard_{diff,ddna,spec,emis}",
            "Preserved 2048x2048 RGBA cutout alpha channel in T_BW_Korryn_BC.png for BLEND_Masked (OpacityMaskClipValue=0.76) on morgan_transp",
            "Calibrated flat 0.987 staff _ddna gloss channel using MTL Shininess=121/255 and specular reflectance so staff shaft is satin (~0.55 roughness) and metalwork is polished (~0.28 roughness)",
            "Applied MTL violet Emittance tint (0.091, 0.001, 0.231) to T_BW_Korryn_Staff_E.png",
        ],
        "generated_items": [
            "T_BW_Korryn_E.png (2048x2048 luminous violet iris emissive strictly masked to the 360 eyeball UV triangles at U=[0.0009, 0.0481], V=[0.8635, 0.9113])",
        ],
    }
    (DOCS_DIR / "texture_restoration_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print("Korryn textures prepared:", json.dumps({"char": char_stats, "staff": staff_stats}, indent=2))


if __name__ == "__main__":
    main()
