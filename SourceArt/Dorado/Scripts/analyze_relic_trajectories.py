"""Summarize actual Relic samples without inferring contacts from velocity alone."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import statistics

PROJECT = Path(__file__).resolve().parents[3]
LOG = PROJECT/'Saved/Logs/dorado-reconstruction-20261030'


def summarize(path, radius):
    record = json.loads(path.read_text())
    rows = [row for row in record['samples'] if row.get('relic', {}).get('simulating')
            and not row['relic'].get('carrier') and row['wall_s'] <= record['restore_started']]
    if not rows:
        raise ValueError('No free-physics samples: '+str(path))
    positions = [row['relic']['position_cm'] for row in rows]
    velocities = [row['relic']['physics_velocity_cm_s'] for row in rows]
    speeds = [math.sqrt(sum(v*v for v in velocity)) for velocity in velocities]
    dt = [b['wall_s']-a['wall_s'] for a, b in zip(rows, rows[1:])]
    final_position, final_velocity = positions[-1], velocities[-1]
    stationary = None
    if speeds[-1] <= 1.0:
        start = len(rows)-1
        while start > 0 and speeds[start-1] <= 1.0 and math.dist(positions[start-1], final_position) <= .1:
            start -= 1
        stationary = {'first_sample_wall_s': rows[start]['wall_s'],
                      'duration_s': rows[-1]['wall_s']-rows[start]['wall_s'], 'samples': len(rows)-start,
                      'speed_threshold_cm_s': 1.0, 'position_tolerance_cm': .1}
    changes = {axis: [] for axis in ('x', 'y', 'z')}
    for axis_index, axis in enumerate(changes):
        previous_sign, previous_index = 0, None
        for index, velocity in enumerate(velocities):
            value = velocity[axis_index]
            sign = 1 if value > 10 else -1 if value < -10 else 0
            if not sign:
                continue
            if previous_sign and sign != previous_sign:
                changes[axis].append({'from_wall_s': rows[previous_index]['wall_s'],
                                      'to_wall_s': rows[index]['wall_s'],
                                      'from_velocity_cm_s': velocities[previous_index][axis_index],
                                      'to_velocity_cm_s': value,
                                      'position_cm': positions[index]})
            previous_sign, previous_index = sign, index
    return {'source': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
            'reported_result': record['result'], 'free_physics_samples_before_restore': len(rows),
            'interval_wall_s': [rows[0]['wall_s'], rows[-1]['wall_s']],
            'sampling_interval_s': {'median': statistics.median(dt), 'maximum': max(dt)},
            'first_position_cm': positions[0], 'final_position_cm': final_position,
            'position_bounds_cm': {'min': [min(p[i] for p in positions) for i in range(3)],
                                   'max': [max(p[i] for p in positions) for i in range(3)]},
            'sampled_path_length_cm': sum(math.dist(a, b) for a, b in zip(positions, positions[1:])),
            'net_displacement_cm': math.dist(positions[0], final_position),
            'maximum_sampled_speed_cm_s': max(speeds), 'final_speed_cm_s': speeds[-1],
            'final_velocity_cm_s': final_velocity, 'stationary_tail': stationary,
            'sphere_bounds_at_final_cm': {'min': [v-radius for v in final_position],
                                         'max': [v+radius for v in final_position]},
            'velocity_sign_change_threshold_cm_s': 10,
            'velocity_sign_changes': changes,
            'velocity_sign_change_counts': {axis: len(events) for axis, events in changes.items()},
            'mass_kg_free_samples': sorted(set(row['relic'].get('mass_kg') for row in rows if row['relic'].get('mass_kg') is not None)),
            'ccd_all_free_samples': all(row['relic'].get('ccd') is True for row in rows),
            'final_is_not_resting': stationary is None,
            'sampling_limits': 'Sign changes describe sampled velocities, not exact hit actor, contact normal, bounce, rolling, tunneling or impact count. Sampled polyline length underestimates motion between samples.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--radius-cm', type=float, default=15.3346)
    args = parser.parse_args()
    paths = [LOG/name for name in ('dorado-relic-floor-openlane-01.json', 'dorado-relic-stairs-down-01.json')]
    results = [summarize(path, args.radius_cm) for path in paths]
    next_initial = json.loads(paths[1].read_text())['initial']
    floor = next_initial['floor']
    endpoint = results[0]['final_position_cm']
    evidence = {'scope': 'Two recorded actual-Relic native simulated-input trajectories in one local PIE world; no physical input or network proof.',
                'native_body_shape_from_root_readback': {'shape': 'Sphere', 'radius_cm': args.radius_cm,
                    'mass_kg': 10.0, 'ccd': True, 'note': 'Radius is from the native collision body, not render bounds or pickup-query extents.'},
                'trajectories': results,
                'open_lane_endpoint_support_comparison': {
                    'independent_floor_source': str(paths[1])+': initial.floor',
                    'independent_floor_point_cm': floor['point_cm'],
                    'xy_distance_to_previous_ball_endpoint_cm': math.dist(endpoint[:2], floor['point_cm'][:2]),
                    'ball_sphere_bottom_z_cm': endpoint[2]-args.radius_cm,
                    'bottom_minus_floor_cm': endpoint[2]-args.radius_cm-floor['point_cm'][2]},
                'coverage_limits': ['Open-lane trajectory stays near X423-424 and ends before Y0; it does not cover the central X0 seam.',
                    'Stair trajectory ends while still moving, before the requested Y1025 target. Its final negative Z alone does not establish penetration: the surface beneath the ball was not sampled.',
                    'Velocity reversals must not be classified as exact contact types or counts without hit/contact evidence.']}
    output = LOG/'relic-trajectory-analysis.json'
    output.write_text(json.dumps(evidence, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'report': str(output), 'trajectories': [
        {key: result[key] for key in ('source', 'free_physics_samples_before_restore', 'sampled_path_length_cm',
         'maximum_sampled_speed_cm_s', 'final_speed_cm_s', 'stationary_tail', 'velocity_sign_change_counts')}
        for result in results]}, indent=2))


if __name__ == '__main__':
    main()
