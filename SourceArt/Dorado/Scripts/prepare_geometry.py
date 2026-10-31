"""Recover source Dorado geometry without modifying the external archive.

Stage and convert: python prepare_geometry.py --stage --convert
Blender assembly: blender -b --factory-startup --python prepare_geometry.py -- --blender
Use --only RING_BASENAME for a disposable subset before a complete conversion.
"""
from __future__ import annotations
import argparse
import base64
import hashlib
import json
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT.parents[1]
SOURCE = Path('F:/BreakawayConverted/Unpacked')
CONVERTER = Path('F:/CryEngineConversionTools/cgf-converter.exe')
CENTER = [513.97998, 666.29779, 73.674896]
TARGET_SCALE = 0.5
NS = {'c': 'http://www.collada.org/2005/11/COLLADASchema'}


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2), encoding='utf-8')


def normalized(value):
    return re.sub(r'[^A-Za-z0-9_]+', '_', value).strip('_')


def stage(only=None):
    report = json.loads((PROJECT/'Saved/Logs/dorado-reconstruction-20261030/source-material-mapping.json').read_text())
    additions = ROOT/'Config/additional_placements.json'
    if additions.exists():
        extra = json.loads(additions.read_text())
        if isinstance(extra, dict):
            extra = extra.get('placements', extra.get('additional_placements', []))
        placements_by_id = {row['Id']: row for row in report['placements']}
        placements_by_id.update({row['Id']: row for row in extra})
        report['placements'] = list(placements_by_id.values())
    index = {}
    for folder in sorted(SOURCE.glob('objects-part*')):
        for file in folder.rglob('*.cgf'):
            index[file.relative_to(folder).as_posix().lower()] = file
    material_index = {}
    for material in report['materials']:
        material_index.setdefault(Path(material['file']).stem.lower(), Path(material['file']))
    placements, unresolved, assets = [], [], {}
    for row in report['placements']:
        path = row.get('Prefab') or row.get('Geometry')
        if not path or not path.lower().endswith('.cgf'):
            continue
        if row.get('HiddenInGame') == '1':
            continue
        key = path.replace('\\', '/').lower()
        src = index.get(key)
        if not src:
            unresolved.append({'name': row.get('Name'), 'geometry': path})
            continue
        if only and src.stem.lower() != only.lower():
            continue
        asset_id = 'SM_Dorado_' + normalized(src.stem)
        if asset_id not in assets:
            strings = [b.decode('ascii') for b in re.findall(rb'[ -~]{5,}', src.read_bytes())]
            mats = [material_index[Path(s.replace('\\', '/')).stem.lower()] for s in strings if Path(s.replace('\\', '/')).stem.lower() in material_index]
            mats = list(dict.fromkeys(mats))
            primary = mats[0] if mats else None
            owned = ROOT/'Sources/CGF'/src.relative_to(SOURCE/'objects-part0') if 'objects-part0' in src.parts else ROOT/'Sources/CGF'/src.name
            owned.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, owned)
            material_rows = []
            if primary:
                material_rows = [m for m in report['materials'] if Path(m['file']) == primary]
            assets[asset_id] = {
                'id': asset_id, 'source_cgf': str(src), 'source_mtl': str(primary) if primary else None,
                'owned_cgf': str(owned), 'source_sha256': hashlib.sha256(src.read_bytes()).hexdigest(),
                'dae': str(ROOT/'Exports/DAE'/(src.stem+'.dae')),
                'fbx': str(ROOT/'Exports/FBX'/(asset_id+'.fbx')),
                'material_rows': material_rows,
                'collision_role': 'source_collision' if re.search('collision|collider', src.stem, re.I) else ('decorative' if any(x in key for x in ('geo_vista', 'vegetation', 'waterfall', 'ring', 'vfx', 'cloud')) else 'architecture'),
            }
            for material_path in mats:
                material_target = ROOT/'Sources/MTL'/material_path.name
                material_target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(material_path, material_target)
                # Geometry conversion must never traverse the archive looking for textures.
                # Original materials stay untouched; texture recovery has its own manifest.
                tree = ET.parse(material_path)
                for parent in tree.iter('Material'):
                    for child in list(parent):
                        if child.tag == 'Textures':
                            parent.remove(child)
                    if parent.find('SubMaterials') is None:
                        textures = ET.SubElement(parent, 'Textures')
                        ET.SubElement(textures, 'Texture', Map='Diffuse', File='DoradoConversionPlaceholder.png')
                derived = owned.parent/material_path.name
                tree.write(derived, encoding='utf-8', xml_declaration=True)
        pos = [float(v) for v in row.get('Pos', '0,0,0').split(',')]
        qw, qx, qy, qz = [float(v) for v in row.get('Rotate', '1,0,0,0').split(',')]
        scale = [float(v) for v in row.get('Scale', '1,1,1').split(',')]
        placements.append({
            'id': row.get('Id'), 'name': row.get('Name'), 'asset_id': asset_id,
            'layer': row.get('Layer'), 'source_layer': row.get('sourceLayerFile'),
            'material_override': row.get('Material'),
            'source_position_m': pos, 'source_rotation_wxyz': [qw, qx, qy, qz], 'source_scale': scale,
            'location_cm': [(pos[0]-CENTER[0])*50, -(pos[1]-CENTER[1])*50, (pos[2]-CENTER[2])*50],
            'rotation_quat_xyzw': [-qx, qy, -qz, qw], 'scale': [v*TARGET_SCALE for v in scale],
            'collision_role': assets[asset_id]['collision_role'],
        })
    (ROOT/'Sources/CGF/DoradoConversionPlaceholder.png').write_bytes(base64.b64decode('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+jwK0AAAAASUVORK5CYII='))
    output = {'source_center_m': CENTER, 'source_units_to_cm': 100, 'target_scale': TARGET_SCALE,
              'axis': 'UE=(x,-y,z)', 'assets': list(assets.values()), 'instances': placements,
              'unresolved': unresolved, 'failures': [], 'status': 'staged'}
    target = ROOT/'Config'/('geometry_stage_'+only+'.json' if only else 'geometry_stage.json')
    write_json(target, output)
    print(json.dumps({'stage': str(target), 'assets': len(assets), 'instances': len(placements), 'unresolved': len(unresolved)}))
    return target


