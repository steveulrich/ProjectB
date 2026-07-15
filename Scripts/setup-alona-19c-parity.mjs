/**
 * Step 19c — Enforce and probe Alona sheet-parity defaults.
 *
 * Requires Unreal Editor open with the ue-mcp bridge after HeroAlonaRuntime
 * has been rebuilt with the Step 19c Radiance properties.
 *
 * Usage:
 *   node Scripts/setup-alona-19c-parity.mjs
 *   node Scripts/setup-alona-19c-parity.mjs --probe-only
 */
import { writeFileSync } from 'node:fs';
import { createMcpClient, text, writeLog } from './lib/bway-widget-setup.mjs';

const PROBE_ONLY = process.argv.includes('--probe-only');

const PATHS = {
  heroData: '/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona',
  abilitySet: '/Hero_Alona/Characters/Heroes/Alona/DA_BW_AbilitySet_Alona',
  primary: '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_PrimaryAttack',
  sunsGrace: '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_SunsGrace',
  radiance: '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_Radiance',
  sunBurst: '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_Sunburst',
  blessing: '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_Blessing',
  placeAbility: '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_BuildablePlacement',
  shrineBP: '/Hero_Alona/Characters/Heroes/Alona/Buildables/BP_BW_Buildable_SunShrine',
  shrineDA: '/Hero_Alona/Characters/Heroes/Alona/Buildables/DA_BW_BuildableData_SunShrine',
  shrineMesh: '/Hero_Alona/Characters/Heroes/Alona/Buildables/SM_BW_alona_buildable',
};

const NATIVE = {
  primary: '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaPrimary',
  sunsGrace: '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaSunsGrace',
  radiance: '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaRadiance',
  sunBurst: '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaSunBurst',
  blessing: '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaBlessing',
  placeAbility: '/Script/BreakawayCoreRuntime.BwayGameplayAbility_PlaceBuildable',
  shrine: '/Script/HeroAlonaRuntime.BwayAlonaSunShrineBuildable',
  cooldownSunsGrace: '/Script/HeroAlonaRuntime.GE_Bway_Cooldown_AlonaSunsGrace',
  cooldownRadiance: '/Script/HeroAlonaRuntime.GE_Bway_Cooldown_AlonaRadiance',
  cooldownSunBurst: '/Script/HeroAlonaRuntime.GE_Bway_Cooldown_AlonaSunBurst',
  cooldownBlessing: '/Script/HeroAlonaRuntime.GE_Bway_Cooldown_AlonaBlessing',
};

