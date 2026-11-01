#!/usr/bin/env python3
r"""
Assemble SourceArt/Spartacus/Spartacus_Restored.blend, verify dual hand-specific gladiator sword
attachments (Sword1 on S_LAttachment, Sword2 on S_RAttachment), test sword edges/grips/wrists
through an attack slash and recovery pose, render neutral-light captures, and export UE 5.8 FBXs + LODs.
"""

from __future__ import annotations

import math
import shutil
import xml.etree.ElementTree as ET
from pathlib import Path

import bpy
import mathutils

ROOT = Path(__file__).resolve().parents[1]
GLB_DIR = ROOT / "Converted" / "GLB"
DAE_DIR = ROOT / "Converted" / "DAE"
TEX_DIR = ROOT / "Textures" / "Prepared"
EXP_DIR = ROOT / "Exports"
REN_DIR = ROOT / "Renders"


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


def build_pbr_material(mat_name: str, bc_file: str, n_file: str, orm_file: str):
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

    mul_ao = nt.nodes.new("ShaderNodeMix")
    mul_ao.data_type = "RGBA"
    mul_ao.blend_type = "MULTIPLY"
    mul_ao.inputs["Factor"].default_value = 0.45
    mul_ao.location = (60, 220)
    nt.links.new(tex_bc.outputs["Color"], mul_ao.inputs["A"])
    nt.links.new(sep_orm.outputs["Red"], mul_ao.inputs["B"])
    nt.links.new(mul_ao.outputs["Result"], bsdf.inputs["Base Color"])
    nt.links.new(sep_orm.outputs["Green"], bsdf.inputs["Roughness"])
    nt.links.new(sep_orm.outputs["Blue"], bsdf.inputs["Metallic"])

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
    bg = world.node_tree.nodes.get("Background")
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
    cam_obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


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


def curl_hand_grip(arm_obj, side: str) -> None:
    y_deg = 52.0
    z_deg = -18.0 if side == "right" else -10.0
    for finger in ("index", "middle", "ring", "pinky"):
        for j, scale in ((1, 0.95), (2, 1.15), (3, 0.85)):
            pb = arm_obj.pose.bones.get(f"bind_{side}_{finger}_{j}")
            if pb:
                pb.rotation_mode = "XYZ"
                pb.rotation_euler = (0.0, math.radians(y_deg * scale), math.radians(z_deg * scale))
    for j, (ty, tz) in ((1, (22.0, -18.0)), (2, (32.0, -12.0)), (3, (24.0, -8.0))):
        pb = arm_obj.pose.bones.get(f"bind_{side}_thumb_{j}")
        if pb:
            pb.rotation_mode = "XYZ"
            pb.rotation_euler = (0.0, math.radians(ty), math.radians(tz))
    bpy.context.view_layer.update()


def import_and_export_static_weapon(stems: list[str], asset_stem: str, mat_name: str, coll_main, coll_lods, display_loc, grip_offset_z: float = 0.0):
    meshes = []
    offset_vec = mathutils.Vector((0.0, 0.0, grip_offset_z))
    for li, stem in enumerate(stems):
        w_arm, w_mesh = import_glb_objects(GLB_DIR / f"{stem}.glb")
        w_mesh.parent = None
        w_mesh.matrix_world = mathutils.Matrix.Identity(4)
        for mod in list(w_mesh.modifiers):
            if mod.type == "ARMATURE":
                w_mesh.modifiers.remove(mod)
        w_mesh.vertex_groups.clear()
        if w_arm:
            bpy.data.objects.remove(w_arm, do_unlink=True)

        w_mesh.name = f"{asset_stem}_LOD{li}"
        w_mesh.data.name = f"{asset_stem}_LOD{li}"
        inject_exact_dae_uvs_and_slots(w_mesh, DAE_DIR / f"{stem}.dae", [mat_name], ["sword", "material"])
        if abs(grip_offset_z) > 1e-6:
            for v in w_mesh.data.vertices:
                v.co += offset_vec
            w_mesh.data.update()
        move_to_collection(w_mesh, coll_main if li == 0 else coll_lods)
        w_mesh.hide_render = True
        if li > 0:
            w_mesh.hide_viewport = True

        was_hidden = w_mesh.hide_viewport
        w_mesh.hide_viewport = False
        w_mesh.location = (0.0, 0.0, 0.0)
        w_mesh.rotation_euler = (0.0, 0.0, 0.0)
        bpy.ops.object.select_all(action="DESELECT")
        w_mesh.select_set(True)
        bpy.context.view_layer.objects.active = w_mesh
        fbx_out = EXP_DIR / f"{asset_stem}_LOD{li}.fbx"
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
            shutil.copy2(fbx_out, EXP_DIR / f"{asset_stem}.fbx")
        w_mesh.location = display_loc
        w_mesh.hide_viewport = was_hidden
        meshes.append(w_mesh)
    return meshes


