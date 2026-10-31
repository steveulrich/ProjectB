"""Measure ring angular gaps from recovered LOD0 Collada geometry, without a DCC.

Writes a diagnostic and proposed native ribbon placement configuration. Source
particle gap markers constrain correspondence; ribbons remain a reconstruction.
"""
import json
import math
import pathlib
import re
import xml.etree.ElementTree as ET
from audit_source_scope import local_matrix

ROOT = pathlib.Path(__file__).resolve().parents[1]
PROJECT = ROOT.parents[1]
LOG = PROJECT/'Saved/Logs/dorado-reconstruction-20261030'
NS = {'c': 'http://www.collada.org/2005/11/COLLADASchema'}


def transform(m, p):
    return [sum(m[i][j]*p[j] for j in range(3))+m[i][3] for i in range(3)]


def parse_mesh(stem, exclude_proxy=False):
    root = ET.parse(ROOT/'Exports/DAE'/(stem+'.dae')).getroot()
    node = root.find('.//c:visual_scene/c:node', NS)
    ref = node.find('c:instance_geometry', NS).get('url')[1:]
    geometry = next(g for g in root.findall('.//c:library_geometries/c:geometry', NS) if g.get('id') == ref)
    mesh = geometry.find('c:mesh', NS)
    sources = {}
    for source in mesh.findall('c:source', NS):
        array = source.find('c:float_array', NS)
        if array is None or not array.text:
            continue
        data = [float(v) for v in array.text.split()]
        stride = int(source.find('c:technique_common/c:accessor', NS).get('stride'))
        sources[source.get('id')] = [data[i:i+stride] for i in range(0, len(data), stride)]
    vertices = mesh.find('c:vertices', NS)
    source_id = vertices.find('c:input[@semantic="POSITION"]', NS).get('source')[1:]
    positions = sources[source_id]
    faces = []
    for triangles in mesh.findall('c:triangles', NS):
        material = triangles.get('material', '').lower()
        if 'shadowproxy' in material or (exclude_proxy and any(tag in material for tag in ('proxy', 'collision', 'nodraw'))):
            continue
        inputs = triangles.findall('c:input', NS)
        stride = max(int(i.get('offset')) for i in inputs)+1
        offset = int(next(i for i in inputs if i.get('semantic') == 'VERTEX').get('offset'))
        indices = [int(v) for v in triangles.find('c:p', NS).text.split()]
        position_indices = indices[offset::stride]
        faces.extend([position_indices[i:i+3] for i in range(0, len(position_indices), 3)])
    return positions, faces


def main():
    dimensions = json.loads((ROOT/'dimensions.json').read_text())
    scope = json.loads((ROOT/'Config/ring_source_transforms.json').read_text())
    effect = next(p for p in scope['effects'] if p['name'] == 'PE_MagnetRing_Energy')
    effect_world = effect['source_world_matrix_m']
    center = dimensions['coordinate_system']['source_center_m']
    # The inherited energy emitter's matrix is almost unit scale; divide each
    # transposed column by its squared length to remove the tiny scale epsilon.
    def energy_local(p):
        d = [p[i]-effect_world[i][3] for i in range(3)]
        return [sum(effect_world[k][i]*d[k] for k in range(3))/sum(effect_world[k][i]**2 for k in range(3)) for i in range(3)]
    occupied = [False]*720
    radii = []
    for anchor in dimensions['source_anchors']:
        if anchor['name'] not in ('rel_ed_ring_a_group2', 'rel_ed_ring_b_group2'):
            continue
        mat = local_matrix({'Pos': ','.join(map(str, anchor['source_position_m'])),
                            'Rotate': ','.join(map(str, anchor['source_rotation_wxyz'])),
                            'Scale': ','.join(map(str, anchor['source_scale_xyz']))})
        vertices, faces = parse_mesh(pathlib.Path(anchor['asset']).stem)
        local = [energy_local(transform(mat, p)) for p in vertices]
        for indices in faces:
            p = [[local[i][a] for a in range(3)] for i in indices]
            midpoint = [sum(v[a] for v in p)/3 for a in range(3)]
            radius = math.hypot(midpoint[0], midpoint[2])
            if not 38 < radius < 55:
                continue
            radii.append(radius)
            angles = [math.degrees(math.atan2(v[2], v[0])) % 360 for v in p]
            ref = angles[0]
            angles = [ref+(a-ref+180) % 360-180 for a in angles]
            for j in range(math.floor(min(angles)*2), math.ceil(max(angles)*2)+1):
                occupied[j % 720] = True
    gaps = []
    for i in range(720):
        if not occupied[i] and occupied[(i-1) % 720]:
            count = 0
            while not occupied[(i+count) % 720] and count < 720:
                count += 1
            if count >= 4:
                gaps.append({'start_angle_degrees': i/2, 'end_angle_degrees': (i+count)/2,
                             'angular_span_degrees': count/2})
    library = ROOT/'Sources/Effects/lvl_eldorado_seattle_a.xml'
    effects = []
    for top in ET.parse(library).getroot().findall('Particles'):
        if top.get('Name') not in ('CenterRing.MagnetRing_Lights', 'CenterRing.MagnetRing_Energy'):
            continue
        for emitter in top.iter('Particles'):
            params = emitter.find('Params')
            effects.append({'group': top.get('Name'), 'name': emitter.get('Name'),
                            'params': dict(params.attrib) if params is not None else {}})
    (LOG/'source-ring-effects.json').write_text(json.dumps({'source': str(library), 'effects': effects}, indent=2)+'\n')
    markers = []
    for e in effects:
        if not e['name'].startswith('Gap_'):
            continue
        raw = e['params'].get('LocalSpawnPosOffset', '')
        values = dict((k, float(v)) for k, v in re.findall(r'([xyz])=([\d.\-]+)', raw))
        p = [values.get(a, 0) for a in ('x', 'y', 'z')]
        world = transform(effect_world, p)
        target = [(world[a]-center[a])*[50, -50, 50][a] for a in range(3)]
        markers.append({'name': e['name'], 'energy_local_m': p, 'target_center_cm': target,
                        'angle_degrees': math.degrees(math.atan2(p[2], p[0])) % 360,
                        'source_marker_alpha': e['params'].get('Alpha')})
    output = {'status': 'Geometry-derived gap arcs and source particle marker correspondence; visual validation pending.',
              'effect_source_world_matrix_m': effect_world, 'radius_sample_median_m': sorted(radii)[len(radii)//2],
              'gaps': gaps, 'source_markers': markers,
              'notes': ['Source Gap_01..05 emitters use alpha0 and are invisible debris containers, not visible ribbon artwork.',
                        'Mag_Ring material exists in source resources, but no direct energy CGF placement was recovered.',
                        'Gap endpoints derive from occupied LOD0 triangles in 0.5-degree bins and radial band38..55m.',
                        'Native ribbon mesh/material reconstruction must preserve these gaps and remain non-colliding.']}
    (LOG/'ring-gap-analysis.json').write_text(json.dumps(output, indent=2)+'\n')
    (ROOT/'Config/ring_energy_geometry.json').write_text(json.dumps(output, indent=2)+'\n')
    print(json.dumps({'gaps': gaps, 'markers': [{'name': m['name'], 'angle': m['angle_degrees']} for m in markers],
                      'radius_median': output['radius_sample_median_m']}))


if __name__ == '__main__':
    main()
