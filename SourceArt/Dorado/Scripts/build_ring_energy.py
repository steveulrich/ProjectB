"""Export four reconstructed Dorado energy-gap ribbons using Blender 5.2.

Run: blender --background --factory-startup --python build_ring_energy.py
Only this script's new blend, FBX files, and energy manifest are written.
Ribbons are reconstructed from measured LOD0 angular gaps; they are not recovered
original energy geometry. Placement uses the recovered parent emitter matrix.
"""
import hashlib
import json
import math
import pathlib
import uuid
import bpy
from mathutils import Matrix

ROOT = pathlib.Path(__file__).resolve().parents[1]
PROJECT = ROOT.parents[1]
LOG = PROJECT/'Saved/Logs/dorado-reconstruction-20261030'
MATERIAL_ID = 'mag_ring_01_matgroup__1__mag_ring_02'


def main():
    analysis = json.loads((ROOT/'Config/ring_energy_geometry.json').read_text())
    dimensions = json.loads((ROOT/'dimensions.json').read_text())
    textures = json.loads((ROOT/'Textures/texture_manifest.json').read_text())
    material_record = next(m for m in textures['materials'] if m['id'].lower() == MATERIAL_ID)
    world = Matrix(analysis['effect_source_world_matrix_m'])
    source_location, source_rotation, source_scale = world.decompose()
    source_center = dimensions['coordinate_system']['source_center_m']
    factor = dimensions['coordinate_system']['centimeters_per_source_unit']
    integration_scale = dimensions['coordinate_system']['integration_uniform_scale']
    target_location = [(source_location[i]-source_center[i])*factor*integration_scale*[1, -1, 1][i] for i in range(3)]
    q = source_rotation
    target_rotation = [-q.x, q.y, -q.z, q.w]
    target_scale = [s*integration_scale for s in source_scale]

    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    bpy.context.scene.unit_settings.system = 'METRIC'
    bpy.context.scene.unit_settings.scale_length = 0.01
    mat_name = 'DOR_'+hashlib.sha1(MATERIAL_ID.encode()).hexdigest()[:16]
    material = bpy.data.materials.new(mat_name)
    material.use_nodes = True
    principled = material.node_tree.nodes.get('Principled BSDF')
    principled.inputs['Base Color'].default_value = (0.309469, 0.14996, 0.651406, 1)
    principled.inputs['Emission Color'].default_value = (0.417885, 0.274677, 0.701102, 1)
    principled.inputs['Emission Strength'].default_value = 1.5
    principled.inputs['Alpha'].default_value = 0.3
    material['source_material_id'] = MATERIAL_ID
    material['source_mtl'] = material_record['source_mtl']
    assets, instances = [], []
    radius = analysis['radius_sample_median_m']
    radial_width = 2.4
    axial_offsets = [-1.65, 0, 1.65]
    steps = 24
    root_exports = ROOT/'Exports/FBX'
    root_exports.mkdir(parents=True, exist_ok=True)
    for number, gap in enumerate(analysis['gaps'], 1):
        name = 'SM_Dorado_RingEnergy_Gap_%02d' % number
        # Half-degree overlap seats the transparent ends inside the stone cuts.
        begin = math.radians(gap['start_angle_degrees']-0.5)
        end = math.radians(gap['end_angle_degrees']+0.5)
        vertices, faces, vertex_uv = [], [], []
        for ribbon, axial in enumerate(axial_offsets):
            base = len(vertices)
            for segment in range(steps+1):
                t = segment/steps
                theta = begin+(end-begin)*t
                # Small deterministic variation produces separate strands while
                # retaining the measured arc and the same stone endpoints.
                wave = math.sin(t*math.pi)*math.sin(t*math.tau*2+ribbon*1.3)*0.12
                for edge in (0, 1):
                    r = radius+(edge-0.5)*radial_width+wave
                    vertices.append([factor*r*math.cos(theta), factor*axial,
                                     factor*r*math.sin(theta)])
                    vertex_uv.append((t+ribbon*0.23, edge))
            for segment in range(steps):
                a = base+segment*2
                faces.extend([(a, a+2, a+1), (a+1, a+2, a+3)])
        mesh = bpy.data.meshes.new(name+'_Mesh')
        mesh.from_pydata(vertices, [], faces)
        mesh.update()
        uv = mesh.uv_layers.new(name='UVMap')
        for poly in mesh.polygons:
            poly.use_smooth = True
            for loop in poly.loop_indices:
                uv.data[loop].uv = vertex_uv[mesh.loops[loop].vertex_index]
        mesh.materials.append(material)
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.collection.objects.link(obj)
        obj['reconstruction_status'] = 'Derived from recovered LOD0 gap bounds and emitter transform; original ribbon geometry unavailable.'
        obj['source_gap_start_degrees'] = gap['start_angle_degrees']
        obj['source_gap_end_degrees'] = gap['end_angle_degrees']
        obj['collision_role'] = 'decorative'
        bpy.ops.object.select_all(action='DESELECT')
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        path = root_exports/(name+'.fbx')
        bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH'},
            global_scale=1.0, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
            axis_forward='-Y', axis_up='Z', use_space_transform=True, bake_space_transform=False,
            mesh_smooth_type='FACE', use_mesh_modifiers=True, add_leaf_bones=False, bake_anim=False)
        assets.append({'id': name, 'fbx': str(path), 'exported': True, 'reconstructed': True,
            'collision_role': 'decorative', 'triangles': len(faces), 'vertices': len(vertices),
            'local_bounds_cm': {'min': [min(v[a] for v in vertices) for a in range(3)],
                                'max': [max(v[a] for v in vertices) for a in range(3)]},
            'materials': [{'slot': 0, 'name': mat_name, 'material_id': MATERIAL_ID,
                           'source_mtl': material_record['source_mtl'], 'source_index': 1}],
            'source_gap_degrees': gap, 'source_hash_sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
        instance_id = '{'+str(uuid.uuid5(uuid.NAMESPACE_URL, 'Dorado/Reconstruction/Energy/'+name)).upper()+'}'
        instances.append({'id': instance_id, 'asset_id': name, 'name': name.replace('SM_', 'D_'),
            'layer': 'Presentation/Energy', 'location_cm': target_location,
            'rotation_quat_xyzw': target_rotation, 'scale': target_scale,
            'source_position_m': list(source_location), 'source_rotation_wxyz': list(source_rotation),
            'source_scale': list(source_scale), 'collision_role': 'decorative', 'reconstructed': True})
        # The editable blend previews the same source-coordinate scene convention
        # as the main recovered blend; Unreal import applies the calibrated Y flip.
        obj.location = [(source_location[i]-source_center[i])*factor*integration_scale for i in range(3)]
        obj.rotation_mode = 'QUATERNION'
        obj.rotation_quaternion = source_rotation
        obj.scale = target_scale
    blend = ROOT/'Dorado_RingEnergy_Reconstructed.blend'
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    report = {'schema_version': 1, 'status': 'source-ready; Unreal import and visual validation pending',
              'blender_version': bpy.app.version_string, 'blend': str(blend), 'assets': assets,
              'instances': instances, 'failures': [], 'unresolved': [],
              'source_center_m': source_center, 'source_units_to_cm': factor,
              'target_scale': integration_scale, 'axis': 'source X,Y,Z -> UE X,-Y,Z',
              'construction': {'radial_width_source_m': radial_width, 'radius_source_m': radius,
                               'axial_offsets_source_m': axial_offsets, 'segments_per_ribbon': steps,
                               'stone_overlap_degrees': 0.5},
              'source_analysis': str(LOG/'ring-gap-analysis.json'),
              'material_notes': ['Use native translucent unlit Energy parent with Mag_Ring_02 source diffuse/alpha and UV tiling.',
                                 'BaseColor tint must feed Emissive for unlit energy. Black default EmissiveMap makes it invisible.',
                                 'Multiply opacity by transverse UV edge fade; source flow alpha supplies streaks.',
                                 'Disable collision and shadow casting. These are reconstructed effects, not recovered original meshes.']}
    (ROOT/'energy_manifest.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('DORADO_ENERGY_SUMMARY='+json.dumps({'assets': len(assets), 'triangles': sum(a['triangles'] for a in assets),
                                             'manifest': str(ROOT/'energy_manifest.json')}), flush=True)


if __name__ == '__main__':
    main()
