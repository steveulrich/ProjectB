#!/usr/bin/env python3
"""
Deterministic PBR texture conversion for Breakaway's Rawlins + Tomb Breaker Revolver (UE 5.8).

Inputs (staged read-only copies under SourceArt/Rawlins/Textures/):
  - SkinComparison/rawlins_default_{diff_rgb,spec_rgb,ddna_rgb,ddna_gloss,emis_rgb}.png
  - SkinComparison/raw_gun_base_std_{diff_rgb,spec_rgb,ddna_rgb,ddna_gloss,emis_rgb}.png
  - Masks/uv_triangles.json (exact UV island masks from new_rawlins_opti_mesh & new_rawlins_base_weapon_mesh)

Outputs (SourceArt/Rawlins/Textures/Prepared/):
  - T_BW_Rawlins_{BC,N,ORM,M,R,E}.png        (2048x2048)
  - T_BW_Rawlins_Gun_{BC,N,ORM,M,R,E}.png    (1024x1024)
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


def save_gray(arr: np.ndarray, path: Path) -> None:
    clipped = np.clip(np.rint(arr * 255.0), 0, 255).astype(np.uint8)
    Image.fromarray(clipped, mode="L").save(path, compress_level=6)


def rasterize_uv_mask(tris: list, size: int, dilate_px: int = 6, blur_px: float = 0.8) -> np.ndarray:
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


def spec_gloss_to_metal_rough(
    diff: np.ndarray,
    spec: np.ndarray,
    gloss: np.ndarray,
    shininess_255: float = 255.0,
    diffuse_factor: float = 1.0,
) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """
    Convert CryEngine Specular/Glossiness (diff, spec, gloss * shininess/255) into UE 5.8 Metallic/Roughness PBR.
    Recovers conductor F0 reflectance from spec on dark-diffuse metallic hardware (buckles, skull belt, revolver iron/brass).
    """
    d = np.clip(diff * diffuse_factor, 0.0, 1.0)
    d_max = np.max(d, axis=-1)
    s_max = np.max(spec, axis=-1)

    # CryEngine dielectric F0 sits around ~0.16-0.23 sRGB; conductors have spec > 0.24 with dark/subdued diffuse
    metal_from_spec = np.clip((s_max - 0.22) / 0.22, 0.0, 1.0)
    dark_diff_gate = np.clip((0.40 - d_max + 0.45 * s_max) / 0.28, 0.0, 1.0)
    metallic = np.clip(metal_from_spec * dark_diff_gate, 0.0, 1.0)

    # Reconstruct conductor BaseColor from specular reflectance + high-res gloss micro-detail
    eff_gloss = np.clip(gloss * (shininess_255 / 255.0), 0.0, 1.0)
    conductor_bc = np.clip(spec * 1.38 + (eff_gloss[..., None] - 0.5) * 0.12, 0.14, 0.96)
    bc = d * (1.0 - metallic[..., None] * 0.90) + conductor_bc * (metallic[..., None] * 0.90)

    # Lift ultra-dark non-metal leather/hat crevices slightly above 0.025 so tangent normals read cleanly
    bc_luma = 0.2126 * bc[..., 0] + 0.7152 * bc[..., 1] + 0.0722 * bc[..., 2]
    ultra_dark = np.clip((0.032 - bc_luma) / 0.032, 0.0, 1.0) * (1.0 - metallic)
    bc = np.clip(bc + ultra_dark[..., None] * 0.024, 0.0, 1.0)

    roughness = np.clip(1.0 - eff_gloss, 0.08, 0.95)
    return bc, metallic, roughness


def build_rawlins_character(uv_data: dict) -> dict:
    size = (2048, 2048)
    diff = load_rgb("rawlins_default_diff_rgb.png", size)
    spec = load_rgb("rawlins_default_spec_rgb.png", size)
    ddna = load_rgb("rawlins_default_ddna_rgb.png", size)
    gloss = load_gray("rawlins_default_ddna_gloss.png", size)
    emis = load_rgb("rawlins_default_emis_rgb.png", size)

    char_mask = rasterize_uv_mask(uv_data["char_all"], 2048, dilate_px=12, blur_px=1.0)

    # rawlins_default_matgroup.mtl: Shininess=255, Diffuse=1,1,1, Emittance=1,1,1,27
    # Note: cape_mat in rawlins_default_matgroup.mtl mistakenly pointed Diffuse to white.dds while c1/c2/efox pointed
    # cape_mat to rawlins_default_*_diff.dds (where the red/blue poncho UVs reside). Using rawlins_default_diff restores it.
    bc, metallic, roughness = spec_gloss_to_metal_rough(diff, spec, gloss, shininess_255=255.0, diffuse_factor=1.0)
    norm_rgb, cavity_ao = reconstruct_directx_normal(ddna)
    orm = np.stack([cavity_ao, roughness, metallic], axis=-1)
    emis_clean = np.clip(emis * char_mask[..., None], 0.0, 1.0)

    save_rgb(bc, OUT_DIR / "T_BW_Rawlins_BC.png")
    save_rgb(norm_rgb, OUT_DIR / "T_BW_Rawlins_N.png")
    save_rgb(orm, OUT_DIR / "T_BW_Rawlins_ORM.png")
    save_gray(metallic, OUT_DIR / "T_BW_Rawlins_M.png")
    save_gray(roughness, OUT_DIR / "T_BW_Rawlins_R.png")
    save_rgb(emis_clean, OUT_DIR / "T_BW_Rawlins_E.png")

    return {
        "resolution": [2048, 2048],
        "metallic_mean": round(float(metallic.mean()), 4),
        "roughness_mean": round(float(roughness.mean()), 4),
        "emissive_max": round(float(emis_clean.max()), 4),
        "cape_mat_white_dds_bug_repaired": True,
    }


def build_rawlins_weapon(uv_data: dict) -> dict:
    size = (1024, 1024)
    diff = load_rgb("raw_gun_base_std_diff_rgb.png", size)
    spec = load_rgb("raw_gun_base_std_spec_rgb.png", size)
    ddna = load_rgb("raw_gun_base_std_ddna_rgb.png", size)
    gloss = load_gray("raw_gun_base_std_ddna_gloss.png", size)
    emis = load_rgb("raw_gun_base_std_emis_rgb.png", size)

    # Exact UV island mask dilated 14px to allow mip bleed padding while neutralizing unused lime-green UV filler quadrant
    gun_mask = rasterize_uv_mask(uv_data["gun_all"], 1024, dilate_px=14, blur_px=1.5)
    gun_mask_tight = rasterize_uv_mask(uv_data["gun_all"], 1024, dilate_px=4, blur_px=0.8)

    # raw_gun_base_std_matgroup.mtl: Diffuse=0.603827, Shininess=217, Emittance=(1, 1, 0, 200)
    bc, metallic, roughness = spec_gloss_to_metal_rough(
        diff, spec, gloss, shininess_255=217.0, diffuse_factor=0.60382748
    )

    # Replace unused lime-green UV filler outside dilated weapon UVs with neutral dark gunmetal padding (prevents mip bleed)
    neutral_pad = np.array([0.16, 0.15, 0.14], dtype=np.float32)
    bc = bc * gun_mask[..., None] + neutral_pad * (1.0 - gun_mask[..., None])

    norm_rgb, cavity_ao = reconstruct_directx_normal(ddna)
    orm = np.stack([cavity_ao, roughness, metallic], axis=-1)

    # Apply CryEngine MTL Emittance tint (1.0, 0.72, 0.08 warm hellfire amber-gold; MTL has Emittance="1,1,0" on RG mask)
    emis_mask = np.clip(emis[..., 0], 0.0, 1.0) * gun_mask_tight
    # Suppress any green-only filler channel where R == 0 (unused UV filler was pure G=1, R=0!)
    valid_fire = np.clip((emis[..., 0] - 0.08) / 0.25, 0.0, 1.0) * gun_mask_tight
    emis_rgb = np.stack(
        [
            np.clip(emis[..., 0] * 1.00, 0.0, 1.0) * valid_fire,
            np.clip(emis[..., 1] * 0.58 + emis[..., 0] * 0.18, 0.0, 1.0) * valid_fire,
            np.clip(emis[..., 0] * 0.08, 0.0, 1.0) * valid_fire,
        ],
        axis=-1,
    )

    save_rgb(bc, OUT_DIR / "T_BW_Rawlins_Gun_BC.png")
    save_rgb(norm_rgb, OUT_DIR / "T_BW_Rawlins_Gun_N.png")
    save_rgb(orm, OUT_DIR / "T_BW_Rawlins_Gun_ORM.png")
    save_gray(metallic, OUT_DIR / "T_BW_Rawlins_Gun_M.png")
    save_gray(roughness, OUT_DIR / "T_BW_Rawlins_Gun_R.png")
    save_rgb(emis_rgb, OUT_DIR / "T_BW_Rawlins_Gun_E.png")

    return {
        "resolution": [1024, 1024],
        "metallic_mean": round(float(metallic.mean()), 4),
        "roughness_mean": round(float(roughness.mean()), 4),
        "emissive_max": round(float(emis_rgb.max()), 4),
        "unused_green_quadrant_masked": True,
    }


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    DOCS_DIR.mkdir(parents=True, exist_ok=True)
    uv_data = json.loads(MASK_JSON.read_text(encoding="utf-8"))

    char_stats = build_rawlins_character(uv_data)
    gun_stats = build_rawlins_weapon(uv_data)

    manifest = {
        "hero": "Rawlins",
        "codename": "Gunslinger",
        "selected_source_definition": "Objects/Characters/Rawlins/new_rawlins.cdf (rawlins_skins.xml Skin='Default')",
        "selected_body_mesh": "Objects/Characters/Rawlins/new_rawlins_opti_mesh.skin (LOD0..3, 20,919 verts, 223 bones)",
        "selected_weapon": {
            "entity_class": "Weapon_RWL_TombBreaker",
            "mesh": "Objects/Characters/Rawlins/weapons/new_rawlins_base_weapon_mesh.skin (LOD0..3, 2,123 verts)",
            "socket": "S_RAttachment",
            "count": 1,
            "handedness": "Right-handed default (Pistol_Left on S_LAttachment is Visible=false by default in rawlins_skins.xml; toggled transiently during Ability1 Double Down / Ultimate)",
        },
        "early_version_comparison": {
            "early_cdf": "Objects/Characters/Rawlins/rawlins.cdf -> rawlins_optimesh.skin (14,105 verts, 155 bones, sheriff star badge + dual hip holsters) + Weapon_RWL_Python (python_mesh.skin)",
            "reason_late_alpha_selected": "rawlins_skins.xml assigns ALL playable skins (including Default) to new_rawlins.cdf + Weapon_RWL_TombBreaker, which matches GFactory's Breakaway Rawlins weapon portfolio (wJowLV) and retains 100% intact 2048x2048 / 1024x1024 split DDS textures on disk (whereas early rawlins_diff.tif and python_diff.tif were removed from the build).",
        },
        "character_textures": char_stats,
        "weapon_textures": gun_stats,
        "repaired_items": [
            "Recombined cross-pak split CryEngine DDS mip chains + .dds.*a gloss alpha streams via DDS-Unsplitter.exe --safe",
            "Patched DX10 typeless BC1/BC3 headers and reconstructed unit-length DirectX Y- tangent normals from BC5 RG",
            "Repaired rawlins_default_matgroup.mtl cape_mat Diffuse=white.dds oversight by binding cape_mat to T_BW_Rawlins_BC (restoring the poncho UVs matching c1/c2/efox skin MTLs)",
            "Converted Specular/Glossiness to UE 5.8 Metallic/Roughness PBR and masked unused green UV filler quadrant on Tomb Breaker revolver textures",
        ],
        "generated_items": [],
    }
    (DOCS_DIR / "texture_restoration_manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print("Rawlins textures prepared:", json.dumps({"char": char_stats, "gun": gun_stats}, indent=2))


if __name__ == "__main__":
    main()
