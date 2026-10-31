"""Read-only source geometry diagnosis of the east platform stair foot."""
import json
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[1]
EXCLUDED = ('relic_deco', 'relic_snake', 'primitive_plane', 'center_point', 'groud_design', 'banner01')


def main():
    manifest = json.loads((ROOT / 'geometry_manifest.json').read_text())
    records = {r['id'].strip('{}').lower(): r for r in manifest['instances']}
    assets = {a['id']: a for a in manifest['assets']}
    candidates, info = [], []
    for obj in bpy.data.objects:
        if obj.type != 'MESH' or not obj.get('source_guid'):
            continue
        vertices = [obj.matrix_world @ v.co for v in obj.data.vertices]
        bounds = {'min': [min(v[i] for v in vertices) for i in range(3)], 'max': [max(v[i] for v in vertices) for i in range(3)]}
        # Blender world Y is reflected by the native FBX conversion.
        target_min = [bounds['min'][0], -bounds['max'][1], bounds['min'][2]]
        target_max = [bounds['max'][0], -bounds['min'][1], bounds['max'][2]]
        if not (target_min[0] < 1450 and target_max[0] > 1050 and target_min[1] < 1200 and target_max[1] > 700 and target_min[2] < 600 and target_max[2] > -150):
            continue
        aid = obj['asset_id']
        record = records.get(str(obj['source_guid']).strip('{}').lower(), {})
        role = record.get('collision_role', assets[aid]['collision_role'])
        collision = role.lower() not in ('none', 'decorative', 'scenery', 'decoration', 'visual') and not any(s in aid.lower() for s in EXCLUDED)
        row = {'object': obj.name, 'asset': aid, 'source_id': obj['source_guid'], 'role': role, 'collision_enabled_by_policy': collision, 'bounds_target_cm': {'min': target_min, 'max': target_max}, 'triangles': len(obj.data.polygons)}
        info.append(row)
        candidates.append((obj, vertices, row))

    def tree_for(collision_only):
        points, faces, owners = [], [], []
        for obj, vertices, row in candidates:
            if collision_only and not row['collision_enabled_by_policy']:
                continue
            offset = len(points)
            points.extend(vertices)
            for poly in obj.data.polygons:
                faces.append(tuple(offset + v for v in poly.vertices))
                owners.append({'asset': row['asset'], 'object': row['object'], 'poly': poly.index, 'material': obj.data.materials[poly.material_index].name})
        return BVHTree.FromPolygons(points, faces, all_triangles=True), owners

    visual, visual_owners = tree_for(False)
    collision, collision_owners = tree_for(True)

    def probe(tree, owners, x, y):
        point, normal, index, distance = tree.ray_cast(Vector((x, -y, 500)), Vector((0, 0, -1)), 1600)
        if point is None:
            return None
        return {'z': round(point.z, 5), 'normal_z': round(normal.z, 5), **owners[index]}

    probes = [{'x': x, 'y': y, 'visual': probe(visual, visual_owners, x, y), 'collision': probe(collision, collision_owners, x, y)}
              for x in (1100, 1150, 1200, 1240, 1258, 1275, 1300, 1350, 1400) for y in range(700, 1201, 10)]
    details = []
    for obj, vertices, row in candidates:
        if row['role'] != 'architecture':
            continue
        polys = []
        for poly in obj.data.polygons:
            world = [[vertices[v].x, -vertices[v].y, vertices[v].z] for v in poly.vertices]
            if min(v[0] for v in world) <= 1325 and max(v[0] for v in world) >= 1200 and min(v[1] for v in world) <= 1130 and max(v[1] for v in world) >= 800 and min(v[2] for v in world) <= 20 and max(v[2] for v in world) >= -120:
                polys.append({'poly': poly.index, 'material': obj.data.materials[poly.material_index].name, 'vertices_target_cm': world})
        if polys:
            details.append({'object': obj.name, 'asset': row['asset'], 'polygons': polys})
    report = {'scope': 'Read-only Blender source mesh analysis; current default transforms. No native physics/editor calls.', 'blend': bpy.data.filepath, 'candidates': info, 'probes': probes, 'near_foot_polygons': details}
    output = ROOT.parents[1] / 'Saved/Logs/dorado-reconstruction-20261030/stair-foot-source-inspection.json'
    output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'report': str(output), 'candidates': len(candidates), 'x1258': [p for p in probes if p['x'] == 1258 and p['y'] % 50 == 0]}))


main()
