"""Scoped native Nanite configuration; authoring only until root invokes it.

After PIE stops, call configure_one(index) for indices 0..6 from the existing
bounded editor job dispatcher. Each call may synchronously rebuild one mesh.
Do not call seven builds from a bridge handler with a 30-second lifetime.
Only the seven listed owned mesh assets are saved; actors/maps are unchanged.
"""

import datetime
import json
import pathlib
import unreal as u

ROOT = '/BreakawayCore/Environments/Dorado'
MAP = ROOT+'/Maps/L_Dorado'
ASSET_IDS = tuple('SM_Dorado_rel_ed_vistamountains_clump_'+letter+'_group' for letter in 'abcdef') + (
    'SM_Dorado_rel_el_mountain_unique_c_group',)


def _settings_record(settings):
    return {'enabled': bool(settings.get_editor_property('enabled')),
            'explicit_tangents': bool(settings.get_editor_property('explicit_tangents')),
            'fallback_target': str(settings.get_editor_property('fallback_target')),
            'fallback_percent_triangles': float(settings.get_editor_property('fallback_percent_triangles')),
            'fallback_relative_error': float(settings.get_editor_property('fallback_relative_error'))}


def _correct(settings):
    return (settings.get_editor_property('enabled') is True
            and settings.get_editor_property('explicit_tangents') is True
            and settings.get_editor_property('fallback_target') == u.NaniteFallbackTarget.PERCENT_TRIANGLES
            and abs(settings.get_editor_property('fallback_percent_triangles')-1.0) < 1e-8
            and abs(settings.get_editor_property('fallback_relative_error')) < 1e-8)


def _log(row):
    project = pathlib.Path(u.Paths.project_dir()).resolve()
    target = project/'Saved/Logs/dorado-reconstruction-20261030/nanite-backdrop-configuration.json'
    report = json.loads(target.read_text(encoding='utf-8')) if target.exists() else {
        'scope': 'Seven owned opaque, noncolliding backdrop mesh assets. No gameplay meshes, cards, maps or actors saved.',
        'api_source': 'Installed UE5.8 StaticMeshEditorSubsystem.h:217,226; EngineTypes.h:3222,3285,3346-3359.',
        'assets': {}, 'not_proven': ['VSM warning removal', 'GPU performance improvement', 'Visual parity', 'Packaged behavior']}
    report['generated_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    old = report['assets'].get(row['asset_id'])
    if old:
        row['original_before'] = old.get('original_before', old.get('before'))
    elif 'before' in row:
        row['original_before'] = row['before']
    report['assets'][row['asset_id']] = row
    report['configured_or_already_correct'] = sum(item.get('status') in ('configured_and_saved', 'already_correct')
                                                  for item in report['assets'].values())
    report['failed'] = sum(item.get('status') == 'failed' for item in report['assets'].values())
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return str(target)


def configure_one(index):
    """Configure/save one explicit mesh and return primitives; safe to repeat."""
    if not isinstance(index, int) or isinstance(index, bool) or not 0 <= index < len(ASSET_IDS):
        raise ValueError('Expected integer index 0 through 6.')
    asset_id = ASSET_IDS[index]
    path = ROOT+'/Meshes/'+asset_id
    row = {'asset_id': asset_id, 'path': path, 'index': index,
           'generated_utc': datetime.datetime.now(datetime.timezone.utc).isoformat()}
    worlds = world = assets = mesh_editor = mesh = settings = after = actor_api = actors = actor = component = material = slots = slot = None
    try:
        worlds = u.EditorLevelLibrary.get_pie_worlds(False)
        if worlds:
            raise RuntimeError('Stop the owned PIE session before rebuilding native static meshes.')
        world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
        if not world or not world.get_path_name().startswith(MAP+'.'):
            raise RuntimeError('Expected the owned L_Dorado editor map.')
        assets = u.get_editor_subsystem(u.EditorAssetSubsystem)
        mesh_editor = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
        mesh = assets.load_asset(path)
        if not mesh or not isinstance(mesh, u.StaticMesh) or mesh.get_path_name().split('.')[0] != path:
            raise RuntimeError('Owned mesh is missing, redirected, or has the wrong class: '+path)
        row['materials'] = []
        slots = mesh.get_editor_property('static_materials')
        if not slots:
            raise RuntimeError('No material slots; eligibility cannot be established.')
        for index_slot, slot in enumerate(slots):
            material = slot.get_editor_property('material_interface')
            if not material or material.get_blend_mode() != u.BlendMode.BLEND_OPAQUE:
                raise RuntimeError('Every mesh material must be opaque; rejected slot '+str(index_slot))
            row['materials'].append(material.get_path_name())
        actor_api = u.get_editor_subsystem(u.EditorActorSubsystem)
        actors = actor_api.get_all_level_actors()
        uses = []
        for actor in actors:
            for component in actor.get_components_by_class(u.StaticMeshComponent):
                if component.get_editor_property('static_mesh') != mesh:
                    continue
                if component.get_editor_property('mobility') != u.ComponentMobility.STATIC:
                    raise RuntimeError('Backdrop component is not static: '+actor.get_actor_label())
                if component.get_collision_enabled() != u.CollisionEnabled.NO_COLLISION:
                    raise RuntimeError('Backdrop has enabled collision; this experiment excludes collision changes: '+actor.get_actor_label())
                for material_index in range(component.get_num_materials()):
                    material = component.get_material(material_index)
                    if not material or material.get_blend_mode() != u.BlendMode.BLEND_OPAQUE:
                        raise RuntimeError('Component override is not opaque: '+actor.get_actor_label())
                uses.append(actor.get_actor_label())
        if not uses:
            raise RuntimeError('No scoped map instances found; asset identity is unverified.')
        row['instances'] = uses
        settings = mesh_editor.get_nanite_settings(mesh)
        row['before'] = _settings_record(settings)
        row['status'] = 'preflight_passed'
        _log(row)  # Preserve the original settings before a potentially long build.
        if _correct(settings):
            row['after'] = row['before'].copy()
            if not assets.save_loaded_asset(mesh, True):
                raise RuntimeError('Saving the already configured owned mesh failed.')
            row['status'] = 'already_correct'
        else:
            settings.set_editor_property('enabled', True)
            settings.set_editor_property('explicit_tangents', True)
            settings.set_editor_property('fallback_target', u.NaniteFallbackTarget.PERCENT_TRIANGLES)
            settings.set_editor_property('fallback_percent_triangles', 1.0)
            settings.set_editor_property('fallback_relative_error', 0.0)
            mesh_editor.set_nanite_settings(mesh, settings, True)
            after = mesh_editor.get_nanite_settings(mesh)
            row['after'] = _settings_record(after)
            if not _correct(after):
                raise RuntimeError('Native Nanite settings did not persist in memory.')
            if not assets.save_loaded_asset(mesh, False):
                raise RuntimeError('Owned static mesh save failed.')
            row['status'] = 'configured_and_saved'
        row['build_scope'] = 'Native post-edit rebuild requested and asset saved; render-resource readiness and warning removal require root verification.'
        row['report'] = _log(row)
        return row
    except Exception as exc:
        row['status'] = 'failed'
        row['error'] = str(exc)
        row['report'] = _log(row)
        raise RuntimeError(str(exc)+'; evidence: '+row['report']) from exc
    finally:
        worlds = world = assets = mesh_editor = mesh = settings = after = actor_api = actors = actor = component = material = slots = slot = None
