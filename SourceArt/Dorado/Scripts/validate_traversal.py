"""Bounded, single-world Dorado PIE movement/ball evidence, without gameplay Python.

Import this module once into Editor Python. Call start_walk(...) for one route,
or start_ball_throw(...) while the real local pawn already carries the real Relic.
Gameplay changes go ONLY through the existing native DelayedExec ticker. Python
observes primitives; it never calls gameplay RPCs, abilities, or CheatManager.

BugItGo enables Ghost in UE5.8; every setup/restore therefore queues Walk next and
verifies collision + grounded mode before input. Do not travel/stop PIE until
status() reports inactive: bounded native input-release timers must drain first.
Ball tests require ending the disposable PIE session to restore Relic match state.
Physical input, remote replication, and all unsampled surfaces remain unproven.
"""
from __future__ import annotations
import json
import math
import pathlib
import re
import time
import unreal as u

MAP = '/BreakawayCore/Environments/Dorado/Maps/L_Dorado'
LOG = pathlib.Path(u.Paths.project_dir()).resolve()/'Saved/Logs/dorado-reconstruction-20261030'
_active = None  # Serialized primitives only.
_handle = None  # Slate delegate handle, never an Unreal world/actor wrapper.


def _xyz(v):
    return [float(v.x), float(v.y), float(v.z)]


def _rotation(v):
    return [float(v.pitch), float(v.yaw), float(v.roll)]


def _floor(world, x, y, z=600, ignore=None):
    value = u.SystemLibrary.line_trace_single(world, u.Vector(x, y, z), u.Vector(x, y, z-3000), u.TraceTypeQuery.ECC_VISIBILITY, False, ignore or [], u.DrawDebugTrace.NONE, True)
    hit = next((x for x in value if isinstance(x, u.HitResult)), None) if isinstance(value, (tuple, list)) else value
    if not isinstance(hit, u.HitResult):
        return {'hit': False}
    fields = hit.to_dict()
    if not fields['blocking_hit']:
        return {'hit': False}
    actor = fields['hit_actor']
    return {'hit': True, 'point_cm': _xyz(fields['impact_point']), 'normal': _xyz(fields['impact_normal']), 'actor': actor.get_path_name() if actor else None}


