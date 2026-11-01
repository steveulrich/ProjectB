#!/usr/bin/env python3
r"""
Assemble SourceArt/Rawlins/Rawlins_Restored.blend, verify aiming pose & revolver grip/holster fit,
render neutral-light studio & action verification captures, and export UE 5.8 FBX meshes + LODs.

Run:
  & "F:\Blender Foundation\Blender 5.2\blender.exe" --background --factory-startup --python-exit-code 1 --python "e:\Unreal Projects\ProjectB\SourceArt\Rawlins\Scripts\assemble_and_export_rawlins.py"
"""

from __future__ import annotations

import json
import math
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

import bmesh
import bpy
import mathutils

ROOT = Path(__file__).resolve().parents[1]
GLB_DIR = ROOT / "Converted" / "GLB"
DAE_DIR = ROOT / "Converted" / "DAE"
TEX_DIR = ROOT / "Textures" / "Prepared"
EXP_DIR = ROOT / "Exports"
REN_DIR = ROOT / "Renders"
DOCS_DIR = ROOT / "Docs"


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


def inject_exact_dae_uvs_and_slots(mesh_obj, dae_path: Path, slot_names: list[str], dae_substrings: list[str]):
    me = mesh_obj.data
    dae_tris = parse_dae_triangles(dae_path)
    while me.uv_layers:
        me.uv_layers.remove(me.uv_layers[0])
    uv_layer = me.uv_layers.new(name="UVMap")

    me.materials.clear()
    for sname in slot_names:
        mat = bpy.data.materials.get(sname) or bpy.data.materials.new(name=sname)
        me.materials.append(mat)

    def qk(pt):
        return (int(round(pt[0] * 1000.0)), int(round(pt[1] * 1000.0)), int(round(pt[2] * 1000.0)))

    tri_lookup = {}
    for mat_name, bcorners in dae_tris:
        ckey = tuple(sorted(qk(bp) for bp, _ in bcorners))
        tri_lookup.setdefault(ckey, []).append((mat_name, bcorners))

    for poly in me.polygons:
        bpts = [me.vertices[me.loops[li].vertex_index].co for li in poly.loop_indices]
        mat_raw, bcorners = None, None
        if poly.index < len(dae_tris):
            cand_mat, cand = dae_tris[poly.index]
            cx_b, cy_b, cz_b = sum(p.x for p in bpts) / 3.0, sum(p.y for p in bpts) / 3.0, sum(p.z for p in bpts) / 3.0
            cx_d, cy_d, cz_d = sum(bp[0] for bp, _ in cand) / 3.0, sum(bp[1] for bp, _ in cand) / 3.0, sum(bp[2] for bp, _ in cand) / 3.0
            if (cx_b - cx_d) ** 2 + (cy_b - cy_d) ** 2 + (cz_b - cz_d) ** 2 < 4e-6:
                mat_raw, bcorners = cand_mat, cand
        if bcorners is None:
            ckey = tuple(sorted(qk((p.x, p.y, p.z)) for p in bpts))
            cands = tri_lookup.get(ckey)
            if cands:
                mat_raw, bcorners = cands[0]
        if bcorners is None:
            cx_b, cy_b, cz_b = sum(p.x for p in bpts) / 3.0, sum(p.y for p in bpts) / 3.0, sum(p.z for p in bpts) / 3.0
            best_d = 1e9
            for cand_mat, cand in dae_tris:
                cx_d, cy_d, cz_d = sum(bp[0] for bp, _ in cand) / 3.0, sum(bp[1] for bp, _ in cand) / 3.0, sum(bp[2] for bp, _ in cand) / 3.0
                d = (cx_b - cx_d) ** 2 + (cy_b - cy_d) ** 2 + (cz_b - cz_d) ** 2
                if d < best_d:
                    best_d, mat_raw, bcorners = d, cand_mat, cand

        slot_idx = 0
        mlow = (mat_raw or "").lower()
        for idx, sub in enumerate(dae_substrings):
            if sub in mlow:
                slot_idx = idx
                break
        poly.material_index = slot_idx

        used = set()
        for li, p in zip(poly.loop_indices, bpts):
            best_ci, best_d = 0, 1e9
            for ci, (bp, _) in enumerate(bcorners):
                d = (p.x - bp[0]) ** 2 + (p.y - bp[1]) ** 2 + (p.z - bp[2]) ** 2
                if ci not in used and d < best_d:
                    best_d, best_ci = d, ci
            used.add(best_ci)
            uv_layer.data[li].uv = bcorners[best_ci][1]


