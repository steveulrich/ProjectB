"""Read-only support audit of excluded arena decoration, with center-point remedy."""
import json
from pathlib import Path

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[1]
EXCLUDED = ('relic_deco', 'relic_snake', 'primitive_plane', 'center_point', 'groud_design', 'banner01')
CENTER = 'SM_Dorado_rel_el_dorado_center_point_a_group'


def main():
    manifest = json.loads((ROOT / 'geometry_manifest.json').read_text())
    records = {r['id'].strip('{}').lower(): r for r in manifest['instances']}
    prepared = []
    for obj in bpy.data.objects:
        if obj.type != 'MESH' or not obj.get('source_guid'):
            continue
        record = records[str(obj['source_guid']).strip('{}').lower()]
        points = [obj.matrix_world @ v.co for v in obj.data.vertices]
        excluded = any(part in record['asset_id'].lower() for part in EXCLUDED)
        collidable = record['collision_role'].lower() not in ('none', 'decorative', 'scenery', 'decoration', 'visual')
        prepared.append((obj, record, points, collidable, excluded))

    def build(restore_center=False, only_part=None):
        vertices, faces, owners = [], [], []
        for obj, record, points, collidable, excluded in prepared:
            if only_part and only_part not in record['asset_id']:
                continue
            if not only_part and (not collidable or (excluded and not (restore_center and record['asset_id'] == CENTER))):
                continue
            offset = len(vertices)
            vertices.extend(points)
            for poly in obj.data.polygons:
                faces.append(tuple(offset + v for v in poly.vertices))
                owners.append(record['asset_id'])
        return BVHTree.FromPolygons(vertices, faces, all_triangles=True), owners

    current, current_owners = build()
    proposed, proposed_owners = build(True)
    ground, ground_owners = build(only_part='groud_design')

    def ray(tree, owners, x, y, start_z=500, depth=1500):
        point, normal, index, _ = tree.ray_cast(Vector((x, -y, start_z)), Vector((0, 0, -1)), depth)
        return {'z': point.z, 'normal_z': normal.z, 'asset': owners[index]} if point is not None else None

    excluded_rows = []
    for obj, record, points, collidable, excluded in prepared:
        if not excluded:
            continue
        bounds = {'min': [min(v.x for v in points), -max(v.y for v in points), min(v.z for v in points)],
                  'max': [max(v.x for v in points), -min(v.y for v in points), max(v.z for v in points)]}
        row = {'asset': record['asset_id'], 'source_id': record['id'], 'name': record['name'], 'collision_role': record['collision_role'], 'triangles': len(obj.data.polygons), 'world_bounds_cm': bounds, 'world_z_span_cm': bounds['max'][2] - bounds['min'][2], 'upward_arena_polygon_samples': 0, 'unsupported_or_drop_over_10cm': [], 'supported_within_10cm': 0}
        for poly in obj.data.polygons:
            vertices = [points[v] for v in poly.vertices]
            normal = (vertices[1] - vertices[0]).cross(vertices[2] - vertices[0]).normalized()
            center = sum(vertices, Vector()) / len(vertices)
            if normal.z < .7 or not (-2000 <= center.x <= 2000 and -4500 <= center.y <= 4500 and -25 <= center.z <= 600):
                continue
            row['upward_arena_polygon_samples'] += 1
            support = ray(current, current_owners, center.x, -center.y, center.z + .05, 250)
            drop = center.z - support['z'] if support else None
            detail = {'poly': poly.index, 'center_target_cm': [center.x, -center.y, center.z], 'normal_z': normal.z, 'support': support, 'support_drop_cm': drop}
            if drop is None or drop > 10:
                row['unsupported_or_drop_over_10cm'].append(detail)
            else:
                row['supported_within_10cm'] += 1
        excluded_rows.append(row)

    lanes = []
    for x in (-1275, -1258, 1258, 1275):
        for side in (-1, 1):
            samples = [{'x': x, 'y': side*y, 'before': ray(current, current_owners, x, side*y), 'after': ray(proposed, proposed_owners, x, side*y)} for y in range(700, 1126)]
            changed = [p for p in samples if p['after'] and p['after']['asset'] == CENTER and (not p['before'] or p['before']['z'] < p['after']['z'] - .01)]
            lanes.append({'x': x, 'side': side, 'step_cm': 1, 'before_missing_count': sum(p['before'] is None for p in samples), 'after_missing_count': sum(p['after'] is None for p in samples), 'restored_y_range_cm': [min(p['y'] for p in changed), max(p['y'] for p in changed)] if changed else None, 'restored_z_range_cm': [min(p['after']['z'] for p in changed), max(p['after']['z'] for p in changed)] if changed else None, 'samples': samples})
    ground_bands = []
    for side in (-1, 1):
        for unsigned_y in (1325, 1484, 2351, 2379, 2441, 2476, 2598, 2686):
            y = side * unsigned_y
            missing_x = []
            for x in range(-760, 761):
                decoration = ray(ground, ground_owners, x, y, 30, 300)
                support = ray(current, current_owners, x, y, 30, 300)
                if decoration and (not support or decoration['z'] - support['z'] > 10):
                    missing_x.append(x)
            bands = []
            for x in missing_x:
                if not bands or x != bands[-1][-1] + 1:
                    bands.append([x, x])
                else:
                    bands[-1][-1] = x
            ground_bands.append({'y': y, 'x_step_cm': 1, 'visible_ground_without_support_within_10cm_bands': bands})
    logo_rows = []
    for row in [r for r in excluded_rows if 'primitive_plane' in r['asset']]:
        minimum, maximum = row['world_bounds_cm']['min'], row['world_bounds_cm']['max']
        samples = [ray(current, current_owners, minimum[0]+(maximum[0]-minimum[0])*ix/20, minimum[1]+(maximum[1]-minimum[1])*iy/20, 1, 200) for ix in range(1, 20) for iy in range(1, 20)]
        logo_rows.append({'id': row['source_id'], 'interior_grid_samples': len(samples), 'missing_support_count': sum(p is None for p in samples), 'support_z_range_cm': [min(p['z'] for p in samples if p), max(p['z'] for p in samples if p)]})
    report = {'scope': 'Offline exact source triangles. Proposed policy restores only center_point architecture, retaining all visible stair treads and open edges. This is not native runtime acceptance.', 'source_blend': bpy.data.filepath, 'excluded_substrings': EXCLUDED, 'excluded_instances': excluded_rows, 'lanes': lanes, 'ground_decoration_risk_cross_sections': ground_bands, 'logo_backing_grid': logo_rows}
    output = ROOT.parents[1] / 'Saved/Logs/dorado-reconstruction-20261030/excluded-floor-support-audit.json'
    output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'report': str(output), 'ground_bands': ground_bands, 'logo_backing': logo_rows}))


main()