def main() -> None:
    EXP_DIR.mkdir(parents=True, exist_ok=True)
    REN_DIR.mkdir(parents=True, exist_ok=True)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")

    c_char = bpy.data.collections.new("Spartacus_Character")
    c_lods = bpy.data.collections.new("Spartacus_LODs")
    c_sw1 = bpy.data.collections.new("Spartacus_SwordL")
    c_sw1_lods = bpy.data.collections.new("Spartacus_SwordL_LODs")
    c_sw2 = bpy.data.collections.new("Spartacus_SwordR")
    c_sw2_lods = bpy.data.collections.new("Spartacus_SwordR_LODs")
    c_prev = bpy.data.collections.new("Spartacus_Equipped_Preview")
    c_light = bpy.data.collections.new("Studio_Lighting")
    for c in (c_char, c_lods, c_sw1, c_sw1_lods, c_sw2, c_sw2_lods, c_prev, c_light):
        scene.collection.children.link(c)

    char_slots = ["spart_main", "spart_skin"]
    build_pbr_material("spart_main", "T_BW_Spartacus_BC.png", "T_BW_Spartacus_N.png", "T_BW_Spartacus_ORM.png")
    build_pbr_material("spart_skin", "T_BW_Spartacus_BC.png", "T_BW_Spartacus_N.png", "T_BW_Spartacus_ORM.png")
    build_pbr_material("sword1_mat", "T_BW_Spartacus_Sword1_BC.png", "T_BW_Spartacus_Sword1_N.png", "T_BW_Spartacus_Sword1_ORM.png")
    build_pbr_material("sword2_mat_matGroup", "T_BW_Spartacus_Sword2_BC.png", "T_BW_Spartacus_Sword2_N.png", "T_BW_Spartacus_Sword2_ORM.png")

    # 1. Character Armature + LOD0..3
    char_stems = [
        "spartacus_opti_mesh",
        "spartacus_opti_mesh_lod1",
        "spartacus_opti_mesh_lod2",
        "spartacus_opti_mesh_lod3",
    ]
    main_arm = None
    char_meshes = []
    for li, stem in enumerate(char_stems):
        arm, mobj = import_glb_objects(GLB_DIR / f"{stem}.glb")
        mobj.name = f"SKM_BW_Spartacus_LOD{li}"
        mobj.data.name = f"SKM_BW_Spartacus_LOD{li}"
        inject_exact_dae_uvs_and_slots(mobj, DAE_DIR / f"{stem}.dae", char_slots, ["spart_main", "spart_skin"])
        if li == 0:
            main_arm = arm
            main_arm.name = "Armature"
            main_arm.data.name = "SK_BW_Spartacus"
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

    for li, mobj in enumerate(char_meshes):
        was_hidden = mobj.hide_viewport
        mobj.hide_viewport = False
        bpy.ops.object.select_all(action="DESELECT")
        main_arm.select_set(True)
        mobj.select_set(True)
        bpy.context.view_layer.objects.active = main_arm
        fbx_out = EXP_DIR / f"SKM_BW_Spartacus_LOD{li}.fbx"
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
            shutil.copy2(fbx_out, EXP_DIR / "SKM_BW_Spartacus.fbx")
        mobj.hide_viewport = was_hidden

    # 2. Both Hand-Specific Swords (SwordL = spartacus_sword_mesh on S_LAttachment; SwordR = sword2_mesh on S_RAttachment)
    sw1_meshes = import_and_export_static_weapon(
        ["spartacus_sword_mesh", "spartacus_sword_mesh_lod1", "spartacus_sword_mesh_lod2", "spartacus_sword_mesh_lod3"],
        "SM_BW_Spartacus_SwordL",
        "sword1_mat",
        c_sw1,
        c_sw1_lods,
        display_loc=(1.08, 0.0, 0.55),
        grip_offset_z=0.065,
    )
    sw2_meshes = import_and_export_static_weapon(
        ["sword2_mesh", "sword2_mesh_lod1", "sword2_mesh_lod2", "sword2_mesh_lod3"],
        "SM_BW_Spartacus_SwordR",
        "sword2_mat_matGroup",
        c_sw2,
        c_sw2_lods,
        display_loc=(-1.08, 0.0, 0.55),
        grip_offset_z=0.085,
    )

    # 3. HandPreviews attached to S_LAttachment and S_RAttachment
    sw1_prev = sw1_meshes[0].copy()
    sw1_prev.data = sw1_meshes[0].data.copy()
    sw1_prev.name = "SM_BW_Spartacus_SwordL_HandPreview"
    sw1_prev.hide_render = False
    sw1_prev.hide_viewport = False
    move_to_collection(sw1_prev, c_prev)
    attach_preview_to_socket(sw1_prev, main_arm, "S_LAttachment")

    sw2_prev = sw2_meshes[0].copy()
    sw2_prev.data = sw2_meshes[0].data.copy()
    sw2_prev.name = "SM_BW_Spartacus_SwordR_HandPreview"
    sw2_prev.hide_render = False
    sw2_prev.hide_viewport = False
    move_to_collection(sw2_prev, c_prev)
    attach_preview_to_socket(sw2_prev, main_arm, "S_RAttachment")

    # 4. Studio lighting + Neutral-light, Dual-Sword Equipment, Attack-pose, & Recovery-pose renders
    setup_studio_lighting(c_light)
    cam_data = bpy.data.cameras.new("Studio_Camera")
    cam_obj = bpy.data.objects.new("Studio_Camera", cam_data)
    c_light.objects.link(cam_obj)
    scene.camera = cam_obj

    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = 28
    scene.cycles.use_denoising = False

    render_view(cam_obj, (0.0, -4.15, 1.32), (0.0, 0.0, 1.25), REN_DIR / "spartacus_front.png", res=(1080, 1280), lens=52.0)
    render_view(cam_obj, (0.0, 4.15, 1.32), (0.0, 0.0, 1.25), REN_DIR / "spartacus_back.png", res=(1080, 1280), lens=52.0)

    curl_hand_grip(main_arm, "right")
    curl_hand_grip(main_arm, "left")
    attach_preview_to_socket(sw1_prev, main_arm, "S_LAttachment")
    attach_preview_to_socket(sw2_prev, main_arm, "S_RAttachment")

    r_hand_pos = (main_arm.matrix_world @ main_arm.pose.bones["S_RAttachment"].matrix).to_translation()
    render_view(
        cam_obj,
        (r_hand_pos.x - 0.72, r_hand_pos.y - 1.38, r_hand_pos.z + 0.35),
        (r_hand_pos.x + 0.05, r_hand_pos.y - 0.15, r_hand_pos.z + 0.22),
        REN_DIR / "spartacus_equipment_closeup.png",
        res=(1024, 1200),
        lens=58.0,
    )

    # Attack slash pose: cross-body dual-sword strike testing blade edges, grips, and wrist alignment
    aim_limb_world(main_arm, "bind_right_upperArm_A", "bind_right_wrist", mathutils.Vector((0.28, -0.94, 0.16)), twist_deg=35.0)
    aim_limb_world(main_arm, "bind_left_upperArm_A", "bind_left_wrist", mathutils.Vector((0.68, 0.35, -0.64)), twist_deg=-25.0)

    attach_preview_to_socket(sw1_prev, main_arm, "S_LAttachment")
    attach_preview_to_socket(sw2_prev, main_arm, "S_RAttachment")
    render_view(cam_obj, (-1.45, -3.75, 1.55), (0.0, -0.20, 1.32), REN_DIR / "spartacus_attack_pose_check.png", res=(1080, 1080), lens=50.0)

    # Recovery guard pose: wide gladiator guard recovery testing wrist return & off-hand blade clearance
    for pb in main_arm.pose.bones:
        pb.matrix_basis = mathutils.Matrix.Identity(4)
    curl_hand_grip(main_arm, "right")
    curl_hand_grip(main_arm, "left")
    aim_limb_world(main_arm, "bind_right_upperArm_A", "bind_right_wrist", mathutils.Vector((-0.78, -0.48, 0.38)), twist_deg=20.0)
    aim_limb_world(main_arm, "bind_left_upperArm_A", "bind_left_wrist", mathutils.Vector((0.45, -0.86, 0.24)), twist_deg=-30.0)

    attach_preview_to_socket(sw1_prev, main_arm, "S_LAttachment")
    attach_preview_to_socket(sw2_prev, main_arm, "S_RAttachment")
    render_view(cam_obj, (1.45, -3.75, 1.55), (0.0, -0.15, 1.32), REN_DIR / "spartacus_recovery_pose_check.png", res=(1080, 1080), lens=50.0)

    # Reset pose before saving Spartacus_Restored.blend
    for pb in main_arm.pose.bones:
        pb.matrix_basis = mathutils.Matrix.Identity(4)
    attach_preview_to_socket(sw1_prev, main_arm, "S_LAttachment")
    attach_preview_to_socket(sw2_prev, main_arm, "S_RAttachment")
    sw1_meshes[0].hide_render = False
    sw2_meshes[0].hide_render = False

    cam_obj.location = mathutils.Vector((0.0, -4.15, 1.32))
    look_at(cam_obj, mathutils.Vector((0.0, 0.0, 1.25)))
    blend_path = ROOT / "Spartacus_Restored.blend"
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_path))
    bpy.ops.file.make_paths_relative()
    bpy.ops.wm.save_mainfile()
    print("Saved Spartacus_Restored.blend and exported all FBXs + renders successfully!")


if __name__ == "__main__":
    main()
