"""Read-only reproduction of the original floor collision exclusion defect.

The fixed exclusion list below is the pre-remedy policy. It deliberately keeps
the two TC floors disabled to reproduce the recorded diagnosis after the native
importer is corrected. This script is not a current native collision validator.
"""
import json
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[1]
NO_COLLISION_PARTS = ('relic_deco', 'relic_snake', 'primitive_plane', 'center_point',
                      'main_floor_tc', 'main_floor_center_tc', 'groud_design', 'banner01')


def build_tree(objects):
    vertices, faces, owners = [], [], []
    for obj in objects:
        offset = len(vertices)
        vertices.extend(tuple(obj.matrix_world @ v.co) for v in obj.data.vertices)
        for poly in obj.data.polygons:
            faces.append(tuple(offset+i for i in poly.vertices))
            owners.append({'object': obj.name, 'asset': obj['asset_id']})
    return BVHTree.FromPolygons(vertices, faces, all_triangles=True), owners


def sample(tree, owners, x, y):
    point, normal, index, _ = tree.ray_cast(Vector((x, -y, 20)), Vector((0, 0, -1)), 45)
    if point is None:
        return None
    return {'z': round(point.z, 6), 'normal_z': round(normal.z, 5), **owners[index]}


def main():
    manifest = json.loads((ROOT/'geometry_manifest.json').read_text())
    assets = {a['id']: a for a in manifest['assets']}
    objects = [o for o in bpy.data.objects if o.type == 'MESH' and o.get('source_guid')
               and assets[o['asset_id']]['collision_role'] == 'architecture']
    visual, visual_owners = build_tree(objects)
    collision_objects = [o for o in objects if not any(part in o['asset_id'].lower() for part in NO_COLLISION_PARTS)]
    collider, collision_owners = build_tree(collision_objects)
    grids = []
    for sign in (-1, 1):
        misses = []
        summaries = []
        for positive_y in range(600, 701):
            y = sign*positive_y
            missing_x = []
            visible_owners = set()
            for x in range(-150, 151, 2):
                hit = sample(collider, collision_owners, x, y)
                visible = sample(visual, visual_owners, x, y)
                if hit is None and visible is not None:
                    misses.append([x, y])
                    missing_x.append(x)
                    visible_owners.add(visible['asset'])
            if missing_x:
                summaries.append({'y': y, 'missing_x_min': min(missing_x), 'missing_x_max': max(missing_x),
                                  'missing_samples': len(missing_x), 'visible_floor_assets': sorted(visible_owners)})
        grids.append({'side': sign, 'x_step_cm': 2, 'y_step_cm': 1, 'rows': summaries,
                      'missing_with_visible_floor_bounds_xy': {'min': [min(p[i] for p in misses) for i in range(2)],
                                                             'max': [max(p[i] for p in misses) for i in range(2)]} if misses else None})
    probes = []
    for y in (-1300, -1200, -1100, -1000, -700, -660, -656, -655, -652, -650, -645, -642, -641, -640,
              -600, -500, 0, 500, 600, 640, 641, 642, 645, 650, 652, 655, 656, 660, 700, 1000, 1100, 1200, 1300):
        for x in (0, .01, -.01, .1, -.1, 1, -1, 25, 50, 75, 90, 100):
            probes.append({'x': x, 'y': y, 'collision': sample(collider, collision_owners, x, y),
                           'visual': sample(visual, visual_owners, x, y)})
    report = {'scope': 'Offline near-floor ray intersection; original visible geometry versus the pre-remedy importer collision exclusions. No current native physics or movement proof.',
              'source_blend': bpy.data.filepath, 'native_y_mirror': True,
              'collision_exclusion_substrings': NO_COLLISION_PARTS, 'grids': grids, 'probes': probes}
    output = ROOT.parents[1]/'Saved/Logs/dorado-reconstruction-20261030/source-floor-gap-measurement.json'
    output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'report': str(output), 'grid_missing_bounds': [g['missing_with_visible_floor_bounds_xy'] for g in grids]}))


if __name__ == '__main__':
    main()