def _observe(extra_floor=None):
    """All Unreal wrappers die before the returned primitive dictionary escapes."""
    worlds = world = pc = pawn = movement = capsule = relic_class = relics = relic = mesh = carrier = settings = None
    try:
        worlds = u.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds) != 1:
            raise RuntimeError('Exactly one PIE game world required; DelayedExec targets first game world.')
        world = worlds[0]
        if not u.SystemLibrary.is_server(world):
            raise RuntimeError('Single local authority/standalone world required.')
        path = world.get_path_name()
        if '/Environments/Dorado/Maps/' not in path or 'L_Dorado' not in path:
            raise RuntimeError('Wrong map: '+path)
        pc = u.GameplayStatics.get_player_controller(world, 0)
        pawn = pc.get_controlled_pawn() if pc else None
        if not pawn:
            raise RuntimeError('Actual local project character is not possessed.')
        movement = pawn.get_component_by_class(u.CharacterMovementComponent)
        capsule = pawn.get_component_by_class(u.CapsuleComponent)
        if not movement or not capsule:
            raise RuntimeError('Possessed pawn is not a capsule-based project character.')
        position = _xyz(pawn.get_actor_location())
        row = {'world': path, 'time_s': float(u.GameplayStatics.get_time_seconds(world)), 'pawn': pawn.get_path_name(), 'pawn_class': pawn.get_class().get_path_name(), 'position_cm': position, 'velocity_cm_s': _xyz(pawn.get_velocity()), 'view': _rotation(pc.get_control_rotation()), 'collision_enabled': bool(pawn.get_actor_enable_collision()), 'move_input_ignored': bool(pc.is_move_input_ignored()), 'movement_mode': str(movement.get_editor_property('movement_mode')), 'capsule_radius_cm': float(capsule.get_scaled_capsule_radius()), 'capsule_half_height_cm': float(capsule.get_scaled_capsule_half_height()), 'max_step_height_cm': float(movement.get_editor_property('max_step_height')), 'max_walk_speed_cm_s': float(movement.get_editor_property('max_walk_speed')), 'has_cheat_manager': pc.get_editor_property('cheat_manager') is not None}
        row['floor'] = _floor(world, position[0], position[1], position[2], [pawn])
        if extra_floor:
            row['setup_floor'] = _floor(world, float(extra_floor[0]), float(extra_floor[1]), float(extra_floor[2]) if len(extra_floor) > 2 else 600, [pawn])
        relic_class = u.load_class(None, '/Script/BreakawayCoreRuntime.RelicActor')
        relics = u.GameplayStatics.get_all_actors_of_class(world, relic_class) if relic_class else []
        row['relic_count'] = len(relics)
        if len(relics) == 1:
            relic = relics[0]
            mesh = relic.get_editor_property('root_component')
            carrier = relic.get_editor_property('current_carrier')
            row['relic'] = {'actor': relic.get_path_name(), 'class': relic.get_class().get_path_name(), 'position_cm': _xyz(relic.get_actor_location()), 'state': str(relic.get_editor_property('current_state')), 'carrier': carrier.get_path_name() if carrier else None, 'physics_velocity_cm_s': _xyz(mesh.get_physics_linear_velocity()), 'simulating': bool(mesh.is_simulating_physics()), 'mass_kg': float(mesh.get_mass()) if mesh.is_simulating_physics() else None, 'component_scale': _xyz(mesh.get_world_scale()), 'collision_profile': str(mesh.get_collision_profile_name())}
            try:
                mesh_min, mesh_max = mesh.get_local_bounds()
                row['relic']['mesh_local_bounds_cm'] = {'min': _xyz(mesh_min), 'max': _xyz(mesh_max)}
                row['relic']['mesh_asset'] = mesh.get_editor_property('static_mesh').get_path_name()
                row['relic']['size_note'] = 'Actual mesh bounds; not an assumed test-sphere radius or collision primitive radius.'
            except Exception as exc:
                row['relic']['size_readback_unverified'] = str(exc)
            try:
                origin, extent = relic.get_actor_bounds(True)
                row['relic']['collision_actor_bounds'] = {'origin_cm': _xyz(origin), 'extent_cm': _xyz(extent), 'approximate_max_extent_cm': max(extent.x, extent.y, extent.z), 'note': 'Colliding actor bounds may include pickup query sphere; not a precise ball collision radius.'}
                body = mesh.get_editor_property('body_instance')
                row['relic']['ccd'] = bool(body.get_editor_property('use_ccd'))
                physical_material = body.get_editor_property('phys_material_override')
                row['relic']['physical_material_override'] = physical_material.get_path_name() if physical_material else None
                body = physical_material = None
            except Exception as exc:
                row['relic']['physics_shape_readback_unverified'] = str(exc)
            settings = relic.get_editor_property('relic_settings')
            if settings:
                row['relic']['settings_asset'] = settings.get_path_name()
                row['relic']['linear_damping'] = float(settings.get_editor_property('linear_damping'))
                row['relic']['angular_damping'] = float(settings.get_editor_property('angular_damping'))
        return row
    finally:
        worlds = world = pc = pawn = movement = capsule = relic_class = relics = relic = mesh = carrier = settings = None


def snapshot():
    return _observe()


def _queue(command, delay=.03):
    worlds = world = None
    try:
        worlds = u.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds) != 1:
            raise RuntimeError('PIE world count changed; native dispatch rejected.')
        world = worlds[0]
        u.SystemLibrary.execute_console_command(world, 'bway.Test.DelayedExec %.3f %s' % (delay, command), None)
        if _active is not None:
            queued_at = time.monotonic()-_active['started']
            _active['native_commands'].append({'queued_at': queued_at, 'delay_s': delay, 'command': command})
            _active['native_timers_drained_after'] = max(_active.get('native_timers_drained_after', 0), queued_at+delay+.15)
    finally:
        worlds = world = None


def _teleport(position, rotation):
    _queue('BugItGo '+' '.join('%.5f' % v for v in position+rotation), .03)
    _queue('Walk', .12)


def _release():
    _queue('Input.-key W')
    _queue('Input.-action IA_BW_Relic_Throw')


def status():
    return {'active': _active is not None, 'label': _active.get('label') if _active else None, 'phase': _active.get('phase') if _active else None}


def cancel():
    """Release owned input, restore same pawn, and drain native release timers."""
    if _active is None:
        return status()
    current = _observe()
    _release()
    if current['pawn'] == _active['initial']['pawn']:
        _teleport(_active['initial']['position_cm'], _active['initial']['view'])
    _active['phase'] = 'cancelling'
    _active['cancelled_at'] = time.monotonic()-_active['started']
    return status()