def build_pbr_material(mat_name: str, bc_file: str, n_file: str, orm_file: str, e_file: str | None = None, emissive_strength: float = 4.0):
    mat = bpy.data.materials.get(mat_name) or bpy.data.materials.new(name=mat_name)
    nt = mat.node_tree
    nt.nodes.clear()

    out = nt.nodes.new("ShaderNodeOutputMaterial")
    out.location = (650, 0)
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (320, 0)
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])

    def load_img(fname: str, non_color: bool):
        img = bpy.data.images.load(str(TEX_DIR / fname), check_existing=True)
        img.colorspace_settings.name = "Non-Color" if non_color else "sRGB"
        return img

    tex_bc = nt.nodes.new("ShaderNodeTexImage")
    tex_bc.location = (-450, 260)
    tex_bc.image = load_img(bc_file, non_color=False)

    tex_orm = nt.nodes.new("ShaderNodeTexImage")
    tex_orm.location = (-450, -20)
    tex_orm.image = load_img(orm_file, non_color=True)
    sep_orm = nt.nodes.new("ShaderNodeSeparateColor")
    sep_orm.location = (-160, -20)
    nt.links.new(tex_orm.outputs["Color"], sep_orm.inputs["Color"])

    # Multiply subtle baked/cavity AO into BaseColor for DCC preview & wire G->Roughness, B->Metallic
    mul_ao = nt.nodes.new("ShaderNodeMix")
    mul_ao.data_type = "RGBA"
    mul_ao.blend_type = "MULTIPLY"
    mul_ao.inputs["Factor"].default_value = 0.65
    mul_ao.location = (60, 220)
    nt.links.new(tex_bc.outputs["Color"], mul_ao.inputs["A"])
    nt.links.new(sep_orm.outputs["Red"], mul_ao.inputs["B"])
    nt.links.new(mul_ao.outputs["Result"], bsdf.inputs["Base Color"])
    nt.links.new(sep_orm.outputs["Green"], bsdf.inputs["Roughness"])
    nt.links.new(sep_orm.outputs["Blue"], bsdf.inputs["Metallic"])

    # DirectX Y- normal map -> invert G for Blender OpenGL Y+ normal evaluation
    tex_n = nt.nodes.new("ShaderNodeTexImage")
    tex_n.location = (-650, -320)
    tex_n.image = load_img(n_file, non_color=True)
    sep_n = nt.nodes.new("ShaderNodeSeparateColor")
    sep_n.location = (-360, -320)
    inv_g = nt.nodes.new("ShaderNodeMath")
    inv_g.operation = "SUBTRACT"
    inv_g.inputs[0].default_value = 1.0
    inv_g.location = (-180, -340)
    comb_n = nt.nodes.new("ShaderNodeCombineColor")
    comb_n.location = (0, -320)
    nmap = nt.nodes.new("ShaderNodeNormalMap")
    nmap.location = (160, -320)

    nt.links.new(tex_n.outputs["Color"], sep_n.inputs["Color"])
    nt.links.new(sep_n.outputs["Red"], comb_n.inputs["Red"])
    nt.links.new(sep_n.outputs["Green"], inv_g.inputs[1])
    nt.links.new(inv_g.outputs["Value"], comb_n.inputs["Green"])
    nt.links.new(sep_n.outputs["Blue"], comb_n.inputs["Blue"])
    nt.links.new(comb_n.outputs["Color"], nmap.inputs["Color"])
    nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])

    if e_file and (TEX_DIR / e_file).exists():
        tex_e = nt.nodes.new("ShaderNodeTexImage")
        tex_e.location = (-180, -560)
        tex_e.image = load_img(e_file, non_color=False)
        nt.links.new(tex_e.outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = emissive_strength

    return mat


def import_glb_objects(glb_path: Path):
    before_names = set(o.name for o in bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(glb_path))
    added = [o for o in bpy.data.objects if o.name not in before_names]
    arm = next((o for o in added if o.type == "ARMATURE"), None)
    meshes = [o for o in added if o.type == "MESH"]
    if len(meshes) > 1:
        skinned = [m for m in meshes if len(m.vertex_groups) > 0 and len(m.data.vertices) > 60]
        if skinned:
            for m in list(meshes):
                if m not in skinned:
                    bpy.data.objects.remove(m, do_unlink=True)
                    meshes.remove(m)
    empties = [o for o in list(bpy.data.objects) if o.name not in before_names and o.type == "EMPTY"]
    for o in empties:
        for child in list(o.children):
            mw = child.matrix_world.copy()
            child.parent = None
            child.matrix_world = mw
        bpy.data.objects.remove(o, do_unlink=True)
    # Remove any placeholder images/materials imported by glTF with non-existent paths
    for img in list(bpy.data.images):
        if img.filepath and not Path(bpy.path.abspath(img.filepath)).exists():
            bpy.data.images.remove(img, do_unlink=True)
    return arm, meshes[0]


def move_to_collection(obj, coll):
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    coll.objects.link(obj)


def setup_studio_lighting(coll):
    world = bpy.context.scene.world
    if world is None:
        world = bpy.data.worlds.new("World")
        bpy.context.scene.world = world
    wnt = world.node_tree
    bg = wnt.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.14, 0.15, 0.17, 1.0)
        bg.inputs["Strength"].default_value = 0.85

    lights = [
        ("Key_Light", (-2.2, -2.6, 2.6), (1.1808, 0.0, -0.7023), 260.0, (1.0, 0.98, 0.95), 1.6),
        ("Fill_Light", (2.4, -2.2, 1.5), (1.4486, 0.0, 0.8288), 130.0, (0.92, 0.96, 1.0), 2.0),
        ("Rim_Light", (0.3, 2.8, 2.4), (1.1984, 0.0, 3.0349), 200.0, (0.96, 0.98, 1.0), 1.4),
    ]
    for name, loc, rot, energy, col, size in lights:
        ldata = bpy.data.lights.new(name=name, type="AREA")
        ldata.energy = energy
        ldata.color = col
        ldata.size = size
        lobj = bpy.data.objects.new(name=name, object_data=ldata)
        lobj.location = loc
        lobj.rotation_euler = rot
        coll.objects.link(lobj)


