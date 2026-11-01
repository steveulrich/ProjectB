#!/usr/bin/env python3
r"""
Blender 5.2 Cycles 3D AO & Curvature Baker + Exact 3D-to-UV Component Classifier for Original Spartacus & Dual Swords.
Outputs to SourceArt/Spartacus/Textures/Masks/Baked3D/:
  - {spartacus_body,spartacus_sword1,spartacus_sword2}_{ao,curv}.png
  - uv_groups.json
"""

from __future__ import annotations

import json
import math
import xml.etree.ElementTree as ET
from pathlib import Path

import bpy

ROOT = Path(__file__).resolve().parents[1]
BAKE_DIR = ROOT / "Textures" / "Masks" / "Baked3D"


def find_file(filename: str) -> Path:
    for p in ROOT.rglob("*"):
        if p.is_file() and p.name.lower() == filename.lower():
            return p
    raise FileNotFoundError(f"Missing {filename} under {ROOT}")


def parse_dae_triangles(dae_path: Path):
    tree = ET.parse(dae_path)
    root = tree.getroot()
    ns = root.tag.split("}")[0] + "}" if root.tag.startswith("{") else ""
    all_tris = []
    for mesh_el in root.findall(f".//{ns}mesh"):
        sources = {}
        for src in mesh_el.findall(f"{ns}source"):
            sid = src.attrib.get("id", "")
            fa = src.find(f"{ns}float_array")
            if fa is None or not fa.text:
                continue
            vals = [float(x) for x in fa.text.split()]
            acc = src.find(f".//{ns}accessor")
            stride = int(acc.attrib.get("stride", "3")) if acc is not None else 3
            sources[sid] = [tuple(vals[i:i + stride]) for i in range(0, len(vals), stride)]
        verts_map = {}
        for vel in mesh_el.findall(f"{ns}vertices"):
            vid = vel.attrib.get("id", "")
            for inp in vel.findall(f"{ns}input"):
                if inp.attrib.get("semantic") == "POSITION":
                    verts_map[vid] = sources.get(inp.attrib.get("source", "").lstrip("#"))
        for tri_el in mesh_el.findall(f"{ns}triangles"):
            mat_name = tri_el.attrib.get("material", "default")
            inputs = tri_el.findall(f"{ns}input")
            stride = max(int(i.attrib.get("offset", "0")) for i in inputs) + 1
            pos_list, uv_list, pos_off, uv_off = None, None, 0, 0
            for inp in inputs:
                sem = inp.attrib.get("semantic")
                off = int(inp.attrib.get("offset", "0"))
                sid = inp.attrib.get("source", "").lstrip("#")
                if sem == "VERTEX":
                    pos_off, pos_list = off, verts_map.get(sid)
                elif sem == "TEXCOORD":
                    uv_off, uv_list = off, sources.get(sid)
            p_el = tri_el.find(f"{ns}p")
            if p_el is None or not p_el.text or not pos_list or not uv_list:
                continue
            idx = [int(x) for x in p_el.text.split()]
            tstride = stride * 3
            for b in range(0, len(idx) - tstride + 1, tstride):
                corners = []
                for c in range(3):
                    p = pos_list[idx[b + c * stride + pos_off]][:3]
                    uv = uv_list[idx[b + c * stride + uv_off]][:2]
                    corners.append(((-p[0], -p[1], p[2]), uv))
                all_tris.append((mat_name, corners))
    return all_tris


