"""Read-only Blender source-scene rays above the six pad Power surfaces.

Run a separate background Blender with Dorado_Recovered.blend, never the live
user scene. The script writes JSON only and never saves the blend file.
"""
import argparse
import json
import pathlib
import sys
from collections import Counter
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--max-distance-cm', type=float, default=30.0)
    args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
    vertices, faces, owners = [], [], []
    pads = []
    for obj in bpy.context.scene.objects:
        if obj.type != 'MESH' or not obj.get('source_guid') or obj.hide_render:
            continue
        world_points = [obj.matrix_world @ v.co for v in obj.data.vertices]
        offset = len(vertices)
        vertices.extend(world_points)
        for poly in obj.data.polygons:
            faces.append([offset + i for i in poly.vertices])
            material = obj.data.materials[poly.material_index] if poly.material_index < len(obj.data.materials) else None
            name = material.name if material else ''
            owners.append({'actor': obj.name, 'asset_id': obj.get('asset_id'), 'material': name, 'polygon': poly.index})
            if 'relic_spawn_' in obj.name.lower() and (name == 'DOR_0d0cca8af39e0d0c' or name.lower().endswith('__2__power')):
                center = sum((world_points[i] for i in poly.vertices), Vector()) / len(poly.vertices)
                cross = (world_points[poly.vertices[1]] - world_points[poly.vertices[0]]).cross(world_points[poly.vertices[2]] - world_points[poly.vertices[0]])
                pads.append({'actor': obj.name, 'polygon': poly.index, 'point': center,
                             'normal_z': cross.normalized().z, 'area_cm2': cross.length * .5,
                             'projected_area_cm2': abs(cross.z) * .5})
    tree = BVHTree.FromPolygons(vertices, faces, all_triangles=False)
    summaries = {}
    for p in pads:
        origin = p['point'] + Vector((0, 0, .001))
        location, normal, index, distance = tree.ray_cast(origin, Vector((0, 0, 1)), args.max_distance_cm)
        row = summaries.setdefault(p['actor'], {'tested': 0, 'occluded': 0, 'hits': [], 'occluders': Counter(),
                                                'upward_polygons': 0, 'downward_polygons': 0, 'area_cm2': 0,
                                                'projected_area_cm2': 0})
        row['tested'] += 1
        row['upward_polygons'] += int(p['normal_z'] > .01)
        row['downward_polygons'] += int(p['normal_z'] < -.01)
        row['area_cm2'] += p['area_cm2']
        row['projected_area_cm2'] += p['projected_area_cm2']
        if location is not None:
            hit = owners[index]
            row['occluded'] += 1
            row['occluders'][hit['actor']] += 1
            row['hits'].append({'power_polygon': p['polygon'], 'power_point_blender_cm': list(p['point']),
                                'occluder': hit, 'distance_above_cm': distance})
    for row in summaries.values():
        row['occluders'] = dict(row['occluders'])
    result = {'scope': 'Recovered source blend visible scene instances, default configuration, upward rays from Power polygon centers. Blender Y has opposite sign to Unreal Y. Not native collision acceptance.',
              'blend': bpy.data.filepath, 'blender_version': bpy.app.version_string,
              'scene_vertices': len(vertices), 'scene_polygons': len(faces),
              'max_distance_cm': args.max_distance_cm, 'pads': summaries}
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'output': str(args.output), 'pads': {k: {'tested': v['tested'], 'occluded': v['occluded'], 'occluders': v['occluders']} for k, v in summaries.items()}}))


if __name__ == '__main__':
    main()
