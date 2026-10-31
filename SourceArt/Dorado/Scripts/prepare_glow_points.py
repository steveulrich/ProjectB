"""Prepare small static surrogates for recovered Dorado violet particle cores.

Runs outside Unreal. Preserves exact source effect centers; only core size and
the small surface offset are reconstruction choices. Does not modify meshes.
"""
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET
import numpy as np
from analyze_ring_gaps import parse_mesh
from audit_source_scope import local_matrix

ROOT = Path(__file__).resolve().parents[1]
LOG = ROOT.parents[1]/'Saved/Logs/dorado-reconstruction-20261030'
CENTER = np.array([513.97998, 666.29779, 73.674896])
AXIS = np.array([50, -50, 50])


def main():
    scope = json.loads((LOG/'source-effects-scope.json').read_text())
    manifest = json.loads((ROOT/'geometry_manifest.json').read_text())
    library = ROOT/'Sources/Effects/lvl_eldorado_seattle_a.xml'
    effect_tree = ET.parse(library).getroot()
    ring_effect = next(e for e in scope['particle_effects'] if e['name']=='PE_MagnetRing_Lights')
    ring_matrix = np.array(ring_effect['source_world_matrix_m'])
    ring_source = next(e for e in effect_tree.findall('Particles') if e.get('Name')=='CenterRing.MagnetRing_Lights')
    positions = []
    for effect in scope['particle_effects']:
        if not effect['name'].startswith('PE_MagnetLight_'):
            continue
        world = (np.array(effect['source_world_matrix_m'])[:3, 3]-CENTER)*AXIS
        positions.append({
            'name': 'D_Glow_'+effect['name'], 'kind': 'pedestal_orb',
            'location_cm': world.tolist(), 'rotation_pitch_yaw_roll_deg': [0, 0, 0],
            'primitive': '/Engine/BasicShapes/Sphere.Sphere',
            'world_size_cm': [44, 44, 44], 'scale_xyz': [0.44, 0.44, 0.44],
            'color_linear_normalized': [0.1719, 0.0166, 1.0],
            'source_effect_id': effect['id'], 'source_effect_name': effect['name'],
            'source_layer': effect['layer'],
            'source_effect': effect['properties']['ParticleEffect'],
            'source_children': ['PinLight_Alpha', 'PinLight_Add'],
            'source_world_matrix_m': effect['source_world_matrix_m'],
            'placement_basis': 'Exact original particle origin. PinLight children have no local offset.',
            'size_basis': 'Reconstructed 44cm core; REF02 nearest orb is approximately 30px across. Original particle halo is larger.',
            'reference_support': 'REF01 small violet pedestal accents; REF02 near-left orb and partially obscured near-right counterpart; source retains both end assemblies.',
        })

    # Read the source ring's beauty triangles for its front/back surface planes.
    meshes = []
    for row in manifest['instances']:
        if 'rel_ed_ring_' not in row['asset_id']:
            continue
        vertices, faces = parse_mesh(row['asset_id'].removeprefix('SM_Dorado_'), exclude_proxy=True)
        matrix = np.array(local_matrix({'Pos': ','.join(map(str, row['source_position_m'])),
                                        'Rotate': ','.join(map(str, row['source_rotation_wxyz'])),
                                        'Scale': ','.join(map(str, row['source_scale']))}))
        verts = ((np.c_[vertices, np.ones(len(vertices))]@matrix.T)[:, :3]-CENTER)*AXIS
        meshes.append((row, verts[np.array(faces)]))

    def surfaces(point):
        origin = np.array([point[0], -1000, point[2]])
        direction = np.array([0, 1, 0])
        hits = []
        for row, tri in meshes:
            e1, e2 = tri[:, 1]-tri[:, 0], tri[:, 2]-tri[:, 0]
            cross = np.cross(direction, e2)
            det = np.einsum('ij,ij->i', e1, cross)
            valid = abs(det)>1e-8
            inv = np.divide(1, det, out=np.zeros_like(det), where=valid)
            offset = origin-tri[:, 0]
            u = np.einsum('ij,ij->i', offset, cross)*inv
            q = np.cross(offset, e1)
            v = q@direction*inv
            t = np.einsum('ij,ij->i', e2, q)*inv
            for index in np.flatnonzero(valid & (u>=0) & (v>=0) & (u+v<=1) & (t>0)):
                hits.append((float(origin[1]+t[index]), row))
        if not hits:
            raise ValueError('No source ring surface at particle center '+str(point))
        return min(hits, key=lambda x: x[0]), max(hits, key=lambda x: x[0])

    supported = {'01': 'REF02 upper-right inset', '02': 'REF02 lower-right hanging segment inset',
                 '06': 'REF02 upper-left inset and REF03 upper inset',
                 '07': 'REF01 upper-right edge, REF02 left-side inset and REF03 lower inset'}
    for emitter in ring_source.iter('Particles'):
        number = emitter.get('Name')
        if number not in supported:
            continue
        params = emitter.find('Params').attrib
        xyz = dict((k, float(v)) for k, v in re.findall(r'([xyz])=([\d.\-]+)', params['LocalSpawnPosOffset']))
        local = np.array([xyz.get(a, 0) for a in ('x', 'y', 'z')]+[1])
        world = ((ring_matrix@local)[:3]-CENTER)*AXIS
        negative, positive = surfaces(world)
        for suffix, (surface_y, owner), sign in [('NegativeY', negative, -1), ('PositiveY', positive, 1)]:
            location = world.copy()
            location[1] = surface_y+sign*3.5
            positions.append({
                'name': 'D_Glow_Ring_'+number+'_'+suffix, 'kind': 'ring_inset_core',
                'location_cm': location.tolist(), 'rotation_pitch_yaw_roll_deg': [0, 0, 0],
                'primitive': '/Engine/BasicShapes/Sphere.Sphere',
                'world_size_cm': [22, 6, 22], 'scale_xyz': [0.22, 0.06, 0.22],
                'color_linear_normalized': [0.17625, 0.04088, 1.0],
                'source_effect_id': ring_effect['id'], 'source_effect_name': ring_effect['name'],
                'source_effect': ring_effect['properties']['ParticleEffect'],
                'source_layer': ring_effect['layer'], 'source_child': number,
                'source_local_center_m': local[:3].tolist(), 'source_world_matrix_m': ring_effect['source_world_matrix_m'],
                'particle_center_target_cm': world.tolist(), 'surface_y_cm': surface_y,
                'source_geometry_id': owner['id'], 'source_geometry_name': owner['name'], 'source_asset_id': owner['asset_id'],
                'placement_basis': 'Exact source particle center X/Z; source LOD0 outer front/back surface Y plus 3.5cm outward offset.',
                'size_basis': 'Reconstructed 22cm core fits the visible small diamond center. Flattened sphere has 6cm depth; its inner side clears source surface by 0.5cm.',
                'reference_support': supported[number],
            })
    for point in positions:
        point.update({'collision_enabled': False, 'cast_shadow': False,
                      'affect_navigation': False, 'material_role': 'small_violet_emissive_core',
                      'status': 'Static native surrogate of original particle glow; visual confirmation required'})
    report = {
        'schema_version': 1, 'status': 'Source-backed placements with reconstructed small core geometry; not yet rendered',
        'source_library': str(library), 'source_library_sha256': hashlib.sha256(library.read_bytes()).hexdigest(),
        'coordinate_convention': 'world_cm=(source_m-center_m)*[50,-50,50]; integration scale already applied',
        'source_center_m': CENTER.tolist(), 'points': positions,
        'excluded_ring_children': {'03': 'Not clearly visible in supplied references', '04': 'Below arena and not clearly visible', '05': 'Below arena and not clearly visible'},
        'native_material_guidance': {
            'scope': 'Assign only to new small cores; never replace a pillar or ring material slot.',
            'shading': 'Unlit emissive. Source particle BlendType is Additive; an additive native core is a reconstruction, not the full original emitter.',
            'core_size': 'scale_xyz assumes the engine Sphere has100cm diameter. Prefer world_size_cm if primitive dimensions differ.',
            'emission': 'Use the existing owned violet emission MI. Tune brightness against fixed exposure, preserving a white-violet center and small purple bloom.',
            'point_lights': 'None required by this manifest.',
        },
    }
    target = ROOT/'Config/inferred_glow_points.json'
    target.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'output': str(target), 'pedestal_cores': 6, 'ring_cores': 8, 'total': len(positions)}))


if __name__ == '__main__':
    main()