const PY_HELPERS = `
import unreal
import json
import math
import re

def exists(path):
    return unreal.EditorAssetLibrary.does_asset_exist(path)

def save(asset):
    if asset:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)

def object_path(value):
    if not value:
        return None
    try:
        return value.get_path_name()
    except Exception:
        return str(value)

def load_bp_class(path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if cls:
        return cls
    return unreal.load_class(None, path + '_C')

def inherits_native(path, native_path):
    if not exists(path):
        return False
    native = unreal.load_class(None, native_path)
    if not native:
        return False
    native_name = native.get_name()
    try:
        asset_data = unreal.EditorAssetLibrary.find_asset_data(path)
        for tag in ('NativeParentClass', 'ParentClass'):
            try:
                tag_value = str(asset_data.get_tag_value(tag) or '')
            except Exception:
                try:
                    tag_value = str(asset_data.get_tag_value(unreal.Name(tag)) or '')
                except Exception:
                    tag_value = ''
            if native_path in tag_value or native_name in tag_value:
                return True
    except Exception:
        pass
    return False

def parent_class_path(path):
    if not exists(path):
        return None
    try:
        asset_data = unreal.EditorAssetLibrary.find_asset_data(path)
        for tag in ('NativeParentClass', 'ParentClass'):
            try:
                tag_value = str(asset_data.get_tag_value(tag) or '')
            except Exception:
                try:
                    tag_value = str(asset_data.get_tag_value(unreal.Name(tag)) or '')
                except Exception:
                    tag_value = ''
            if tag_value:
                # Normalise "/Script/CoreUObject.Class'/Script/Module.Class'" → "/Script/Module.Class"
                marker = "'/Script/"
                if marker in tag_value:
                    start = tag_value.index(marker) + 1
                    end = tag_value.find("'", start)
                    return tag_value[start:end] if end > start else tag_value[start:]
                if '/Script/' in tag_value:
                    start = tag_value.index('/Script/')
                    end_q = tag_value.find("'", start)
                    return tag_value[start:end_q] if end_q > start else tag_value[start:]
                return tag_value
    except Exception as error:
        return 'error:' + str(error)
    try:
        bp = unreal.load_asset(path)
        return object_path(bp.generated_class()) if bp else None
    except Exception as error:
        return 'error:' + str(error)

def tag_name(tag):
    if not tag:
        return ''
    try:
        return str(tag.get_editor_property('TagName'))
    except Exception:
        pass
    try:
        exported = tag.export_text() or ''
        match = re.search(r'TagName=\\"([^\\"]+)\\"', exported)
        return match.group(1) if match else str(tag)
    except Exception:
        return str(tag)

def simple_value(value):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    return object_path(value)

def read_properties(cdo, names):
    values = {}
    for name in names:
        try:
            values[name] = simple_value(cdo.get_editor_property(name))
        except Exception as error:
            values[name] = 'error:' + str(error)
    return values

def duration_seconds(effect_class):
    if not effect_class:
        return None
    try:
        effect_cdo = unreal.get_default_object(effect_class)
        magnitude = effect_cdo.get_editor_property('DurationMagnitude')
    except Exception:
        return None

    for magnitude_name in ('ScalableFloatMagnitude', 'scalable_float_magnitude'):
        try:
            scalable = magnitude.get_editor_property(magnitude_name)
            for value_name in ('Value', 'value'):
                try:
                    return float(scalable.get_editor_property(value_name))
                except Exception:
                    pass
        except Exception:
            pass

    try:
        exported = magnitude.export_text() or ''
    except Exception:
        exported = str(magnitude)
    match = re.search(r'Value=([-+0-9.eE]+)', exported)
    return float(match.group(1)) if match else exported

def ability_summary(path, property_names, native_path=None):
    if not exists(path):
        return {'exists': False, 'path': path}
    bp = unreal.load_asset(path)
    generated = bp.generated_class() if bp else None
    cdo = unreal.get_default_object(generated) if generated else None
    if not cdo:
        return {'exists': True, 'path': path, 'error': 'missing CDO'}
    cooldown_class = None
    try:
        cooldown_class = cdo.get_editor_property('CooldownGameplayEffectClass')
    except Exception:
        pass
    return {
        'exists': True,
        'path': path,
        'parent': parent_class_path(path),
        'inheritsNative': inherits_native(path, native_path) if native_path else None,
        'properties': read_properties(cdo, property_names),
        'cooldownClass': object_path(cooldown_class),
        'cooldownSeconds': duration_seconds(cooldown_class),
    }

def ability_set_summary(path):
    if not exists(path):
        return {'exists': False, 'path': path}
    ability_set = unreal.load_asset(path)
    grants = []
    for grant in ability_set.get_editor_property('GrantedGameplayAbilities') or []:
        ability = grant.get_editor_property('Ability')
        input_tag = grant.get_editor_property('InputTag')
        grants.append({
            'ability': object_path(ability),
            'inputTag': tag_name(input_tag),
        })
    return {'exists': True, 'path': path, 'grants': grants}

def hero_summary(path):
    if not exists(path):
        return {'exists': False, 'path': path}
    hero = unreal.load_asset(path)
    output = {'exists': True, 'path': path}
    try:
        output['DisplayName'] = str(hero.get_editor_property('DisplayName'))
        stats = hero.get_editor_property('HeroStats')
        output['MaxHealth'] = float(stats.get_editor_property('MaxHealth'))
        output['BaseDamage'] = float(stats.get_editor_property('BaseDamage'))
        output['Armor'] = float(stats.get_editor_property('Armor'))
        output['MoveSpeed'] = float(stats.get_editor_property('MoveSpeed'))
        output['BuildableDataAsset'] = object_path(hero.get_editor_property('BuildableDataAsset'))
    except Exception as error:
        output['error'] = str(error)
    return output

def shrine_summary(path, native_path=None):
    if not exists(path):
        return {'exists': False, 'path': path}
    bp = unreal.load_asset(path)
    generated = bp.generated_class() if bp else None
    cdo = unreal.get_default_object(generated) if generated else None
    return {
        'exists': True,
        'path': path,
        'parent': parent_class_path(path),
        'inheritsNative': inherits_native(path, native_path) if native_path else None,
        'properties': read_properties(
            cdo,
            ['MaxHealth', 'HealRadius', 'HealPerSecond', 'BuildTime', 'bPersistsBetweenRounds'])
            if cdo else {},
    }

def shrine_da_summary(path):
    if not exists(path):
        return {'exists': False, 'path': path}
    data = unreal.load_asset(path)
    return {
        'exists': True,
        'path': path,
        'BuildableActorClass': object_path(data.get_editor_property('BuildableActorClass')),
        'Cost': float(data.get_editor_property('Cost')),
        'MaxActiveBuildablesPerPlayer': int(data.get_editor_property('MaxActiveBuildablesPerPlayer')),
        'PrimaryBuildableMesh': object_path(data.get_editor_property('PrimaryBuildableMesh')),
    }

def approximately_equal(current, expected):
    if isinstance(expected, float):
        try:
            return math.isclose(float(current), expected, rel_tol=1e-6, abs_tol=1e-6)
        except Exception:
            return False
    return current == expected

def set_value_if_needed(cdo, property_name, expected, changes):
    current = cdo.get_editor_property(property_name)
    if approximately_equal(current, expected):
        return False
    cdo.set_editor_property(property_name, expected)
    changes.append({'property': property_name, 'from': simple_value(current), 'to': simple_value(expected)})
    return True

def configure_ability(key, path, parent_path, values, cooldown_path=None):
    result = {'ability': key, 'path': path, 'changes': []}
    if not exists(path):
        result['error'] = 'missing Blueprint'
        return result

    bp = unreal.load_asset(path)
    parent_class = unreal.load_class(None, parent_path)
    if not bp or not parent_class:
        result['error'] = 'missing Blueprint or native parent'
        return result

    changed = False
    if not inherits_native(path, parent_path):
        try:
            unreal.BlueprintEditorLibrary.reparent_blueprint(bp, parent_class)
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            result['changes'].append({'parent': parent_path})
            changed = True
        except Exception as error:
            result['error'] = 'reparent failed: ' + str(error)
            return result

    generated = bp.generated_class()
    cdo = unreal.get_default_object(generated) if generated else None
    if not cdo:
        result['error'] = 'missing generated-class CDO'
        return result

    for property_name, expected in values.items():
        try:
            changed = set_value_if_needed(cdo, property_name, expected, result['changes']) or changed
        except Exception as error:
            result['changes'].append({'property': property_name, 'error': str(error)})

    if cooldown_path:
        cooldown_class = unreal.load_class(None, cooldown_path)
        if not cooldown_class:
            result['changes'].append({'property': 'CooldownGameplayEffectClass', 'error': 'missing ' + cooldown_path})
        else:
            try:
                current = cdo.get_editor_property('CooldownGameplayEffectClass')
                if object_path(current) != cooldown_path:
                    cdo.set_editor_property('CooldownGameplayEffectClass', cooldown_class)
                    result['changes'].append({
                        'property': 'CooldownGameplayEffectClass',
                        'from': object_path(current),
                        'to': cooldown_path,
                    })
                    changed = True
            except Exception as error:
                result['changes'].append({'property': 'CooldownGameplayEffectClass', 'error': str(error)})

    if changed:
        save(bp)
    result['parent'] = parent_class_path(path)
    result['inheritsNative'] = inherits_native(path, parent_path)
    result['saved'] = changed
    return result

def configure_parent_only(key, path, parent_path):
    result = {'asset': key, 'path': path, 'changes': []}
    if not exists(path):
        result['error'] = 'missing Blueprint'
        return result
    bp = unreal.load_asset(path)
    parent_class = unreal.load_class(None, parent_path)
    if not bp or not parent_class:
        result['error'] = 'missing Blueprint or native parent'
        return result
    if not inherits_native(path, parent_path):
        try:
            unreal.BlueprintEditorLibrary.reparent_blueprint(bp, parent_class)
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            save(bp)
            result['changes'].append({'parent': parent_path})
        except Exception as error:
            result['error'] = str(error)
    result['parent'] = parent_class_path(path)
    result['inheritsNative'] = inherits_native(path, parent_path)
    return result
`;