def _begin(label, kind, start_xy, end_xy, seconds, pitch=0, charge=.25, expected_end_z=None, allow_fall=False, floor_z_hint=None):
    global _active, _handle
    if _active is not None or _handle is not None:
        raise RuntimeError('Existing Dorado observer must finish; do not redeclare it.')
    if not re.fullmatch('[A-Za-z0-9_-]{1,48}', label):
        raise ValueError('Fresh simple attempt label required.')
    if not 1 <= seconds <= 15 or not 0.1 <= charge <= 1.5:
        raise ValueError('Bounded duration 1..15s and charge .1..1.5s required.')
    output = LOG/(label+'.json')
    if output.exists():
        raise RuntimeError('Attempt label already has evidence; use a fresh label.')
    # floor_z_hint is expected floor height, not ray origin. Trace from 200cm
    # above it to avoid overhead architecture, then reject a different deck.
    initial = _observe(list(start_xy)+[float(floor_z_hint)+200 if floor_z_hint is not None else 600])
    if not initial['has_cheat_manager'] or not initial['collision_enabled'] or 'WALKING' not in initial['movement_mode'].upper() or initial['move_input_ignored']:
        raise RuntimeError('Need grounded, collision-enabled actual character with movement input and native CheatManager available.')
    if not initial['setup_floor']['hit'] or initial['setup_floor']['normal'][2] < .7:
        raise RuntimeError('No walkable actual collision under proposed start point.')
    if floor_z_hint is not None and abs(initial['setup_floor']['point_cm'][2]-float(floor_z_hint)) > 100:
        raise RuntimeError('Setup collision lies more than 100cm from expected floor_z_hint.')
    carried = initial.get('relic', {}).get('carrier') == initial['pawn']
    if kind == 'ball' and not carried:
        raise RuntimeError('Actual local pawn must already carry the actual Relic through normal gameplay. No synthetic ball/pickup is created.')
    if kind == 'walk' and carried:
        raise RuntimeError('Release Relic through normal input before isolated walking route.')
    delta = [end_xy[0]-start_xy[0], end_xy[1]-start_xy[1]]
    distance = math.hypot(*delta)
    if distance < 100:
        raise ValueError('Route must exceed 100cm.')
    yaw = math.degrees(math.atan2(delta[1], delta[0]))
    start = [float(start_xy[0]), float(start_xy[1]), initial['setup_floor']['point_cm'][2]+initial['capsule_half_height_cm']+8]
    _active = {'label': label, 'kind': kind, 'phase': 'settling', 'started': time.monotonic(), 'last_sample': 0, 'initial': initial, 'start_cm': start, 'end_xy': list(end_xy), 'direction': [delta[0]/distance, delta[1]/distance], 'length_cm': distance, 'seconds': seconds, 'pitch': pitch, 'charge': charge, 'expected_end_z': expected_end_z, 'allow_fall': allow_fall, 'samples': [], 'native_commands': [], 'errors': [], 'output': str(output), 'reached': False, 'released': False, 'scope': 'One actual project character and actual Relic, one local PIE world, native simulated Enhanced Input. No physical input or network acceptance.', 'ball_state_cleanup': 'End disposable PIE session after all probes.' if kind == 'ball' else 'No Relic manipulation.'}
    try:
        _handle = u.register_slate_post_tick_callback(_tick)
        _teleport(start, [float(pitch), yaw, 0.0])
    except Exception as exc:
        _active['errors'].append(str(exc))
        if _handle is not None:
            _active['phase'] = 'draining_error'
            _active['error_drain_until'] = max(_active.get('native_timers_drained_after', 0), 1)
        else:
            _finish('SETUP_FAILED_NO_NATIVE_COMMANDS')
        raise
    return status()


def start_walk(label, start_xy, end_xy, seconds=6, expected_end_z=None, allow_fall=False, floor_z_hint=None):
    return _begin(label, 'walk', start_xy, end_xy, seconds, expected_end_z=expected_end_z, allow_fall=allow_fall, floor_z_hint=floor_z_hint)


def start_ball_throw(label, start_xy, aim_xy, seconds=8, pitch=-12, charge=.25, floor_z_hint=None):
    return _begin(label, 'ball', start_xy, aim_xy, seconds, pitch=pitch, charge=charge, floor_z_hint=floor_z_hint)


