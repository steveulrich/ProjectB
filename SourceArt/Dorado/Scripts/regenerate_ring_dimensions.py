"""Optional Blender background bake of a changed radial ring thickness.

Reads the original recovered blend and writes separate owned variants. Defaults
are a no-op. Use dorado_dimensions.load_config() after a successful changed bake.
"""

import argparse
import hashlib
import json
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from dorado_dimensions import BASELINE, ROOT, deform_ring_point, validate

CENTERLINE_RADIUS_CM = 4537.19877
RING_ASSETS = ('SM_Dorado_rel_ed_ring_a_group', 'SM_Dorado_rel_ed_ring_b_group')


def transformed_normal(point, normal, ratio):
    """Inverse-transpose Jacobian keeps source hard-edge corner normals intact."""
    if ratio == 1:
        return tuple(normal)
    x, _, z = point
    radius = math.hypot(x, z)
    target = CENTERLINE_RADIUS_CM+(radius-CENTERLINE_RADIUS_CM)*ratio
    if radius <= 0 or target <= 0:
        raise ValueError('Invalid radial deformation.')
    ux, uz = x/radius, z/radius
    nx, ny, nz = normal
    radial = nx*ux+nz*uz
    tangent_scale = target/radius
    result = (radial*ux/ratio+(nx-radial*ux)/tangent_scale,
              ny, radial*uz/ratio+(nz-radial*uz)/tangent_scale)
    length = math.sqrt(sum(v*v for v in result))
    return tuple(v/length for v in result) if length else (0.0, 0.0, 1.0)


def main():
    argv = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else sys.argv[1:]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, default=ROOT/'dimensions.json')
    args = parser.parse_args(argv)
    config = json.loads(args.config.read_text(encoding='utf-8-sig'))
    p = validate(config, require_geometry_ready=False)
    desired = p['ring_radial_thickness_cm']
    ratio = desired/BASELINE['ring_radial_thickness_cm']
    if ratio == 1:
        print(json.dumps({'status': 'baseline_no_op', 'files_changed': 0}))
        return
    import bpy
    from mathutils import Vector
    original = ROOT/'Dorado_Recovered.blend'
    target = ROOT/'Exports'/'DimensionVariants'/('ring_thickness_'+format(desired, '.8g').replace('.', '_'))
    target.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.open_mainfile(filepath=str(original))
    # Original source always loaded: repeated runs never compound deformation.
    for collection in bpy.data.collections:
        collection.hide_viewport = False
    report = {'schema_version': 1, 'ring_radial_thickness_cm': desired,
              'baseline_radial_thickness_cm': BASELINE['ring_radial_thickness_cm'],
              'centerline_radius_source_cm': CENTERLINE_RADIUS_CM,
              'source_blend': str(original), 'source_blend_sha256': hashlib.sha256(original.read_bytes()).hexdigest(),
              'operation': 'Scale radial offsets about fixed source XZ centerline; preserve UVs and material slots.',
              'assets': []}
    for asset_id in RING_ASSETS:
        obj = bpy.data.objects.get(asset_id)
        if obj is None or obj.type != 'MESH':
            raise ValueError('Missing recovered kit object: '+asset_id)
        mesh = obj.data
        points = [tuple(v.co) for v in mesh.vertices]
        normals = [tuple(n.vector) for n in mesh.corner_normals]
        deformed = [deform_ring_point(point, ratio, CENTERLINE_RADIUS_CM) for point in points]
        remapped_normals = [transformed_normal(points[loop.vertex_index], normals[i], ratio)
                            for i, loop in enumerate(mesh.loops)]
        for vertex, point in zip(mesh.vertices, deformed):
            vertex.co = Vector(point)
        mesh.update()
        mesh.normals_split_custom_set(remapped_normals)
        bpy.ops.object.select_all(action='DESELECT')
        obj.hide_set(False)
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        fbx = target/(asset_id+'.fbx')
        bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={'MESH'},
            global_scale=1.0, apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE',
            axis_forward='-Y', axis_up='Z', use_space_transform=True, bake_space_transform=False,
            mesh_smooth_type='FACE', use_mesh_modifiers=True, add_leaf_bones=False, bake_anim=False, path_mode='AUTO')
        report['assets'].append({'id': asset_id, 'fbx': str(fbx),
            'fbx_sha256': hashlib.sha256(fbx.read_bytes()).hexdigest(),
            'local_bounds_cm': {'min': [min(v[i] for v in deformed) for i in range(3)],
                                'max': [max(v[i] for v in deformed) for i in range(3)]},
            'vertices': len(deformed), 'triangles': len(mesh.polygons),
            'materials': [material.name for material in mesh.materials]})
    variant_blend = target/'Dorado_RingThickness_Variant.blend'
    bpy.ops.wm.save_as_mainfile(filepath=str(variant_blend))
    report['blend'] = str(variant_blend)
    # Publish readiness metadata only after both exports and the blend succeed.
    (ROOT/'Config'/'ring_dimension_bake.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({'status': 'variant_ready', 'assets': len(report['assets']), 'directory': str(target)}))


if __name__ == '__main__':
    main()