def inject_exact_dae_uvs(mesh_obj, dae_path: Path):
    me = mesh_obj.data
    dae_tris = parse_dae_triangles(dae_path)
    while me.uv_layers:
        me.uv_layers.remove(me.uv_layers[0])
    uv_layer = me.uv_layers.new(name="UVMap")

    def qk(pt):
        return (int(round(pt[0] * 1000.0)), int(round(pt[1] * 1000.0)), int(round(pt[2] * 1000.0)))

    tri_lookup = {}
    for mat_name, bcorners in dae_tris:
        ckey = tuple(sorted(qk(bp) for bp, _ in bcorners))
        tri_lookup.setdefault(ckey, []).append((mat_name, bcorners))

    direct_hits, lookup_hits, nearest_hits = 0, 0, 0
    for poly in me.polygons:
        bpts = [me.vertices[me.loops[li].vertex_index].co for li in poly.loop_indices]
        bcorners = None
        if poly.index < len(dae_tris):
            cand = dae_tris[poly.index][1]
            cx_b, cy_b, cz_b = sum(p.x for p in bpts) / 3.0, sum(p.y for p in bpts) / 3.0, sum(p.z for p in bpts) / 3.0
            cx_d, cy_d, cz_d = sum(bp[0] for bp, _ in cand) / 3.0, sum(bp[1] for bp, _ in cand) / 3.0, sum(bp[2] for bp, _ in cand) / 3.0
            if (cx_b - cx_d) ** 2 + (cy_b - cy_d) ** 2 + (cz_b - cz_d) ** 2 < 4e-6:
                bcorners = cand
                direct_hits += 1
        if bcorners is None:
            ckey = tuple(sorted(qk((p.x, p.y, p.z)) for p in bpts))
            cands = tri_lookup.get(ckey)
            if cands:
                bcorners = cands[0][1]
                lookup_hits += 1
        if bcorners is None:
            cx_b, cy_b, cz_b = sum(p.x for p in bpts) / 3.0, sum(p.y for p in bpts) / 3.0, sum(p.z for p in bpts) / 3.0
            best_d = 1e9
            for _, cand in dae_tris:
                cx_d, cy_d, cz_d = sum(bp[0] for bp, _ in cand) / 3.0, sum(bp[1] for bp, _ in cand) / 3.0, sum(bp[2] for bp, _ in cand) / 3.0
                d = (cx_b - cx_d) ** 2 + (cy_b - cy_d) ** 2 + (cz_b - cz_d) ** 2
                if d < best_d:
                    best_d = d
                    bcorners = cand
            nearest_hits += 1

        used = set()
        for li, p in zip(poly.loop_indices, bpts):
            best_ci, best_d = 0, 1e9
            for ci, (bp, _) in enumerate(bcorners):
                d = (p.x - bp[0]) ** 2 + (p.y - bp[1]) ** 2 + (p.z - bp[2]) ** 2
                if ci not in used and d < best_d:
                    best_d, best_ci = d, ci
            used.add(best_ci)
            uv_layer.data[li].uv = bcorners[best_ci][1]

    print(f"  UV inject {dae_path.name}: direct={direct_hits}, lookup={lookup_hits}, nearest={nearest_hits} / {len(me.polygons)}")
    return dae_tris