def start_blocking(label, start_xy, end_xy, maximum_progress_cm, seconds=2, floor_z_hint=0):
    result = start_walk(label, start_xy, end_xy, seconds=seconds, floor_z_hint=floor_z_hint)
    _active['expected_blocking_maximum_progress_cm'] = float(maximum_progress_cm)
    return result


def start_edge_fall(label, start_xy, end_xy, seconds=1, floor_z_hint=209):
    result = start_walk(label, start_xy, end_xy, seconds=seconds, floor_z_hint=floor_z_hint, allow_fall=True)
    _active['expected_open_edge_fall'] = True
    return result


def _finish(result):
    global _active, _handle
    state = _active
    if _handle is not None:
        u.unregister_slate_post_tick_callback(_handle)
        _handle = None
    if state is None:
        return
    state['result'] = result
    state['duration_s'] = time.monotonic()-state['started']
    state['not_proven'] = ['Physical input', 'Remote replication', 'Unsampled arena surfaces', 'Ball contact topology between samples', 'Manual visual traversal acceptance']
    state.pop('started', None)
    state.pop('last_sample', None)
    LOG.mkdir(parents=True, exist_ok=True)
    pathlib.Path(state['output']).write_text(json.dumps(state, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'dorado_traversal': result, 'report': state['output'], 'errors': state['errors']}))
    _active = None