def convert(stage_path):
    report = json.loads(stage_path.read_text())
    logs = PROJECT/'Saved/Logs/dorado-reconstruction-20261030/conversion'
    logs.mkdir(parents=True, exist_ok=True)
    for asset in report['assets']:
        target = Path(asset['dae'])
        target.parent.mkdir(parents=True, exist_ok=True)
        signature = conversion_signature(asset)
        provenance = target.with_suffix('.conversion.json')
        if target.exists() and target.stat().st_size > 200 and provenance.exists() and json.loads(provenance.read_text()) == signature:
            continue
        args = [str(CONVERTER), asset['owned_cgf'], '-dae', '-png', '-notex', '-objectdir', str(ROOT/'Sources/CGF')]
        if asset['source_mtl']:
            args.extend(['-mtl', str(Path(asset['owned_cgf']).parent/Path(asset['source_mtl']).name)])
        try:
            proc = subprocess.run(args, capture_output=True, text=True, timeout=60, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
            (logs/(asset['id']+'.log')).write_text(proc.stdout+'\n'+proc.stderr, encoding='utf-8')
            generated = Path(asset['owned_cgf']).with_suffix('.dae')
            if proc.returncode != 0 or not generated.exists():
                raise RuntimeError('conversion exit='+str(proc.returncode))
            shutil.copy2(generated, target)
            write_json(provenance, signature)
            asset['converted'] = True
            print('CONVERTED '+asset['id'], flush=True)
        except Exception as exc:
            report['failures'].append({'id': asset['id'], 'stage': 'convert', 'error': str(exc)})
            print('FAILED '+asset['id']+': '+str(exc), flush=True)
    report['status'] = 'converted' if not report['failures'] else 'partial_conversion'
    write_json(stage_path, report)


def conversion_signature(asset):
    material = Path(asset['owned_cgf']).parent/Path(asset['source_mtl']).name if asset['source_mtl'] else None
    return {'pipeline_version': 1, 'converter_version': '1.6.0.0',
            'cgf_sha256': hashlib.sha256(Path(asset['owned_cgf']).read_bytes()).hexdigest(),
            'derived_mtl_sha256': hashlib.sha256(material.read_bytes()).hexdigest() if material and material.exists() else None}


def source_material(asset, name):
    for row in asset['material_rows']:
        if row['name'].lower() == name.lower() or name.lower().endswith('_'+row['name'].lower()):
            return row
    return None


def assemble(stage_path):
    import bpy
    from mathutils import Matrix, Quaternion, Vector
    report = json.loads(stage_path.read_text())
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 0.01
    kit = bpy.data.collections.new('Dorado_Modular_Kit')
    bpy.context.scene.collection.children.link(kit)
    material_cache = {}
    asset_objects = {}
    excluded = []

    def material_for(asset, name):
        row = source_material(asset, name)
        if row:
            stem = Path(row['file']).stem
            full_id = normalized(f"{stem}__{row['submaterial_index']}__{row['name']}")
        else:
            full_id = normalized(f"{Path(asset['source_cgf']).stem}__unknown__{name}")
        short_id = 'DOR_'+hashlib.sha1(full_id.encode()).hexdigest()[:16]
        if full_id not in material_cache:
            mat = bpy.data.materials.new(short_id)
            if row:
                diffuse = row.get('properties', {}).get('Diffuse', '0.5,0.5,0.5,1')
                color = [float(v) for v in diffuse.split(',')]
                mat.diffuse_color = tuple((color+[1])[:4])
            mat['source_material_id'] = full_id
            mat['source_material_name'] = name
            material_cache[full_id] = mat
        return material_cache[full_id], {'name': short_id, 'original_name': name, 'material_id': full_id,
                                      'source_mtl': row['file'] if row else asset['source_mtl'],
                                      'source_index': row['submaterial_index'] if row else None}

    def read_dae(asset):
        tree = ET.parse(asset['dae'])
        doc = tree.getroot()
        geometries = {g.get('id'): g for g in doc.findall('c:library_geometries/c:geometry', NS)}
        materials = {m.get('id'): m.get('name', m.get('id')) for m in doc.findall('c:library_materials/c:material', NS)}
        nodes = doc.findall('c:library_visual_scenes/c:visual_scene/c:node', NS)
        all_verts, all_faces, all_uvs, all_normals, face_materials, slots = [], [], [], [], [], []
        slot_lookup = {}

        def visit(node, parent):
            node_name = node.get('name', node.get('id', ''))
            if re.search(r'\$lod|physics|shadowproxy|proxy|collision', node_name, re.I) and asset['collision_role'] != 'source_collision':
                excluded.append({'asset': asset['id'], 'node': node_name})
                return
            matrix = Matrix.Identity(4)
            for child in node:
                tag = child.tag.rsplit('}', 1)[-1]
                if tag == 'matrix':
                    nums = [float(v) for v in child.text.split()]
                    matrix = matrix @ Matrix([nums[i:i+4] for i in range(0, 16, 4)]).transposed()
                elif tag == 'translate':
                    matrix = matrix @ Matrix.Translation(Vector([float(v) for v in child.text.split()]))
                elif tag == 'scale':
                    nums = [float(v) for v in child.text.split()]
                    matrix = matrix @ Matrix.Diagonal(Vector(nums+[1]))
                elif tag == 'rotate':
                    nums = [float(v) for v in child.text.split()]
                    import math
                    matrix = matrix @ Matrix.Rotation(math.radians(nums[3]), 4, Vector(nums[:3]))
            world = parent @ matrix
            normal_matrix = world.to_3x3().inverted_safe().transposed()
            for instance in node.findall('c:instance_geometry', NS):
                geometry = geometries[instance.get('url').lstrip('#')]
                mesh = geometry.find('c:mesh', NS)
                data = {}
                for source in mesh.findall('c:source', NS):
                    array = source.find('c:float_array', NS)
                    if array is not None and array.text:
                        accessor = source.find('c:technique_common/c:accessor', NS)
                        data[source.get('id')] = ([float(v) for v in array.text.split()], int(accessor.get('stride', '1')))
                vertex_sources = {}
                for vertices in mesh.findall('c:vertices', NS):
                    inp = vertices.find('c:input', NS)
                    vertex_sources[vertices.get('id')] = inp.get('source').lstrip('#')
                bindings = {m.get('symbol'): materials.get(m.get('target').lstrip('#'), m.get('target').lstrip('#')) for m in instance.findall('c:bind_material/c:technique_common/c:instance_material', NS)}
                for triangles in mesh.findall('c:triangles', NS):
                    if triangles.find('c:p', NS) is None or not triangles.find('c:p', NS).text:
                        continue
                    name = bindings.get(triangles.get('material'), triangles.get('material', 'Default'))
                    source_row = source_material(asset, name)
                    source_shader = source_row.get('properties', {}).get('Shader', '').lower() if source_row else ''
                    hidden_source_shader = source_shader in ('nodraw', 'helper')
                    if (hidden_source_shader or re.search('proxy|nodraw|collision|invisible', name, re.I)) and asset['collision_role'] != 'source_collision':
                        excluded.append({'asset': asset['id'], 'material': name, 'source_shader': source_shader,
                                         'triangles': int(triangles.get('count', 0)),
                                         'reason': 'source shader is non-rendering' if hidden_source_shader else 'source proxy/collision material'})
                        continue
                    if name not in slot_lookup:
                        mat, metadata = material_for(asset, name)
                        slot_lookup[name] = len(slots)
                        slots.append((mat, metadata))
                    inputs = {inp.get('semantic'): (inp.get('source').lstrip('#'), int(inp.get('offset', '0'))) for inp in triangles.findall('c:input', NS)}
                    stride = max(i[1] for i in inputs.values())+1
                    p = [int(v) for v in triangles.find('c:p', NS).text.split()]
                    for offset in range(0, len(p), stride*3):
                        face, uvs, normals = [], [], []
                        for corner in range(3):
                            idx = offset+corner*stride
                            key, slot = inputs['VERTEX']
                            key = vertex_sources.get(key, key)
                            nums, size = data[key]
                            point = world @ Vector(nums[p[idx+slot]*size:p[idx+slot]*size+3])
                            face.append(len(all_verts))
                            all_verts.append(tuple(point*100))
                            if 'TEXCOORD' in inputs:
                                key, slot = inputs['TEXCOORD']
                                nums, size = data[key]
                                uvs.append(tuple(nums[p[idx+slot]*size:p[idx+slot]*size+2]))
                            else:
                                uvs.append((0, 0))
                            if 'NORMAL' in inputs:
                                key, slot = inputs['NORMAL']
                                nums, size = data[key]
                                normal = normal_matrix @ Vector(nums[p[idx+slot]*size:p[idx+slot]*size+3])
                                normals.append(tuple(normal.normalized()))
                            else:
                                normals.append((0, 0, 0))
                        all_faces.append(face)
                        all_uvs.append(uvs)
                        all_normals.append(normals)
                        face_materials.append(slot_lookup[name])
            for child in node.findall('c:node', NS):
                visit(child, world)
        for node in nodes:
            visit(node, Matrix.Identity(4))
        if not all_faces:
            raise ValueError('No beauty triangles after filters')
        mesh = bpy.data.meshes.new(asset['id'])
        mesh.from_pydata(all_verts, [], all_faces)
        mesh.update()
        uv = mesh.uv_layers.new(name='UVMap')
        for poly, coords, index in zip(mesh.polygons, all_uvs, face_materials):
            poly.material_index = index
            poly.use_smooth = True
            for loop, coord in zip(poly.loop_indices, coords):
                uv.data[loop].uv = coord
        for mat, _ in slots:
            mesh.materials.append(mat)
        obj = bpy.data.objects.new(asset['id'], mesh)
        kit.objects.link(obj)
        obj['source_cgf'] = asset['source_cgf']
        obj['source_mtl'] = asset['source_mtl'] or ''
        # Preserve the authored per-corner normals across hard trim edges and bevels.
        # FBX/Unreal can deduplicate equivalent vertices without changing corner data.
        mesh.normals_split_custom_set([normal for face in all_normals for normal in face])
        asset['normals'] = 'source Collada corner normals; inverse-transpose transformed'
        asset['triangles'] = len(all_faces)
        asset['vertices'] = len(mesh.vertices)
        asset['materials'] = [{'slot': i, **metadata} for i, (_, metadata) in enumerate(slots)]
        asset['local_bounds_cm'] = {'min': [min(v[axis] for v in all_verts) for axis in range(3)], 'max': [max(v[axis] for v in all_verts) for axis in range(3)]}
        return obj

    for asset in report['assets']:
        try:
            obj = read_dae(asset)
            asset_objects[asset['id']] = obj
            bpy.ops.object.select_all(action='DESELECT')
            obj.select_set(True)
            bpy.context.view_layer.objects.active = obj
            Path(asset['fbx']).parent.mkdir(parents=True, exist_ok=True)
            bpy.ops.export_scene.fbx(filepath=asset['fbx'], use_selection=True, object_types={'MESH'},
                                     global_scale=1.0, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
                                     axis_forward='-Y', axis_up='Z', use_space_transform=True, bake_space_transform=False,
                                     mesh_smooth_type='FACE', use_mesh_modifiers=True, add_leaf_bones=False,
                                     bake_anim=False, path_mode='AUTO')
            asset['exported'] = True
            asset['fbx_sha256'] = hashlib.sha256(Path(asset['fbx']).read_bytes()).hexdigest()
            print('EXPORTED '+asset['id'], flush=True)
        except Exception as exc:
            import traceback
            traceback.print_exc()
            report['failures'].append({'id': asset['id'], 'stage': 'blender', 'error': str(exc)})
            print('FAILED '+asset['id']+': '+str(exc), flush=True)
    kit.hide_render = True
    kit.hide_viewport = True
    groups = {}
    for instance in report['instances']:
        if instance['asset_id'] not in asset_objects:
            continue
        source_obj = asset_objects[instance['asset_id']]
        layer = instance['layer'] or 'Environment'
        if layer not in groups:
            groups[layer] = bpy.data.collections.new(layer)
            bpy.context.scene.collection.children.link(groups[layer])
        obj = bpy.data.objects.new(normalized(instance['name']), source_obj.data)
        groups[layer].objects.link(obj)
        source_pos = instance['source_position_m']
        obj.location = [(source_pos[i]-CENTER[i])*50 for i in range(3)]
        obj.rotation_mode = 'QUATERNION'
        obj.rotation_quaternion = Quaternion(instance['source_rotation_wxyz'])
        obj.scale = instance['scale']
        obj['source_guid'] = instance['id'] or ''
        obj['asset_id'] = instance['asset_id']
        if instance['collision_role'] == 'source_collision':
            obj.hide_render = True
            obj.display_type = 'WIRE'
    report['excluded'] = excluded
    report['blender_version'] = bpy.app.version_string
    report['pipeline_version'] = 2
    report['beauty_filter'] = 'Exclude source Nodraw/Helper shaders, proxy/collision materials, and LOD/shadowproxy nodes; original CGF/DAE retained.'
    report['status'] = 'exported' if not report['failures'] else 'partial_export'
    for asset in report['assets']:
        asset.pop('material_rows', None)
    suffix = '_'+stage_path.stem.removeprefix('geometry_stage_') if stage_path.stem != 'geometry_stage' else ''
    blend = ROOT/('Dorado_Recovered'+suffix+'.blend')
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    report['blend'] = str(blend)
    write_json(ROOT/('geometry_manifest'+suffix+'.json'), report)
    print('DORADO_EXPORT_SUMMARY='+json.dumps({'assets': len(asset_objects), 'instances': len(report['instances']), 'failures': len(report['failures']), 'manifest': str(ROOT/('geometry_manifest'+suffix+'.json'))}))
    if not suffix and (ROOT/'Textures/texture_manifest.json').exists():
        bind_source_materials()


def main():
    arguments = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else sys.argv[1:]
    parser = argparse.ArgumentParser()
    parser.add_argument('--stage', action='store_true')
    parser.add_argument('--convert', action='store_true')
    parser.add_argument('--blender', action='store_true')
    parser.add_argument('--only')
    parser.add_argument('--calibration', action='store_true')
    parser.add_argument('--bind-source-materials', action='store_true')
    args = parser.parse_args(arguments)
    stage_path = ROOT/'Config'/('geometry_stage_'+args.only+'.json' if args.only else 'geometry_stage.json')
    if args.stage:
        stage_path = stage(args.only)
    if args.convert:
        convert(stage_path)
    if args.blender:
        assemble(stage_path)
    if args.calibration:
        import bpy
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.object.delete(use_global=False)
        bpy.context.scene.unit_settings.system = 'METRIC'
        bpy.context.scene.unit_settings.scale_length = 0.01
        bpy.ops.mesh.primitive_cube_add(size=1, location=(25, 50, 75))
        obj = bpy.context.object
        obj.name = 'SM_Dorado_CentimeterCalibration'
        obj.dimensions = (100, 200, 300)
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
        path = ROOT/'Exports/FBX/SM_Dorado_CentimeterCalibration.fbx'
        bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH'},
            global_scale=1.0, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
            axis_forward='-Y', axis_up='Z', use_space_transform=True, bake_space_transform=False,
            mesh_smooth_type='FACE', use_mesh_modifiers=True, add_leaf_bones=False, bake_anim=False)
        write_json(ROOT/'Config/calibration.json', {'fbx': str(path), 'source_unit': 'centimeters',
            'source_bounds_cm': {'min': [-25, -50, -75], 'max': [75, 150, 225]},
            'expected_ue_bounds_cm': {'min': [-25, -150, -75], 'max': [75, 50, 225]},
            'expected_dimensions_cm': [100, 200, 300], 'actor_scale': [1, 1, 1]})
    if args.bind_source_materials:
        bind_source_materials()