async function callEditorPython(client, code) {
  const result = await client.callTool({
    name: 'editor',
    arguments: { action: 'execute_python', code },
  });
  return { isError: Boolean(result.isError), raw: text(result.content) };
}

function tryParseJson(candidate) {
  try {
    return JSON.parse(candidate);
  } catch {
    return null;
  }
}

function extractBalancedJson(text) {
  const start = text.indexOf('{');
  if (start < 0) return null;
  let depth = 0;
  let inString = false;
  let escape = false;
  for (let i = start; i < text.length; i++) {
    const ch = text[i];
    if (inString) {
      if (escape) {
        escape = false;
        continue;
      }
      if (ch === '\\') {
        escape = true;
        continue;
      }
      if (ch === '"') inString = false;
      continue;
    }
    if (ch === '"') {
      inString = true;
      continue;
    }
    if (ch === '{') depth += 1;
    else if (ch === '}') {
      depth -= 1;
      if (depth === 0) return text.slice(start, i + 1);
    }
  }
  return null;
}

function extractJsonMarker(raw, marker) {
  const markerIndex = raw.indexOf(marker);
  if (markerIndex < 0) return { raw: String(raw).slice(0, 8000) };
  let after = raw.slice(markerIndex + marker.length);
  const variants = [
    after,
    after.replace(/\\"/g, '"').replace(/\\r\\n/g, '\n').replace(/\\n/g, '\n'),
  ];
  for (const variant of variants) {
    const jsonText = extractBalancedJson(variant);
    if (!jsonText) continue;
    const parsed = tryParseJson(jsonText);
    if (parsed) return parsed;
  }
  return { raw: after.slice(0, 8000) };
}

const pyProbe = `
${PY_HELPERS}
report = {
    'hero': hero_summary('${PATHS.heroData}'),
    'abilitySet': ability_set_summary('${PATHS.abilitySet}'),
    'abilities': {
        'primary': ability_summary('${PATHS.primary}', ['AbilityBaseDamage', 'DamageScaling'], '${NATIVE.primary}'),
        'sunsGrace': ability_summary('${PATHS.sunsGrace}', [], '${NATIVE.sunsGrace}'),
        'radiance': ability_summary(
            '${PATHS.radiance}',
            ['ActiveDuration', 'HealTickInterval', 'HealPerTick', 'StrengthDivisor', 'MaxHealMultiplier'],
            '${NATIVE.radiance}'),
        'sunBurst': ability_summary('${PATHS.sunBurst}', ['AbilityBaseDamage', 'DamageScaling'], '${NATIVE.sunBurst}'),
        'blessing': ability_summary('${PATHS.blessing}', ['InitialHeal', 'HealPerSecond'], '${NATIVE.blessing}'),
        'placeAbility': ability_summary('${PATHS.placeAbility}', [], '${NATIVE.placeAbility}'),
    },
    'shrine': shrine_summary('${PATHS.shrineBP}', '${NATIVE.shrine}'),
    'shrineData': shrine_da_summary('${PATHS.shrineDA}'),
    'shrineMesh': {
        'exists': exists('${PATHS.shrineMesh}'),
        'class': object_path(unreal.load_asset('${PATHS.shrineMesh}').get_class())
            if exists('${PATHS.shrineMesh}') else None,
    },
    'nativeClasses': {
        key: bool(unreal.load_class(None, path)) for key, path in {
            'primary': '${NATIVE.primary}',
            'sunsGrace': '${NATIVE.sunsGrace}',
            'radiance': '${NATIVE.radiance}',
            'sunBurst': '${NATIVE.sunBurst}',
            'blessing': '${NATIVE.blessing}',
            'placeAbility': '${NATIVE.placeAbility}',
            'shrine': '${NATIVE.shrine}',
            'cooldownSunsGrace': '${NATIVE.cooldownSunsGrace}',
            'cooldownRadiance': '${NATIVE.cooldownRadiance}',
            'cooldownSunBurst': '${NATIVE.cooldownSunBurst}',
            'cooldownBlessing': '${NATIVE.cooldownBlessing}',
        }.items()
    },
}
out_path = r'E:/Unreal Projects/ProjectB/AI_Planning/setup_alona_19c_probe_py.json'
with open(out_path, 'w', encoding='utf-8') as f:
    json.dump(report, f, indent=2)
print('BWAY_19C_PROBE=' + json.dumps(report))
print('BWAY_19C_PROBE_FILE=' + out_path)
`;

async function main() {
  const client = await createMcpClient('setup-alona-19c');
  const log = { probe: null, actions: null, after: null };

  const initial = await callEditorPython(client, pyProbe);
  log.probe = extractJsonMarker(initial.raw, 'BWAY_19C_PROBE=');
  writeFileSync('AI_Planning/setup_alona_19c_probe.json', JSON.stringify(log.probe, null, 2));
  console.log('Probe OK — wrote AI_Planning/setup_alona_19c_probe.json');

  if (PROBE_ONLY) {
    writeLog('setup_alona_19c_parity_log.json', log);
    console.log(JSON.stringify(log.probe, null, 2));
    await client.close?.();
    return;
  }

  const pyApply = `
${PY_HELPERS}
actions = []

actions.append(configure_ability(
    'primary',
    '${PATHS.primary}',
    '${NATIVE.primary}',
    {'AbilityBaseDamage': 28.0, 'DamageScaling': 0.25}))
actions.append(configure_ability(
    'sunsGrace',
    '${PATHS.sunsGrace}',
    '${NATIVE.sunsGrace}',
    {},
    '${NATIVE.cooldownSunsGrace}'))
actions.append(configure_ability(
    'radiance',
    '${PATHS.radiance}',
    '${NATIVE.radiance}',
    {
        'ActiveDuration': 4.0,
        'HealTickInterval': 1.0,
        'HealPerTick': 20.0,
        'StrengthDivisor': 52.0,
        'MaxHealMultiplier': 1.75,
    },
    '${NATIVE.cooldownRadiance}'))
actions.append(configure_ability(
    'sunBurst',
    '${PATHS.sunBurst}',
    '${NATIVE.sunBurst}',
    {'AbilityBaseDamage': 30.0, 'DamageScaling': 0.4},
    '${NATIVE.cooldownSunBurst}'))
actions.append(configure_ability(
    'blessing',
    '${PATHS.blessing}',
    '${NATIVE.blessing}',
    {'InitialHeal': 70.0, 'HealPerSecond': 35.0},
    '${NATIVE.cooldownBlessing}'))
actions.append(configure_parent_only(
    'placeAbility',
    '${PATHS.placeAbility}',
    '${NATIVE.placeAbility}'))

# Hero sheet defaults.
if exists('${PATHS.heroData}'):
    hero = unreal.load_asset('${PATHS.heroData}')
    hero_changes = []
    stats = hero.get_editor_property('HeroStats')
    for property_name, expected in {
        'MaxHealth': 350.0,
        'BaseDamage': 40.0,
        'Armor': 0.0,
        'MoveSpeed': 10.0,
    }.items():
        current = stats.get_editor_property(property_name)
        if not approximately_equal(current, expected):
            stats.set_editor_property(property_name, expected)
            hero_changes.append({'property': property_name, 'from': float(current), 'to': expected})
    canonical_da = unreal.load_asset('${PATHS.shrineDA}') if exists('${PATHS.shrineDA}') else None
    current_da = hero.get_editor_property('BuildableDataAsset')
    if canonical_da and object_path(current_da) != object_path(canonical_da):
        hero.set_editor_property('BuildableDataAsset', canonical_da)
        hero_changes.append({
            'property': 'BuildableDataAsset',
            'from': object_path(current_da),
            'to': object_path(canonical_da),
        })
    if hero_changes:
        hero.set_editor_property('HeroStats', stats)
        save(hero)
    actions.append({'hero': '${PATHS.heroData}', 'changes': hero_changes, 'saved': bool(hero_changes)})
else:
    actions.append({'hero': '${PATHS.heroData}', 'error': 'missing hero data'})

# Shrine native parent and CDO sheet defaults.
shrine_parent_result = configure_parent_only('sunShrine', '${PATHS.shrineBP}', '${NATIVE.shrine}')
actions.append(shrine_parent_result)
if exists('${PATHS.shrineBP}'):
    shrine_bp = unreal.load_asset('${PATHS.shrineBP}')
    shrine_class = shrine_bp.generated_class() if shrine_bp else None
    shrine_cdo = unreal.get_default_object(shrine_class) if shrine_class else None
    shrine_changes = []
    if shrine_cdo:
        for property_name, expected in {
            'MaxHealth': 750.0,
            'HealRadius': 500.0,
            'HealPerSecond': 35.0,
            'BuildTime': 0.0,
            'bPersistsBetweenRounds': True,
        }.items():
            try:
                set_value_if_needed(shrine_cdo, property_name, expected, shrine_changes)
            except Exception as error:
                shrine_changes.append({'property': property_name, 'error': str(error)})
        if shrine_changes:
            save(shrine_bp)
    actions.append({'shrineDefaults': shrine_changes, 'saved': bool(shrine_changes)})

# Canonical buildable DA. Keep legacy DAs untouched.
if exists('${PATHS.shrineDA}'):
    data = unreal.load_asset('${PATHS.shrineDA}')
    data_changes = []
    actor_class = load_bp_class('${PATHS.shrineBP}') if exists('${PATHS.shrineBP}') else None
    current_actor_class = data.get_editor_property('BuildableActorClass')
    if actor_class and object_path(current_actor_class) != object_path(actor_class):
        data.set_editor_property('BuildableActorClass', actor_class)
        data_changes.append({
            'property': 'BuildableActorClass',
            'from': object_path(current_actor_class),
            'to': object_path(actor_class),
        })
    set_value_if_needed(data, 'Cost', 0.0, data_changes)
    set_value_if_needed(data, 'MaxActiveBuildablesPerPlayer', 3, data_changes)

    # PrimaryBuildableMesh is USkeletalMesh. SM_BW_alona_buildable is StaticMesh — leave ghost mesh alone.
    shrine_mesh = unreal.load_asset('${PATHS.shrineMesh}') if exists('${PATHS.shrineMesh}') else None
    current_mesh = data.get_editor_property('PrimaryBuildableMesh')
    mesh_note = {
        'property': 'PrimaryBuildableMesh',
        'note': 'requires SkeletalMesh; shrine SM_BW_alona_buildable is StaticMesh (actor mesh, not DA ghost)',
        'current': object_path(current_mesh),
        'staticMeshCandidate': object_path(shrine_mesh) if shrine_mesh else None,
        'staticMeshCandidateClass': object_path(shrine_mesh.get_class()) if shrine_mesh else None,
    }

    if data_changes:
        save(data)
    actions.append({
        'shrineData': '${PATHS.shrineDA}',
        'changes': data_changes,
        'meshNote': mesh_note,
        'saved': bool(data_changes),
    })
else:
    actions.append({'shrineData': '${PATHS.shrineDA}', 'error': 'missing canonical shrine data'})

print('BWAY_19C_APPLY=' + json.dumps({'actions': actions}))
out_path = r'E:/Unreal Projects/ProjectB/AI_Planning/setup_alona_19c_apply_py.json'
with open(out_path, 'w', encoding='utf-8') as f:
    json.dump({'actions': actions}, f, indent=2)
print('BWAY_19C_APPLY_FILE=' + out_path)
`;

  const applied = await callEditorPython(client, pyApply);
  log.actions = extractJsonMarker(applied.raw, 'BWAY_19C_APPLY=');
  writeFileSync('AI_Planning/setup_alona_19c_apply.json', JSON.stringify(log.actions, null, 2));
  console.log('Apply result — wrote AI_Planning/setup_alona_19c_apply.json');

  // Re-run probe and also rewrite the Python probe file as the after snapshot.
  const after = await callEditorPython(
    client,
    pyProbe.replace(
      'setup_alona_19c_probe_py.json',
      'setup_alona_19c_probe_after_py.json',
    ),
  );
  log.after = extractJsonMarker(after.raw, 'BWAY_19C_PROBE=');
  writeFileSync('AI_Planning/setup_alona_19c_probe_after.json', JSON.stringify(log.after, null, 2));
  writeLog('setup_alona_19c_parity_log.json', log);
  console.log('Probe after — wrote AI_Planning/setup_alona_19c_probe_after.json');

  const grants = log.after?.abilitySet?.grants ?? [];
  console.log('Alona grants:', grants.map((grant) => grant.inputTag).join(', '));
  console.log('Radiance:', JSON.stringify(log.after?.abilities?.radiance ?? {}, null, 2));
  console.log('Sun Shrine:', JSON.stringify(log.after?.shrine ?? {}, null, 2));
  console.log(
    'Native inherit:',
    JSON.stringify(
      {
        primary: log.after?.abilities?.primary?.inheritsNative,
        radiance: log.after?.abilities?.radiance?.inheritsNative,
        blessing: log.after?.abilities?.blessing?.inheritsNative,
        shrine: log.after?.shrine?.inheritsNative,
      },
      null,
      2,
    ),
  );

  await client.close?.();
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
