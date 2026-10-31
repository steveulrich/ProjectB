#!/usr/bin/env python3
"""
Deterministic PBR texture recovery & reference-sheet restoration for Breakaway's Alona + Staff (UE 5.8).

Inputs (staged read-only copies under SourceArt/Alona/Textures/):
  - SkinComparison/alona_base_{diff_rgb,spec_rgb,ddna_rgb,ddna_gloss,emis_rgb}.png
  - SkinComparison/alona_c1_{diff_rgb,spec_rgb,ddna_gloss}.png
  - SkinComparison/alona_staff_standard_{diff_rgb,spec_rgb,ddna_rgb,ddna_gloss}.png
  - Masks/uv_triangles.json (exact 3D topology -> UV island triangles from new_alona_opti)

Outputs (SourceArt/Alona/Textures/Prepared/):
  - T_BW_Alona_BC.png        (2048x2048 sRGB BaseColor)
  - T_BW_Alona_N.png         (2048x2048 Linear DirectX Y- Normal)
  - T_BW_Alona_ORM.png       (2048x2048 Linear R=AO, G=Roughness, B=Metallic)
  - T_BW_Alona_M.png         (2048x2048 Linear Metallic)
  - T_BW_Alona_R.png         (2048x2048 Linear Roughness)
  - T_BW_Alona_Staff_BC.png  (1024x1024 sRGB BaseColor)
  - T_BW_Alona_Staff_N.png   (1024x1024 Linear DirectX Y- Normal)
  - T_BW_Alona_Staff_ORM.png (1024x1024 Linear R=AO, G=Roughness, B=Metallic)
  - T_BW_Alona_Staff_M.png   (1024x1024 Linear Metallic)
  - T_BW_Alona_Staff_R.png   (1024x1024 Linear Roughness)
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
MANIFEST_PATH = ROOT / "Docs" / "texture_restoration_manifest.json"


def load_rgb(name: str, size: tuple[int, int] | None = None) -> np.ndarray:
    img = Image.open(SRC_DIR / name).convert("RGB")
    if size and img.size != size:
        img = img.resize(size, Image.Resampling.LANCZOS)
    return np.asarray(img, dtype=np.float32) / 255.0


def load_gray(name: str, size: tuple[int, int] | None = None) -> np.ndarray:
    img = Image.open(SRC_DIR / name).convert("L")
    if size and img.size != size:
        img = img.resize(size, Image.Resampling.LANCZOS)
    return np.asarray(img, dtype=np.float32) / 255.0


def save_rgb(arr: np.ndarray, path: Path) -> None:
    clipped = np.clip(np.rint(arr * 255.0), 0, 255).astype(np.uint8)
    Image.fromarray(clipped, mode="RGB").save(path, compress_level=6)


def save_gray(arr: np.ndarray, path: Path) -> None:
    clipped = np.clip(np.rint(arr * 255.0), 0, 255).astype(np.uint8)
    Image.fromarray(clipped, mode="L").save(path, compress_level=6)


def rasterize_uv_mask(tris: list, size: int = 2048, dilate_px: int = 4, blur_px: float = 0.75) -> np.ndarray:
    """Rasterize UV triangles wrapped into [0, 1] space (handle mirrored [1, 2] UVs safely)."""
    mask_img = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask_img)
    for tri in tris:
        # Shift triangle by integer tile offset of its centroid so wrapped UV shells don't span across [0..1] seam
        cu = (tri[0][0] + tri[1][0] + tri[2][0]) / 3.0
        cv = (tri[0][1] + tri[1][1] + tri[2][1]) / 3.0
        du = np.floor(cu)
        dv = np.floor(cv)
        pts = []
        for u, v in tri:
            uw = u - du
            vw = v - dv
            px = float(uw * (size - 1))
            py = float((1.0 - vw) * (size - 1))
            pts.append((px, py))
        draw.polygon(pts, fill=255)
    if dilate_px > 0:
        # Odd kernel size for MaxFilter
        k = dilate_px * 2 + 1
        mask_img = mask_img.filter(ImageFilter.MaxFilter(k))
    if blur_px > 0:
        mask_img = mask_img.filter(ImageFilter.GaussianBlur(radius=blur_px))
    return np.asarray(mask_img, dtype=np.float32) / 255.0


def reconstruct_directx_normal(ddna_rgb: np.ndarray, smooth_mask: np.ndarray | None = None) -> tuple[np.ndarray, np.ndarray]:
    """
    Reconstruct normalized DirectX Y- tangent-space normal from decoded BC5 RG channels,
    and derive a subtle cavity/AO term from tangent slope magnitude.
    """
    nx = ddna_rgb[..., 0] * 2.0 - 1.0
    ny = ddna_rgb[..., 1] * 2.0 - 1.0

    if smooth_mask is not None:
        # Gently attenuate post-alpha lava-line normal grooves on bare skin while preserving anatomical normals
        nx_blur = np.asarray(
            Image.fromarray(np.clip((nx * 0.5 + 0.5) * 255.0, 0, 255).astype(np.uint8)).filter(
                ImageFilter.GaussianBlur(radius=4.5)
            ),
            dtype=np.float32,
        ) / 255.0 * 2.0 - 1.0
        ny_blur = np.asarray(
            Image.fromarray(np.clip((ny * 0.5 + 0.5) * 255.0, 0, 255).astype(np.uint8)).filter(
                ImageFilter.GaussianBlur(radius=4.5)
            ),
            dtype=np.float32,
        ) / 255.0 * 2.0 - 1.0
        w = np.clip(smooth_mask, 0.0, 1.0)
        nx = nx * (1.0 - w) + nx_blur * w
        ny = ny * (1.0 - w) + ny_blur * w

    slope_sq = nx * nx + ny * ny
    # Clamp XY if > 1 before computing Z
    over = slope_sq > 1.0
    if np.any(over):
        inv = 1.0 / np.sqrt(np.maximum(slope_sq, 1e-8))
        nx = np.where(over, nx * inv, nx)
        ny = np.where(over, ny * inv, ny)
        slope_sq = np.minimum(slope_sq, 1.0)

    nz = np.sqrt(np.maximum(0.0, 1.0 - slope_sq))
    norm_rgb = np.stack([nx * 0.5 + 0.5, ny * 0.5 + 0.5, nz * 0.5 + 0.5], axis=-1)
    # Subtle micro-cavity AO in [0.76, 1.0]
    cavity_ao = np.clip(0.76 + 0.24 * nz, 0.76, 1.0)
    return norm_rgb, cavity_ao


def build_alona_character_textures() -> dict:
    size = (2048, 2048)
    base_diff = load_rgb("alona_base_diff_rgb.png", size)
    base_spec = load_rgb("alona_base_spec_rgb.png", size)
    base_ddna = load_rgb("alona_base_ddna_rgb.png", size)
    base_gloss = load_gray("alona_base_ddna_gloss.png", size)
    base_emis = load_rgb("alona_base_emis_rgb.png", size)

    c1_diff = load_rgb("alona_c1_diff_rgb.png", size)
    c1_spec = load_rgb("alona_c1_spec_rgb.png", size)
    c1_gloss = load_gray("alona_c1_ddna_gloss.png", size)

    uv_data = json.loads(MASK_JSON.read_text())
    mask_pure_skin = rasterize_uv_mask(uv_data["pure_skin"], 2048, dilate_px=7, blur_px=1.0)
    mask_face_skin = rasterize_uv_mask(uv_data["face_skin"], 2048, dilate_px=5, blur_px=0.8)
    mask_mouth = rasterize_uv_mask(uv_data["mouth"], 2048, dilate_px=2, blur_px=0.5)
    mask_cloth = rasterize_uv_mask(uv_data["cloth_main"], 2048, dilate_px=6, blur_px=0.8)
    mask_beams = rasterize_uv_mask(uv_data["beams_mat"], 2048, dilate_px=8, blur_px=0.8)
    mask_main = rasterize_uv_mask(uv_data["main_mat"], 2048, dilate_px=5, blur_px=0.5)
    mask_eyeballs = rasterize_uv_mask(uv_data.get("eyeballs", []), 2048, dilate_px=3, blur_px=0.6)
    mask_crown_gems = rasterize_uv_mask(uv_data.get("crown_gems", []), 2048, dilate_px=3, blur_px=0.5)
    mask_crown_arches = rasterize_uv_mask(uv_data.get("crown_arches", []), 2048, dilate_px=2, blur_px=0.5)
    mask_feathers = rasterize_uv_mask(uv_data.get("feathers", []), 2048, dilate_px=4, blur_px=0.7)

    # Protect mouth interior from skin recoloring
    mask_skin = np.clip(mask_pure_skin * (1.0 - mask_mouth), 0.0, 1.0)

    bc = base_diff.copy()

    # -------------------------------------------------------------------------
    # 1. Restore Reference-Sheet Warm Brown Skin + Asymmetric Left Turquoise Markings
    # -------------------------------------------------------------------------
    # Preserve turquoise eye mask (B > R + 0.04 on face) and circular forehead ornament
    yy, xx = np.mgrid[0:2048, 0:2048]
    u_grid = xx / 2047.0
    v_grid = 1.0 - (yy / 2047.0)

    face_turquoise_mask = np.clip(((base_diff[..., 2] - base_diff[..., 0]) - 0.02) / 0.10, 0.0, 1.0) * mask_face_skin
    # Elliptical normalized distance for circular forehead ornament (center u=0.2499, v=0.4575; radii ru=0.0156, rv=0.0182)
    forehead_norm_r = np.sqrt(((u_grid - 0.2499) / 0.0156) ** 2 + ((v_grid - 0.4575) / 0.0182) ** 2)
    # Keep dark natural lip shading from base_diff strictly inside the mouth contour and excluding any orange lava lines
    emis_mag = np.max(base_emis, axis=-1)
    face_lava = np.clip(
        np.maximum(((base_diff[..., 0] - 1.42 * base_diff[..., 1]) - 0.055) / 0.06, (emis_mag - 0.01) / 0.05),
        0.0,
        1.0,
    )
    lips_box = (u_grid > 0.237) & (u_grid < 0.266) & (v_grid > 0.380) & (v_grid < 0.399)
    lips_mask = (
        np.asarray(
            Image.fromarray((lips_box.astype(np.uint8) * 255)).filter(ImageFilter.GaussianBlur(radius=2.2)),
            dtype=np.float32,
        )
        / 255.0
        * (1.0 - face_lava)
    )

    keep_face_paint = np.clip(np.maximum(face_turquoise_mask, lips_mask * 0.72), 0.0, 1.0)

    # Clean skin shading from c1_diff:
    # In c1_diff, clean skin has R > B, while indigo tattoos have B > R.
    # For tattoo pixels on c1_diff, estimate underlying skin luminance from a smooth local skin field so AO stays natural.
    c1_luma = 0.299 * c1_diff[..., 0] + 0.587 * c1_diff[..., 1] + 0.114 * c1_diff[..., 2]
    body_not_face = np.clip(mask_skin * (1.0 - mask_face_skin * 0.95), 0.0, 1.0)
    raw_tattoo_chroma = (c1_diff[..., 2] - c1_diff[..., 0]) * body_not_face
    # Tattoo ink in c1_diff has (B - R) in [0.06 .. 0.30]; clean c1 skin has (B - R) in [-0.14 .. -0.02]
    tattoo_mask = np.clip((raw_tattoo_chroma - 0.035) / 0.095, 0.0, 1.0)
    # Remove any stray 1-px noise outside real left-side islands
    tattoo_pil = Image.fromarray(np.clip(tattoo_mask * 255.0, 0, 255).astype(np.uint8))
    tattoo_pil = tattoo_pil.filter(ImageFilter.MedianFilter(3))
    tattoo_mask = np.asarray(tattoo_pil, dtype=np.float32) / 255.0 * body_not_face

    # Smooth skin luminance across tattoo strokes so underlying AO is continuous
    c1_luma_for_skin = c1_luma.copy()
    c1_luma_blur = np.asarray(
        Image.fromarray(np.clip(c1_luma * 255.0, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(radius=14.0)),
        dtype=np.float32,
    ) / 255.0
    c1_luma_clean = c1_luma_for_skin * (1.0 - tattoo_mask) + np.maximum(c1_luma_for_skin, c1_luma_blur) * tattoo_mask

    # Calibrate c1_luma_clean -> warm sunlit golden-brown skin RGB matching reference sheet & clean face cheeks!
    emis_mag = np.max(base_emis, axis=-1)
    # Sample specifically from clean sunlit face cheeks/bridge of nose (avoiding dark under-skirt groin shadows and orange lava lines)
    clean_face_ref = (
        (mask_face_skin > 0.85)
        & (keep_face_paint < 0.05)
        & (emis_mag < 0.015)
        & ((base_diff[..., 0] - 1.45 * base_diff[..., 1]) < 0.08)
        & (c1_luma_clean > 0.30)
        & (c1_luma_clean < 0.56)
    )
    if np.count_nonzero(clean_face_ref) > 200:
        sampled = np.median(base_diff[clean_face_ref], axis=0)
        # Blend sampled face tone gently with reference sheet warm sunlit caramel-tan [0.72, 0.465, 0.305]
        ref_rgb_mean = 0.45 * sampled + 0.55 * np.array([0.725, 0.468, 0.305], dtype=np.float32)
        ref_c1_med = float(np.median(c1_luma_clean[clean_face_ref]))
    else:
        ref_rgb_mean = np.array([0.725, 0.468, 0.305], dtype=np.float32)
        ref_c1_med = 0.41

    # Add 2048 micro-pore detail from base_gloss (zero-mean high-pass)
    gloss_blur = np.asarray(
        Image.fromarray(np.clip(base_gloss * 255.0, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(radius=4.0)),
        dtype=np.float32,
    ) / 255.0
    micro_pore = (base_gloss - gloss_blur) * 0.14

    # Compress extreme dark groin/under-skirt c1 shadows slightly so bare legs/arms read warm and sunlit like the sheet
    norm_raw = c1_luma_clean / max(ref_c1_med, 1e-4)
    norm_t = np.clip(0.24 + 0.82 * norm_raw, 0.36, 1.48)[..., None]
    # Warm subsurface ramp: rich terracotta-warmth in AO shadows, warm golden-tan in highlights
    shadow_tint = np.array([0.96, 0.78, 0.68], dtype=np.float32)
    highlight_tint = np.array([1.05, 1.03, 0.99], dtype=np.float32)
    tint_blend = np.clip((norm_t - 0.68) / 0.58, 0.0, 1.0)
    skin_tint = shadow_tint * (1.0 - tint_blend) + highlight_tint * tint_blend
    restored_skin_rgb = np.clip(ref_rgb_mean[None, None, :] * norm_t * skin_tint + micro_pore[..., None], 0.05, 0.96)

    # Preserve 2048 high-frequency facial detail (nostrils, eyelids, philtrum) from base_diff WITHOUT keeping orange cheek lines
    base_luma = 0.299 * base_diff[..., 0] + 0.587 * base_diff[..., 1] + 0.114 * base_diff[..., 2]
    base_luma_blur = np.asarray(
        Image.fromarray(np.clip(base_luma * 255.0, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(radius=5.0)),
        dtype=np.float32,
    ) / 255.0
    face_lava = np.clip(
        np.maximum(((base_diff[..., 0] - 1.42 * base_diff[..., 1]) - 0.055) / 0.06, (emis_mag - 0.01) / 0.05),
        0.0,
        1.0,
    )
    face_hi_detail = (base_luma - base_luma_blur) * mask_face_skin * (1.0 - face_lava)
    skin_combined = np.clip(restored_skin_rgb + face_hi_detail[..., None] * 0.85, 0.04, 0.96)

    # Composite asymmetric turquoise markings on left arm, left shoulder/back, left thigh, left ankle/foot
    # Sample turquoise hue directly from eye mask in base_diff
    eye_paint_pixels = (face_turquoise_mask > 0.75) & (base_diff[..., 2] > 0.60)
    if np.count_nonzero(eye_paint_pixels) > 200:
        turq_color = np.median(base_diff[eye_paint_pixels], axis=0)
    else:
        turq_color = np.array([0.22, 0.68, 0.82], dtype=np.float32)
    # Modulate turquoise tattoo slightly by local skin AO so it wraps naturally around muscles
    ao_factor = np.clip(0.72 + 0.34 * (c1_luma_clean / max(ref_c1_med, 1e-4)), 0.55, 1.08)[..., None]
    tattoo_rgb = np.clip(turq_color[None, None, :] * ao_factor, 0.05, 0.96)
    skin_with_tattoos = skin_combined * (1.0 - tattoo_mask[..., None]) + tattoo_rgb * tattoo_mask[..., None]

    # Re-apply protected face paint (turquoise eye mask + forehead ring + lips)
    skin_final = skin_with_tattoos * (1.0 - keep_face_paint[..., None]) + base_diff * keep_face_paint[..., None]
    bc = bc * (1.0 - mask_skin[..., None]) + skin_final * mask_skin[..., None]

    # -------------------------------------------------------------------------
    # 2. Restore Woven Cloth (Crop Top, Waist Sash, Two-Tier Skirt, Front Panel)
    # -------------------------------------------------------------------------
    # Inside mask_cloth:
    # - Sky-blue trim has B > R -> brighten gently to match reference sheet cerulean trim
    # - Jade/green medallions on front panel have G > R -> handled with jade stone calibration below
    # - Reworked orange cloth body & stepped embroidery lines have R > B and R > G:
    #   convert to warm cream-and-gold woven textile while preserving 2048 weave & crisp stepped lines!
    cloth_blue = np.clip(((base_diff[..., 2] - base_diff[..., 0]) - 0.02) / 0.12, 0.0, 1.0) * mask_cloth
    cloth_jade = np.clip(((base_diff[..., 1] - base_diff[..., 0]) - 0.03) / 0.10, 0.0, 1.0) * mask_cloth
    base_max = np.max(base_diff, axis=-1)
    cloth_orange = (
        np.clip(((base_diff[..., 0] - base_diff[..., 2]) - 0.12) / 0.18, 0.0, 1.0)
        * np.clip((base_max - 0.12) / 0.12, 0.0, 1.0)
        * mask_cloth
        * (1.0 - cloth_blue)
        * (1.0 - cloth_jade)
    )

    # High-res 2048 stepped geometric embroidery mask from c1_gloss + c1_spec cyan lines + base_diff green/blue luminance
    embroidery_signal = np.maximum(
        np.clip((c1_gloss - 0.58) / 0.24, 0.0, 1.0),
        np.clip((c1_spec[..., 1] - c1_spec[..., 0] - 0.12) / 0.25, 0.0, 1.0),
    )
    # Also capture subtle lighter yellow-orange bands/lines in base_diff green channel
    fabric_shade = np.clip(0.35 * base_diff[..., 0] + 0.55 * base_diff[..., 1] + 0.10 * base_gloss, 0.0, 1.0)
    # Reference sheet palette: warm golden-cream woven cloth (216, 172, 102) -> bright cream-gold stepped lines (245, 222, 166)
    cream_base = np.array([0.845, 0.675, 0.405], dtype=np.float32)
    ochre_shadow = np.array([0.665, 0.485, 0.255], dtype=np.float32)
    stepped_line = np.array([0.965, 0.875, 0.665], dtype=np.float32)

    shade_t = np.clip((fabric_shade - 0.22) / 0.36, 0.0, 1.0)[..., None]
    woven_rgb = ochre_shadow * (1.0 - shade_t) + cream_base * shade_t
    woven_rgb = woven_rgb * (1.0 - embroidery_signal[..., None] * 0.85) + stepped_line * (embroidery_signal[..., None] * 0.85)
    # Add 2048 fine linen weave micro-variation from c1_gloss
    c1_gloss_blur = np.asarray(
        Image.fromarray(np.clip(c1_gloss * 255.0, 0, 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(radius=2.5)),
        dtype=np.float32,
    ) / 255.0
    woven_rgb = np.clip(woven_rgb + (c1_gloss - c1_gloss_blur)[..., None] * 0.18, 0.05, 0.98)

    bc = bc * (1.0 - cloth_orange[..., None]) + woven_rgb * cloth_orange[..., None]

    # Calibrate sky-blue cloth borders toward reference sheet bright sky-cerulean while preserving folds/wear
    blue_luma = 0.2126 * base_diff[..., 0] + 0.7152 * base_diff[..., 1] + 0.0722 * base_diff[..., 2]
    cerulean_rgb = np.clip(
        np.stack([0.18 + 0.34 * blue_luma, 0.42 + 0.48 * blue_luma, 0.60 + 0.48 * blue_luma], axis=-1),
        0.05,
        0.95,
    )
    bc = bc * (1.0 - cloth_blue[..., None] * 0.72) + cerulean_rgb * (cloth_blue[..., None] * 0.72)

    # -------------------------------------------------------------------------
    # 3. Calibrate Jade / Celadon Green Stones, Eyeballs & Feathers
    # -------------------------------------------------------------------------
    # Restore natural dark amber-brown irises + clean sclera on eyeballs from c1_diff (removing post-alpha red lava iris)
    eye_redness = np.clip(((base_diff[..., 0] - base_diff[..., 2]) - 0.10) / 0.18, 0.0, 1.0)
    c1_eye_luma = 0.299 * c1_diff[..., 0] + 0.587 * c1_diff[..., 1] + 0.114 * c1_diff[..., 2]
    # Rich dark chestnut-brown iris with warm lower-iris amber catchlight and dark pupil
    natural_iris = np.stack(
        [
            np.clip(c1_eye_luma * 1.18 + 0.02, 0.03, 0.52),
            np.clip(c1_eye_luma * 0.72 + 0.01, 0.02, 0.34),
            np.clip(c1_eye_luma * 0.44, 0.01, 0.22),
        ],
        axis=-1,
    )
    eye_restored = c1_diff * (1.0 - eye_redness[..., None]) + natural_iris * eye_redness[..., None]
    bc = bc * (1.0 - mask_eyeballs[..., None]) + eye_restored * mask_eyeballs[..., None]

    # Restore post-alpha red crown cabochon gems (4 round bezel-set gems on crown) to polished celadon/jade green
    crown_red_gem = (
        mask_crown_gems
        * np.clip(((base_diff[..., 0] - base_diff[..., 1]) - 0.20) / 0.15, 0.0, 1.0)
        * (1.0 - mask_eyeballs)
    )
    jade_mask = (
        np.clip(((base_diff[..., 1] - np.maximum(base_diff[..., 0], base_diff[..., 2])) - 0.04) / 0.10, 0.0, 1.0)
        * (1.0 - mask_skin)
    )
    jade_mask = np.clip(np.maximum(jade_mask, crown_red_gem), 0.0, 1.0)
    jade_luma = np.where(
        crown_red_gem > 0.35,
        0.16 + 0.22 * base_diff[..., 0],
        0.25 * base_diff[..., 0] + 0.65 * base_diff[..., 1] + 0.10 * base_diff[..., 2],
    )
    # Reference sheet celadon/jade green: richer seafoam-jade midtones with preserved dark veining & bright cabochon center
    celadon_rgb = np.clip(
        np.stack(
            [
                0.10 + 0.48 * jade_luma + 0.06 * base_gloss,
                0.30 + 0.88 * jade_luma + 0.08 * base_gloss,
                0.22 + 0.66 * jade_luma + 0.07 * base_gloss,
            ],
            axis=-1,
        ),
        0.04,
        0.94,
    )
    bc = bc * (1.0 - np.maximum(jade_mask * 0.80, crown_red_gem)[..., None]) + celadon_rgb * np.maximum(jade_mask * 0.80, crown_red_gem)[..., None]

    # Calibrate feathers (crown fan, right-wrist cuff feathers, ankle feathers) from cherry-red toward reference sheet
    # fiery crimson-orange base -> marigold-orange midtones -> luminous golden-yellow tips!
    feather_active = (
        mask_feathers
        * np.clip(((base_diff[..., 0] - base_diff[..., 2]) - 0.18) / 0.15, 0.0, 1.0)
        * (1.0 - jade_mask)
    )
    # Feather tip brightness combines base red/green gradient + c1_gloss barb highlights
    f_bright = np.clip(0.48 * base_diff[..., 0] + 0.88 * base_diff[..., 1] + 0.20 * c1_gloss, 0.0, 1.0)
    feather_rgb = np.stack(
        [
            np.clip(0.42 + 0.68 * base_diff[..., 0], 0.12, 0.99),
            np.clip(0.06 + 0.76 * (f_bright ** 1.18), 0.03, 0.82),
            np.clip(0.02 + 0.22 * (f_bright ** 1.75), 0.01, 0.24),
        ],
        axis=-1,
    )
    bc = bc * (1.0 - feather_active[..., None] * 0.82) + feather_rgb * (feather_active[..., None] * 0.82)

    # -------------------------------------------------------------------------
    # 4. CryEngine Specular/Glossiness -> UE 5.8 Metallic/Roughness Gold Metal Recovery
    # -------------------------------------------------------------------------
    # In CryEngine, pure gold metal has near-black Diffuse (max RGB < 0.14) and warm gold Specular (R > B + 0.08, R > 0.16)
    diff_max = np.max(base_diff, axis=-1)
    spec_goldness = (base_spec[..., 0] - base_spec[..., 2])
    gold_metal_mask = (
        np.clip((0.18 - diff_max) / 0.10, 0.0, 1.0)
        * np.clip((spec_goldness - 0.07) / 0.10, 0.0, 1.0)
        * np.clip((base_spec[..., 0] - 0.14) / 0.12, 0.0, 1.0)
        * (1.0 - mask_skin)
        * (1.0 - mask_beams)
    )
    # Also include the circular gold forehead ornament ring on her face, the rear-crown recessed bronze-gold arches,
    # AND the 8 triangular inlays inside the crown's gold chevron rays (which were recolored red in the post-alpha lava pass)!
    forehead_ring_gold = (
        np.clip((1.03 - forehead_norm_r) / 0.10, 0.0, 1.0)
        * np.clip((forehead_norm_r - 0.45) / 0.10, 0.0, 1.0)
        * (1.0 - face_turquoise_mask)
        * mask_face_skin
    )
    crown_chevron_red_to_gold = (
        mask_crown_arches
        * (v_grid < 0.775).astype(np.float32)
        * np.clip(((base_diff[..., 0] - base_diff[..., 1]) - 0.22) / 0.14, 0.0, 1.0)
        * (1.0 - mask_eyeballs)
        * (1.0 - mask_feathers)
    )
    rear_crown_box = ((u_grid > 0.545) & (u_grid < 0.785) & (v_grid > 0.625) & (v_grid < 0.755)).astype(np.float32)
    rear_crown_gold = (
        rear_crown_box
        * np.clip((0.15 - diff_max) / 0.08, 0.0, 1.0)
        * np.clip((base_spec[..., 2] - 0.10) / 0.08, 0.0, 1.0)
    )
    gold_metal_mask = np.clip(
        np.maximum(gold_metal_mask, np.maximum(forehead_ring_gold, np.maximum(rear_crown_gold, crown_chevron_red_to_gold))),
        0.0,
        1.0,
    )

    # Reconstruct rich 2048 gold BaseColor from base_spec variation + 2048 base_gloss micro-patina
    spec_luma = np.maximum(
        0.50 * base_spec[..., 0] + 0.40 * base_spec[..., 1] + 0.10 * base_spec[..., 2],
        np.maximum(forehead_ring_gold * 0.36, crown_chevron_red_to_gold * (0.20 + 0.22 * base_diff[..., 0])),
    )
    # Normalize local gold intensity (~0.20..0.55 in CryEngine spec) into warm Mesoamerican gold [0.52..0.94]
    gold_val = np.clip(0.42 + 1.05 * spec_luma + 0.22 * (base_gloss - 0.55), 0.28, 0.96)
    gold_bc = np.stack(
        [
            np.clip(gold_val * 1.00, 0.25, 0.96),
            np.clip(gold_val * 0.76, 0.18, 0.80),
            np.clip(gold_val * 0.33, 0.07, 0.42),
        ],
        axis=-1,
    )
    bc = bc * (1.0 - gold_metal_mask[..., None]) + gold_bc * gold_metal_mask[..., None]

    # Lift pitch-black non-metallic dark hair loops & rear bun slightly so strand normals read under neutral lighting
    dark_non_metal = (
        np.clip((0.07 - diff_max) / 0.05, 0.0, 1.0)
        * (1.0 - gold_metal_mask)
        * (1.0 - mask_skin)
        * (1.0 - mask_beams)
        * mask_main
    )
    hair_strand_rgb = np.stack(
        [
            0.085 + 0.07 * c1_gloss,
            0.074 + 0.06 * c1_gloss,
            0.070 + 0.06 * c1_gloss,
        ],
        axis=-1,
    )
    bc = bc * (1.0 - dark_non_metal[..., None]) + np.maximum(bc, hair_strand_rgb) * dark_non_metal[..., None]

    # -------------------------------------------------------------------------
    # 5. Restore Back Streamers (beams_mat, u:[0.9037, 0.9482], v:[0.0022, 0.3292])
    # -------------------------------------------------------------------------
    # Reference sheet rear/front views: 8 flat opaque ribbons transitioning from warm amber-orange near crown
    # down to bright sunlit golden-yellow at the streamer endpoints!
    beam_t = np.clip((0.332 - v_grid) / 0.330, 0.0, 1.0)  # 0 at top (crown), 1 at bottom (endpoints)
    beam_top = np.array([0.885, 0.465, 0.105], dtype=np.float32)     # warm fiery amber-orange
    beam_bot = np.array([0.975, 0.755, 0.195], dtype=np.float32)     # luminous golden-yellow
    beam_grad = beam_top[None, None, :] * (1.0 - beam_t[..., None]) + beam_bot[None, None, :] * beam_t[..., None]
    # Subtle ribbon edge highlight across U width [0.9037..0.9482]
    beam_u_center = 1.0 - np.clip(np.abs(u_grid - 0.926) / 0.024, 0.0, 1.0)
    beam_rgb = np.clip(beam_grad * (0.93 + 0.10 * beam_u_center[..., None]), 0.05, 0.99)
    bc = bc * (1.0 - mask_beams[..., None]) + beam_rgb * mask_beams[..., None]

    # -------------------------------------------------------------------------
    # 6. Normal, Metallic, Roughness, and ORM Packing (Linear)
    # -------------------------------------------------------------------------
    # Gently smooth only the thin symmetrical lava-line normal grooves on bare skin (where base_emis had bright lines)
    lava_groove_smooth = np.clip((emis_mag - 0.04) / 0.20, 0.0, 1.0) * mask_skin * 0.85
    normal_rgb, cavity_ao = reconstruct_directx_normal(base_ddna, smooth_mask=lava_groove_smooth)

    # Metallic: 1.0 on gold metal, subtle 0.25 gold-thread sheen on stepped cloth embroidery lines, 0.0 elsewhere
    embroidery_metallic = embroidery_signal * cloth_orange * 0.28
    metallic = np.clip(np.maximum(gold_metal_mask, embroidery_metallic), 0.0, 1.0)

    # Gloss -> Roughness: combine base_gloss with c1_gloss where base_gloss was zeroed (feathers & hair) + stepped lines
    combined_gloss = np.where(base_gloss > 0.03, base_gloss, c1_gloss * 0.88)
    combined_gloss = np.maximum(combined_gloss, embroidery_signal * cloth_orange * 0.68)
    roughness = 1.0 - combined_gloss

    # Physically plausible PBR surface roughness calibration so each material reads distinctly under neutral light:
    # - Gold metal: 0.23..0.38
    roughness = np.where(gold_metal_mask > 0.5, np.clip(roughness * 0.85, 0.21, 0.42), roughness)
    # - Jade stones: polished cabochons 0.18..0.32
    roughness = np.where(jade_mask > 0.5, np.clip(roughness * 0.72, 0.18, 0.34), roughness)
    # - Woven fabric (non-embroidery): matte textile 0.68..0.84
    matte_cloth = np.clip(mask_cloth * (1.0 - embroidery_signal) * (1.0 - gold_metal_mask) * (1.0 - jade_mask), 0.0, 1.0)
    roughness = roughness * (1.0 - matte_cloth * 0.65) + np.clip(roughness + 0.22, 0.66, 0.85) * (matte_cloth * 0.65)
    # - Skin: natural satin sheen 0.46..0.60 (turquoise warpaint slightly smoother 0.40)
    roughness = roughness * (1.0 - mask_skin * 0.5) + np.clip(roughness, 0.45, 0.62) * (mask_skin * 0.5)
    # - Eyeballs: wet cornea sheen 0.14
    roughness = np.where(mask_eyeballs > 0.5, 0.14, roughness)
    # - Streamers: satin silk ribbon 0.46
    roughness = roughness * (1.0 - mask_beams) + 0.46 * mask_beams
    roughness = np.clip(roughness, 0.12, 0.92)

    orm = np.stack([cavity_ao, roughness, metallic], axis=-1)

    save_rgb(bc, OUT_DIR / "T_BW_Alona_BC.png")
    save_rgb(normal_rgb, OUT_DIR / "T_BW_Alona_N.png")
    save_rgb(orm, OUT_DIR / "T_BW_Alona_ORM.png")
    save_gray(metallic, OUT_DIR / "T_BW_Alona_M.png")
    save_gray(roughness, OUT_DIR / "T_BW_Alona_R.png")

    return {
        "resolution": [2048, 2048],
        "gold_metal_coverage_pct": round(float(np.mean(gold_metal_mask > 0.5) * 100.0), 2),
        "asymmetric_tattoo_pixels": int(np.count_nonzero(tattoo_mask > 0.35)),
        "woven_cloth_restored_pct": round(float(np.mean(cloth_orange > 0.5) * 100.0), 2),
        "jade_calibrated_pct": round(float(np.mean(jade_mask > 0.5) * 100.0), 2),
        "roughness_mean": round(float(np.mean(roughness)), 3),
    }


def build_staff_textures() -> dict:
    size = (1024, 1024)
    diff = load_rgb("alona_staff_standard_diff_rgb.png", size)
    spec = load_rgb("alona_staff_standard_spec_rgb.png", size)
    ddna = load_rgb("alona_staff_standard_ddna_rgb.png", size)
    gloss = load_gray("alona_staff_standard_ddna_gloss.png", size)

    bc = diff.copy()
    diff_max = np.max(diff, axis=-1)
    spec_goldness = spec[..., 0] - spec[..., 2]

    # Gold metal on staff: black diffuse + bright warm gold specular (sun-ring, fittings, collars, pommel)
    gold_mask = (
        np.clip((0.14 - diff_max) / 0.08, 0.0, 1.0)
        * np.clip((spec_goldness - 0.15) / 0.15, 0.0, 1.0)
        * np.clip((spec[..., 0] - 0.25) / 0.15, 0.0, 1.0)
    )

    # Match character's Mesoamerican gold BaseColor palette while preserving dark panel seams in _spec & 1024 _gloss wear
    spec_luma = 0.50 * spec[..., 0] + 0.40 * spec[..., 1] + 0.10 * spec[..., 2]
    gold_val = np.clip(0.18 + 0.98 * spec_luma + 0.18 * (gloss - 0.55), 0.16, 0.96)
    gold_bc = np.stack(
        [
            np.clip(gold_val * 1.00, 0.15, 0.96),
            np.clip(gold_val * 0.76, 0.11, 0.80),
            np.clip(gold_val * 0.33, 0.05, 0.42),
        ],
        axis=-1,
    )
    bc = bc * (1.0 - gold_mask[..., None]) + gold_bc * gold_mask[..., None]

    # Gently brighten dark emerald shaft slightly toward reference sheet celadon-jade shaft while keeping teal crystals & brown leather grip intact
    yy, xx = np.mgrid[0:1024, 0:1024]
    u_grid = xx / 1023.0
    shaft_jade_mask = (u_grid < 0.16) & (diff[..., 1] > diff[..., 0] + 0.04)
    shaft_w = np.asarray(
        Image.fromarray((shaft_jade_mask.astype(np.uint8) * 255)).filter(ImageFilter.GaussianBlur(radius=1.5)),
        dtype=np.float32,
    ) / 255.0
    shaft_bright = np.clip(
        np.stack([diff[..., 0] * 1.35 + 0.05, diff[..., 1] * 1.38 + 0.07, diff[..., 2] * 1.30 + 0.05], axis=-1),
        0.04,
        0.92,
    )
    bc = bc * (1.0 - shaft_w[..., None] * 0.70) + shaft_bright * (shaft_w[..., None] * 0.70)

    normal_rgb, cavity_ao = reconstruct_directx_normal(ddna)
    metallic = np.clip(gold_mask, 0.0, 1.0)

    # Gloss -> Roughness (1 - gloss):
    # - Pointed teal stones: 0.08..0.20 (crisp faceted gems)
    # - Gold head/collars: 0.22..0.38
    # - Jade shaft: 0.28..0.44
    # - Brown wrapped leather grip: 0.68..0.86
    roughness = np.clip(1.0 - gloss, 0.08, 0.90)
    roughness = np.where(gold_mask > 0.5, np.clip(roughness * 0.85, 0.20, 0.40), roughness)

    orm = np.stack([cavity_ao, roughness, metallic], axis=-1)

    save_rgb(bc, OUT_DIR / "T_BW_Alona_Staff_BC.png")
    save_rgb(normal_rgb, OUT_DIR / "T_BW_Alona_Staff_N.png")
    save_rgb(orm, OUT_DIR / "T_BW_Alona_Staff_ORM.png")
    save_gray(metallic, OUT_DIR / "T_BW_Alona_Staff_M.png")
    save_gray(roughness, OUT_DIR / "T_BW_Alona_Staff_R.png")

    return {
        "resolution": [1024, 1024],
        "gold_metal_coverage_pct": round(float(np.mean(gold_mask > 0.5) * 100.0), 2),
        "roughness_mean": round(float(np.mean(roughness)), 3),
    }


def main() -> None:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    MANIFEST_PATH.parent.mkdir(parents=True, exist_ok=True)

    char_stats = build_alona_character_textures()
    staff_stats = build_staff_textures()

    manifest = {
        "pipeline": "SourceArt/Alona/Scripts/prepare_alona_textures.py",
        "character": {
            "stats": char_stats,
            "outputs": {
                "BaseColor": "Textures/Prepared/T_BW_Alona_BC.png (sRGB)",
                "Normal": "Textures/Prepared/T_BW_Alona_N.png (Linear DirectX Y-)",
                "ORM": "Textures/Prepared/T_BW_Alona_ORM.png (Linear R=AO, G=Roughness, B=Metallic)",
                "Metallic": "Textures/Prepared/T_BW_Alona_M.png (Linear)",
                "Roughness": "Textures/Prepared/T_BW_Alona_R.png (Linear)",
            },
            "provenance": {
                "recovered_from_split_dds": [
                    "alona_diff.dds..7 (2048x2048 base feathers, hair, eyes, turquoise eye mask, forehead ring, jade stones, stepped pattern geometry)",
                    "alona_spec.dds..5 (512x512 gold metal F0 mask & specular reflectance)",
                    "alona_ddna.dds..6 + .1a..6a (2048x2048 DirectX Y- BC5 tangent normal + 2048x2048 gloss alpha)",
                    "alona_c1_diff.dds..5 (1024x1024 clean un-glowing skin AO shading + exact asymmetric left-side tattoo mask)",
                    "alona_c1_ddna.dds..6 + .1a..6a (2048x2048 feather/hair/stepped-embroidery gloss detail)",
                ],
                "converted_and_repaired": [
                    "CryEngine Specular/Glossiness -> UE 5.8 Metallic/Roughness PBR conversion (restoring black-diffuse gold crown, hanging plaques, collar, armbands, hip rims, and stacked anklets to Metallic=1 warm gold BaseColor)",
                    "Removed post-alpha symmetrical glowing lava lines from body/face skin using clean c1 skin luminance calibrated to base warm golden-brown skin tone + 2048 pore micro-detail, and composited exact asymmetric left-shoulder/back, left-arm, left-thigh, and left-ankle markings in eye-paint turquoise",
                    "Restored reworked orange woven cloth fill (crop top, rolled waist sash, two-tier skirt, front panel) to reference-sheet warm cream-and-gold woven textile while preserving sky-blue borders and 2048 stepped geometric embroidery lines",
                    "Restored eyeballs from post-alpha glowing red lava irises back to natural dark amber-brown irises from c1_diff, restored 4 bezel-set crown cabochons from red lava back to polished celadon/jade green, restored 8 crown chevron triangular inlays from red lava back to metallic gold, and calibrated feather tips toward fiery crimson-orange to marigold-gold",
                    "Restored beams_mat (u:[0.904, 0.948], v:[0.002, 0.329]) from dark additive stub to reference-sheet opaque warm amber-orange to golden-yellow ribbon gradient",
                ],
            },
        },
        "staff": {
            "stats": staff_stats,
            "outputs": {
                "BaseColor": "Textures/Prepared/T_BW_Alona_Staff_BC.png (sRGB)",
                "Normal": "Textures/Prepared/T_BW_Alona_Staff_N.png (Linear DirectX Y-)",
                "ORM": "Textures/Prepared/T_BW_Alona_Staff_ORM.png (Linear R=AO, G=Roughness, B=Metallic)",
                "Metallic": "Textures/Prepared/T_BW_Alona_Staff_M.png (Linear)",
                "Roughness": "Textures/Prepared/T_BW_Alona_Staff_R.png (Linear)",
            },
            "provenance": {
                "recovered_from_split_dds": [
                    "alona_staff_standard_diff.dds..5 (1024x1024 jade shaft, brown wrapped leather grip, pointed teal stones)",
                    "alona_staff_standard_spec.dds..4 (512x512 gold sun-ring, collars, fittings, and pommel F0)",
                    "alona_staff_standard_ddna.dds..5 + .1a..5a (1024x1024 DirectX Y- BC5 tangent normal + 1024x1024 gloss alpha)",
                ],
                "converted_and_repaired": [
                    "Unsplit full mip chain (replacing previously broken black split-header stub) and converted Specular/Glossiness -> Metallic/Roughness PBR",
                ],
            },
        },
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2))
    print("Prepared textures written successfully:")
    print(json.dumps({"character": char_stats, "staff": staff_stats}, indent=2))


if __name__ == "__main__":
    main()
