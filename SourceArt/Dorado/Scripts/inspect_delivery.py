"""Read final persisted Dorado settings; write evidence only, never save assets."""
import datetime
import hashlib
import json
import pathlib
import unreal as u


def run(report_name='delivery-native-readback.json'):
    project = pathlib.Path(u.Paths.project_dir()).resolve()
    log = project / 'Saved/Logs/dorado-reconstruction-20261030'
    root = '/BreakawayCore/Environments/Dorado'
    world = actors = component = actor = mesh = None
    try:
        world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
        actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
        if not world.get_path_name().startswith(root+'/Maps/L_Dorado.'):
            raise RuntimeError('Expected saved Dorado map')
        rows = []
        cameras = []
        lights = {}
        for actor in actors:
            label = actor.get_actor_label()
            if 'rel_eldorado_mid_stair_c_' in label:
                component = actor.get_component_by_class(u.StaticMeshComponent)
                slope = component.get_walkable_slope_override()
                rows.append({'actor':label,'angle':slope.get_editor_property('walkable_slope_angle'),
                             'behavior':str(slope.get_editor_property('walkable_slope_behavior'))})
            if label.startswith('CAM_'):
                component = actor.get_component_by_class(u.CameraComponent)
                cameras.append({'name':label,'location':list(actor.get_actor_location().to_tuple()),
                                'projection':str(component.get_editor_property('projection_mode')),
                                'fov':component.field_of_view,'aspect':component.aspect_ratio})
            if label in ('D_Sun','D_Sky'):
                component = actor.get_component_by_class(u.DirectionalLightComponent if label=='D_Sun' else u.SkyLightComponent)
                color = component.get_editor_property('light_color')
                lights[label] = {'intensity':component.get_editor_property('intensity'), 'color_rgb':[color.r,color.g,color.b]}
        nanite = []
        config = json.loads((log/'nanite-backdrop-configuration.json').read_text())
        subsystem = u.get_editor_subsystem(u.StaticMeshEditorSubsystem)
        for entry in config['assets'].values():
            mesh = u.load_asset(entry['path'])
            settings = subsystem.get_nanite_settings(mesh)
            nanite.append({'path':entry['path'],'enabled':settings.get_editor_property('enabled'),
                           'explicit_tangents':settings.get_editor_property('explicit_tangents'),
                           'fallback_percent_triangles':settings.get_editor_property('fallback_percent_triangles')})
        masters = []
        for name in ('Surface','Foliage','Water','Energy','Cloud','MaskedLogo','Glow'):
            material = u.load_asset(root+'/Materials/M_Dorado_'+name)
            masters.append({'name':name,'blend_mode':str(material.get_editor_property('blend_mode')),
                            'shading_model':str(material.get_editor_property('shading_model'))})
        dirty = [p.get_path_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_map_packages()+u.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
        native_map = project/'Plugins/GameFeatures/BreakawayCore/Content/Environments/Dorado/Maps/L_Dorado.umap'
        data = {'generated_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
                'engine':u.SystemLibrary.get_engine_version(),'map':world.get_path_name(),
                'actor_count':len(actors),'cameras':cameras,'lights':lights,'stair_bevels':rows,
                'nanite_backdrops':nanite,'master_materials':masters,'dirty_packages':dirty,
                'pie_world_count':len(u.EditorLevelLibrary.get_pie_worlds(False)),
                'map_sha256':hashlib.sha256(native_map.read_bytes()).hexdigest()}
        data['persisted_settings_pass'] = len(rows)==4 and all(abs(r['angle']-47)<.001 and 'INCREASE' in r['behavior'] for r in rows) and len(cameras)==7 and len(nanite)==7 and all(r['enabled'] and r['explicit_tangents'] for r in nanite)
        (log/pathlib.Path(report_name).name).write_text(json.dumps(data,indent=2)+'\n')
        print(json.dumps({'report':report_name,'settings_pass':data['persisted_settings_pass'],'dirty':dirty,'actors':len(actors),'cameras':len(cameras)}))
        return data
    finally:
        world = actors = component = actor = mesh = None


if __name__ == '__main__':
    run()
