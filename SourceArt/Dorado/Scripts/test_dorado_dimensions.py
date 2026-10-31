"""Bounded source-transform invariants; no editor, render, or collision claims."""

import copy
import hashlib
import json
import math
from pathlib import Path
import unittest
import tempfile
from unittest.mock import patch

import dorado_dimensions as dims
from regenerate_ring_dimensions import transformed_normal


def rotate(q, point):
    norm = math.sqrt(sum(v*v for v in q))
    x, y, z, w = [v/norm for v in q]
    # Quaternion-vector formula independently checks the resulting world point.
    tx, ty, tz = 2*(y*point[2]-z*point[1]), 2*(z*point[0]-x*point[2]), 2*(x*point[1]-y*point[0])
    return [point[0]+w*tx+y*tz-z*ty, point[1]+w*ty+z*tx-x*tz, point[2]+w*tz+x*ty-y*tx]


def world(transform, point):
    location, q, scale = transform
    offset = rotate(q, [point[i]*scale[i] for i in range(3)])
    return [location[i]+offset[i] for i in range(3)]


class DimensionInvariants(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        manifest = json.loads((dims.ROOT/'geometry_manifest.json').read_text(encoding='utf-8'))
        energy = json.loads((dims.ROOT/'energy_manifest.json').read_text(encoding='utf-8'))
        cls.records = manifest['instances']+energy['instances']
        cls.assets = {asset['id']: asset for asset in manifest['assets']}

    def config(self, key=None, value=None):
        result = {'parameters': copy.deepcopy(dims.BASELINE)}
        if key:
            result['parameters'][key] = value
        return result

    @staticmethod
    def original(record):
        return tuple(record[k] for k in ('location_cm', 'rotation_quat_xyzw', 'scale'))

    def test_defaults_are_exact_identity_and_do_not_mutate(self):
        records = copy.deepcopy(self.records)
        config = self.config()
        before = copy.deepcopy(config)
        for record in records:
            self.assertEqual(dims.apply(record, config), self.original(record))
        self.assertEqual(records, self.records)
        self.assertEqual(config, before)

    def test_each_transform_control_changes_geometry_with_positive_scales(self):
        for name, baseline in dims.BASELINE.items():
            if name in ('ring_radial_thickness_cm', 'main_floor_core_width_cm'):
                continue
            value = [v*1.1 for v in baseline] if isinstance(baseline, list) else (100.0 if baseline == 0 else baseline*1.1)
            config = self.config(name, value)
            changed = 0
            for record in self.records:
                actual = dims.apply(record, config)
                self.assertTrue(all(math.isfinite(v) and v > 0 for v in actual[2]), name)
                self.assertEqual(actual[1], record['rotation_quat_xyzw'], name)
                if dims.classify(record) == 'scenery':
                    self.assertEqual(actual, self.original(record), name)
                changed += actual != self.original(record)
            self.assertGreater(changed, 0, name)

    def test_court_affine_preserves_structural_shared_points(self):
        # Test the actual transformed vertices, not just actor origins. A floor
        # and a stair that meet at a source point still meet after this affine.
        samples = [(-173.1, 412.8, 20.5), (511.6, -923.2, -140.3)]
        for key, axis in [('arena_width_cm', 0), ('arena_length_cm', 1)]:
            config = self.config(key, dims.BASELINE[key]*1.2)
            for record in self.records:
                aid = dims._asset(record)
                if dims.classify(record) != 'court' or aid.startswith(('rel_ed_block_clump_', 'rel_ed_banner', 'rel_el_dorado_snakeheadpillar_')):
                    continue
                result = dims.apply(record, config)
                for point in samples:
                    expected = world(self.original(record), point)
                    expected[axis] *= 1.2
                    actual = world(result, point)
                    for a, b in zip(actual, expected):
                        self.assertAlmostEqual(a, b, delta=0.001, msg=record['name'])

    def test_side_height_preserves_ground_plane(self):
        config = self.config('side_platform_height_cm', dims.BASELINE['side_platform_height_cm']*1.2)
        for record in self.records:
            if not dims._side(record):
                continue
            location, q, scale = self.original(record)
            # A world-floor point expressed in each source mesh's local frame.
            offset = [121.0-location[0], 235.0-location[1], -location[2]]
            local = rotate([-q[0], -q[1], -q[2], q[3]], offset)
            local = [local[i]/scale[i] for i in range(3)]
            self.assertAlmostEqual(world(dims.apply(record, config), local)[2], 0.0, delta=0.001)

    def test_pylon_spacing_moves_giant_pairs_and_preserves_clump_joins(self):
        config = self.config('pylon_spacing_cm', 2300.0)
        pylons = [r for r in self.records if dims._asset(r) in dims.GIANT_PYLON_IDS]
        self.assertEqual(len(pylons), 8)
        pairs, movements = {}, {}
        for record in pylons:
            actual = dims.apply(record, config)
            self.assertEqual(actual[2], record['scale'])
            delta = tuple(actual[0][i]-record['location_cm'][i] for i in range(3))
            self.assertEqual(delta[1:], (0.0, 0.0))
            self.assertAlmostEqual(abs(delta[0]), 200.0)
            aid = dims._asset(record)
            key = (record['location_cm'][1] > 0, '_wall_a_' in aid)
            if key in movements:
                self.assertEqual(delta, movements[key])
            movements[key] = delta
            if '_clump_' not in aid:
                bounds = self.assets[record['asset_id']]['local_bounds_cm']
                center = [(bounds['min'][i]+bounds['max'][i])/2 for i in range(3)]
                center[1] *= -1  # Source mesh Y is mirrored by Unreal import.
                before = world(self.original(record), center)[0]
                after = world(actual, center)[0]
                pairs.setdefault(record['location_cm'][1] > 0, []).append((before, after))
        self.assertEqual(len(pairs), 2)
        for pair in pairs.values():
            self.assertEqual(len(pair), 2)
            before_gap = abs(pair[0][0]-pair[1][0])
            after_gap = abs(pair[0][1]-pair[1][1])
            self.assertAlmostEqual(after_gap-before_gap, 400.0, places=5)
        braces = [r for r in self.records if dims._asset(r) == 'rel_el_dorado_snakeheadpillar_b_group']
        self.assertEqual(len(braces), 8)
        for record in braces:
            self.assertEqual(dims.apply(record, config), self.original(record))

    def test_changed_ring_bake_rejects_stale_source_and_exports(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root/'Config').mkdir()
            source = root/'Dorado_Recovered.blend'
            export = root/'ring.fbx'
            source.write_bytes(b'original source')
            export.write_bytes(b'original export')
            config = self.config('ring_radial_thickness_cm', 500.0)
            config_path = root/'dimensions.json'
            config_path.write_text(json.dumps(config))
            bake = {'ring_radial_thickness_cm': 500.0,
                    'source_blend_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
                    'assets': [{'id': 'ring', 'fbx': str(export),
                                'fbx_sha256': hashlib.sha256(export.read_bytes()).hexdigest()}]}
            (root/'Config/ring_dimension_bake.json').write_text(json.dumps(bake))
            with patch.object(dims, 'ROOT', root):
                self.assertEqual(dims.load_config(config_path)['ring_geometry_bake'], bake)
                source.write_bytes(b'new source')
                with self.assertRaises(dims.DimensionError):
                    dims.load_config(config_path)
                source.write_bytes(b'original source')
                export.write_bytes(b'changed export')
                with self.assertRaises(dims.DimensionError):
                    dims.load_config(config_path)
                export.unlink()
                with self.assertRaises(dims.DimensionError):
                    dims.load_config(config_path)
                config_path.write_text(json.dumps(self.config()))
                dims.load_config(config_path)  # Baseline never requires variants.

    def test_invalid_dimensions_fail_before_any_scene_operation(self):
        for invalid in (0, -1, float('nan'), float('inf'), True):
            with self.assertRaises(dims.DimensionError):
                dims.validate(self.config('arena_width_cm', invalid))
        with self.assertRaises(dims.DimensionError):
            dims.validate(self.config('ring_radial_thickness_cm', 500))

    def test_ring_radial_deformation_preserves_centerline_angle_and_uv_domain(self):
        centerline = 4537.19877
        for radial_offset in (-200, 0, 200):
            radius = centerline+radial_offset
            point = (radius*0.6, 123.5, radius*0.8)
            self.assertEqual(dims.deform_ring_point(point, 1), point)
            actual = dims.deform_ring_point(point, 1.5)
            self.assertAlmostEqual(math.hypot(actual[0], actual[2]), centerline+radial_offset*1.5)
            self.assertEqual(actual[1], point[1])
            self.assertAlmostEqual(math.atan2(actual[2], actual[0]), math.atan2(point[2], point[0]))
            normal = transformed_normal(point, (0.6, 0, 0.8), 1.5)
            self.assertAlmostEqual(sum(v*v for v in normal), 1)
        with self.assertRaises(dims.DimensionError):
            dims.deform_ring_point((100, 0, 0), 100)


if __name__ == '__main__':
    unittest.main(verbosity=2)
