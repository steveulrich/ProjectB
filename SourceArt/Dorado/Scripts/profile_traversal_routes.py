"""Plan arena traversal routes from recovered geometry; this is not Unreal collision proof.

blender -b Dorado_Recovered.blend --python profile_traversal_routes.py
"""
import json
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree


def main():
    root = Path(__file__).resolve().parents[1]
    manifest = json.loads((root/'geometry_manifest.json').read_text())
    assets = {asset['id']: asset for asset in manifest['assets']}
    geometry = []
    vertices, faces, owners = [], [], []
    for obj in bpy.data.objects:
        if obj.type != 'MESH' or not obj.get('source_guid'):
            continue
        asset = assets.get(obj.get('asset_id'))
        if not asset or asset['collision_role'] not in ('architecture', 'source_collision'):
            continue
        offset = len(vertices)
        vertices.extend(tuple(obj.matrix_world @ vertex.co) for vertex in obj.data.vertices)
        for poly in obj.data.polygons:
            faces.append(tuple(offset+index for index in poly.vertices))
            owners.append(obj.name)
    tree = BVHTree.FromPolygons(vertices, faces, all_triangles=True)

    def sample(x, y):
        # The source .blend retains right-handed Y, whereas target Unreal mirrors it.
        point, normal, index, distance = tree.ray_cast(Vector((x, -y, 900)), Vector((0, 0, -1)), 2000)
        return {'x': x, 'y': y, 'z': round(point.z, 3) if point is not None else None,
                'normal_z': round(normal.z, 4) if normal is not None else None,
                'mesh': owners[index] if index is not None else None}

    plans = [
        {'name': 'CenterFloorSeams', 'start': [0, -3000], 'end': [0, 3000], 'interval': 50},
        {'name': 'CenterOffsetRight', 'start': [25, -650], 'end': [25, 650], 'interval': 25},
        {'name': 'CenterOffsetLeft', 'start': [-25, -650], 'end': [-25, 650], 'interval': 25},
        {'name': 'RightPositiveStairs', 'start': [1275, 1025], 'end': [1275, 0], 'interval': 25},
        {'name': 'LeftPositiveStairs', 'start': [-1275, 1025], 'end': [-1275, 0], 'interval': 25},
        {'name': 'RightNegativeStairs', 'start': [1275, -1025], 'end': [1275, 0], 'interval': 25},
        {'name': 'LeftNegativeStairs', 'start': [-1275, -1025], 'end': [-1275, 0], 'interval': 25},
        {'name': 'PositiveEndApproach', 'start': [0, 3000], 'end': [0, 3900], 'interval': 25},
        {'name': 'NegativeEndApproach', 'start': [0, -3000], 'end': [0, -3900], 'interval': 25},
        {'name': 'EastOpenEdge', 'start': [600, 1800], 'end': [2000, 1800], 'interval': 25},
        {'name': 'EastPlatformEdge', 'start': [1425, 0], 'end': [1900, 0], 'interval': 25},
        {'name': 'EastCourtEdge', 'start': [600, 1300], 'end': [1700, 1300], 'interval': 25},
    ]
    for plan in plans:
        a, b = Vector(plan['start']), Vector(plan['end'])
        count = max(1, round((b-a).length/plan['interval']))
        plan['samples'] = [sample(*(a+(b-a)*(i/count))) for i in range(count+1)]
        valid = [p for p in plan['samples'] if p['z'] is not None]
        differences = [abs(q['z']-p['z']) for p, q in zip(plan['samples'], plan['samples'][1:]) if p['z'] is not None and q['z'] is not None]
        plan['summary'] = {'samples': len(plan['samples']), 'misses': len(plan['samples'])-len(valid),
                           'min_z': min((p['z'] for p in valid), default=None),
                           'max_z': max((p['z'] for p in valid), default=None),
                           'max_height_delta_between_samples': max(differences, default=None)}
    report = {'scope': 'Offline source-mesh vertical-ray route planning; no Unreal collision, pawn, ball or navigation acceptance.',
              'units': 'target centimeters', 'source': bpy.data.filepath, 'routes': plans}
    output = root.parents[1]/'Saved/Logs/dorado-reconstruction-20261030/source-route-profiles.json'
    output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({plan['name']: plan['summary'] for plan in plans}))


if __name__ == '__main__':
    main()