def bake_ao_and_pointiness(mesh_obj, size: int, prefix: str) -> None:
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 16
    scene.render.bake.margin = max(8, size // 128)
    scene.render.bake.use_clear = True

    me = mesh_obj.data
    src_uv = me.uv_layers["UVMap"]
    bake_uv = me.uv_layers.new(name="BakeUV")
    for poly in me.polygons:
        us = [src_uv.data[li].uv.x for li in poly.loop_indices]
        vs = [src_uv.data[li].uv.y for li in poly.loop_indices]
        du = math.floor(sum(us) / len(us))
        dv = math.floor(sum(vs) / len(vs))
        for li in poly.loop_indices:
            bake_uv.data[li].uv = (src_uv.data[li].uv.x - du, src_uv.data[li].uv.y - dv)
    me.uv_layers.active = bake_uv

    bake_mat = bpy.data.materials.new(name=f"BakeMat_{prefix}")
    bake_mat.use_nodes = True
    nt = bake_mat.node_tree
    nt.nodes.clear()
    out_node = nt.nodes.new("ShaderNodeOutputMaterial")
    emit_node = nt.nodes.new("ShaderNodeEmission")
    geom_node = nt.nodes.new("ShaderNodeNewGeometry")
    ramp_node = nt.nodes.new("ShaderNodeMapRange")
    ramp_node.inputs["From Min"].default_value = 0.42
    ramp_node.inputs["From Max"].default_value = 0.58
    ramp_node.clamp = True
    nt.links.new(geom_node.outputs["Pointiness"], ramp_node.inputs["Value"])
    nt.links.new(ramp_node.outputs["Result"], emit_node.inputs["Color"])
    nt.links.new(emit_node.outputs["Emission"], out_node.inputs["Surface"])

    img_ao = bpy.data.images.new(f"{prefix}_ao", width=size, height=size, alpha=False)
    img_ao.colorspace_settings.name = "Non-Color"
    img_curv = bpy.data.images.new(f"{prefix}_curv", width=size, height=size, alpha=False)
    img_curv.colorspace_settings.name = "Non-Color"

    tex_node = nt.nodes.new("ShaderNodeTexImage")
    me.materials.clear()
    me.materials.append(bake_mat)
    for p in me.polygons:
        p.material_index = 0

    bpy.ops.object.select_all(action="DESELECT")
    mesh_obj.select_set(True)
    bpy.context.view_layer.objects.active = mesh_obj

    tex_node.image = img_ao
    nt.nodes.active = tex_node
    bpy.ops.object.bake(type="AO")
    img_ao.filepath_raw = str(BAKE_DIR / f"{prefix}_ao.png")
    img_ao.file_format = "PNG"
    img_ao.save()

    tex_node.image = img_curv
    nt.nodes.active = tex_node
    bpy.ops.object.bake(type="EMIT")
    img_curv.filepath_raw = str(BAKE_DIR / f"{prefix}_curv.png")
    img_curv.file_format = "PNG"
    img_curv.save()


def import_clean_mesh(glb_name: str, dae_name: str):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    if bpy.context.scene.world is None:
        bpy.context.scene.world = bpy.data.worlds.new("World")
    bpy.ops.import_scene.gltf(filepath=str(find_file(glb_name)))
    meshes = [o for o in bpy.data.objects if o.type == "MESH"]
    if len(meshes) > 1:
        skinned = [m for m in meshes if len(m.vertex_groups) > 0 and len(m.data.vertices) > 60]
        if skinned:
            for m in list(meshes):
                if m not in skinned:
                    bpy.data.objects.remove(m, do_unlink=True)
                    meshes.remove(m)
    m = meshes[0]
    dae_tris = inject_exact_dae_uvs(m, find_file(dae_name))
    return m, dae_tris


def classify_spartacus_body(mesh_obj, dae_tris) -> dict[str, list]:
    me = mesh_obj.data
    uv_layer = me.uv_layers["UVMap"]
    adj = {i: [] for i in range(len(me.vertices))}
    v_polys = {i: [] for i in range(len(me.vertices))}
    for poly in me.polygons:
        vids = [me.loops[li].vertex_index for li in poly.loop_indices]
        for vi in vids:
            v_polys[vi].append(poly.index)
        for a in range(len(vids)):
            va, vb = vids[a], vids[(a + 1) % len(vids)]
            adj[va].append(vb)
            adj[vb].append(va)

    groups: dict[str, list] = {
        "skin": [], "mouth": [], "eyeballs": [],
        "crest_hair_red": [], "crest_hair_white": [],
        "helmet_bronze": [], "helmet_steel_trim": [],
        "pauldron_crimson": [], "armor_bronze": [], "armor_steel": [],
        "leather_dark": [], "leather_tan": [], "linen_wrap": [], "pteruges_crimson": [],
    }
    visited = set()
    for vi in range(len(me.vertices)):
        if vi in visited or not v_polys[vi]:
            continue
        stack = [vi]
        visited.add(vi)
        cverts, cpolys = [], set()
        while stack:
            cur = stack.pop()
            cverts.append(cur)
            for pi in v_polys[cur]:
                cpolys.add(pi)
            for nb in adj[cur]:
                if nb not in visited:
                    visited.add(nb)
                    stack.append(nb)

        xs = [me.vertices[v].co.x for v in cverts]
        ys = [me.vertices[v].co.y for v in cverts]
        zs = [me.vertices[v].co.z for v in cverts]
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = min(ys), max(ys)
        min_z, max_z = min(zs), max(zs)
        nv = len(cverts)

        sample_pi = next(iter(cpolys))
        mat_name = dae_tris[sample_pi][0].lower() if sample_pi < len(dae_tris) else "spart_main"
        is_skin_slot = "spart_skin" in mat_name

        for pi in cpolys:
            poly = me.polygons[pi]
            utri = [[round(uv_layer.data[li].uv.x, 5), round(uv_layer.data[li].uv.y, 5)] for li in poly.loop_indices]
            pts = [me.vertices[me.loops[li].vertex_index].co for li in poly.loop_indices]
            cx = sum(p.x for p in pts) / 3.0
            cy = sum(p.y for p in pts) / 3.0
            cz = sum(p.z for p in pts) / 3.0

            if is_skin_slot:
                if nv < 220 and min_z > 2.00 and max_z < 2.13 and abs(min_x) < 0.06 and abs(max_x) < 0.06:
                    groups["mouth"].append(utri)
                else:
                    groups["skin"].append(utri)
                continue

            if 65 <= nv <= 80 and 2.12 <= min_z and max_z <= 2.17 and max_y < -0.04 and 0.01 <= min(abs(min_x), abs(max_x)):
                groups["eyeballs"].append(utri)
            elif max_z > 2.33:
                if max_x - min_x < 0.045:
                    groups["helmet_bronze"].append(utri)
                elif cy < -0.045:
                    groups["crest_hair_white"].append(utri)
                else:
                    groups["crest_hair_red"].append(utri)
            elif min_z > 1.90:
                if (cy < -0.118 and 2.05 <= cz <= 2.185 and abs(cx) < 0.095) or (nv == 77 and min_z > 2.15):
                    groups["helmet_steel_trim"].append(utri)
                else:
                    groups["helmet_bronze"].append(utri)
            elif max_x < -0.12 and min_z > 1.20:
                if nv == 595:
                    groups["leather_dark"].append(utri)
                elif nv in (492, 162):
                    groups["pauldron_crimson"].append(utri)
                elif nv in (315, 146):
                    groups["armor_steel"].append(utri)
                else:
                    if int(round(cz * 12.0)) % 2 == 0:
                        groups["armor_bronze"].append(utri)
                    else:
                        groups["armor_steel"].append(utri)
            elif min_x > 0.35 and min_z > 1.25:
                groups["linen_wrap"].append(utri)
            elif min_z > 1.22:
                if nv in (160, 167):
                    groups["leather_tan"].append(utri)
                else:
                    groups["leather_dark"].append(utri)
            elif min_z >= 0.85:
                if nv > 350:
                    groups["armor_bronze"].append(utri)
                elif abs(cx) < 0.085 and cy < 0.0:
                    groups["pteruges_crimson"].append(utri)
                else:
                    groups["leather_dark"].append(utri)
            else:
                if nv in (656, 470, 132, 90):
                    groups["armor_bronze"].append(utri)
                elif nv in (406, 201, 200) or (95 <= nv <= 106 and min_z > 0.12):
                    groups["linen_wrap"].append(utri)
                else:
                    groups["leather_dark"].append(utri)
    return groups


def classify_sword(mesh_obj, blade_z_min: float, grip_z_max: float, pommel_z_max: float) -> dict[str, list]:
    me = mesh_obj.data
    uv_layer = me.uv_layers["UVMap"]
    groups = {"blade_steel": [], "blade_edge": [], "hilt_bronze": [], "grip_leather": []}
    xs_all = [abs(v.co.x) for v in me.vertices if v.co.z > blade_z_min]
    blade_half_w = max(xs_all) if xs_all else 0.12

    for poly in me.polygons:
        utri = [[round(uv_layer.data[li].uv.x, 5), round(uv_layer.data[li].uv.y, 5)] for li in poly.loop_indices]
        pts = [me.vertices[me.loops[li].vertex_index].co for li in poly.loop_indices]
        cz = sum(p.z for p in pts) / 3.0
        max_ax = max(abs(p.x) for p in pts)
        if cz >= blade_z_min:
            if max_ax > blade_half_w * 0.58:
                groups["blade_edge"].append(utri)
            else:
                groups["blade_steel"].append(utri)
        elif cz <= pommel_z_max or cz >= grip_z_max:
            groups["hilt_bronze"].append(utri)
        else:
            groups["grip_leather"].append(utri)
    return groups


def main() -> None:
    BAKE_DIR.mkdir(parents=True, exist_ok=True)
    payload = {}

    print("1/3 Baking Cycles 3D AO + Curvature for Spartacus Body (2048x2048)...")
    body_obj, body_dae = import_clean_mesh("spartacus_opti_mesh.glb", "spartacus_opti_mesh.dae")
    payload["body"] = classify_spartacus_body(body_obj, body_dae)
    bake_ao_and_pointiness(body_obj, 2048, "spartacus_body")

    print("2/3 Baking Cycles 3D AO + Curvature for Sword 1 (1024x1024)...")
    sw1_obj, _ = import_clean_mesh("spartacus_sword_mesh.glb", "spartacus_sword_mesh.dae")
    payload["sword1"] = classify_sword(sw1_obj, blade_z_min=0.250, grip_z_max=0.132, pommel_z_max=-0.146)
    bake_ao_and_pointiness(sw1_obj, 1024, "spartacus_sword1")

    print("3/3 Baking Cycles 3D AO + Curvature for Sword 2 (1024x1024)...")
    sw2_obj, _ = import_clean_mesh("sword2_mesh.glb", "sword2_mesh.dae")
    payload["sword2"] = classify_sword(sw2_obj, blade_z_min=0.310, grip_z_max=0.130, pommel_z_max=-0.195)
    bake_ao_and_pointiness(sw2_obj, 1024, "spartacus_sword2")

    (BAKE_DIR / "uv_groups.json").write_text(json.dumps(payload), encoding="utf-8")
    print("3D bakes & UV classifications complete!")


if __name__ == "__main__":
    main()
