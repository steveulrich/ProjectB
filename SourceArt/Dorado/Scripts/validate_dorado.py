"""Read-only structural validation for the currently open Dorado editor map.

Run inside UE 5.8:
    exec(open(r'E:/Unreal Projects/ProjectB/SourceArt/Dorado/Scripts/validate_dorado.py').read())
    run()

The default run does not change maps, actors, assets, selection, or viewport.
preview_camera('CAM_Ref_01') optionally navigates the editor viewport without
saving anything. It is not called by run(). JSON reports are the only writes.

API basis: installed UE source headers IAssetRegistry.h, StaticMesh.h,
KismetSystemLibrary.h, GameplayStatics.h, NavigationSystem.h, FileHelpers.h,
and UnrealEditorSubsystem.h. Structural traces do not prove traversal or ball
behavior. No editor execution is claimed until a report is produced.
"""
from __future__ import annotations
import collections
import datetime
import hashlib
import json
import math
import pathlib
import unreal as u

ROOT = '/BreakawayCore/Environments/Dorado'
MAP = ROOT+'/Maps/L_Dorado'
TAG = 'DoradoReconstruction'
PROJECT = pathlib.Path(u.Paths.project_dir()).resolve()
SOURCE = PROJECT/'SourceArt/Dorado'
LOG = PROJECT/'Saved/Logs/dorado-reconstruction-20261030'


def xyz(value):
    return [float(value.x), float(value.y), float(value.z)]


def object_path(value):
    return value.get_path_name() if value else None


def package_of(value):
    return str(value).split('.')[0]


def camera_actors():
    return [a for a in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
            if a.get_component_by_class(u.CameraComponent)]


def preview_camera(name):
    """Navigate only the editor viewport; no package or actor mutation/save."""
    matches = [a for a in camera_actors() if a.get_actor_label() == name]
    if len(matches) != 1:
        raise RuntimeError('Expected exactly one camera named '+name)
    a = matches[0]
    u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(
        a.get_actor_location(), a.get_actor_rotation())
    return {'name': name, 'location_cm': xyz(a.get_actor_location()),
            'note': 'Viewport camera moved; saved actor unchanged. FOV/aspect require camera piloting or render setup.'}


def trace_point(world, point, expected_height, tolerance=65.0):
    start = u.Vector(point[0], point[1], expected_height+650)
    end = u.Vector(point[0], point[1], expected_height-900)
    value = u.SystemLibrary.line_trace_single(world, start, end,
        u.TraceTypeQuery.ECC_VISIBILITY, False, [], u.DrawDebugTrace.NONE, True)
    # Python wrappers expose a bool/out-result signature as an optional HitResult
    # or tuple, depending on wrapper metadata. Accept either without hiding errors.
    hit = next((x for x in value if isinstance(x, u.HitResult)), None) if isinstance(value, (tuple, list)) else value
    if not isinstance(hit, u.HitResult):
        return {'xy_cm': point, 'hit': False, 'expected_z_cm': expected_height}
    fields = hit.to_dict()
    if not fields['blocking_hit']:
        return {'xy_cm': point, 'hit': False, 'expected_z_cm': expected_height}
    location, normal, actor = fields['impact_point'], fields['impact_normal'], fields['hit_actor']
    return {'xy_cm': point, 'hit': True, 'location_cm': xyz(location), 'normal': xyz(normal),
            'actor': actor.get_actor_label() if actor else None,
            'expected_z_cm': expected_height, 'height_error_cm': abs(location.z-expected_height),
            'height_within_tolerance': abs(location.z-expected_height) <= tolerance,
            'upward_surface': normal.z >= 0.7}