def bind_source_materials():
    """Bind recovered, adjacent PNGs into the editable source scene only."""
    import bpy
    textures = json.loads((ROOT/'Textures/texture_manifest.json').read_text())
    lookup = {row['id'].lower(): row for row in textures['materials']}
    counts = {'bound': 0, 'missing': [], 'external_files': []}
    for mat in list(bpy.data.materials):
        material_id = mat.get('source_material_id', '').lower()
        if not material_id:
            continue
        row = lookup.get(material_id)
        if not row:
            counts['missing'].append(material_id)
            continue
        mat.use_nodes = True
        tree = mat.node_tree
        tree.nodes.clear()
        output = tree.nodes.new('ShaderNodeOutputMaterial')
        shader = tree.nodes.new('ShaderNodeBsdfPrincipled')
        tree.links.new(shader.outputs['BSDF'], output.inputs['Surface'])
        shader.inputs['Base Color'].default_value = tuple(row.get('color_factor', [1, 1, 1, 1]))
        shader.inputs['Metallic'].default_value = float(row.get('metallic', 0))
        shader.inputs['Roughness'].default_value = float(row.get('roughness_default', .55))
        shader.inputs['Alpha'].default_value = float(row.get('opacity', 1))
        uv = tree.nodes.new('ShaderNodeTexCoord')
        for key, entry in row.get('maps', {}).items():
            if key not in ('BaseColor', 'Normal', 'Metallic', 'Roughness', 'Opacity', 'Emissive'):
                continue
            path = Path(entry['path'])
            if not path.is_file():
                counts['missing'].append(str(path))
                continue
            image = bpy.data.images.load(str(path), check_existing=True)
            image.colorspace_settings.name = 'sRGB' if entry.get('srgb') else 'Non-Color'
            image.filepath = bpy.path.relpath(str(path), start=str(ROOT))
            tex = tree.nodes.new('ShaderNodeTexImage')
            tex.image = image
            tex.label = key
            uv_scale = entry.get('uv_scale', [1, 1])
            scale = tree.nodes.new('ShaderNodeVectorMath')
            scale.operation = 'MULTIPLY'
            scale.inputs[1].default_value = (uv_scale[0], uv_scale[1], 1)
            tree.links.new(uv.outputs['UV'], scale.inputs[0])
            tree.links.new(scale.outputs['Vector'], tex.inputs['Vector'])
            if key == 'Normal':
                # Prepared maps retain DirectX Y orientation for Unreal; Blender expects +Y.
                flip = tree.nodes.new('ShaderNodeVectorMath')
                flip.operation = 'MULTIPLY_ADD'
                flip.inputs[1].default_value = (1, -1, 1)
                flip.inputs[2].default_value = (0, 1, 0)
                tree.links.new(tex.outputs['Color'], flip.inputs[0])
                normal = tree.nodes.new('ShaderNodeNormalMap')
                tree.links.new(flip.outputs['Vector'], normal.inputs['Color'])
                tree.links.new(normal.outputs['Normal'], shader.inputs['Normal'])
            elif key == 'BaseColor':
                tint = tree.nodes.new('ShaderNodeVectorMath')
                tint.operation = 'MULTIPLY'
                tint.inputs[1].default_value = tuple(row.get('color_factor', [1, 1, 1])[:3])
                tree.links.new(tex.outputs['Color'], tint.inputs[0])
                tree.links.new(tint.outputs['Vector'], shader.inputs['Base Color'])
            elif key == 'Emissive':
                tree.links.new(tex.outputs['Color'], shader.inputs['Emission Color'])
                shader.inputs['Emission Strength'].default_value = max(float(row.get('emission_strength_source', 0)), 1)
            elif key == 'Opacity':
                tree.links.new(tex.outputs['Color'], shader.inputs['Alpha'])
            else:
                tree.links.new(tex.outputs['Color'], shader.inputs[key])
        counts['bound'] += 1
    # Remove unused factory materials that point at Blender's own brush library.
    for material in list(bpy.data.materials):
        if material.users == 0:
            bpy.data.materials.remove(material)
    bpy.ops.wm.save_as_mainfile(filepath=str(ROOT/'Dorado_Recovered.blend'))
    write_json(PROJECT/'Saved/Logs/dorado-reconstruction-20261030/blender-material-bindings.json', counts)
    print('DORADO_SOURCE_MATERIALS='+json.dumps(counts))


if __name__ == '__main__':
    main()