def look_at(cam_obj, target_pt: mathutils.Vector):
    direction = target_pt - cam_obj.location
    rot_quat = direction.to_track_quat("-Z", "Y")
    cam_obj.rotation_euler = rot_quat.to_euler()


def render_view(cam_obj, loc, target, out_path: Path, res=(1080, 1280), lens=55.0):
    scene = bpy.context.scene
    cam_obj.location = mathutils.Vector(loc)
    cam_obj.data.lens = lens
    look_at(cam_obj, mathutils.Vector(target))
    bpy.context.view_layer.update()
    scene.render.resolution_x = res[0]
    scene.render.resolution_y = res[1]
    scene.render.filepath = str(out_path)
    bpy.ops.render.render(write_still=True)


def attach_preview_to_socket(preview_obj, arm_obj, socket_bone_name: str, extra_euler_deg=(0.0, 0.0, 0.0)):
    bpy.context.view_layer.update()
    pbone = arm_obj.pose.bones[socket_bone_name]
    rx_neg90 = mathutils.Matrix.Rotation(math.radians(-90.0), 4, "X")
    extra = mathutils.Euler([math.radians(a) for a in extra_euler_deg], "XYZ").to_matrix().to_4x4()
    preview_obj.matrix_world = arm_obj.matrix_world @ pbone.matrix @ rx_neg90 @ extra


def aim_limb_world(arm_obj, upper_bone_name: str, wrist_bone_name: str, target_dir_world: mathutils.Vector, twist_deg: float = 0.0):
    bpy.context.view_layer.update()
    pb_up = arm_obj.pose.bones[upper_bone_name]
    pb_wr = arm_obj.pose.bones[wrist_bone_name]
    p0 = (arm_obj.matrix_world @ pb_up.matrix).to_translation()
    p1 = (arm_obj.matrix_world @ pb_wr.matrix).to_translation()
    cur_dir = (p1 - p0).normalized()
    tgt_dir = target_dir_world.normalized()
    q_world = cur_dir.rotation_difference(tgt_dir)
    if abs(twist_deg) > 1e-4:
        q_world = mathutils.Quaternion(tgt_dir, math.radians(twist_deg)) @ q_world
    m_pose = pb_up.matrix.copy()
    new_m = (q_world.to_matrix() @ m_pose.to_3x3()).to_4x4()
    new_m.translation = m_pose.translation
    pb_up.matrix = new_m
    bpy.context.view_layer.update()


