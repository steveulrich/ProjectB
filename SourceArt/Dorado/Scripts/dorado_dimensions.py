"""Pure-Python dimensional adapter for recovered Dorado instance records.

Call apply(original_record, load_config()) once per instance. Never feed an
already-adjusted transform back into this adapter. No Unreal or Blender imports.
"""

import copy
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASELINE = {
    'arena_length_cm': 8000.0, 'arena_width_cm': 2950.0,
    'main_floor_core_width_cm': 1800.0, 'main_floor_elevation_cm': 0.0,
    'end_approach_rise_cm': 239.6469, 'side_platform_height_cm': 208.33435,
    'side_platform_radius_cm': 300.0, 'pylon_spacing_cm': 1900.0,
    'ring_outer_diameter_cm': 5167.25, 'ring_axial_depth_cm': 370.5625,
    'ring_radial_thickness_cm': 370.0,
    'sun_temple_dimensions_cm': [5000.0, 3000.0, 8000.0],
    'narrow_shrine_dimensions_cm': [1600.0, 1200.0, 4000.0],
}
COURT_LAYERS = {'geo_blue_start', 'geo_red_start', 'geo_middle'}
RING_IDS = {'rel_ed_ring_a_group', 'rel_ed_ring_b_group', 'magnet_force_01_group'}
GIANT_PYLON_IDS = {
    'rel_ed_serpent_wall_a_group', 'rel_ed_serpent_wall_b_group',
    'rel_ed_serpent_wall_a_clump_group', 'rel_ed_serpent_wall_b_clump_group',
}
SIDE_PREFIXES = (
    'rel_eldorado_mid_circle_top_', 'rel_eldorado_mid_stair_',
    'rel_eldorado_mid_staris_', 'rel_eldorado_mid_buff_',
    'rel_eldorado_mid_sidewall_', 'rel_eldorado_sidewall_gold_',
    'rel_prxy_eldorado_mid_steps_', 'rel_el_dorado_center_point_',
)
END_IDS = {
    'rel_eldorado_startramp_clump_group', 'rel_eldorado_floor_v2_group',
    'rel_eldorado_floor_v2_gapfill_group',
}
RING_ANCHOR = [1.001, -0.6105, -58.581525]
RING_QUATERNION = [-0.13917325, 5.3270863e-9, -0.99026805, -4.338559e-8]
TEMPLE_ANCHORS = {
    'sun': [0.0, 5379.5, 0.0],
    'shrine': [0.0, -4505.5, 239.6469],
}


class DimensionError(ValueError):
    """A configuration cannot be represented safely by the source adapter."""


def load_config(path=None):
    """Read central dimensions and optional generated ring variant metadata."""
    path = Path(path) if path else ROOT / 'dimensions.json'
    config = json.loads(path.read_text(encoding='utf-8-sig'))
    bake_path = path.parent / 'Config' / 'ring_dimension_bake.json'
    if bake_path.exists():
        config['ring_geometry_bake'] = json.loads(bake_path.read_text(encoding='utf-8'))
    if parameters(config)['ring_radial_thickness_cm'] != BASELINE['ring_radial_thickness_cm']:
        validate(config)
        bake = config['ring_geometry_bake']
        expected = [(ROOT/'Dorado_Recovered.blend', bake.get('source_blend_sha256'))]
        expected += [(Path(asset['fbx']), asset.get('fbx_sha256')) for asset in bake['assets']]
        for asset_path, recorded_hash in expected:
            if not asset_path.is_file() or not recorded_hash:
                raise DimensionError('Ring variant source/export is missing; rerun regenerate_ring_dimensions.py.')
            with asset_path.open('rb') as stream:
                actual_hash = hashlib.file_digest(stream, 'sha256').hexdigest()
            if actual_hash != recorded_hash:
                raise DimensionError('Ring variant source/export changed; rerun regenerate_ring_dimensions.py.')
    return config


def parameters(config):
    result = copy.deepcopy(BASELINE)
    result.update(config.get('parameters', config))
    return result


