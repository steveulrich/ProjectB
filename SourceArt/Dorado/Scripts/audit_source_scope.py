"""Inventory environmental geometry and effects omitted by a CGF-only layer scan.

Run with system Python. Reads the external source archive and writes only owned
JSON reports. Parent transforms are composed before producing additional rows.
"""
from __future__ import annotations
import argparse
import collections
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET


def vector(text, default):
    return [float(x) for x in text.split(',')] if text else default[:]


def quat_matrix(q):
    w, x, y, z = q
    return [[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
            [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
            [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]]


def matrix_quat(m):
    trace = m[0][0] + m[1][1] + m[2][2]
    if trace > 0:
        s = math.sqrt(trace+1)*2
        return [s/4, (m[2][1]-m[1][2])/s, (m[0][2]-m[2][0])/s, (m[1][0]-m[0][1])/s]
    i = max(range(3), key=lambda j: m[j][j])
    j, k = (i+1) % 3, (i+2) % 3
    s = math.sqrt(1+m[i][i]-m[j][j]-m[k][k])*2
    v = [0., 0., 0.]
    v[i], v[j], v[k] = s/4, (m[i][j]+m[j][i])/s, (m[i][k]+m[k][i])/s
    return [(m[k][j]-m[j][k])/s, *v]


def local_matrix(row):
    p = vector(row.get('Pos'), [0, 0, 0])
    s = vector(row.get('Scale'), [1, 1, 1])
    r = quat_matrix(vector(row.get('Rotate'), [1, 0, 0, 0]))
    return [[r[i][j]*s[j] for j in range(3)]+[p[i]] for i in range(3)]+[[0, 0, 0, 1]]


def mul(a, b):
    return [[sum(a[i][k]*b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def main(source, project):
    layer_root = source/'levels/levels/01_eldorado_a_temple/layers'
    source_report = json.loads((project/'Saved/Logs/dorado-reconstruction-20261030/source-material-mapping.json').read_text())
    known = {x.get('Id') for x in source_report['placements']}
    nodes, layer_counts = {}, {}
    for path in sorted(layer_root.glob('*.lyr')):
        objects = list(ET.parse(path).iter('Object'))
        layer_counts[path.name] = dict(collections.Counter(o.get('Type') for o in objects))
        for o in objects:
            nodes[o.get('Id')] = (o, path)
    cache = {}

    def world_matrix(object_id, chain=()):
        if object_id in cache:
            return cache[object_id]
        if object_id in chain:
            raise RuntimeError('Source hierarchy cycle: '+object_id)
        o, _ = nodes[object_id]
        local = local_matrix(o.attrib)
        parent = o.get('Parent')
        cache[object_id] = mul(world_matrix(parent, chain+(object_id,)), local) if parent in nodes else local
        return cache[object_id]

    additional, corrections, particles, errors = [], [], [], []
    included_layers = ('geo_', 'sys_spec_min_off', 'vfx_', 'environmental_effects')
    for object_id, (o, path) in nodes.items():
        if not path.stem.startswith(included_layers):
            continue
        props = o.find('Properties')
        props = dict(props.attrib) if props is not None else {}
        if o.get('Type') == 'ParticleEntity':
            particles.append({'name': o.get('Name'), 'id': object_id, 'layer': path.name,
                              'source_world_matrix_m': world_matrix(object_id), 'properties': props,
                              'parent': o.get('Parent'), 'status': 'Native effect reconstruction required'})
        geometry = o.get('Prefab') or o.get('Geometry') or props.get('object_Model')
        if not geometry or not geometry.lower().endswith('.cgf'):
            continue
        if object_id in known and not o.get('Parent'):
            continue
        if not geometry.lower().startswith('objects/01_eldorado_a_temple/') or o.get('HiddenInGame') == '1':
            continue
        row = dict(o.attrib)
        m = world_matrix(object_id)
        scale = [math.sqrt(sum(m[i][j]**2 for i in range(3))) for j in range(3)]
        if min(scale) < 1e-8:
            errors.append({'name': o.get('Name'), 'reason': 'Degenerate scale'})
            continue
        r = [[m[i][j]/scale[j] for j in range(3)] for i in range(3)]
        shear = max(abs(sum(r[k][i]*r[k][j] for k in range(3))) for i, j in [(0, 1), (0, 2), (1, 2)])
        if shear > 0.001:
            errors.append({'name': o.get('Name'), 'reason': 'Parent composition introduces shear', 'max_dot': shear})
            continue
        row.update({'Geometry': geometry, 'Pos': ','.join(str(m[i][3]) for i in range(3)),
                    'Scale': ','.join(map(str, scale)), 'Rotate': ','.join(map(str, matrix_quat(r))),
                    'sourceLayerFile': str(path), 'sourceOriginalTransform': dict(o.attrib),
                    'sourceWorldMatrix': m, 'sourceTransformBakedFromParent': bool(o.get('Parent'))})
        row.pop('Parent', None)
        (corrections if object_id in known else additional).append(row)
    effects = [m for m in source_report['materials'] if any(k in (m['name']+' '+m['file']).lower()
               for k in ('waterfall', 'mag_ring', 'river', 'ringlight', 'energy'))]
    report = {'layers': layer_counts, 'additional_geometry_count': len(additional),
              'additional_by_layer': dict(collections.Counter(Path(p['sourceLayerFile']).name for p in additional)),
              'parent_transform_correction_count': len(corrections),
              'particle_effect_count': len(particles), 'particle_effects': particles,
              'effect_materials': effects, 'errors': errors,
              'notes': ['Source particles require native effects; source material names do not execute CryEngine shaders in Unreal.',
                        'Additional rows have resolved world transforms; do not apply original Parent transforms again.',
                        'Far treecards remain cards. Inspect player-visible foliage separately.',
                        'Source particle transforms do not reproduce their emitter geometry or animation.']}
    config = project/'SourceArt/Dorado/Config'
    log = project/'Saved/Logs/dorado-reconstruction-20261030'
    config.mkdir(parents=True, exist_ok=True)
    log.mkdir(parents=True, exist_ok=True)
    (config/'additional_placements.json').write_text(json.dumps(additional, indent=2)+'\n', encoding='utf-8')
    (config/'parent_transform_corrections.json').write_text(json.dumps(corrections, indent=2)+'\n', encoding='utf-8')
    (config/'ring_source_transforms.json').write_text(json.dumps(
        {'source_level': str(layer_root), 'effects': [p for p in particles if 'MagnetRing' in p['name']]}, indent=2)+'\n', encoding='utf-8')
    (log/'source-effects-scope.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('additional_geometry_count', 'additional_by_layer', 'parent_transform_correction_count', 'particle_effect_count', 'errors')}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, default=Path('F:/BreakawayConverted/Unpacked'))
    parser.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[3])
    args = parser.parse_args()
    main(args.source, args.project)
