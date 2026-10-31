"""Dorado native assets, material restoration and scene assembly (Unreal 5.8.1).

Run inside the editor. Every mutation is confined to ROOT. Existing gameplay
maps and source assets are dependencies, never mutation targets.
"""
import hashlib
import importlib
import json
import math
import pathlib
import re
import time
import unreal as u
import dorado_dimensions as dimensions
importlib.reload(dimensions)

PROJECT = pathlib.Path(u.Paths.project_dir()).resolve()
SOURCE = PROJECT / 'SourceArt/Dorado'
LOG = PROJECT / 'Saved/Logs/dorado-reconstruction-20261030'
ROOT = '/BreakawayCore/Environments/Dorado'
MAP = ROOT + '/Maps/L_Dorado'
TAG = 'DoradoReconstruction'
ASSETS = u.get_editor_subsystem(u.EditorAssetSubsystem)
TOOLS = u.AssetToolsHelpers.get_asset_tools()
ML = u.MaterialEditingLibrary


def write_report(name, data):
    LOG.mkdir(parents=True, exist_ok=True)
    (LOG / (name + '.json')).write_text(json.dumps(data, indent=2), encoding='utf-8')
    print(json.dumps({'report': name, **{k: v for k, v in data.items() if not isinstance(v, (list, dict))}}))


def safe_name(value, prefix=''):
    clean = re.sub('[^A-Za-z0-9_]', '_', value)
    return prefix + clean[:100] + ('_' + hashlib.sha1(value.encode()).hexdigest()[:8] if len(clean) > 100 else '')


def save(asset):
    if not asset.get_path_name().startswith(ROOT + '/'):
        raise RuntimeError('Refusing to save outside owned root: ' + asset.get_path_name())
    if not ASSETS.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError('Failed save: ' + asset.get_path_name())


def create(name, folder, cls, factory):
    path = ROOT + '/' + folder + '/' + name
    existing = ASSETS.load_asset(path) if ASSETS.does_asset_exist(path) else None
    return existing or TOOLS.create_asset(name, ROOT + '/' + folder, cls, factory)


def import_texture(path, srgb, normal=False):
    p = pathlib.Path(path)
    name = safe_name(p.stem, 'T_')
    asset_path = ROOT + '/Textures/' + name
    tex = ASSETS.load_asset(asset_path) if ASSETS.does_asset_exist(asset_path) else None
    digest = hashlib.sha256(p.read_bytes()).hexdigest()
    changed = tex and ASSETS.get_metadata_tag(tex, 'DoradoSourceSHA256') != digest
    if not tex or changed:
        task = u.AssetImportTask()
        task.filename = str(p)
        task.destination_path = ROOT + '/Textures'
        task.destination_name = name
        task.automated = True
        task.replace_existing = bool(changed)
        task.save = False
        TOOLS.import_asset_tasks([task])
        tex = ASSETS.load_asset(asset_path)
        if not tex:
            raise RuntimeError('Texture import failed: ' + str(p))
    tex.set_editor_property('srgb', srgb)
    tex.set_editor_property('compression_settings', u.TextureCompressionSettings.TC_NORMALMAP if normal else u.TextureCompressionSettings.TC_DEFAULT)
    if normal:
        tex.set_editor_property('flip_green_channel', False)
    ASSETS.set_metadata_tag(tex, 'DoradoSourceSHA256', digest)
    save(tex)
    return tex


def node(mat, cls, **props):
    n = ML.create_material_expression(mat, cls)
    for key, value in props.items():
        n.set_editor_property(key, value)
    return n


def connect(a, output, b, input_name):
    if not ML.connect_material_expressions(a, output, b, input_name):
        raise RuntimeError('Material connection failed: ' + input_name)


def scalar(mat, name, value):
    return node(mat, u.MaterialExpressionScalarParameter, parameter_name=name, default_value=value)


def vector(mat, name, value):
    return node(mat, u.MaterialExpressionVectorParameter, parameter_name=name, default_value=u.LinearColor(*value))


def multiply(mat, a, b, a_out='', b_out=''):
    n = node(mat, u.MaterialExpressionMultiply)
    connect(a, a_out, n, 'A')
    connect(b, b_out, n, 'B')
    return n