def validate(config, require_geometry_ready=True):
    """Validate inputs; changed ring thickness requires its generated variant."""
    p = parameters(config)
    for name, baseline in BASELINE.items():
        values = p[name] if isinstance(baseline, list) else [p[name]]
        if isinstance(baseline, list) and (not isinstance(values, (list, tuple)) or len(values) != 3):
            raise DimensionError(name + ' must contain width, depth, height.')
        for value in values:
            if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
                raise DimensionError(name + ' must be finite numeric values.')
            if name != 'main_floor_elevation_cm' and value <= 0:
                raise DimensionError(name + ' must be positive.')
    # The center floor width is an informational source envelope. Independent
    # changes would need a split/deformed center floor, not instance transforms.
    if p['main_floor_core_width_cm'] != BASELINE['main_floor_core_width_cm']:
        raise DimensionError('main_floor_core_width_cm is a measured envelope; use arena_width_cm.')
    if require_geometry_ready and p['ring_radial_thickness_cm'] != BASELINE['ring_radial_thickness_cm']:
        bake = config.get('ring_geometry_bake', {})
        if bake.get('ring_radial_thickness_cm') != p['ring_radial_thickness_cm'] or not bake.get('assets'):
            raise DimensionError('Run regenerate_ring_dimensions.py with this dimensions.json, then load_config().')
    return p


def _asset(record):
    return record.get('asset_id', '').lower().removeprefix('sm_dorado_')


def classify(record):
    """Return the explicit architectural group; unknown scenery is untouched."""
    aid = _asset(record)
    if aid in RING_IDS or aid.startswith('ringenergy_gap_'):
        return 'ring'
    if aid == 'rel_el_arenatemple_group':
        return 'sun'
    if aid in ('rel_el_arenastatue_a_group', 'rel_el_arenastatue_b_group'):
        return 'shrine'
    if record.get('layer', '').lower() in COURT_LAYERS or aid == 'rel_el_arenastartareawall_b_group':
        return 'court'
    return 'scenery'


def _matrix(q):
    norm = math.sqrt(sum(v * v for v in q))
    if norm < 1e-12:
        raise DimensionError('Instance has a zero quaternion.')
    x, y, z, w = [v / norm for v in q]
    return [[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)],
            [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)],
            [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]]


def _mul(matrix, vector):
    return [sum(a*b for a, b in zip(row, vector)) for row in matrix]


def _affine(location, scale, q, factors, anchor, rigid=False):
    """A world-axis affine is exact for the source's cardinal structural kit."""
    if factors == [1.0, 1.0, 1.0]:
        return location, scale
    location = [anchor[i] + (location[i]-anchor[i])*factors[i] for i in range(3)]
    if rigid:
        return location, scale
    rotation = _matrix(q)
    local_factors = []
    for axis in range(3):
        direction = [rotation[i][axis] for i in range(3)]
        dominant = max(range(3), key=lambda i: abs(direction[i]))
        if 1-abs(direction[dominant]) > 1e-8 and max(factors)-min(factors) > 1e-10:
            raise DimensionError('Noncardinal structural transform would require shear; regenerate this mesh.')
        local_factors.append(factors[dominant])
    return location, [scale[i]*local_factors[i] for i in range(3)]


def _side(record):
    aid, loc = _asset(record), record['location_cm']
    return (aid.startswith(SIDE_PREFIXES) or aid == 'rel_eldorado_centerline_group'
            or (aid.startswith('rel_eldorado_stairs_sides_') and abs(loc[1]) < 1500)
            or (aid.startswith('rel_eldorado_relic_spawn_') and abs(loc[0]) > 1000))


def _end(record):
    aid = _asset(record)
    return (aid in END_IDS or aid.startswith('rel_ed_serpent_')
            or (aid.startswith('rel_eldorado_stairs_sides_') and abs(record['location_cm'][1]) > 2000))


