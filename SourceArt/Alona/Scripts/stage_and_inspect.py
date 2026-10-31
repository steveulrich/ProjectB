"""Inventory, stage copies, and run single-mesh + single-material converter pilot for Alona.

Originals under F:/BreakawayConverted are strictly read-only. All conversion runs on staged copies
under SourceArt/Alona.
"""
import hashlib
import io
import json
import re
import shutil
import struct
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image


PROJECT_ROOT = Path(r"E:\Unreal Projects\ProjectB")
STAGE_ROOT = PROJECT_ROOT / "SourceArt" / "Alona"
RAW_STAGE = STAGE_ROOT / "Sources" / "RawCopies"
SRC_CONVERTED = Path(r"F:\BreakawayConverted")
CGF_CONVERTER = Path(r"F:\CryEngineConversionTools\cgf-converter.exe")
DDS_UNSPLITTER = Path(r"F:\CryEngineConversionTools\DDS-Unsplitter.exe")


def decode_dds_rgba(path: Path) -> Image.Image:
    data = bytearray(path.read_bytes())
    if data[84:88] == b"DX10":
        dxgi = struct.unpack_from("<I", data, 128)[0]
        if dxgi in (72, 75, 78):
            struct.pack_into("<I", data, 128, dxgi - 1)
    im = Image.open(io.BytesIO(data))
    im.load()
    return im


def unsplit_and_preview_dds(src_dds: Path, stage_dir: Path, preview_dir: Path, label: str) -> dict:
    stage_dir.mkdir(parents=True, exist_ok=True)
    preview_dir.mkdir(parents=True, exist_ok=True)
    parts = [src_dds] + sorted(
        p
        for p in src_dds.parent.glob(src_dds.name + ".*")
        if re.fullmatch(r"(?:[0-9]+a?|a)", p.name[len(src_dds.name) + 1 :])
    )
    for part in parts:
        shutil.copy2(part, stage_dir / part.name)

    local = stage_dir / src_dds.name
    res = subprocess.run(
        [str(DDS_UNSPLITTER), str(local), "--safe"],
        capture_output=True,
        text=True,
        timeout=60,
    )
    combined = local.with_name(local.stem + ".combined.dds")
    gloss = local.with_name(local.stem + ".combined_gloss.dds")
    if res.returncode != 0 or not combined.exists():
        raise RuntimeError(f"DDS-Unsplitter failed for {src_dds}: {res.stdout} {res.stderr}")

    im = decode_dds_rgba(combined)
    arr = np.asarray(im)
    rgb_preview = preview_dir / f"{label}_rgb.png"
    Image.fromarray(arr[:, :, :3] if arr.ndim == 3 and arr.shape[2] >= 3 else arr).save(rgb_preview)

    alpha_extrema = None
    alpha_preview = None
    if im.mode == "RGBA":
        a = im.getchannel("A")
        alpha_extrema = a.getextrema()
        alpha_preview = str((preview_dir / f"{label}_alpha.png").resolve())
        a.save(alpha_preview)

    gloss_info = None
    if gloss.exists():
        gim = decode_dds_rgba(gloss).convert("L")
        gpath = preview_dir / f"{label}_gloss.png"
        gim.save(gpath)
        gloss_info = {
            "size": gim.size,
            "extrema": gim.getextrema(),
            "preview": str(gpath.resolve()),
        }

    header = combined.read_bytes()[:148]
    dxgi = struct.unpack_from("<I", header, 128)[0] if header[84:88] == b"DX10" else None
    fourcc = header[84:88].decode("latin1", errors="replace")

    return {
        "source": str(src_dds),
        "part_count": len(parts),
        "fourcc": fourcc,
        "dxgi_format": dxgi,
        "mode": im.mode,
        "size": im.size,
        "alpha_extrema": alpha_extrema,
        "rgb_preview": str(rgb_preview.resolve()),
        "alpha_preview": alpha_preview,
        "gloss": gloss_info,
    }