def run(report_name='validation-structural.json', trace_floors=True, check_navigation=True):
    assets = u.get_editor_subsystem(u.EditorAssetSubsystem)
    actors_api = u.get_editor_subsystem(u.EditorActorSubsystem)
    registry = u.AssetRegistryHelpers.get_asset_registry()
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    result = {'generated_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
              'engine': u.SystemLibrary.get_engine_version(), 'map': object_path(world),
              'scope': 'Read-only saved-asset, dependency, transform, camera, and sampled collision/navigation checks.',
              'not_proven': ['Visual match', 'Character traversal', 'Ball physics', 'Runtime cost',
                            'Migration', 'Close/reopen success', 'Physical input'],
              'errors': [], 'warnings': []}

    def error(kind, **data):
        result['errors'].append({'kind': kind, **data})

    def warning(kind, **data):
        result['warnings'].append({'kind': kind, **data})

    def section(name, operation):
        try:
            result[name] = operation()
        except Exception as exc:
            error('section_failed', section=name, message=str(exc))
            result[name] = {'status': 'unverified', 'error': str(exc)}

    def dirty_packages():
        packages = list(u.EditorLoadingAndSavingUtils.get_dirty_content_packages())
        packages += list(u.EditorLoadingAndSavingUtils.get_dirty_map_packages())
        return sorted({p.get_name() for p in packages if p.get_name().startswith(ROOT)})

    if not world or not world.get_path_name().startswith(MAP+'.'):
        error('wrong_current_map', expected=MAP, actual=object_path(world))
    else:
        all_actors = actors_api.get_all_level_actors()
        owned = [a for a in all_actors if TAG in [str(t) for t in a.tags]]
        section('dirty_owned_packages', dirty_packages)
        if result.get('dirty_owned_packages'):
            error('unsaved_owned_packages', packages=result['dirty_owned_packages'])

        def inventory():
            data = registry.get_assets_by_path(ROOT, True, True)
            counts, mesh_rows = collections.Counter(), []
            for datum in data:
                path = str(datum.package_name)
                class_name = str(datum.asset_class_path.asset_name)
                counts[class_name] += 1
                if class_name != 'StaticMesh':
                    continue
                mesh = assets.load_asset(path)
                if not mesh:
                    error('asset_load_failed', path=path)
                    continue
                slots = mesh.get_editor_property('static_materials')
                mats = []
                is_calibration = path.endswith('/SM_Dorado_CentimeterCalibration')
                for index, slot in enumerate(slots):
                    mat = slot.get_editor_property('material_interface')
                    mats.append(object_path(mat))
                    if mat is None and not is_calibration:
                        error('null_mesh_material', path=path, slot=index)
                    elif mat and not is_calibration and ('WorldGridMaterial' in mat.get_path_name() or 'DefaultMaterial' in mat.get_path_name()):
                        error('fallback_mesh_material', path=path, slot=index, material=mat.get_path_name())
                box = mesh.get_bounding_box()
                mesh_rows.append({'path': path, 'materials': mats, 'bounds_min': xyz(box.min), 'bounds_max': xyz(box.max)})
            if not data:
                error('no_saved_owned_assets')
            return {'counts': dict(counts), 'saved_asset_count': len(data), 'meshes': mesh_rows}
        section('asset_inventory', inventory)

        def dependencies():
            options = u.AssetRegistryDependencyOptions()
            options.set_editor_property('include_hard_package_references', True)
            options.set_editor_property('include_soft_package_references', True)
            options.set_editor_property('include_game_package_references', True)
            options.set_editor_property('include_editor_only_package_references', True)
            pending = {str(d.package_name) for d in registry.get_assets_by_path(ROOT, True, True)}
            pending.add(MAP)
            visited, external, missing, dependency_cache = set(), set(), set(), {}

            def dependency_list(package):
                if package not in dependency_cache:
                    dependency_cache[package] = sorted(str(d) for d in registry.get_dependencies(package, options))
                return dependency_cache[package]
            while pending:
                package = pending.pop()
                if package in visited or package.startswith('/Script/'):
                    continue
                visited.add(package)
                entries = registry.get_assets_by_package_name(package, True)
                if not entries and not assets.does_asset_exist(package):
                    missing.add(package)
                    continue
                for dep in dependency_list(package):
                    if not dep.startswith(ROOT+'/'):
                        external.add(dep)
                    if dep not in visited and not dep.startswith('/Script/'):
                        pending.add(dep)
            inherited = []
            candidate = '/Game/System/FrontEnd/Maps/L_LyraFrontEnd_BuiltData'
            baseline_map = '/BreakawayCore/Maps/L_BW_Dorado'
            provenance_file = LOG/'inherited-frontend-builddata.json'
            if candidate in missing and provenance_file.exists():
                evidence = json.loads(provenance_file.read_text(encoding='utf-8-sig'))
                source_file = PROJECT/'Content/System/FrontEnd/Maps/L_LyraFrontEnd.umap'
                baseline_file = PROJECT/'Plugins/GameFeatures/BreakawayCore/Content/Maps/L_BW_Dorado.umap'
                proven = (evidence.get('proven_inherited_source_reference') is True
                          and source_file.exists() and baseline_file.exists()
                          and hashlib.sha256(source_file.read_bytes()).hexdigest() == evidence.get('source_map_sha256')
                          and hashlib.sha256(baseline_file.read_bytes()).hexdigest() == evidence.get('baseline_dorado_sha256'))
                if proven:
                    # Independently prove the original Dorado map reaches this
                    # same missing external asset. Never whitelist by name alone.
                    queue, parents = collections.deque([baseline_map]), {baseline_map: None}
                    while queue and candidate not in parents and len(parents) <= 5000:
                        package = queue.popleft()
                        for dep in dependency_list(package):
                            if dep not in parents and not dep.startswith('/Script/'):
                                parents[dep] = package
                                queue.append(dep)
                    if candidate in parents:
                        chain, cursor = [], candidate
                        while cursor is not None:
                            chain.append(cursor)
                            cursor = parents[cursor]
                        row = {'path': candidate, 'baseline_dependency_chain': list(reversed(chain)),
                               'provenance': str(provenance_file),
                               'classification': 'Inherited stale external frontend lighting-data reference; unchanged source assets match Git HEAD LFS payload hashes.',
                               'action': 'Reference retained. Repair belongs to the existing frontend content; migration/packaging must still account for it.'}
                        inherited.append(row)
                        warning('inherited_stale_external_dependency', **row)
            for p in sorted(missing):
                if p not in {row['path'] for row in inherited}:
                    error('missing_dependency', path=p)
            return {'visited_package_count': len(visited), 'external_packages': sorted(external),
                    'missing_packages': sorted(missing), 'inherited_stale_external_references': inherited}
        section('dependencies', dependencies)

        def scene():
            labels = collections.Counter(a.get_actor_label() for a in owned)
            for label, count in labels.items():
                if count > 1:
                    error('duplicate_owned_label', label=label, count=count)
            rows = []
            for actor in owned:
                scale = xyz(actor.get_actor_scale3d())
                if min(scale) <= 0:
                    error('nonpositive_actor_scale', label=actor.get_actor_label(), scale=scale)
                for component in actor.get_components_by_class(u.StaticMeshComponent):
                    mesh = component.get_editor_property('static_mesh')
                    if not mesh:
                        error('null_component_mesh', actor=actor.get_actor_label())
                    for slot in range(component.get_num_materials()):
                        if component.get_material(slot) is None:
                            error('null_component_material', actor=actor.get_actor_label(), slot=slot)
                rows.append({'label': actor.get_actor_label(), 'class': actor.get_class().get_name(),
                             'location_cm': xyz(actor.get_actor_location()), 'scale': scale})
            return {'owned_actor_count': len(owned), 'all_actor_count': len(all_actors), 'actors': rows}
        section('scene', scene)

        def calibration():
            specification = json.loads((SOURCE/'Config/calibration.json').read_text())
            path = ROOT+'/Meshes/SM_Dorado_CentimeterCalibration'
            mesh = assets.load_asset(path)
            if not mesh:
                error('missing_calibration_asset', path=path)
                return {'status': 'unverified', 'path': path}
            box = mesh.get_bounding_box()
            actual = {'min': xyz(box.min), 'max': xyz(box.max)}
            expected = specification['expected_ue_bounds_cm']
            deviations = [abs(actual[k][i]-expected[k][i]) for k in ('min', 'max') for i in range(3)]
            passed = max(deviations) <= 0.1
            if not passed:
                error('calibration_bounds_mismatch', actual=actual, expected=expected)
            return {'passed': passed, 'actual': actual, 'expected': expected, 'max_error_cm': max(deviations),
                    'note': 'Checks numerical scale/axis bounds only; visual face, UV, and normal checks remain required.'}
        section('calibration', calibration)

        def cameras():
            expected = ['CAM_Ref_01', 'CAM_Ref_02', 'CAM_Ref_03']
            rows = []
            actual = camera_actors()
            for name in expected:
                matches = [a for a in actual if a.get_actor_label() == name]
                if len(matches) != 1:
                    error('camera_count_mismatch', name=name, count=len(matches))
                    continue
                actor = matches[0]
                comp = actor.get_component_by_class(u.CameraComponent)
                rot = actor.get_actor_rotation()
                rows.append({'name': name, 'location_cm': xyz(actor.get_actor_location()),
                             'rotation_pitch_yaw_roll': [rot.pitch, rot.yaw, rot.roll],
                             'horizontal_fov_degrees': comp.get_editor_property('field_of_view'),
                             'aspect_ratio': comp.get_editor_property('aspect_ratio'),
                             'constrain_aspect_ratio': comp.get_editor_property('constrain_aspect_ratio')})
            return rows
        section('cameras', cameras)

        def protected_gameplay():
            baseline = json.loads((LOG/'map-baseline.json').read_text())
            protected_classes = {'LyraPlayerStart', 'BwaySpawnPoint', 'BwayBaseZone', 'NavMeshBoundsVolume'}
            by_label = {a.get_actor_label(): a for a in all_actors}
            checked = []
            for original in baseline:
                if original['class'] not in protected_classes:
                    continue
                a = by_label.get(original['label'])
                if not a:
                    error('missing_protected_gameplay_actor', original=original)
                    continue
                delta = max(abs(x-y) for x, y in zip(xyz(a.get_actor_location()), original['location']))
                if a.get_class().get_name() != original['class'] or delta > 0.01:
                    error('protected_actor_changed', label=original['label'], location_error_cm=delta)
                checked.append(original['label'])
            return {'checked': checked, 'note': 'Class and location comparison only; not full gameplay property or runtime validation.'}
        section('protected_gameplay', protected_gameplay)

        if trace_floors:
            def floors():
                points = [(x, y) for y in (-2000, -1000, 0, 1000, 2000) for x in (-450, 25, 450)]
                # These two fixed fixture points lie on visible source-authored
                # cover. Require its exact actor and height; do not ignore it or
                # let any arbitrary obstacle satisfy a ground-floor assertion.
                cover_samples = {
                    (25, -2000): 'D_rel_ed_block_clump_d_group2_A5C77A90',
                    (25, 2000): 'D_rel_ed_block_clump_d_group1_5C84F46D',
                }
                rows = []
                for point in points:
                    expected_actor = cover_samples.get(point)
                    row = trace_point(world, list(point), 96.0 if expected_actor else 0.0,
                                      tolerance=5.0 if expected_actor else 65.0)
                    row['sample_role'] = 'visible_cover' if expected_actor else 'floor'
                    if expected_actor:
                        row['expected_actor'] = expected_actor
                        row['actor_matches_expected'] = row.get('actor') == expected_actor
                    passed = (row['hit'] and row.get('height_within_tolerance') and
                              row.get('upward_surface') and row.get('actor_matches_expected', True))
                    if not passed:
                        error('cover_sample_failed' if expected_actor else 'floor_sample_failed', sample=row)
                    rows.append(row)
                centerline = [trace_point(world, [0, y], 0) for y in (-2000, -1000, 0, 1000, 2000)]
                for row in centerline:
                    if not row['hit'] or not row.get('height_within_tolerance') or not row.get('upward_surface'):
                        warning('centerline_collision_diagnostic', sample=row,
                                note='Centerline includes cover blocks and exact mesh seams. Retained diagnostic; actual capsule/Relic movement decides seam behavior.')
                return {'trace_channel': 'Visibility / ECC_VISIBILITY', 'complex': False,
                        'samples': rows, 'centerline_diagnostic_samples': centerline,
                        'sample_selection_evidence': str(LOG/'validation-structural-before-open-lanes.json'),
                        'cover_classification_evidence': str(LOG/'cover-sample-classification-adjustment.json'),
                        'note': 'Thirteen ground-floor samples plus two exact named source-cover samples at X25,Y+/-2000 with expected Z96cm +/-5cm. No actors are ignored. Original X=0 rays remain explicit diagnostics. Sparse editor rays do not prove continuous collision, movement, or ball behavior.'}
            section('floor_samples', floors)
        if check_navigation:
            def navigation():
                nav_actors = [a for a in all_actors if a.get_class().get_name() == 'RecastNavMesh']
                profiles = []
                for nav in nav_actors:
                    profile = {'actor': object_path(nav)}
                    for prop in ('agent_radius', 'agent_height', 'agent_max_slope'):
                        try:
                            profile[prop] = float(nav.get_editor_property(prop))
                        except Exception as exc:
                            profile[prop+'_unverified'] = str(exc)
                    profiles.append(profile)
                if len(nav_actors) != 1:
                    error('navigation_data_ambiguous_or_absent', count=len(nav_actors))
                    return {'status': 'unverified', 'nav_data': profiles, 'paths': []}
                context = nav_actors[0]
                if u.NavigationSystemV1.is_navigation_being_built_or_locked(world):
                    error('navigation_build_incomplete')
                    return {'status': 'unverified', 'nav_data': profiles, 'paths': []}
                routes = [
                    ('main_court_length', [450, -2000, 10], [450, 2000, 10]),
                    ('main_court_width', [-600, 500, 10], [600, 500, 10]),
                    ('east_south_stairs', [1275, -1050, 10], [1275, -50, 219]),
                    ('east_north_stairs', [1275, 1050, 10], [1275, 50, 219]),
                    ('west_south_stairs', [-1275, -1050, 10], [-1275, -50, 219]),
                    ('west_north_stairs', [-1275, 1050, 10], [-1275, 50, 219]),
                ]
                paths = []
                for name, start, end in routes:
                    row = {'route': name, 'requested_start_cm': start, 'requested_end_cm': end}
                    projected = []
                    for label, point in (('start', start), ('end', end)):
                        value = u.NavigationSystemV1.project_point_to_navigation(
                            world, u.Vector(*point), context, None, u.Vector(75, 75, 80))
                        vector = next((v for v in value if isinstance(v, u.Vector)), None) if isinstance(value, (tuple, list)) else value
                        if isinstance(value, (tuple, list)) and any(v is False for v in value):
                            vector = None
                        if not isinstance(vector, u.Vector):
                            vector = None
                        row['projected_'+label+'_cm'] = xyz(vector) if vector is not None else None
                        if vector is not None:
                            row[label+'_projection_error_cm'] = math.dist(point, xyz(vector))
                        projected.append(vector)
                    if any(v is None for v in projected):
                        row['passed'] = False
                        error('navigation_route_projection_failed', route=name, sample=row)
                        paths.append(row)
                        continue
                    path = u.NavigationSystemV1.find_path_to_location_synchronously(world, projected[0], projected[1], context, None)
                    row['valid'] = bool(path and path.is_valid())
                    row['partial'] = bool(path and path.is_partial())
                    points = [xyz(v) for v in path.get_editor_property('path_points')] if path else []
                    row['path_points_cm'] = points
                    row['path_length_cm'] = float(path.get_path_length()) if path else None
                    row['start_error_cm'] = math.dist(points[0], xyz(projected[0])) if points else None
                    row['end_error_cm'] = math.dist(points[-1], xyz(projected[1])) if points else None
                    row['passed'] = bool(row['valid'] and not row['partial'] and len(points) >= 2
                        and row['start_error_cm'] <= 60 and row['end_error_cm'] <= 60
                        and row['start_projection_error_cm'] <= 60 and row['end_projection_error_cm'] <= 60)
                    if not row['passed']:
                        error('navigation_route_not_connected', route=name, sample=row)
                    paths.append(row)
                    path = None
                return {'nav_data': profiles, 'query_extent_cm': [75, 75, 80], 'endpoint_tolerance_cm': 60,
                        'paths': paths, 'passed': all(row['passed'] for row in paths),
                        'note': 'Six bounded complete-path queries using the sole recorded Recast agent profile. No movement, collision tuning, navigation rebuild, or actual-character/AI traversal is performed.'}
            section('navigation', navigation)
    result['structural_status'] = 'FAILED' if result['errors'] else 'PASSED_SAMPLED_STRUCTURAL_SCOPE_ONLY'
    result['ready_to_integrate'] = False
    result['ready_to_integrate_reason'] = 'This script cannot satisfy visual, traversal, ball, reopening, or migration requirements.'
    LOG.mkdir(parents=True, exist_ok=True)
    output = LOG/pathlib.Path(report_name).name
    output.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'report': str(output), 'status': result['structural_status'],
                      'error_count': len(result['errors']), 'warning_count': len(result['warnings'])}))
    return result
