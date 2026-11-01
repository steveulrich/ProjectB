"""Single-skinned-mesh + single-textured-material pilot for Rawlins before batching.

Originals under F:/BreakawayConverted are strictly read-only.
"""
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
STAGE_ROOT = PROJECT_ROOT / "SourceArt" / "Rawlins"
RAW_STAGE = STAGE_ROOT / "Sources" / "RawCopies"
SRC_CONVERTED = Path(r"F:\BreakawayConverted")
CGF_CONVERTER = Path(r"F:\CryEngineConversionTools\cgf-converter.exe")
DDS_UNSPLITTER = Path(r"F:\CryEngineConversionTools\DDS-Unsplitter.exe")
BLENDER_EXE = Path(r"F:\Blender Foundation\Blender 5.2\blender.exe")


def decode_dds_rgba(path: Path) -> Image.Image:
    data = bytearray(path.read_bytes())
    if data[:4] == b"DDS " and data[84:88] == b"DX10" and len(data) >= 148:
        dxgi = struct.unpack_from("<I", data, 128)[0]
        if dxgi in (72, 75, 78):
            struct.pack_into("<I", data, 128, dxgi - 1)
    im = Image.open(io.BytesIO(bytes(data)))
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

    # 1. Stage Rawlins character tree (read-only copy)
    rw_root = SRC_CONVERTED / "Unpacked" / "rawlins"
    rw_char_src = rw_root / "Objects" / "Characters" / "Rawlins"
    rw_char_dst = RAW_STAGE / "rawlins" / "Objects" / "Characters" / "Rawlins"
    if rw_char_dst.exists():
        shutil.rmtree(rw_char_dst)
    shutil.copytree(rw_char_src, rw_char_dst)

    # 2. Single skinned mesh pilot: compare -dae vs -glb on new_rawlins_opti_mesh.skin
    pilot_out = STAGE_ROOT / "Exports" / "Pilot"
    pilot_out.mkdir(parents=True, exist_ok=True)
    staged_base = RAW_STAGE / "rawlins"
    target_skin = staged_base / "Objects" / "Characters" / "Rawlins" / "new_rawlins_opti_mesh.skin"

    mesh_pilots = {}
    for fmt_flag, ext in [("-dae", "dae"), ("-glb", "glb")]:
        out_file = pilot_out / f"pilot_new_rawlins_opti_mesh.{ext}"
        if out_file.exists():
            out_file.unlink()
        proc = subprocess.run(
            [
                str(CGF_CONVERTER),
                str(target_skin),
                "-objectdir",
                str(staged_base),
                fmt_flag,
                "-notex",
                "-outputfile",
                str(out_file),
            ],
            capture_output=True,
            text=True,
            timeout=60,
        )
        mesh_pilots[ext] = {
            "returncode": proc.returncode,
            "exists": out_file.exists(),
            "size": out_file.stat().st_size if out_file.exists() else 0,
            "path": str(out_file),
            "stdout_tail": proc.stdout[-400:],
            "stderr_tail": proc.stderr[-400:],
        }

    # 3. Inspect both DAE and GLB in Blender 5.2 to verify UVs, vertex colors, normals, skin weights, bones, scale, facing
    blender_check_py = pilot_out / "inspect_pilot_meshes.py"
    dae_path = str(pilot_out / "pilot_new_rawlins_opti_mesh.dae")
    glb_path = str(pilot_out / "pilot_new_rawlins_opti_mesh.glb")
    out_json = pilot_out / "blender_pilot_comparison.json"

    blender_check_py.write_text(
        f'''import bpy, json

def check_import(fmt, filepath):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if fmt == "dae":
        if hasattr(bpy.ops.wm, "collada_import"):
            bpy.ops.wm.collada_import(filepath=filepath)
        else:
            return {{"supported_in_blender_5_2": False}}
    elif fmt == "glb":
        bpy.ops.import_scene.gltf(filepath=filepath)

    res = {{"supported_in_blender_5_2": True, "meshes": [], "armatures": []}}
    for obj in bpy.data.objects:
        if obj.type == "MESH":
            me = obj.data
            ws_coords = [obj.matrix_world @ v.co for v in me.vertices]
            xs = [c.x for c in ws_coords]
            ys = [c.y for c in ws_coords]
            zs = [c.z for c in ws_coords]
            weighted_verts = sum(1 for v in me.vertices if len(v.groups) > 0)
            res["meshes"].append({{
                "name": obj.name,
                "verts": len(me.vertices),
                "tris_or_polys": len(me.polygons),
                "has_custom_normals": me.has_custom_normals,
                "uv_layers": [uv.name for uv in me.uv_layers],
                "color_attributes": [ca.name for ca in me.color_attributes],
                "vertex_groups": len(obj.vertex_groups),
                "weighted_verts": weighted_verts,
                "materials": [ms.material.name if ms.material else None for ms in obj.material_slots],
                "bounds_x": [round(min(xs), 4), round(max(xs), 4)],
                "bounds_y": [round(min(ys), 4), round(max(ys), 4)],
                "bounds_z": [round(min(zs), 4), round(max(zs), 4)],
            }})
        elif obj.type == "ARMATURE":
            arm = obj.data
            res["armatures"].append({{
                "name": obj.name,
                "bones": len(arm.bones),
                "roots": [b.name for b in arm.bones if b.parent is None],
                "attach_bones": [b.name for b in arm.bones if "attach" in b.name.lower() or "cape" in b.name.lower() or "coat" in b.name.lower() or "holster" in b.name.lower()],
            }})
    return res

report = {{
    "dae": check_import("dae", r"{dae_path}"),
    "glb": check_import("glb", r"{glb_path}"),
}}
with open(r"{out_json}", "w", encoding="utf-8") as f:
    json.dump(report, f, indent=2)
''',
        encoding="utf-8",
    )

    bproc = subprocess.run(
        [str(BLENDER_EXE), "--background", "--factory-startup", "--python", str(blender_check_py)],
        capture_output=True,
        text=True,
        timeout=60,
    )
    blender_report = json.loads(out_json.read_text(encoding="utf-8")) if out_json.exists() else {"error": bproc.stderr[-600:]}

    # 4. Single textured material pilot: rawlins_default (diff, ddna, spec, emis)
    tex_root = SRC_CONVERTED / "Unpacked" / "rawlins_textures-part0" / "Objects" / "Characters" / "Rawlins"
    dds_stage = STAGE_ROOT / "Textures" / "RecoveredDDS"
    preview_dir = STAGE_ROOT / "Textures" / "SkinComparison"
    texture_pilot = {}
    for map_name in [
        "rawlins_default_diff.dds",
        "rawlins_default_ddna.dds",
        "rawlins_default_spec.dds",
        "rawlins_default_emis.dds",
    ]:
        src_p = tex_root / map_name
        label = Path(map_name).stem
        texture_pilot[label] = unsplit_and_preview_dds(src_p, dds_stage / label, preview_dir, label)

    manifest = {
        "mesh_pilots": mesh_pilots,
        "blender_5_2_import_verification": blender_report,
        "single_material_pilot": texture_pilot,
    }
    doc_out = STAGE_ROOT / "Docs" / "pilot_verification.json"
    doc_out.parent.mkdir(parents=True, exist_ok=True)
    doc_out.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