def _tick(delta):
    if _active is None:
        return
    state = _active
    now = time.monotonic()
    if now-state['last_sample'] < .05:
        return
    state['last_sample'] = now
    elapsed = now-state['started']
    # Error paths must continue draining even if PIE vanished or the observation
    # API failed. Never report inactive while a delayed release can hit a new world.
    if state['phase'] == 'draining_error':
        if elapsed < state['error_drain_until']:
            return
        try:
            after = _observe()
            state['cleanup'] = {'native_release_timers_drained': True, 'pawn': after['pawn'], 'collision_enabled': after['collision_enabled'], 'movement_mode': after['movement_mode'], 'position_error_cm': math.dist(after['position_cm'], state['initial']['position_cm'])}
        except Exception as exc:
            state['cleanup'] = {'native_release_timers_drained': True, 'restoration_unverified': str(exc)}
        _finish('FAILED_OBSERVATION_OR_DISPATCH')
        return
    try:
        row = _observe()
        row['wall_s'] = elapsed
        row['phase'] = state['phase']
        state['samples'].append(row)
        if state['phase'] == 'cancelling':
            drained = elapsed >= max(state.get('native_timers_drained_after', 0), state['cancelled_at']+.8)
            if drained:
                state['cleanup'] = {'collision_enabled': row['collision_enabled'], 'movement_mode': row['movement_mode'], 'position_error_cm': math.dist(row['position_cm'], state['initial']['position_cm']), 'native_release_timers_drained': True}
                _finish('CANCELLED_NO_ACCEPTANCE')
            return
        if row['pawn'] != state['initial']['pawn']:
            raise RuntimeError('Actual pawn changed; observer cannot claim continuous traversal.')
        if state['phase'] == 'settling' and elapsed >= .8:
            if not row['collision_enabled'] or 'WALKING' not in row['movement_mode'].upper():
                raise RuntimeError('Native Walk did not restore grounded collision after BugItGo setup.')
            state['phase'] = 'moving'
            state['input_started'] = elapsed
            hold = state['seconds'] if state['kind'] == 'walk' else state['charge']
            if state['kind'] == 'walk':
                _queue('Input.+key W 1', .03)
                _queue('Input.-key W', hold+.06)
            else:
                _queue('Input.+action IA_BW_Relic_Throw true', .03)
                _queue('Input.-action IA_BW_Relic_Throw', hold+.06)
                state['throw_dispatch_scope'] = 'Actual project RelicThrow action forced through native EnhancedInput; bypasses physical key mapping.'
        elif state['phase'] == 'moving':
            moving_for = elapsed-state['input_started']
            if state['kind'] == 'walk':
                progress = sum((row['position_cm'][i]-state['start_cm'][i])*state['direction'][i] for i in range(2))
                row['route_progress_cm'] = progress
                if progress >= state['length_cm']-40 and not state['released']:
                    _release()
                    state['released'] = True
                    state['reached'] = True
                if not row['collision_enabled']:
                    state['errors'].append('Collision disabled during measured movement.')
                if 'FALLING' in row['movement_mode'].upper() and not state['allow_fall']:
                    state['errors'].append('Unexpected airborne sample during grounded route.')
            if moving_for >= state['seconds'] and elapsed >= state['native_timers_drained_after']:
                _release()
                state['measured_final'] = row
                state['phase'] = 'restoring'
                state['restore_started'] = elapsed
                _teleport(state['initial']['position_cm'], state['initial']['view'])
        elif state['phase'] == 'restoring' and elapsed-state['restore_started'] >= .8:
            state['cleanup'] = {'collision_enabled': row['collision_enabled'], 'movement_mode': row['movement_mode'], 'position_error_cm': math.dist(row['position_cm'], state['initial']['position_cm'])}
            restored = row['collision_enabled'] and 'WALKING' in row['movement_mode'].upper() and state['cleanup']['position_error_cm'] < 30
            if not restored:
                state['errors'].append('Character restoration not verified.')
            if state['kind'] == 'walk':
                expected = state['expected_end_z']
                final = state['measured_final']
                if expected is not None and abs(final['position_cm'][2]-final['capsule_half_height_cm']-expected) > 45:
                    state['errors'].append('Final feet height differs from expected platform.')
                if 'expected_blocking_maximum_progress_cm' in state:
                    progress = [r.get('route_progress_cm', 0) for r in state['samples'] if r['phase'] == 'moving']
                    maximum = max(progress, default=0)
                    blocked = 20 < maximum <= state['expected_blocking_maximum_progress_cm'] and math.dist(final['velocity_cm_s'], [0,0,0]) < 5
                    state['obstacle_observation'] = {'maximum_progress_cm': maximum, 'required_maximum_cm': state['expected_blocking_maximum_progress_cm'], 'moved_then_stopped': blocked}
                    _finish('PASS_VISIBLE_OBSTACLE_BLOCKING' if blocked and restored and not state['errors'] else 'FAIL_OBSTACLE_OR_CLEANUP')
                elif state.get('expected_open_edge_fall'):
                    falling = [r for r in state['samples'] if r['phase'] == 'moving' and 'FALLING' in r['movement_mode'].upper()]
                    drop = state['start_cm'][2] - min((r['position_cm'][2] for r in falling), default=state['start_cm'][2])
                    state['edge_observation'] = {'falling_samples': len(falling), 'maximum_drop_cm': drop}
                    _finish('PASS_EXPOSED_EDGE_FALL' if drop > 50 and restored and not state['errors'] else 'FAIL_EDGE_OR_CLEANUP')
                else:
                    passed = state['reached'] and restored and not state['errors']
                    _finish('PASS_SAMPLED_NATIVE_MOVEMENT' if passed else 'FAIL_ROUTE_OR_CLEANUP')
            else:
                airborne = [r for r in state['samples'] if r['phase'] == 'moving' and r.get('relic', {}).get('simulating') and r['relic'].get('carrier') is None]
                state['ball_observation'] = {'free_physics_samples': len(airborne), 'maximum_speed_cm_s': max((math.sqrt(sum(v*v for v in r['relic']['physics_velocity_cm_s'])) for r in airborne), default=0), 'trajectory_observed': len(airborne) >= 3, 'contact_assertions': 'Not inferred solely from sampled motion; inspect floor/stair collision and video evidence.'}
                _finish('OBSERVED_ACTUAL_RELIC_TRAJECTORY' if len(airborne) >= 3 and restored else 'FAILED_TO_OBSERVE_RELIC_TRAJECTORY')
        if elapsed > state['seconds']+5:
            raise RuntimeError('Bounded observer deadline exceeded.')
    except Exception as exc:
        state['errors'].append(str(exc))
        try:
            _release()
            # Do not teleport a replacement pawn. Stopping disposable PIE is the
            # cleanup path when world/character identity is no longer valid.
            if 'row' in locals() and row.get('pawn') == state['initial']['pawn']:
                _teleport(state['initial']['position_cm'], state['initial']['view'])
        except Exception as cleanup_exc:
            state['errors'].append('Cleanup dispatch: '+str(cleanup_exc))
        state['cleanup_required'] = 'Observer remains active until native timers drain. If restoration is unverified, stop disposable PIE after inactive.'
        state['phase'] = 'draining_error'
        state['error_drain_until'] = max(state.get('native_timers_drained_after', 0), elapsed+1)