def build_master(family, defaults, rebuild=False):
    name = 'M_Dorado_' + family
    existing = ASSETS.load_asset(ROOT + '/Materials/' + name) if ASSETS.does_asset_exist(ROOT + '/Materials/' + name) else None
    if existing and not rebuild:
        return existing
    mat = existing or create(name, 'Materials', u.Material, u.MaterialFactoryNew())
    if existing:
        ML.delete_all_material_expressions(mat)
    translucent = family in ('Water', 'Energy', 'Cloud')
    foliage = family == 'Foliage'
    masked = foliage or family == 'MaskedLogo'
    mat.set_editor_property('blend_mode', u.BlendMode.BLEND_TRANSLUCENT if translucent else u.BlendMode.BLEND_MASKED if masked else u.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('two_sided', foliage or translucent)
    if family == 'MaskedLogo':
        mat.set_editor_property('opacity_mask_clip_value', 0.98)
    if family in ('Energy', 'Glow'):
        mat.set_editor_property('shading_model', u.MaterialShadingModel.MSM_UNLIT)
    uv = node(mat, u.MaterialExpressionTextureCoordinate)
    maps = {}
    for slot, default in defaults.items():
        typ = u.MaterialSamplerType.SAMPLERTYPE_NORMAL if slot == 'Normal' else u.MaterialSamplerType.SAMPLERTYPE_COLOR if slot in ('BaseColor', 'Emissive', 'SpecularColor', 'BlendLayer') else u.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
        sampler = node(mat, u.MaterialExpressionTextureSampleParameter2D, parameter_name=slot + 'Map', texture=default, sampler_type=typ)
        scale = vector(mat, slot + 'UVScale', [1, 1, 0, 0])
        xy = node(mat, u.MaterialExpressionComponentMask, r=True, g=True, b=False, a=False)
        connect(scale, '', xy, '')
        scaled_uv = multiply(mat, uv, xy)
        if translucent:
            # Recovered waterfall U follows vertical Z; V spans its width.
            panner = node(mat, u.MaterialExpressionPanner, speed_x=0.12 if family == 'Water' else 0.005 if family == 'Cloud' else 0.0, speed_y=0.0 if family in ('Water', 'Cloud') else 0.045)
            connect(scaled_uv, '', panner, 'Coordinate')
            clock = node(mat, u.MaterialExpressionTime)
            animated_time = multiply(mat, clock, scalar(mat, 'AnimationRate', 1.0))
            connect(animated_time, '', panner, 'Time')
            connect(panner, '', sampler, 'UVs')
        else:
            connect(scaled_uv, '', sampler, 'UVs')
        maps[slot] = sampler
    blend = node(mat, u.MaterialExpressionLinearInterpolate)
    connect(maps['BaseColor'], 'RGB', blend, 'A')
    connect(maps['BlendLayer'], 'RGB', blend, 'B')
    blend_mask = multiply(mat, maps['BlendMask'], scalar(mat, 'BlendLayerStrength', 0.0), 'R')
    connect(blend_mask, '', blend, 'Alpha')
    color = multiply(mat, blend, vector(mat, 'Tint', [1, 1, 1, 1]))
    inlay = node(mat, u.MaterialExpressionLinearInterpolate)
    connect(vector(mat, 'Untinted', [1, 1, 1, 1]), '', inlay, 'A')
    connect(vector(mat, 'InlayTint', [1, 1, 1, 1]), '', inlay, 'B')
    mask = multiply(mat, maps['TintMask'], scalar(mat, 'InlayTintStrength', 0.0), 'R')
    connect(mask, '', inlay, 'Alpha')
    color = multiply(mat, color, inlay)
    ML.connect_material_property(color, '', u.MaterialProperty.MP_BASE_COLOR)
    ML.connect_material_property(maps['Normal'], 'RGB', u.MaterialProperty.MP_NORMAL)
    rough = multiply(mat, maps['Roughness'], scalar(mat, 'RoughnessScale', 0.75), 'R')
    rough_floor = node(mat, u.MaterialExpressionMax)
    connect(rough, '', rough_floor, 'A')
    connect(scalar(mat, 'RoughnessFloor', 0.28 if family == 'Surface' else 0.0), '', rough_floor, 'B')
    ML.connect_material_property(rough_floor, '', u.MaterialProperty.MP_ROUGHNESS)
    metal = multiply(mat, maps['Metallic'], scalar(mat, 'Metallic', 0.0), 'R')
    ML.connect_material_property(metal, '', u.MaterialProperty.MP_METALLIC)
    ML.connect_material_property(scalar(mat, 'Specular', 0.4), '', u.MaterialProperty.MP_SPECULAR)
    emission = color if family in ('Energy', 'Water') else multiply(mat, maps['Emissive'], vector(mat, 'EmissionTint', [1, 1, 1, 1]), 'RGB')
    emission = multiply(mat, emission, scalar(mat, 'EmissionStrength', 0.0))
    ML.connect_material_property(emission, '', u.MaterialProperty.MP_EMISSIVE_COLOR)
    if translucent or masked:
        opacity = multiply(mat, maps['BaseColor'] if family == 'Cloud' else maps['Opacity'], scalar(mat, 'Opacity', 1.0), 'R')
        ML.connect_material_property(opacity, '', u.MaterialProperty.MP_OPACITY if translucent else u.MaterialProperty.MP_OPACITY_MASK)
    ML.layout_material_expressions(mat)
    ML.recompile_material(mat)
    save(mat)
    return mat


def material_family(record):
    shader = record.get('shader', '').lower()
    name = (record.get('name', '') + ' ' + record.get('source_mtl', '')).lower()
    if 'rel_el_breakaway_material' in name:
        return 'MaskedLogo'  # Recovered AlphaTest=.98 on the two logo planes.
    if record.get('id', '').lower() == 'rel_eldorado_buff_spawn_material__2__power':
        return 'Glow'  # Opaque glyph surfaces emit their pad tint independently of daylight.
    if 'mag_ring' in name:
        return 'Energy'
    if any(x in name for x in ['cloud', 'mist', 'spray']):
        return 'Cloud'
    if 'water' in shader or 'waterfall' in name:
        return 'Water'
    if any(x in name for x in ['forcefield', 'energy', 'beam']) or ('glass' in shader):
        return 'Energy'
    if any(x in name for x in ['leaf', 'leaves', 'foliage', 'fern', 'grass', 'treecard', 'vine_card']) or 'vegetation' in shader:
        return 'Foliage'
    return 'Surface'


def source_material_key(path):
    path = str(path).replace('\\', '/').lower()
    if '/objects/' in path:
        path = 'objects/' + path.split('/objects/', 1)[1]
    return path.removesuffix('.mtl')


def build_materials(limit=None, start=0, rebuild_masters=False, used_only=True, only_ids=None):
    # UE 5.8 asserts when an on-screen child MI changes from a lit to an
    # unlit parent's shader map during a batch. Drop the scene's render proxies
    # before editing; assembly/presentation reloads this saved map afterwards.
    current_path = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world().get_path_name()
    if current_path.startswith(MAP + '.'):
        if u.EditorLoadingAndSavingUtils.get_dirty_map_packages():
            raise RuntimeError('Save the owned Dorado map before changing material graphs or parents.')
        u.EditorLoadingAndSavingUtils.new_blank_map(False)
        write_report('material-scene-unload', {'saved_map': MAP, 'reason': 'Avoid live child-MI UniformExpressionCache shader-map invalidation; assembly reloads saved scene.'})
    manifest = json.loads((SOURCE / 'Textures/texture_manifest.json').read_text(encoding='utf-8'))
    default_dir = SOURCE / 'Textures/Defaults'
    defaults = {s: import_texture(default_dir / f'{s}.png', s in ('BaseColor', 'Emissive', 'SpecularColor'), s == 'Normal') for s in ['BaseColor', 'Normal', 'Roughness', 'SpecularColor', 'Metallic', 'Opacity', 'Emissive']}
    defaults['TintMask'] = defaults['Opacity']
    defaults['BlendMask'] = defaults['Opacity']
    defaults['BlendLayer'] = defaults['BaseColor']
    overrides_file = SOURCE / 'Config/material_art_overrides.json'
    art_overrides = json.loads(overrides_file.read_text()).get('overrides', {}) if overrides_file.exists() else {}
    masters = {s: build_master(s, defaults, rebuild_masters) for s in ['Surface', 'Foliage', 'Water', 'Energy', 'Cloud', 'MaskedLogo', 'Glow']}
    previous = LOG / 'material-import.json'
    result = json.loads(previous.read_text()).get('materials', {}) if previous.exists() else {}
    texture_cache = {}
    errors = []
    all_records = manifest['materials']
    geometry_file = SOURCE / 'geometry_manifest.json'
    if used_only and geometry_file.exists():
        geometry = json.loads(geometry_file.read_text())
        used_ids = {slot['material_id'].lower() for a in geometry['assets'] for slot in a.get('materials', [])}
        overrides = {source_material_key(i.get('material_override', '')) for i in geometry['instances']}
        all_records = [r for r in all_records if r['id'].lower() in used_ids or source_material_key(r['source_mtl']) in overrides or 'mag_ring' in r['name'].lower()]
    if only_ids:
        selected = {s.lower() for s in only_ids}
        selected.update(k.lower() for k, v in result.items() if v.get('art_override_applied') and k.lower() not in art_overrides)
        all_records = [r for r in all_records if r['id'].lower() in selected]
    records = all_records[start:start+limit] if limit else all_records[start:]
    for record in records:
        try:
            mid = record['id']
            material = create(safe_name(mid, 'MI_'), 'Materials/Instances', u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
            family = material_family(record)
            ML.clear_all_material_instance_parameters(material)
            ML.set_material_instance_parent(material, masters[family])
            native_maps = dict(record.get('maps', {}))
            if family == 'Cloud':
                if native_maps.get('OriginalDiffuse'):
                    native_maps['BaseColor'] = native_maps['OriginalDiffuse']
                native_maps.pop('Metallic', None)
            for slot, info in native_maps.items():
                if not info or not info.get('path') or slot not in defaults:
                    continue
                key = (info['path'], info.get('srgb', slot in ('BaseColor', 'Emissive', 'SpecularColor')), slot == 'Normal')
                if key not in texture_cache:
                    texture_cache[key] = import_texture(*key)
                ML.set_material_instance_texture_parameter_value(material, slot + 'Map', texture_cache[key])
                uv_scale = info.get('uv_scale', [1, 1])
                ML.set_material_instance_vector_parameter_value(material, slot + 'UVScale', u.LinearColor(uv_scale[0], uv_scale[1], 0, 0))
            tint = record.get('color_factor', [1, 1, 1, 1])
            if len(tint) < 4:
                tint = tint[:3] + [1]
            # Source Illum often leaves Diffuse=0 on textured surfaces whose tint
            # is shader-independent. Preserve texture color; document conversion.
            if max(tint[:3]) == 0 and record.get('maps', {}).get('BaseColor'):
                tint = [1, 1, 1, 1]
            ML.set_material_instance_vector_parameter_value(material, 'Tint', u.LinearColor(*tint))
            emit_color = record.get('emission_color_source', [1, 1, 1])
            ML.set_material_instance_vector_parameter_value(material, 'EmissionTint', u.LinearColor(*emit_color[:3], 1))
            has_rough = bool(record.get('maps', {}).get('Roughness'))
            values = {'Metallic': 1.0 if record.get('maps', {}).get('Metallic') else record.get('metallic', 0), 'RoughnessScale': 1.0 if has_rough else record.get('roughness_default', 0.7), 'Opacity': record.get('opacity', 1.0), 'EmissionStrength': record.get('emission_strength_source', 0.0)}
            if family == 'Energy':
                values['EmissionStrength'] = max(1.0, values['EmissionStrength']) * 3000.0
                values['Metallic'] = 0.0
            elif family == 'Cloud':
                values['EmissionStrength'] = 0.0
                values['Metallic'] = 0.0
                values['Opacity'] = min(values['Opacity'], 0.65)
            for key, value in values.items():
                ML.set_material_instance_scalar_parameter_value(material, key, float(value))
            art = art_overrides.get(mid.lower(), {})
            for key, value in art.get('vectors', {}).items():
                ML.set_material_instance_vector_parameter_value(material, key, u.LinearColor(*value))
            for key, value in art.get('scalars', {}).items():
                ML.set_material_instance_scalar_parameter_value(material, key, float(value))
            for key, value in art.get('textures', {}).items():
                slot = key.removesuffix('Map')
                srgb = slot in ('BaseColor', 'Emissive', 'SpecularColor', 'BlendLayer')
                ML.set_material_instance_texture_parameter_value(material, key, import_texture(value, srgb, slot == 'Normal'))
            save(material)
            result[mid] = {'path': material.get_path_name(), 'family': family, 'source': record.get('source_mtl'), 'name': record.get('name'), 'art_override_applied': bool(art)}
        except Exception as exc:
            errors.append({'id': record.get('id'), 'error': str(exc)})
    write_report('material-import', {'saved': len(result), 'texture_count': len(texture_cache), 'failed': len(errors), 'materials': result, 'errors': errors})
    if errors:
        raise RuntimeError('Material imports failed; inspect material-import.json')
    return result


def preflight():
    dirty = [p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_content_packages()] + [p.get_name() for p in u.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    plan = {'engine': u.SystemLibrary.get_engine_version(), 'root': ROOT, 'map': MAP, 'dirty_packages': dirty, 'source': str(SOURCE), 'map_exists': ASSETS.does_asset_exist(MAP), 'scope': 'Owned environment assets/map only; gameplay maps, assets, input and collision profiles preserved.'}
    write_report('import-preflight', plan)
    return plan


def import_geometry(limit=None, start=0, manifest_name='geometry_manifest.json', force_ids=None):
    manifest = json.loads((SOURCE / manifest_name).read_text(encoding='utf-8'))
    asset_overrides = dimensions.asset_overrides(dimensions.load_config())
    materials_file = LOG / 'material-import.json'
    materials = json.loads(materials_file.read_text())['materials'] if materials_file.exists() else {}
    materials = {k.lower(): v for k, v in materials.items()}
    previous = LOG / 'geometry-import.json'
    rows = json.loads(previous.read_text()).get('meshes', {}) if previous.exists() else {}
    failures = []
    for record in (manifest['assets'][start:start+limit] if limit else manifest['assets'][start:]):
        try:
            record = {**record, **{k: v for k, v in asset_overrides.get(record['id'], {}).items() if k not in ('materials', 'id')}}
            name = safe_name(record['id'])
            path = ROOT + '/Meshes/' + name
            mesh = ASSETS.load_asset(path) if ASSETS.does_asset_exist(path) else None
            digest = hashlib.sha256(pathlib.Path(record['fbx']).read_bytes()).hexdigest()
            force_import = record['id'] in (force_ids or []) or (mesh and rows.get(record['id'], {}).get('fbx_sha256') != digest)
            if not mesh or force_import:
                opts = u.FbxImportUI()
                opts.set_editor_property('import_mesh', True)
                opts.set_editor_property('import_as_skeletal', False)
                opts.set_editor_property('import_materials', False)
                opts.set_editor_property('import_textures', False)
                opts.set_editor_property('automated_import_should_detect_type', False)
                opts.set_editor_property('mesh_type_to_import', u.FBXImportType.FBXIT_STATIC_MESH)
                sm = opts.get_editor_property('static_mesh_import_data')
                for key, value in {'combine_meshes': True, 'auto_generate_collision': False, 'generate_lightmap_u_vs': False, 'convert_scene': True, 'convert_scene_unit': True, 'force_front_x_axis': False, 'transform_vertex_to_absolute': True, 'build_nanite': False}.items():
                    sm.set_editor_property(key, value)
                sm.set_editor_property('normal_import_method', u.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
                task = u.AssetImportTask()
                task.filename = str(record['fbx'])
                task.destination_path = ROOT + '/Meshes'
                task.destination_name = name
                task.options = opts
                task.automated = True
                task.replace_existing = force_import
                task.save = False
                TOOLS.import_asset_tasks([task])
                mesh = ASSETS.load_asset(path)
                if not mesh:
                    raise RuntimeError('No imported mesh at ' + path)
            unresolved = []
            for index, slot in enumerate(mesh.static_materials):
                slot_name = str(slot.material_slot_name)
                match = materials.get(slot_name.lower())
                if not match:
                    source_slot = next((s for s in record.get('materials', []) if s.get('slot') == slot_name or s.get('material_id') == slot_name or s.get('name') == slot_name), None)
                    match = materials.get(source_slot.get('material_id', '').lower()) if source_slot else None
                if match:
                    mesh.set_material(index, ASSETS.load_asset(match['path']))
                else:
                    unresolved.append(slot_name)
            # Exact static triangles retain holes/open edges and stair treads.
            # Decoration collision is disabled per placed instance below.
            body = mesh.get_editor_property('body_setup')
            if body:
                body.set_editor_property('collision_trace_flag', u.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                body.set_editor_property('double_sided_geometry', False)
            save(mesh)
            b = mesh.get_bounding_box()
            rows[record['id']] = {'path': mesh.get_path_name(), 'unresolved_slots': unresolved, 'fbx_sha256': hashlib.sha256(pathlib.Path(record['fbx']).read_bytes()).hexdigest(), 'bounds_min': list(b.min.to_tuple()), 'bounds_max': list(b.max.to_tuple()), 'slots': [str(s.material_slot_name) for s in mesh.static_materials]}
        except Exception as exc:
            failures.append({'id': record['id'], 'error': str(exc)})
    write_report('geometry-import', {'saved': len(rows), 'failed': len(failures), 'meshes': rows, 'errors': failures})
    if failures:
        raise RuntimeError('Geometry import failures; see geometry-import.json')
    return rows


def import_calibration():
    record = json.loads((SOURCE / 'Config/calibration.json').read_text())
    mesh_id = 'SM_Dorado_CentimeterCalibration'
    manifest = {'assets': [{'id': mesh_id, 'fbx': record['fbx'], 'materials': []}]}
    (SOURCE / 'calibration_manifest.json').write_text(json.dumps(manifest, indent=2))
    imported = import_geometry(manifest_name='calibration_manifest.json')[mesh_id]
    expected = record['expected_ue_bounds_cm']
    errors = [abs(a-b) for key, measured in [('min', imported['bounds_min']), ('max', imported['bounds_max'])] for a, b in zip(expected[key], measured)]
    write_report('calibration', {'pass': max(errors) < 0.01, 'maximum_error_cm': max(errors), 'expected': expected, 'actual': {'min': imported['bounds_min'], 'max': imported['bounds_max']}, 'scope': 'Asymmetric imported FBX at identity confirms cm units and reflected Y axis.'})
    if max(errors) >= 0.01:
        raise RuntimeError('FBX unit/axis calibration failed')


def refresh_scene():
    import_geometry()
    # The source floor logo reuses a primitive plane. Give the source mesh its
    # actual default so standalone mesh inspection also has no fallback slot.
    mats = json.loads((LOG / 'material-import.json').read_text())['materials']
    logo = next((v for k, v in mats.items() if k.startswith('rel_el_breakaway_material__')), None)
    plane = ASSETS.load_asset(ROOT + '/Meshes/SM_Dorado_primitive_plane_noproxy')
    if plane and logo:
        plane.set_material(0, ASSETS.load_asset(logo['path']))
        save(plane)
    import_calibration()
    assemble_geometry()
    setup_cameras()


def visual_iteration():
    changes = json.loads((LOG / 'geometry-shader-filter-changes.json').read_text())['changes']
    changed_ids = [c['id'] for c in changes] + ['SM_Dorado_rel_ed_ring_a_group']
    import_geometry(force_ids=changed_ids)
    import_geometry(manifest_name='energy_manifest.json')
    build_materials(rebuild_masters=True)
    assemble_geometry()
    assemble_geometry(manifest_name='energy_manifest.json')
    setup_cameras()


def prepare_map():
    levels = u.get_editor_subsystem(u.LevelEditorSubsystem)
    current = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if ASSETS.does_asset_exist(MAP):
        if not current.get_path_name().startswith(MAP + '.'):
            if u.EditorLoadingAndSavingUtils.get_dirty_map_packages():
                raise RuntimeError('Unsaved map conflict; refusing to change levels')
            if not levels.load_level(MAP):
                raise RuntimeError('Cannot load owned map')
        return
    if u.EditorLoadingAndSavingUtils.get_dirty_map_packages():
        raise RuntimeError('Unsaved map conflict before cloning baseline')
    if not current.get_path_name().startswith('/BreakawayCore/Maps/L_BW_Dorado.'):
        if not levels.load_level('/BreakawayCore/Maps/L_BW_Dorado'):
            raise RuntimeError('Cannot load baseline map')
    current = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if not u.EditorLoadingAndSavingUtils.save_map(current, MAP):
        raise RuntimeError('Native map copy failed')
    # save_map writes a copy without switching the live editor world.
    if not levels.load_level(MAP):
        raise RuntimeError('Could not open newly saved map copy')
    current = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if not current.get_path_name().startswith(MAP + '.'):
        raise RuntimeError('Owned world verification failed; refusing mutation')
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    removal_classes = {'StaticMeshActor', 'ExponentialHeightFog', 'VolumetricCloud', 'SkyAtmosphere', 'DirectionalLight', 'SkyLight', 'AtmosphericFog', 'SphereReflectionCapture', 'B_Tool_RampMakerControl_BGM_C'}
    removed = []
    for actor in actors.get_all_level_actors():
        if actor.get_class().get_name() in removal_classes:
            removed.append(actor.get_actor_label())
            if not actors.destroy_actor(actor):
                raise RuntimeError('Could not remove copied placeholder ' + actor.get_actor_label())
    levels.save_current_level()
    write_report('map-copy', {'map': MAP, 'source_map': '/BreakawayCore/Maps/L_BW_Dorado', 'removed_copied_placeholders': removed, 'count': len(removed), 'gameplay': 'Existing Lyra world settings, experience, spawns, goals, base zones and navigation retained in the new copy.'})


def owned_actor(label, cls, folder):
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    existing = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == label and TAG in [str(t) for t in a.tags]), None)
    actor = existing or actors.spawn_actor_from_class(cls, u.Vector(0, 0, 0))
    actor.set_actor_label(label)
    actor.set_editor_property('tags', [u.Name(TAG)])
    actor.set_folder_path(u.Name('Dorado/' + folder))
    return actor


def assemble_geometry(manifest_name='geometry_manifest.json'):
    prepare_map()
    manifest = json.loads((SOURCE / manifest_name).read_text(encoding='utf-8'))
    report_name = {'geometry_manifest.json': 'scene-assembly', 'energy_manifest.json': 'scene-assembly-energy'}.get(manifest_name)
    if not report_name:
        report_name = 'scene-assembly-' + safe_name(pathlib.Path(manifest_name).stem) + '-' + hashlib.sha1(manifest_name.encode()).hexdigest()[:8]
    previous_file = LOG / (report_name + '.json')
    previous = json.loads(previous_file.read_text()) if previous_file.exists() else {}
    previous_labels = set()
    if previous.get('map') == MAP and previous.get('manifest') == manifest_name:
        previous_labels.update(row['label'] for row in previous.get('actors', []) if row.get('label'))
        previous_labels.update(previous.get('reconciliation_pending_labels', []))
    current_labels = {safe_name(record['name'] + '_' + record['id'].strip('{}')[:8], 'D_') for record in manifest['instances']}
    imported = json.loads((LOG / 'geometry-import.json').read_text())['meshes']
    material_records = json.loads((SOURCE / 'Textures/texture_manifest.json').read_text())['materials']
    materials = json.loads((LOG / 'material-import.json').read_text())['materials']
    dimension_config = dimensions.load_config()
    dimensions.validate(dimension_config)
    art_file = SOURCE / 'Config/material_art_overrides.json'
    variants = json.loads(art_file.read_text()).get('instance_material_variants', []) if art_file.exists() else []
    failures, placed = [], []
    for record in manifest['instances']:
        try:
            if record['asset_id'] not in imported:
                raise RuntimeError('Asset not imported: ' + record['asset_id'])
            label = safe_name(record['name'] + '_' + record['id'].strip('{}')[:8], 'D_')
            actor = owned_actor(label, u.StaticMeshActor, 'Architecture/' + record.get('layer', 'Scene'))
            actor.static_mesh_component.set_static_mesh(ASSETS.load_asset(imported[record['asset_id']]['path']))
            comp = actor.static_mesh_component
            for slot_index in range(len(comp.get_editor_property('override_materials'))):
                comp.set_material(slot_index, None)
            location, rotation, scale = dimensions.apply(record, dimension_config)
            actor.set_actor_location(u.Vector(*location), False, False)
            actor.set_actor_rotation(u.Quat(*rotation).rotator(), False)
            actor.set_actor_scale3d(u.Vector(*scale))
            collision = record.get('collision_role', 'none').lower()
            enabled = collision not in ('none', 'decorative', 'scenery', 'decoration', 'visual')
            actor.static_mesh_component.set_collision_enabled(u.CollisionEnabled.QUERY_AND_PHYSICS if enabled else u.CollisionEnabled.NO_COLLISION)
            if enabled:
                actor.static_mesh_component.set_collision_profile_name('BlockAll')
            else:
                actor.static_mesh_component.set_collision_profile_name('NoCollision')
            if collision == 'source_collision':
                actor.set_actor_hidden_in_game(True)
                actor.set_is_temporarily_hidden_in_editor(True)
            # Low-relief decals and central ornament must not alter ball contact.
            # The two main_floor_*_tc meshes are planar walking fill, not decals.
            # Their measured <0.002cm world thickness closes actual slab gaps.
            # center_point and groud_design contain visible shallow walking fill.
            # Source surface-ray audits prove gaps if either family is excluded.
            if any(s in record['asset_id'].lower() for s in ['relic_deco', 'relic_snake', 'primitive_plane', 'banner01']):
                actor.static_mesh_component.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
                actor.static_mesh_component.set_collision_profile_name('NoCollision')
                enabled = False
            actor.static_mesh_component.set_editor_property('mobility', u.ComponentMobility.STATIC)
            # 45.55-degree tread bevels exceed this project's 44.765-degree
            # character threshold and can trap the capsule at Y=506. Keep the
            # visible triangle mesh for ball physics; only this stair surface's
            # character walkability is relaxed, never the movement component.
            if record['asset_id'] == 'SM_Dorado_rel_eldorado_mid_stair_c_group':
                comp.set_walkable_slope_override(u.WalkableSlopeOverride(
                    walkable_slope_behavior=u.WalkableSlopeBehavior.WALKABLE_SLOPE_INCREASE,
                    walkable_slope_angle=47.0))
            if manifest_name == 'energy_manifest.json' or any(x in record['asset_id'].lower() for x in ['cloud', 'mist', 'spray']):
                actor.static_mesh_component.set_editor_property('cast_shadow', False)
            override = record.get('material_override')
            if override:
                choices = [m for m in material_records if source_material_key(m['source_mtl']) == source_material_key(override)]
                source_mesh = next(a for a in manifest['assets'] if a['id'] == record['asset_id'])
                for slot in source_mesh.get('materials', []):
                    choice = next((m for m in choices if m['submaterial_index'] == slot.get('source_index', 0)), None)
                    if not choice and len(choices) == 1:
                        choice = choices[0]
                    if choice and choice['id'] in materials:
                        native_index = comp.static_mesh.get_material_index(u.Name(slot['name']))
                        if native_index < 0:
                            raise RuntimeError('Missing stable native slot: ' + slot['name'])
                        comp.set_material(native_index, ASSETS.load_asset(materials[choice['id']]['path']))
            for variant in [v for v in variants if v['instance_id'] == record['id']]:
                mid = variant['source_material_id']
                parent = next(v for k, v in materials.items() if k.lower() == mid.lower())
                instance = create(safe_name(mid + '__' + variant['variant_suffix'], 'MI_'), 'Materials/Instances', u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
                ML.clear_all_material_instance_parameters(instance)
                ML.set_material_instance_parent(instance, ASSETS.load_asset(parent['path']))
                for key, value in variant.get('vectors', {}).items():
                    ML.set_material_instance_vector_parameter_value(instance, key, u.LinearColor(*value))
                for key, value in variant.get('scalars', {}).items():
                    ML.set_material_instance_scalar_parameter_value(instance, key, float(value))
                save(instance)
                source_mesh = next(a for a in manifest['assets'] if a['id'] == record['asset_id'])
                for slot in source_mesh.get('materials', []):
                    if slot['material_id'].lower() == mid.lower():
                        native_index = comp.static_mesh.get_material_index(u.Name(slot['name']))
                        if native_index < 0:
                            raise RuntimeError('Missing stable native slot: ' + slot['name'])
                        comp.set_material(native_index, instance)
            placed.append({'label': label, 'source_id': record['id'], 'collision': enabled})
        except Exception as exc:
            failures.append({'id': record.get('id'), 'error': str(exc)})
    reconciliation = {'status': 'skipped_placement_failure' if failures else 'complete', 'candidates': sorted(previous_labels - current_labels), 'deleted': [], 'preserved_unowned': [], 'already_absent': []}
    pending_labels = previous_labels | current_labels if failures else set()
    # A replacement must be completely placed before removing earlier actors.
    # Prior labels are scoped to this exact manifest/map, never the whole tag.
    if not failures:
        actors = u.get_editor_subsystem(u.EditorActorSubsystem)
        existing_by_label = {}
        for actor in actors.get_all_level_actors():
            existing_by_label.setdefault(actor.get_actor_label(), []).append(actor)
        for label in reconciliation['candidates']:
            matches = existing_by_label.get(label, [])
            if not matches:
                reconciliation['already_absent'].append(label)
            for actor in matches:
                if TAG not in [str(tag) for tag in actor.tags]:
                    reconciliation['preserved_unowned'].append(label)
                    continue
                try:
                    if not actors.destroy_actor(actor):
                        raise RuntimeError('Editor refused to destroy stale owned actor')
                    reconciliation['deleted'].append(label)
                except Exception as exc:
                    pending_labels.add(label)
                    failures.append({'label': label, 'stage': 'stale_actor_reconciliation', 'error': str(exc)})
        if failures:
            reconciliation['status'] = 'incomplete'
    if not u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level():
        pending_labels.update(previous_labels | current_labels)
        failures.append({'stage': 'save_level', 'error': 'Could not save owned map'})
        reconciliation['status'] = 'save_failed'
    write_report(report_name, {'map': MAP, 'manifest': manifest_name, 'placed': len(placed), 'failed': len(failures), 'actors': placed, 'errors': failures, 'reconciliation': reconciliation, 'reconciliation_pending_labels': sorted(pending_labels)})
    if failures:
        raise RuntimeError('Scene assembly incomplete; see ' + report_name + '.json')


def setup_presentation():
    prepare_map()
    sun = owned_actor('D_Sun', u.DirectionalLight, 'Presentation/Lighting')
    sun.set_actor_rotation(u.Rotator(pitch=-35, yaw=-20, roll=0), False)
    light = sun.get_component_by_class(u.DirectionalLightComponent)
    light.set_editor_property('mobility', u.ComponentMobility.MOVABLE)
    light.set_editor_property('intensity', 85000.0)
    light.set_editor_property('light_color', u.Color(r=255, g=244, b=228, a=255))
    light.set_editor_property('light_source_angle', 1.0)
    light.set_editor_property('atmosphere_sun_light', True)
    sky = owned_actor('D_Sky', u.SkyLight, 'Presentation/Lighting')
    sc = sky.get_component_by_class(u.SkyLightComponent)
    sc.set_editor_property('mobility', u.ComponentMobility.MOVABLE)
    sc.set_editor_property('real_time_capture', True)
    sc.set_editor_property('intensity', 2.5)
    sc.set_editor_property('light_color', u.Color(r=160, g=200, b=255, a=255))
    atmosphere = owned_actor('D_Atmosphere', u.SkyAtmosphere, 'Presentation/Atmosphere')
    ac = atmosphere.get_component_by_class(u.SkyAtmosphereComponent)
    ac.set_editor_property('rayleigh_scattering_scale', 0.5)
    ac.set_editor_property('mie_scattering_scale', 0.2)
    fog = owned_actor('D_GorgeMist', u.ExponentialHeightFog, 'Presentation/Atmosphere')
    fog.set_actor_location(u.Vector(0, 0, 0), False, False)
    fc = fog.get_component_by_class(u.ExponentialHeightFogComponent)
    fc.set_editor_property('fog_density', 0.003)
    fc.set_editor_property('fog_height_falloff', 0.035)
    fc.set_editor_property('start_distance', 1800.0)
    fc.set_editor_property('fog_inscattering_luminance', u.LinearColor(0.7, 0.76, 0.8, 1))
    fc.set_editor_property('directional_inscattering_exponent', 4.0)
    fc.set_editor_property('directional_inscattering_start_distance', 1800.0)
    fc.set_editor_property('directional_inscattering_luminance', u.LinearColor(0.045, 0.018, 0.015, 1))
    post = owned_actor('D_Exposure', u.PostProcessVolume, 'Presentation/Lighting')
    post.set_editor_property('unbound', True)
    settings = post.get_editor_property('settings')
    for prop, value in {'override_auto_exposure_min_brightness': True, 'override_auto_exposure_max_brightness': True, 'auto_exposure_min_brightness': 11.0, 'auto_exposure_max_brightness': 11.0, 'override_bloom_intensity': True, 'bloom_intensity': 0.35, 'override_white_temp': True, 'white_temp': 6500.0}.items():
        settings.set_editor_property(prop, value)
    post.set_editor_property('settings', settings)
    for label, location in [('ANCHOR_ArenaCenter', [0, 0, 0]), ('ANCHOR_End_Sun_Inferred', [0, 4000, 230]), ('ANCHOR_End_Shrine_Inferred', [0, -4000, 230])]:
        a = owned_actor(label, u.TargetPoint, 'IntegrationAnchors')
        a.set_actor_location(u.Vector(*location), False, False)
    u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    write_report('presentation', {'sun_lux': 85000, 'sun_color_rgb': [255,244,228], 'sun_rotation_pitch_yaw_roll': [-35, -20, 0], 'exposure_EV100': 11.0, 'sky_intensity': 2.5, 'sky_color_rgb': [160,200,255], 'bloom': 0.35, 'white_balance_K': 6500, 'fog_density': 0.003, 'fog_falloff': 0.035, 'directional_fog_linear': [0.045,0.018,0.015], 'directional_fog_exponent': 4, 'rayleigh_scale': 0.5, 'mie_scale': 0.2, 'map': MAP, 'scope': 'Warm daylight and cool base atmosphere; coherent image-inferred lighting across all cameras. Color uses explicit RGB keywords to avoid native BGRA constructor ordering.'})


def setup_cameras():
    config = json.loads((SOURCE / 'dimensions.json').read_text(encoding='utf-8'))
    rows = config['cameras'] + config.get('supplementary_views', [])
    saved = []
    for record in rows:
        camera = owned_actor(record['name'], u.CameraActor, 'Presentation/ComparisonCameras')
        camera.set_actor_location(u.Vector(*record['location_cm']), False, False)
        if 'look_at_cm' in record:
            rotation = u.MathLibrary.find_look_at_rotation(u.Vector(*record['location_cm']), u.Vector(*record['look_at_cm']))
        else:
            pitch, yaw, roll = record['rotation_pitch_yaw_roll_deg']
            rotation = u.Rotator(pitch=pitch, yaw=yaw, roll=roll)
        camera.set_actor_rotation(rotation, False)
        comp = camera.get_component_by_class(u.CameraComponent)
        comp.set_editor_property('field_of_view', record.get('horizontal_fov_degrees', 75))
        comp.set_editor_property('aspect_ratio', record.get('aspect_ratio', record.get('image_width', 1920) / record.get('image_height', 1080)))
        comp.set_editor_property('constrain_aspect_ratio', True)
        if record.get('projection') == 'orthographic':
            comp.set_editor_property('projection_mode', u.CameraProjectionMode.ORTHOGRAPHIC)
            comp.set_editor_property('ortho_width', record.get('ortho_width_cm', 11000))
        else:
            comp.set_editor_property('projection_mode', u.CameraProjectionMode.PERSPECTIVE)
        saved.append({'name': record['name'], 'location': list(camera.get_actor_location().to_tuple()), 'rotation_pitch_yaw_roll': [rotation.pitch, rotation.yaw, rotation.roll], 'fov': comp.field_of_view, 'aspect': comp.aspect_ratio})
    u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    write_report('cameras', {'count': len(saved), 'cameras': saved})


def remove_copied_placeholders():
    world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_editor_world()
    if not world.get_path_name().startswith(MAP + '.'):
        raise RuntimeError('Wrong map for owned cleanup')
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    classes = {'StaticMeshActor', 'ExponentialHeightFog', 'VolumetricCloud', 'SkyAtmosphere', 'DirectionalLight', 'SkyLight', 'AtmosphericFog', 'SphereReflectionCapture', 'B_Tool_RampMakerControl_BGM_C'}
    removed = []
    for actor in actors.get_all_level_actors():
        if TAG not in [str(t) for t in actor.tags] and actor.get_class().get_name() in classes:
            removed.append(actor.get_actor_label())
            actors.destroy_actor(actor)
    u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    setup_presentation()
    write_report('copied-placeholder-cleanup', {'removed': len(removed), 'labels': removed, 'map': MAP})


def structural_validate():
    import runpy
    validator = runpy.run_path(str(SOURCE / 'Scripts/validate_dorado.py'))
    validator['run']()


def update_visual_materials():
    build_materials(rebuild_masters=True)
    assemble_geometry()
    assemble_geometry(manifest_name='energy_manifest.json')
    setup_glow_points()
    apply_backdrop_budget(shadow=True)
    setup_cameras()
    setup_presentation()


def rebuild_environment():
    """Idempotent native rebuild from already exported source manifests."""
    preflight()
    build_materials(rebuild_masters=True)
    import_geometry()
    import_geometry(manifest_name='energy_manifest.json')
    import_calibration()
    assemble_geometry()
    assemble_geometry(manifest_name='energy_manifest.json')
    setup_glow_points()
    apply_backdrop_budget(shadow=True)
    setup_presentation()
    setup_cameras()
    configure_nanite_all()
    structural_validate()


def configure_traversal_pie():
    settings = u.get_default_object(u.load_class(None, '/Script/UnrealEd.LevelEditorPlaySettings'))
    path = LOG / 'pie-settings-baseline.json'
    keys = ['PlayNumberOfClients', 'RunUnderOneProcess', 'AdditionalServerGameOptions', 'bLaunchSeparateServer']
    if not path.exists():
        write_report('pie-settings-baseline', {k: settings.get_editor_property(k) for k in keys})
    settings.set_editor_property('PlayNumberOfClients', 1)
    settings.set_editor_property('RunUnderOneProcess', True)
    settings.set_editor_property('bLaunchSeparateServer', False)
    settings.set_editor_property('AdditionalServerGameOptions', '?Experience=B_BW_Experience_CaptureTheRelic?SkipHeroSelection=1?NumBots=0?Hero=Argus?PrematchDuration=1?WarmupDuration=1?RoundDuration=1800?PointsToWin=9')
    write_report('pie-settings-configured', {'scope': 'Existing PIE_ListenServer, one local authority world, zero bots.', **{k: settings.get_editor_property(k) for k in keys}})


def restore_traversal_pie():
    settings = u.get_default_object(u.load_class(None, '/Script/UnrealEd.LevelEditorPlaySettings'))
    baseline = json.loads((LOG / 'pie-settings-baseline.json').read_text())
    for key, value in baseline.items():
        settings.set_editor_property(key, value)
    write_report('pie-settings-restored', {'matches': all(settings.get_editor_property(k) == v for k, v in baseline.items()), 'baseline': baseline, 'scope': 'In-memory restoration only. After normal editor shutdown, run Scripts/Restore-DoradoPIESettings.ps1 to restore generated user INI and verify on reopen; Unreal Python does not expose SaveConfig on these settings.'})


def apply_backdrop_budget(pie=False, shadow=False):
    scope = json.loads((SOURCE / 'Config/backdrop_budget.json').read_text())
    if pie:
        worlds = u.EditorLevelLibrary.get_pie_worlds(False)
        if len(worlds) != 1:
            raise RuntimeError('Exactly one owned PIE world required')
        world = worlds[0]
        actors = u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
    else:
        prepare_map()
        actors = u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors()
    changed = []
    for actor in actors:
        comp = actor.get_component_by_class(u.StaticMeshComponent)
        mesh = comp.static_mesh if comp else None
        if not mesh or not mesh.get_path_name().startswith(ROOT + '/'):
            continue
        if mesh.get_name() not in scope['gi_off_asset_ids']:
            continue
        before = {'gi': comp.get_editor_property('affect_dynamic_indirect_lighting'), 'cast_shadow': comp.get_editor_property('cast_shadow')}
        comp.set_affect_dynamic_indirect_lighting(False)
        if shadow and mesh.get_name() in scope['cast_shadow_off_asset_ids']:
            comp.set_cast_shadow(False)
        changed.append({'actor': actor.get_path_name(), 'mesh': mesh.get_name(), 'before': before, 'after': {'gi': comp.get_editor_property('affect_dynamic_indirect_lighting'), 'cast_shadow': comp.get_editor_property('cast_shadow')}})
    if not pie:
        u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    write_report('perf-backdrop-' + ('pie' if pie else 'saved') + ('-shadow' if shadow else '-gi'), {'changed': len(changed), 'actors': changed})


def configure_nanite(index):
    import configure_nanite_backdrop
    configure_nanite_backdrop.configure_one(index)


def configure_nanite_all():
    for index in range(7):
        configure_nanite(index)


def setup_glow_points():
    prepare_map()
    material = create('MI_Dorado_VioletCore', 'Materials/Instances', u.MaterialInstanceConstant, u.MaterialInstanceConstantFactoryNew())
    ML.clear_all_material_instance_parameters(material)
    ML.set_material_instance_parent(material, ASSETS.load_asset(ROOT + '/Materials/M_Dorado_Energy'))
    ML.set_material_instance_vector_parameter_value(material, 'Tint', u.LinearColor(0.4, 0.12, 1.0, 1))
    for key, value in {'EmissionStrength': 60000, 'Opacity': 1, 'AnimationRate': 0}.items():
        ML.set_material_instance_scalar_parameter_value(material, key, value)
    save(material)
    config = json.loads((SOURCE / 'Config/inferred_glow_points.json').read_text())
    for point in config['points']:
        actor = owned_actor(point['name'], u.StaticMeshActor, 'Effects/VioletCores')
        comp = actor.static_mesh_component
        comp.set_static_mesh(ASSETS.load_asset(point['primitive']))
        comp.set_material(0, material)
        comp.set_collision_profile_name('NoCollision')
        comp.set_cast_shadow(False)
        comp.set_affect_dynamic_indirect_lighting(False)
        actor.set_actor_location(u.Vector(*point['location_cm']), False, False)
        pitch, yaw, roll = point['rotation_pitch_yaw_roll_deg']
        actor.set_actor_rotation(u.Rotator(pitch=pitch, yaw=yaw, roll=roll), False)
        actor.set_actor_scale3d(u.Vector(*point['scale_xyz']))
    u.get_editor_subsystem(u.LevelEditorSubsystem).save_current_level()
    write_report('glow-points', {'count': len(config['points']), 'source': str(SOURCE / 'Config/inferred_glow_points.json')})


def finish_visual_iteration():
    overrides = json.loads((SOURCE / 'Config/material_art_overrides.json').read_text())['overrides']
    build_materials(only_ids=list(overrides))
    assemble_geometry()
    assemble_geometry(manifest_name='energy_manifest.json')
    setup_glow_points()
    apply_backdrop_budget(shadow=True)
    setup_presentation()
    setup_cameras()


def refine_presentation():
    build_materials(limit=1, rebuild_masters=True)
    setup_presentation()


def capture(camera_name, iteration='initial'):
    config = json.loads((SOURCE / 'dimensions.json').read_text(encoding='utf-8'))
    specs = config['cameras'] + config.get('supplementary_views', [])
    record = next(s for s in specs if s['name'] == camera_name)
    actors = u.get_editor_subsystem(u.EditorActorSubsystem)
    camera = next(a for a in actors.get_all_level_actors() if a.get_actor_label() == camera_name)
    out = LOG / 'Renders' / iteration
    out.mkdir(parents=True, exist_ok=True)
    path = str(out / (camera_name + '.png'))
    u.get_editor_subsystem(u.UnrealEditorSubsystem).set_level_viewport_camera_info(camera.get_actor_location(), camera.get_actor_rotation())
    # UE5.8's True branch removes a named override and asserts if absent.
    # Establish then remove our own override; preserve the viewport preference.
    level_subsystem = u.get_editor_subsystem(u.LevelEditorSubsystem)
    level_subsystem.editor_set_viewport_realtime(False)
    level_subsystem.editor_set_viewport_realtime(True)
    u.AutomationLibrary.take_high_res_screenshot(record.get('image_width', 1920), record.get('image_height', 1080), path, camera=camera, delay=1.0)
    print('CAPTURE_REQUESTED', path)


if __name__ == '__main__':
    preflight()