def apply(record, config):
    """Return (location_cm, quaternion_xyzw, scale); inputs are never mutated.

    The baseline result is exactly equal to the original stored arrays. Side
    radius is deliberately coupled to the whole connected court's XY footprint.
    Camera, lighting, terrain and gameplay actor records are outside this API.
    """
    p = validate(config)
    location, q, scale = [list(record[k]) for k in ('location_cm', 'rotation_quat_xyzw', 'scale')]
    if any(v <= 0 or not math.isfinite(v) for v in scale):
        raise DimensionError('The adapter requires positive source actor scales.')
    group = classify(record)
    if p == BASELINE or group == 'scenery':
        return location, q, scale
    aid = _asset(record)
    radius = p['side_platform_radius_cm']/BASELINE['side_platform_radius_cm']
    width = p['arena_width_cm']/BASELINE['arena_width_cm']*radius
    length = p['arena_length_cm']/BASELINE['arena_length_cm']*radius
    elevation = p['main_floor_elevation_cm']-BASELINE['main_floor_elevation_cm']
    if group == 'court':
        # Loose covers and banners retain their shapes. Their original three
        # noncardinal rotations otherwise require a shear Unreal cannot store.
        rigid = aid.startswith(('rel_ed_block_clump_', 'rel_ed_banner'))
        location, scale = _affine(location, scale, q, [width, length, 1.0], [0, 0, 0], rigid)
        if _side(record):
            height = p['side_platform_height_cm']/BASELINE['side_platform_height_cm']
            location, scale = _affine(location, scale, q, [1.0, 1.0, height], [0, 0, 0])
        if _end(record):
            rise = p['end_approach_rise_cm']/BASELINE['end_approach_rise_cm']
            location, scale = _affine(location, scale, q, [1.0, 1.0, rise], [0, 0, 0])
        if aid in GIANT_PYLON_IDS:
            # These meshes have centerline pivots and baked left/right offsets.
            # Source A is left and B right; the opposite end rotates them 180deg.
            # Move each body and its matching clump together without resizing.
            # 1900 is a retained nominal source setting, not measured head span.
            local_side = -1 if '_wall_a_' in aid else 1
            world_side = _mul(_matrix(q), [local_side, 0, 0])[0]
            side = 1 if world_side > 0 else -1
            location[0] += side*(p['pylon_spacing_cm']-BASELINE['pylon_spacing_cm'])/2
    elif group in TEMPLE_ANCHORS:
        key = 'sun_temple_dimensions_cm' if group == 'sun' else 'narrow_shrine_dimensions_cm'
        ratios = [p[key][i]/BASELINE[key][i] for i in range(3)]
        anchor = TEMPLE_ANCHORS[group]
        location, scale = _affine(location, scale, q, ratios, anchor)
        location[0] += anchor[0]*(width-1)
        location[1] += anchor[1]*(length-1)
        if group == 'shrine':
            # Its front rests on the raised end approach. Keep that attachment
            # height when the approach rises, without stretching shrine details.
            location[2] += anchor[2]*(p['end_approach_rise_cm']/BASELINE['end_approach_rise_cm']-1)
    elif group == 'ring':
        diameter = p['ring_outer_diameter_cm']/BASELINE['ring_outer_diameter_cm']
        depth = p['ring_axial_depth_cm']/BASELINE['ring_axial_depth_cm']
        factors = [diameter, depth, diameter]
        if factors != [1.0, 1.0, 1.0]:
            matrix = _matrix(RING_QUATERNION)
            inverse = [list(row) for row in zip(*matrix)]
            relative = _mul(inverse, [location[i]-RING_ANCHOR[i] for i in range(3)])
            relative = _mul(matrix, [relative[i]*factors[i] for i in range(3)])
            location = [RING_ANCHOR[i]+relative[i] for i in range(3)]
            scale = [scale[i]*factors[i] for i in range(3)]
    location[2] += elevation
    return location, q, scale


def asset_overrides(config):
    """Return generated ring variants to import before applying their instances."""
    p = validate(config)
    if p['ring_radial_thickness_cm'] == BASELINE['ring_radial_thickness_cm']:
        return {}
    return {a['id']: a for a in config['ring_geometry_bake']['assets']}


def deform_ring_point(point, ratio, centerline_radius_cm=4537.19877):
    """Rescale radial distance from the source XZ centerline, preserving Y/angle."""
    if not math.isfinite(ratio) or ratio <= 0:
        raise DimensionError('Ring thickness ratio must be positive and finite.')
    if ratio == 1:
        return tuple(point)
    x, y, z = point
    radius = math.hypot(x, z)
    if radius < 1e-8:
        raise DimensionError('Ring mesh has a vertex on its radial axis.')
    target = centerline_radius_cm + (radius-centerline_radius_cm)*ratio
    if target <= 0:
        raise DimensionError('Ring thickness collapses an inner vertex through its axis.')
    return (x*target/radius, y, z*target/radius)


if __name__ == '__main__':
    cfg = load_config()
    validate(cfg)
    manifest = json.loads((ROOT/'geometry_manifest.json').read_text(encoding='utf-8'))
    groups = {}
    changed = []
    for row in manifest['instances']:
        group = classify(row)
        groups[group] = groups.get(group, 0)+1
        actual = apply(row, cfg)
        original = tuple(row[k] for k in ('location_cm', 'rotation_quat_xyzw', 'scale'))
        if actual != original:
            changed.append(row['id'])
    print(json.dumps({'groups': groups, 'changed_instances': len(changed),
                      'ring_geometry_overrides': list(asset_overrides(cfg))}, indent=2))