def main() -> None:
    EXP_DIR.mkdir(parents=True, exist_ok=True)
    REN_DIR.mkdir(parents=True, exist_ok=True)
    DOCS_DIR.mkdir(parents=True, exist_ok=True)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")

    # Collections matching Alona_Restored.blend
    c_char = bpy.data.collections.new("Rawlins_Character")
    c_lods = bpy.data.collections.new("Rawlins_LODs")
    c_gun = bpy.data.collections.new("Rawlins_Gun")
    c_gun_lods = bpy.data.collections.new("Rawlins_Gun_LODs")
    c_prev = bpy.data.collections.new("Rawlins_Equipped_Preview")
    c_light = bpy.data.collections.new("Studio_Lighting")
    for c in (c_char, c_lods, c_gun, c_gun_lods, c_prev, c_light):
        scene.collection.children.link(c)

    # Build PBR materials
    char_slots = ["main_mat", "cape_mat", "ammo_mat"]
    for s in char_slots:
        build_pbr_material(s, "T_BW_Rawlins_BC.png", "T_BW_Rawlins_N.png", "T_BW_Rawlins_ORM.png", "T_BW_Rawlins_E.png", emissive_strength=4.5)
    build_pbr_material("raw_gun_base_std_mat", "T_BW_Rawlins_Gun_BC.png", "T_BW_Rawlins_Gun_N.png", "T_BW_Rawlins_Gun_ORM.png", "T_BW_Rawlins_Gun_E.png", emissive_strength=5.0)

    # 1. Character Armature + LOD0..3
    char_stems = [
        "new_rawlins_opti_mesh",
        "new_rawlins_opti_mesh_lod1",
        "new_rawlins_opti_mesh_lod2",
        "new_rawlins_opti_mesh_lod3",
    ]
    main_arm = None
    char_meshes = []
    for li, stem in enumerate(char_stems):
        arm, mobj = import_glb_objects(GLB_DIR / f"{stem}.glb")
        mobj.name = f"SKM_BW_Rawlins_LOD{li}"
        mobj.data.name = f"SKM_BW_Rawlins_LOD{li}"
        inject_exact_dae_uvs_and_slots(mobj, DAE_DIR / f"{stem}.dae", char_slots, ["main_mat", "cape_mat", "ammo_mat"])
        if li == 0:
            main_arm = arm
            main_arm.name = "Armature"
            main_arm.data.name = "SK_BW_Rawlins"
            main_arm.location = (0.0, 0.0, 0.0)
            main_arm.rotation_euler = (0.0, 0.0, 0.0)
            main_arm.scale = (1.0, 1.0, 1.0)
            move_to_collection(main_arm, c_char)
            move_to_collection(mobj, c_char)
        else:
            mobj.parent = main_arm
            for mod in mobj.modifiers:
                if mod.type == "ARMATURE":
                    mod.object = main_arm
            if arm and arm != main_arm:
                bpy.data.objects.remove(arm, do_unlink=True)
            move_to_collection(mobj, c_lods)
            mobj.hide_render = True
            mobj.hide_viewport = True
        char_meshes.append(mobj)

    # 2. Weapon Tomb Breaker StaticMeshes LOD0..3 (strip weapon skeleton so StaticMesh pivot sits cleanly at (0,0,0))
    gun_stems = [
        "new_rawlins_base_weapon_mesh",
        "new_rawlins_base_weapon_mesh_lod1",
        "new_rawlins_base_weapon_mesh_lod2",
        "new_rawlins_base_weapon_mesh_lod3",
    ]
    gun_meshes = []
    grip_align_mat = (
        mathutils.Matrix.Translation((0.0, -0.022, 0.0))
        @ mathutils.Matrix.Rotation(math.radians(90.0), 4, "Z")
        @ mathutils.Matrix.Translation((0.0, -0.235, 0.095))
    )
    for li, stem in enumerate(gun_stems):
        g_arm, g_mesh = import_glb_objects(GLB_DIR / f"{stem}.glb")
        g_mesh.parent = None
        g_mesh.matrix_world = mathutils.Matrix.Identity(4)
        for mod in list(g_mesh.modifiers):
            if mod.type == "ARMATURE":
                g_mesh.modifiers.remove(mod)
        if g_arm:
            bpy.data.objects.remove(g_arm, do_unlink=True)

        g_mesh.name = f"SM_BW_Rawlins_TombBreaker_LOD{li}"
        g_mesh.data.name = f"SM_BW_Rawlins_TombBreaker_LOD{li}"
        # 1) Inject exact 1:1 DAE UVs at original CryEngine coordinates first
        inject_exact_dae_uvs_and_slots(g_mesh, DAE_DIR / f"{stem}.dae", ["raw_gun_base_std_mat"], ["material", "raw_gun"])

        # 2) Strip transient bind_ammo VFX shell vertices (absent from raw_gun_base_mesh.cgf) before clearing groups
        vg_ammo = g_mesh.vertex_groups.get("bind_ammo")
        if vg_ammo is not None:
            ammo_gi = vg_ammo.index
            kill_indices = {
                v.index for v in g_mesh.data.vertices
                if any(g.group == ammo_gi and g.weight > 0.1 for g in v.groups)
            }
            if kill_indices:
                bm = bmesh.new()
                bm.from_mesh(g_mesh.data)
                bm.verts.ensure_lookup_table()
                bmesh.ops.delete(bm, geom=[bm.verts[i] for i in kill_indices], context="VERTS")
                bm.to_mesh(g_mesh.data)
                bm.free()

        g_mesh.vertex_groups.clear()
        # 3) Align pistol grip & trigger origin + muzzle aiming axis to S_RAttachment socket convention
        g_mesh.data.transform(grip_align_mat)
        g_mesh.data.update()

        move_to_collection(g_mesh, c_gun if li == 0 else c_gun_lods)
        g_mesh.hide_render = True
        if li > 0:
            g_mesh.hide_viewport = True
        gun_meshes.append(g_mesh)

    # 3. Export SkeletalMesh FBXs (LOD0..3 + base SKM_BW_Rawlins.fbx) and Weapon StaticMesh FBXs (LOD0..3 + base)
    for li, mobj in enumerate(char_meshes):
        was_hidden = mobj.hide_viewport
        mobj.hide_viewport = False
        bpy.ops.object.select_all(action="DESELECT")
        main_arm.select_set(True)
        mobj.select_set(True)
        bpy.context.view_layer.objects.active = main_arm
        fbx_out = EXP_DIR / f"SKM_BW_Rawlins_LOD{li}.fbx"
        bpy.ops.export_scene.fbx(
            filepath=str(fbx_out),
            use_selection=True,
            object_types={"ARMATURE", "MESH"},
            use_mesh_modifiers=True,
            add_leaf_bones=False,
            bake_anim=False,
            mesh_smooth_type="FACE",
            use_tspace=True,
            path_mode="AUTO",
        )
        if li == 0:
            shutil.copy2(fbx_out, EXP_DIR / "SKM_BW_Rawlins.fbx")
        mobj.hide_viewport = was_hidden

    for li, gobj in enumerate(gun_meshes):
        was_hidden = gobj.hide_viewport
        gobj.hide_viewport = False
        gobj.location = (0.0, 0.0, 0.0)
        gobj.rotation_euler = (0.0, 0.0, 0.0)
        bpy.ops.object.select_all(action="DESELECT")
        gobj.select_set(True)
        bpy.context.view_layer.objects.active = gobj
        fbx_out = EXP_DIR / f"SM_BW_Rawlins_TombBreaker_LOD{li}.fbx"
        bpy.ops.export_scene.fbx(
            filepath=str(fbx_out),
            use_selection=True,
            object_types={"MESH"},
            use_mesh_modifiers=True,
            mesh_smooth_type="FACE",
            use_tspace=True,
            path_mode="AUTO",
        )
        if li == 0:
            shutil.copy2(fbx_out, EXP_DIR / "SM_BW_Rawlins_TombBreaker.fbx")
        # Offset standalone weapon display beside character like Alona_Restored.blend
        gobj.location = (-1.15, 0.0, 1.15)
        gobj.hide_viewport = was_hidden

    # 4. Create HandPreview revolver attached to S_RAttachment
    gun_preview = gun_meshes[0].copy()
    gun_preview.data = gun_meshes[0].data.copy()
    gun_preview.name = "SM_BW_Rawlins_TombBreaker_HandPreview"
    gun_preview.hide_render = False
    gun_preview.hide_viewport = False
    move_to_collection(gun_preview, c_prev)
    attach_preview_to_socket(gun_preview, main_arm, "S_RAttachment")

    # 5. Studio lighting + Camera + Neutral-light & Aiming-pose verification renders
    setup_studio_lighting(c_light)
    cam_data = bpy.data.cameras.new("Studio_Camera")
    cam_obj = bpy.data.objects.new("Studio_Camera", cam_data)
    c_light.objects.link(cam_obj)
    scene.camera = cam_obj

    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 28
    scene.cycles.use_denoising = False

    # Rest-pose neutral-light front, rear, and right-hand grip/revolver close-up
    render_view(cam_obj, (0.0, -3.55, 1.15), (0.0, 0.0, 1.08), REN_DIR / "rawlins_front.png", res=(1080, 1280), lens=54.0)
    render_view(cam_obj, (0.0, 3.55, 1.15), (0.0, 0.0, 1.08), REN_DIR / "rawlins_back.png", res=(1080, 1280), lens=54.0)

    r_hand_pos = (main_arm.matrix_world @ main_arm.pose.bones["S_RAttachment"].matrix).to_translation()
    render_view(
        cam_obj,
        (r_hand_pos.x - 0.65, r_hand_pos.y - 1.15, r_hand_pos.z + 0.05),
        (r_hand_pos.x - 0.16, r_hand_pos.y - 0.06, r_hand_pos.z - 0.18),
        REN_DIR / "rawlins_equipment_closeup.png",
        res=(1024, 1024),
        lens=60.0,
    )

    # Aiming pose check: raise right arm horizontally forward toward target (-Y) and curl fingers onto grip + trigger
    aim_limb_world(main_arm, "bind_right_upperArm_A", "bind_right_wrist", mathutils.Vector((-0.14, -0.98, 0.12)), twist_deg=52.0)
    for bname, degs in [
        ("bind_right_index_1", (0.0, 0.0, -24.0)),
        ("bind_right_index_2", (0.0, 0.0, -34.0)),
        ("bind_right_middle_1", (0.0, 0.0, -58.0)),
        ("bind_right_middle_2", (0.0, 0.0, -56.0)),
        ("bind_right_ring_1", (0.0, 0.0, -62.0)),
        ("bind_right_ring_2", (0.0, 0.0, -58.0)),
        ("bind_right_pinky_1", (0.0, 0.0, -64.0)),
        ("bind_right_pinky_2", (0.0, 0.0, -58.0)),
        ("bind_right_thumb_1", (18.0, 0.0, -28.0)),
    ]:
        pb = main_arm.pose.bones.get(bname)
        if pb:
            pb.rotation_mode = "XYZ"
            pb.rotation_euler = [math.radians(a) for a in degs]

    attach_preview_to_socket(gun_preview, main_arm, "S_RAttachment")
    aim_hand_pos = (main_arm.matrix_world @ main_arm.pose.bones["S_RAttachment"].matrix).to_translation()
    render_view(
        cam_obj,
        (-1.85, -2.85, 1.48),
        (-0.20, -0.35, 1.32),
        REN_DIR / "rawlins_aiming_pose_check.png",
        res=(1080, 1080),
        lens=54.0,
    )
    render_view(
        cam_obj,
        (aim_hand_pos.x - 0.88, aim_hand_pos.y - 0.72, aim_hand_pos.z + 0.16),
        (aim_hand_pos.x - 0.05, aim_hand_pos.y - 0.22, aim_hand_pos.z + 0.06),
        REN_DIR / "rawlins_aiming_grip_closeup.png",
        res=(1024, 1024),
        lens=62.0,
    )

    # Reset pose cleanly to rest/bind pose before saving Rawlins_Restored.blend
    for pb in main_arm.pose.bones:
        pb.matrix_basis = mathutils.Matrix.Identity(4)
    attach_preview_to_socket(gun_preview, main_arm, "S_RAttachment")
    gun_meshes[0].hide_render = False

    cam_obj.location = mathutils.Vector((0.0, -3.55, 1.15))
    look_at(cam_obj, mathutils.Vector((0.0, 0.0, 1.08)))
    blend_path = ROOT / "Rawlins_Restored.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    bpy.ops.file.make_paths_relative()
    bpy.ops.wm.save_mainfile()
    print("Saved Rawlins_Restored.blend and exported all FBXs + renders successfully!")


if __name__ == "__main__":
    main()