def main():
    STAGE_ROOT.mkdir(parents=True, exist_ok=True)
    RAW_STAGE.mkdir(parents=True, exist_ok=True)

    # 1. Copy pre-existing root Blend/FBX copies into staging
    preconverted_stage = RAW_STAGE / "PreconvertedRoot"
    preconverted_stage.mkdir(parents=True, exist_ok=True)
    for fname in ["alona.blend", "alona.fbx", "alona_staff.fbx", "aloan_buildable.blend", "aloan_buildable.fbx"]:
        src = SRC_CONVERTED / fname
        if src.exists():
            shutil.copy2(src, preconverted_stage / fname)

    # 2. Copy sunpriestess character directory tree into staging (preserving Objects/Characters/alona relative layout for -objectdir)
    sp_root = SRC_CONVERTED / "Unpacked" / "sunpriestess"
    sp_char_src = sp_root / "Objects" / "Characters" / "alona"
    sp_char_dst = RAW_STAGE / "sunpriestess" / "Objects" / "Characters" / "alona"
    if sp_char_dst.exists():
        shutil.rmtree(sp_char_dst)
    shutil.copytree(sp_char_src, sp_char_dst)

    # 3. Inventory all Alona animation files (without modifying source)
    anim_root = sp_root / "animations" / "IntermediateAssets" / "Gameplay" / "Character" / "Alona"
    anim_files = sorted(str(p.relative_to(anim_root)) for p in anim_root.rglob("*") if p.is_file())

    # 4. Pilot test 1: cgf-converter on ONE mesh (new_alona_opti.skin and alona.cdf) -> GLB
    pilot_out = STAGE_ROOT / "Exports" / "Pilot"
    pilot_out.mkdir(parents=True, exist_ok=True)
    staged_sp_base = RAW_STAGE / "sunpriestess"

    pilot_results = {}
    for target_rel, out_name in [
        ("Objects/Characters/alona/new_alona_opti.skin", "pilot_new_alona_opti.glb"),
        ("Objects/Characters/alona/alona.cdf", "pilot_alona_cdf.glb"),
        ("Objects/Characters/alona/alona_skel.chr", "pilot_alona_skel.glb"),
        ("Objects/Characters/alona/weapons/staff_base/staff_base_mesh.skin", "pilot_staff_base_mesh.glb"),
        ("Objects/Characters/alona/weapons/staff_base/staff_base.cdf", "pilot_staff_base_cdf.glb"),
    ]:
        target_path = staged_sp_base / target_rel
        out_path = pilot_out / out_name
        if out_path.exists():
            out_path.unlink()
        proc = subprocess.run(
            [
                str(CGF_CONVERTER),
                str(target_path),
                "-objectdir",
                str(staged_sp_base),
                "-glb",
                "-notex",
                "-outputfile",
                str(out_path),
            ],
            capture_output=True,
            text=True,
            timeout=60,
        )
        pilot_results[out_name] = {
            "returncode": proc.returncode,
            "exists": out_path.exists(),
            "size": out_path.stat().st_size if out_path.exists() else 0,
            "stdout_tail": proc.stdout[-600:],
            "stderr_tail": proc.stderr[-600:],
        }

    # Test one .caf file on a staged copy to check if cgf-converter supports Breakaway .caf
    sample_caf = next((p for p in anim_root.rglob("*.caf") if "idle" in p.name.lower() or "fidget" in p.name.lower()), None)
    if sample_caf:
        staged_caf = pilot_out / sample_caf.name
        shutil.copy2(sample_caf, staged_caf)
        caf_out = pilot_out / "pilot_anim.glb"
        proc = subprocess.run(
            [str(CGF_CONVERTER), str(staged_caf), "-glb", "-outputfile", str(caf_out)],
            capture_output=True,
            text=True,
            timeout=30,
        )
        pilot_results["pilot_caf"] = {
            "caf_name": sample_caf.name,
            "returncode": proc.returncode,
            "exists": caf_out.exists(),
            "size": caf_out.stat().st_size if caf_out.exists() else 0,
            "stdout_tail": proc.stdout[-600:],
            "stderr_tail": proc.stderr[-600:],
        }

    # 5. Pilot test 2: DDS-Unsplitter on ONE material first (c1), then decode remaining skin & staff variants for comparison
    tex_char_root = SRC_CONVERTED / "Unpacked" / "sunpriestess_textures" / "Objects" / "Characters" / "alona"
    dds_stage = STAGE_ROOT / "Textures" / "RecoveredDDS"
    preview_dir = STAGE_ROOT / "Textures" / "SkinComparison"

    texture_checks = {}
    # Single material pilot: alona_c1 (Diffuse, DDNA, Specular)
    for map_name in ["alona_c1_diff.dds", "alona_c1_ddna.dds", "alona_c1_spec.dds"]:
        src_p = tex_char_root / map_name
        label = Path(map_name).stem
        texture_checks[label] = unsplit_and_preview_dds(src_p, dds_stage / label, preview_dir, label)

    # Skin comparison: base alona_diff, alona_emis, alona_c3_diff, and staff variants
    comparison_targets = [
        (tex_char_root / "alona_diff.dds", "alona_base_diff"),
        (tex_char_root / "alona_ddna.dds", "alona_base_ddna"),
        (tex_char_root / "alona_spec.dds", "alona_base_spec"),
        (tex_char_root / "alona_emis.dds", "alona_base_emis"),
        (tex_char_root / "alona_c3_diff.dds", "alona_c3_diff"),
        (tex_char_root / "alona_c3_ddna.dds", "alona_c3_ddna"),
        (tex_char_root / "alona_c3_spec.dds", "alona_c3_spec"),
        (tex_char_root / "weapons" / "staff_base" / "alona_staff_standard_diff.dds", "alona_staff_standard_diff"),
        (tex_char_root / "weapons" / "staff_base" / "alona_staff_standard_ddna.dds", "alona_staff_standard_ddna"),
        (tex_char_root / "weapons" / "staff_base" / "alona_staff_standard_spec.dds", "alona_staff_standard_spec"),
        (tex_char_root / "weapons" / "staff_base" / "alona_staff_rare_diff.dds", "alona_staff_rare_diff"),
        (tex_char_root / "weapons" / "staff_base" / "alona_staff_epic_diff.dds", "alona_staff_epic_diff"),
        (tex_char_root / "weapons" / "staff_base" / "alona_staff_legendary_diff.dds", "alona_staff_legendary_diff"),
    ]
    for src_p, label in comparison_targets:
        if src_p.exists():
            texture_checks[label] = unsplit_and_preview_dds(src_p, dds_stage / label, preview_dir, label)

    manifest = {
        "animation_file_count": len(anim_files),
        "animation_sample_files": [f for f in anim_files if any(k in f.lower() for k in ("idle", "run", "attack", "shoot", "primary", "staff", "fidget", "victory"))][:40],
        "pilot_mesh_conversions": pilot_results,
        "texture_recovery_checks": texture_checks,
        "missing_skin_variants_on_disk": ["alona_c2_* (referenced by alona_skin_c2_matgroup.mtl, absent from sunpriestess_textures)", "alona_c4_* (referenced by alona_skin_c4_matgroup.mtl, absent from sunpriestess_textures)"],
    }
    manifest_path = STAGE_ROOT / "Docs" / "source_inventory_and_pilot.json"
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2))
    print("WROTE MANIFEST:", manifest_path)
    print(json.dumps({
        "anim_count": manifest["animation_file_count"],
        "pilots": {k: {"rc": v["returncode"], "size": v["size"]} for k, v in pilot_results.items()},
        "textures": {k: {"size": v["size"], "mode": v["mode"], "alpha": v["alpha_extrema"], "dxgi": v["dxgi_format"], "has_gloss": v["gloss"] is not None} for k, v in texture_checks.items()},
    }, indent=2))


if __name__ == "__main__":
    main()
