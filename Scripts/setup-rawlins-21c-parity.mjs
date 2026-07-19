/**
 * Step 21c — Enforce and probe Rawlins sheet-parity defaults.
 *
 * Sheet-authoritative fields only (feel/radii left untouched unless already
 * seeded by 21a/21b). Requires Unreal Editor open with the ue-mcp bridge.
 *
 * Usage:
 *   node Scripts/setup-rawlins-21c-parity.mjs
 *   node Scripts/setup-rawlins-21c-parity.mjs --probe-only
 */
import { writeFileSync } from 'node:fs';
import { createMcpClient, text, writeLog } from './lib/bway-widget-setup.mjs';

const PROBE_ONLY = process.argv.includes('--probe-only');

const PATHS = {
  heroData: '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger',
  abilitySet: '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_AbilitySet_Gunslinger',
  kitConfig: '/Hero_Rawlins/Kit/DA_BW_RawlinsKitConfig',
  cageBP: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable/BP_BW_Gunslinger_JailCage',
  trapBP: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable/BP_BW_Gunslinger_Buildable',
  placeGA: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_BuildablePlacement',
  experienceDev: '/BreakawayCore/Experiences/B_BW_Experience_Dev',
};

const NATIVE = {
  primary: '/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsPrimary',
  doubleDown: '/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsDoubleDown',
  powerShot: '/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsPowerShot',
  slideShot: '/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsSlideShot',
  blazingBarrage: '/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsBlazingBarrage',
  attributeSet: '/Script/BreakawayCoreRuntime.BwayHeroAttributeSet',
  kitConfigClass: '/Script/HeroRawlinsRuntime.BwayRawlinsKitConfig',
  cooldownDoubleDown: '/Script/HeroRawlinsRuntime.GE_Bway_Cooldown_RawlinsDoubleDown',
  cooldownPowerShot: '/Script/HeroRawlinsRuntime.GE_Bway_Cooldown_RawlinsPowerShot',
  cooldownSlideShot: '/Script/HeroRawlinsRuntime.GE_Bway_Cooldown_RawlinsSlideShot',
  cooldownBarrage: '/Script/HeroRawlinsRuntime.GE_Bway_Cooldown_RawlinsBlazingBarrage',
  cageActor: '/Script/HeroRawlinsRuntime.BwayRawlinsJailCage',
  trapActor: '/Script/HeroRawlinsRuntime.BwayRawlinsJailBuildable',
};

/** Sheet-authoritative kit fields only. Feel / radii are NOT rewritten here. */
const SHEET_KIT = {
  PrimaryBaseDamage: 1.0,
  PrimaryDamageScaling: 0.3,
  PrimaryShotCount: 2,
  DoubleDownCooldown: 14.0,
  PowerShotBaseDamage: 30.0,
  PowerShotDamageScaling: 0.65,
  PowerShotCooldown: 8.0,
  SlideShotBaseDamage: 10.0,
  SlideShotDamageScaling: 0.6,
  SlideShotCooldown: 18.0,
  BarrageBaseDamagePerBullet: 4.0,
  BarrageDamageScaling: 0.3,
  BarrageShotCount: 12,
  BarrageCooldown: 25.0,
  JailMaxHealth: 450.0,
};

const SHEET_HERO = {
  MaxHealth: 400.0,
  BaseDamage: 60.0,
  Armor: 0.0,
  MoveSpeed: 10.5,
};

const EXPECTED_PLUGINS = [
  'BreakawayCore',
  'Hero_Spartacus',
  'Hero_Alona',
  'Hero_Morgan',
  'Hero_Rawlins',
];

/** Accept native class grants OR BP subclasses that inherit the native parent. */
const EXPECTED_GRANTS = [
  {
    tag: 'InputTag.Ability.Primary',
    abilityContainsAny: ['BwayGameplayAbility_RawlinsPrimary', 'GA_BW_Rawlins_PrimaryAttack'],
    nativeParent: NATIVE.primary,
  },
  {
    tag: 'InputTag.Ability.Ability4',
    abilityContainsAny: ['BwayGameplayAbility_RawlinsDoubleDown', 'GA_BW_Rawlins_DoubleDown'],
    nativeParent: NATIVE.doubleDown,
  },
  {
    tag: 'InputTag.Ability.Ability1',
    abilityContainsAny: ['BwayGameplayAbility_RawlinsPowerShot', 'GA_BW_Rawlins_PowerShot'],
    nativeParent: NATIVE.powerShot,
  },
  {
    tag: 'InputTag.Ability.Ability2',
    abilityContainsAny: ['BwayGameplayAbility_RawlinsSlideShot', 'GA_BW_Rawlins_SlideShot'],
    nativeParent: NATIVE.slideShot,
  },
  {
    tag: 'InputTag.Ability.Ability3',
    abilityContainsAny: ['BwayGameplayAbility_RawlinsBlazingBarrage', 'GA_BW_Rawlins_BlazingBarrage'],
    nativeParent: NATIVE.blazingBarrage,
  },
  {
    tag: 'InputTag.Ability.Buildable',
    abilityContainsAny: ['GA_BW_Rawlins_BuildablePlacement', 'BwayGameplayAbility_PlaceBuildable'],
    nativeParent: '/Script/BreakawayCoreRuntime.BwayGameplayAbility_PlaceBuildable',
  },
];

const ABILITY_BP_PATHS = {
  primary: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_PrimaryAttack',
  doubleDown: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_DoubleDown',
  powerShot: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_PowerShot',
  slideShot: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_SlideShot',
  blazingBarrage: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_BlazingBarrage',
  placeGA: PATHS.placeGA,
};

const COOLDOWN_TARGETS = {
  DoubleDown: 14.0,
  PowerShot: 8.0,
  SlideShot: 18.0,
  BlazingBarrage: 25.0,
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

def approximately_equal(current, expected):
    if isinstance(expected, float):
        try:
            return math.isclose(float(current), expected, rel_tol=1e-5, abs_tol=1e-4)
        except Exception:
            return False
    if isinstance(expected, int):
        try:
            return int(float(current)) == expected
        except Exception:
            return False
    return current == expected

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

def hero_summary(path):
    if not exists(path):
        return {'exists': False, 'path': path}
    hero = unreal.load_asset(path)
    out = {'exists': True, 'path': path}
    try:
        out['DisplayName'] = str(hero.get_editor_property('DisplayName'))
        stats = hero.get_editor_property('HeroStats')
        out['MaxHealth'] = float(stats.get_editor_property('MaxHealth'))
        out['BaseDamage'] = float(stats.get_editor_property('BaseDamage'))
        out['Armor'] = float(stats.get_editor_property('Armor'))
        out['MoveSpeed'] = float(stats.get_editor_property('MoveSpeed'))
        attr = hero.get_editor_property('AttributeSetClass')
        out['AttributeSetClass'] = object_path(attr)
        buildable = hero.get_editor_property('BuildableDataAsset')
        out['BuildableDataAsset'] = object_path(buildable)
    except Exception as error:
        out['error'] = str(error)
    return out

def kit_summary(path, props):
    if not exists(path):
        return {'exists': False, 'path': path}
    kit = unreal.load_asset(path)
    out = {'exists': True, 'path': path, 'values': {}}
    for prop in props:
        try:
            val = kit.get_editor_property(prop)
            out['values'][prop] = float(val) if not isinstance(val, int) else int(val)
            # BarrageShotCount / PrimaryShotCount are ints
            if prop.endswith('ShotCount'):
                out['values'][prop] = int(float(val))
        except Exception as error:
            out['values'][prop] = 'error:' + str(error)
    return out

def ability_set_summary(path):
    if not exists(path):
        return {'exists': False, 'path': path}
    aset = unreal.load_asset(path)
    grants = []
    for grant in aset.get_editor_property('GrantedGameplayAbilities') or []:
        ability = grant.get_editor_property('Ability')
        input_tag = grant.get_editor_property('InputTag')
        grants.append({
            'ability': object_path(ability),
            'inputTag': tag_name(input_tag),
        })
    return {'exists': True, 'path': path, 'grants': grants, 'grant_count': len(grants)}

def experience_plugins(path):
    if not exists(path):
        return {'exists': False}
    exp = unreal.load_asset(path)
    cdo = unreal.get_default_object(exp.generated_class()) if hasattr(exp, 'generated_class') else exp
    plugins = list(cdo.get_editor_property('GameFeaturesToEnable') or [])
    return {'exists': True, 'plugins': plugins}

def cage_summary(path):
    if not exists(path):
        return {'exists': False, 'path': path}
    bp = unreal.load_asset(path)
    out = {'exists': True, 'path': path}
    try:
        cdo = unreal.get_default_object(bp.generated_class())
        out['MaxHealth'] = float(cdo.get_editor_property('MaxHealth'))
        out['CageRadius'] = float(cdo.get_editor_property('CageRadius'))
        out['bPersistsBetweenRounds'] = bool(cdo.get_editor_property('bPersistsBetweenRounds'))
        kit = cdo.get_editor_property('KitConfig')
        out['KitConfig'] = object_path(kit)
    except Exception as error:
        out['error'] = str(error)
    return out

def trap_summary(path):
    if not exists(path):
        return {'exists': False, 'path': path}
    bp = unreal.load_asset(path)
    out = {'exists': True, 'path': path}
    try:
        cdo = unreal.get_default_object(bp.generated_class())
        out['TriggerRadius'] = float(cdo.get_editor_property('TriggerRadius'))
        out['CaptureRadius'] = float(cdo.get_editor_property('CaptureRadius'))
        out['bPersistsBetweenRounds'] = bool(cdo.get_editor_property('bPersistsBetweenRounds'))
        cage = cdo.get_editor_property('JailCageClass')
        out['JailCageClass'] = object_path(cage)
        kit = cdo.get_editor_property('KitConfig')
        out['KitConfig'] = object_path(kit)
    except Exception as error:
        out['error'] = str(error)
    return out

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
    return None

def ability_parent_summary(path, native_path):
    if not exists(path):
        return {'exists': False, 'path': path}
    return {
        'exists': True,
        'path': path,
        'parent': parent_class_path(path),
        'inheritsNative': inherits_native(path, native_path),
    }
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

function extractBalancedJson(rawText) {
  const start = rawText.indexOf('{');
  if (start < 0) return null;
  let depth = 0;
  let inString = false;
  let escape = false;
  for (let i = start; i < rawText.length; i++) {
    const ch = rawText[i];
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
      if (depth === 0) return rawText.slice(start, i + 1);
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

function approxEqual(current, expected) {
  if (typeof expected === 'number') {
    const n = Number(current);
    if (Number.isNaN(n)) return false;
    return Math.abs(n - expected) < 1e-3;
  }
  return current === expected;
}

function evaluateParity(probe) {
  const failures = [];
  const checks = [];

  const hero = probe?.hero ?? {};
  for (const [key, expected] of Object.entries(SHEET_HERO)) {
    const ok = approxEqual(hero[key], expected);
    checks.push({ scope: 'hero', key, expected, actual: hero[key], ok });
    if (!ok) failures.push(`hero.${key}: expected ${expected}, got ${hero[key]}`);
  }
  if (hero.DisplayName !== 'Rawlins') {
    failures.push(`hero.DisplayName: expected Rawlins, got ${hero.DisplayName}`);
    checks.push({ scope: 'hero', key: 'DisplayName', expected: 'Rawlins', actual: hero.DisplayName, ok: false });
  } else {
    checks.push({ scope: 'hero', key: 'DisplayName', expected: 'Rawlins', actual: hero.DisplayName, ok: true });
  }

  const kitValues = probe?.kitConfig?.values ?? probe?.kitConfig ?? {};
  for (const [key, expected] of Object.entries(SHEET_KIT)) {
    const actual = kitValues[key] ?? kitValues.values?.[key];
    const ok = approxEqual(actual, expected);
    checks.push({ scope: 'kit', key, expected, actual, ok });
    if (!ok) failures.push(`kit.${key}: expected ${expected}, got ${actual}`);
  }

  const cage = probe?.cage ?? {};
  const cageOk = approxEqual(cage.MaxHealth, 450.0);
  checks.push({ scope: 'cage', key: 'MaxHealth', expected: 450.0, actual: cage.MaxHealth, ok: cageOk });
  if (!cageOk) failures.push(`cage.MaxHealth: expected 450, got ${cage.MaxHealth}`);

  const grants = probe?.abilitySet?.grants ?? [];
  for (const expected of EXPECTED_GRANTS) {
    const match = grants.find((g) => g.inputTag === expected.tag);
    const abilityPath = match?.ability ?? '';
    const ok = Boolean(match) && expected.abilityContainsAny.some((needle) => String(abilityPath).includes(needle));
    checks.push({
      scope: 'grant',
      key: expected.tag,
      expected: expected.abilityContainsAny.join(' | '),
      actual: abilityPath,
      ok,
    });
    if (!ok) failures.push(`grant.${expected.tag}: missing or wrong ability (got ${abilityPath})`);
  }
  if (grants.length !== 6) {
    failures.push(`abilitySet.grant_count: expected 6, got ${grants.length}`);
  }

  // BP ability parents must inherit sheet-accurate native classes when BPs are used.
  const bpParents = probe?.abilityParents ?? {};
  for (const [key, expectedNative] of Object.entries({
    primary: NATIVE.primary,
    doubleDown: NATIVE.doubleDown,
    powerShot: NATIVE.powerShot,
    slideShot: NATIVE.slideShot,
    blazingBarrage: NATIVE.blazingBarrage,
  })) {
    const info = bpParents[key];
    if (!info || info.exists === false) {
      // Native-only grant path is OK (no BP asset).
      checks.push({ scope: 'abilityParent', key, expected: expectedNative, actual: 'native-or-missing-bp', ok: true });
      continue;
    }
    const ok = info.inheritsNative === true;
    checks.push({
      scope: 'abilityParent',
      key,
      expected: expectedNative,
      actual: info.parent,
      inheritsNative: info.inheritsNative,
      ok,
    });
    if (!ok) failures.push(`abilityParent.${key}: expected inherit ${expectedNative}, got ${info.parent}`);
  }

  const plugins = probe?.experience?.plugins ?? [];
  for (const required of EXPECTED_PLUGINS) {
    const ok = plugins.includes(required);
    checks.push({ scope: 'experience', key: required, expected: true, actual: ok, ok });
    if (!ok) failures.push(`experience missing plugin: ${required}`);
  }

  const cds = probe?.cooldownGEs ?? {};
  for (const [key, expected] of Object.entries(COOLDOWN_TARGETS)) {
    const actual = cds[key];
    const ok = approxEqual(actual, expected);
    checks.push({ scope: 'cooldownGE', key, expected, actual, ok });
    if (!ok) failures.push(`cooldownGE.${key}: expected ${expected}, got ${actual}`);
  }

  const natives = probe?.nativeClasses ?? {};
  for (const [key, loaded] of Object.entries(natives)) {
    const ok = loaded === true;
    checks.push({ scope: 'native', key, expected: true, actual: loaded, ok });
    if (!ok) failures.push(`native class missing: ${key}`);
  }

  return { pass: failures.length === 0, failures, checks };
}

const KIT_PROP_LIST = Object.keys(SHEET_KIT);

const pyProbe = `
${PY_HELPERS}
kit_props = ${JSON.stringify(KIT_PROP_LIST)}
report = {
    'hero': hero_summary('${PATHS.heroData}'),
    'abilitySet': ability_set_summary('${PATHS.abilitySet}'),
    'kitConfig': kit_summary('${PATHS.kitConfig}', kit_props),
    'cage': cage_summary('${PATHS.cageBP}'),
    'trap': trap_summary('${PATHS.trapBP}'),
    'experience': experience_plugins('${PATHS.experienceDev}'),
    'abilityParents': {
        'primary': ability_parent_summary('${ABILITY_BP_PATHS.primary}', '${NATIVE.primary}'),
        'doubleDown': ability_parent_summary('${ABILITY_BP_PATHS.doubleDown}', '${NATIVE.doubleDown}'),
        'powerShot': ability_parent_summary('${ABILITY_BP_PATHS.powerShot}', '${NATIVE.powerShot}'),
        'slideShot': ability_parent_summary('${ABILITY_BP_PATHS.slideShot}', '${NATIVE.slideShot}'),
        'blazingBarrage': ability_parent_summary('${ABILITY_BP_PATHS.blazingBarrage}', '${NATIVE.blazingBarrage}'),
        'placeGA': ability_parent_summary('${ABILITY_BP_PATHS.placeGA}', '/Script/BreakawayCoreRuntime.BwayGameplayAbility_PlaceBuildable'),
    },
    'cooldownGEs': {},
    'nativeClasses': {
        key: bool(unreal.load_class(None, path)) for key, path in {
            'primary': '${NATIVE.primary}',
            'doubleDown': '${NATIVE.doubleDown}',
            'powerShot': '${NATIVE.powerShot}',
            'slideShot': '${NATIVE.slideShot}',
            'blazingBarrage': '${NATIVE.blazingBarrage}',
            'attributeSet': '${NATIVE.attributeSet}',
            'kitConfigClass': '${NATIVE.kitConfigClass}',
            'cageActor': '${NATIVE.cageActor}',
            'trapActor': '${NATIVE.trapActor}',
            'cooldownDoubleDown': '${NATIVE.cooldownDoubleDown}',
            'cooldownPowerShot': '${NATIVE.cooldownPowerShot}',
            'cooldownSlideShot': '${NATIVE.cooldownSlideShot}',
            'cooldownBarrage': '${NATIVE.cooldownBarrage}',
        }.items()
    },
}
for label, path in [
    ('DoubleDown', '${NATIVE.cooldownDoubleDown}'),
    ('PowerShot', '${NATIVE.cooldownPowerShot}'),
    ('SlideShot', '${NATIVE.cooldownSlideShot}'),
    ('BlazingBarrage', '${NATIVE.cooldownBarrage}'),
]:
    cls = unreal.load_class(None, path)
    report['cooldownGEs'][label] = duration_seconds(cls) if cls else None

print('BWAY_21C_PROBE=' + json.dumps(report))
`;

async function main() {
  const client = await createMcpClient('setup-rawlins-21c');
  const log = { probe: null, actions: null, after: null, parity: null };

  const initial = await callEditorPython(client, pyProbe);
  log.probe = extractJsonMarker(initial.raw, 'BWAY_21C_PROBE=');
  writeFileSync('AI_Planning/setup_rawlins_21c_probe.json', JSON.stringify(log.probe, null, 2));
  console.log('Probe OK — wrote AI_Planning/setup_rawlins_21c_probe.json');

  if (PROBE_ONLY) {
    log.parity = evaluateParity(log.probe);
    writeLog('setup_rawlins_21c_parity_log.json', log);
    console.log(JSON.stringify({ parity: log.parity, probe: log.probe }, null, 2));
    if (!log.parity.pass) {
      console.error('PARITY FAIL:', log.parity.failures.join('; '));
      await client.close?.();
      process.exit(1);
    } else {
      console.log('PARITY PASS');
      await client.close?.();
      process.exit(0);
    }
    return;
  }

  const sheetKitJson = JSON.stringify(SHEET_KIT);
  const sheetHeroJson = JSON.stringify(SHEET_HERO);
  const expectedPluginsJson = JSON.stringify(EXPECTED_PLUGINS);

  const pyApply = `
${PY_HELPERS}
actions = []
SHEET_KIT = ${sheetKitJson}
SHEET_HERO = ${sheetHeroJson}
EXPECTED_PLUGINS = ${expectedPluginsJson}

# --- Kit config: sheet fields only ---
if exists('${PATHS.kitConfig}'):
    kit = unreal.load_asset('${PATHS.kitConfig}')
    kit_changes = []
    for prop, expected in SHEET_KIT.items():
        try:
            current = kit.get_editor_property(prop)
            if not approximately_equal(current, expected):
                kit.set_editor_property(prop, expected)
                kit_changes.append({'property': prop, 'from': float(current) if not isinstance(current, int) else int(current), 'to': expected})
        except Exception as error:
            kit_changes.append({'property': prop, 'error': str(error)})
    if kit_changes:
        save(kit)
    actions.append({'kit': '${PATHS.kitConfig}', 'changes': kit_changes, 'saved': bool(kit_changes)})
else:
    actions.append({'kit': '${PATHS.kitConfig}', 'error': 'missing kit config'})

# --- Hero DA sheet stats ---
if exists('${PATHS.heroData}'):
    hero = unreal.load_asset('${PATHS.heroData}')
    hero_changes = []
    stats = hero.get_editor_property('HeroStats')
    for prop, expected in SHEET_HERO.items():
        current = stats.get_editor_property(prop)
        if not approximately_equal(current, expected):
            stats.set_editor_property(prop, expected)
            hero_changes.append({'property': prop, 'from': float(current), 'to': expected})
    try:
        display = str(hero.get_editor_property('DisplayName'))
        if display != 'Rawlins':
            hero.set_editor_property('DisplayName', unreal.Text('Rawlins'))
            hero_changes.append({'property': 'DisplayName', 'from': display, 'to': 'Rawlins'})
    except Exception as error:
        hero_changes.append({'property': 'DisplayName', 'error': str(error)})
    attr = unreal.load_class(None, '${NATIVE.attributeSet}')
    if attr:
        current_attr = hero.get_editor_property('AttributeSetClass')
        if object_path(current_attr) != '${NATIVE.attributeSet}':
            hero.set_editor_property('AttributeSetClass', attr)
            hero_changes.append({'property': 'AttributeSetClass', 'to': '${NATIVE.attributeSet}'})
    if hero_changes:
        hero.set_editor_property('HeroStats', stats)
        save(hero)
    actions.append({'hero': '${PATHS.heroData}', 'changes': hero_changes, 'saved': bool(hero_changes)})
else:
    actions.append({'hero': '${PATHS.heroData}', 'error': 'missing hero data'})

# --- Jail cage CDO HP ---
if exists('${PATHS.cageBP}'):
    cage_bp = unreal.load_asset('${PATHS.cageBP}')
    cage_changes = []
    try:
        cdo = unreal.get_default_object(cage_bp.generated_class())
        current_hp = cdo.get_editor_property('MaxHealth')
        if not approximately_equal(current_hp, 450.0):
            cdo.set_editor_property('MaxHealth', 450.0)
            cage_changes.append({'property': 'MaxHealth', 'from': float(current_hp), 'to': 450.0})
        if exists('${PATHS.kitConfig}'):
            kit = unreal.load_asset('${PATHS.kitConfig}')
            current_kit = cdo.get_editor_property('KitConfig')
            if object_path(current_kit) != object_path(kit):
                cdo.set_editor_property('KitConfig', kit)
                cage_changes.append({'property': 'KitConfig', 'to': object_path(kit)})
        if cage_changes:
            save(cage_bp)
    except Exception as error:
        cage_changes.append({'error': str(error)})
    actions.append({'cage': '${PATHS.cageBP}', 'changes': cage_changes, 'saved': bool(cage_changes)})
else:
    actions.append({'cage': '${PATHS.cageBP}', 'error': 'missing cage BP'})

# --- Experience plugins (append only) ---
if exists('${PATHS.experienceDev}'):
    exp = unreal.load_asset('${PATHS.experienceDev}')
    cdo = unreal.get_default_object(exp.generated_class()) if hasattr(exp, 'generated_class') else exp
    plugins = list(cdo.get_editor_property('GameFeaturesToEnable') or [])
    exp_changes = []
    for required in EXPECTED_PLUGINS:
        if required not in plugins:
            plugins.append(required)
            exp_changes.append({'add': required})
    if exp_changes:
        cdo.set_editor_property('GameFeaturesToEnable', plugins)
        save(exp)
    actions.append({'experience': '${PATHS.experienceDev}', 'changes': exp_changes, 'plugins': plugins, 'saved': bool(exp_changes)})
else:
    actions.append({'experience': '${PATHS.experienceDev}', 'error': 'missing experience'})

# --- Reparent combat ability BPs to sheet-accurate native parents (if BPs exist) ---
ABILITY_REPARENTS = [
    ('${ABILITY_BP_PATHS.primary}', '${NATIVE.primary}'),
    ('${ABILITY_BP_PATHS.doubleDown}', '${NATIVE.doubleDown}'),
    ('${ABILITY_BP_PATHS.powerShot}', '${NATIVE.powerShot}'),
    ('${ABILITY_BP_PATHS.slideShot}', '${NATIVE.slideShot}'),
    ('${ABILITY_BP_PATHS.blazingBarrage}', '${NATIVE.blazingBarrage}'),
]
kit_asset = unreal.load_asset('${PATHS.kitConfig}') if exists('${PATHS.kitConfig}') else None
for bp_path, native_path in ABILITY_REPARENTS:
    if not exists(bp_path):
        actions.append({'reparent_skip': bp_path, 'reason': 'missing BP (native grant OK)'})
        continue
    bp = unreal.load_asset(bp_path)
    parent_class = unreal.load_class(None, native_path)
    result = {'path': bp_path, 'native': native_path, 'changes': []}
    if not bp or not parent_class:
        result['error'] = 'missing BP or native parent'
        actions.append({'reparent': result})
        continue
    if not inherits_native(bp_path, native_path):
        try:
            unreal.BlueprintEditorLibrary.reparent_blueprint(bp, parent_class)
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            result['changes'].append({'parent': native_path})
        except Exception as error:
            result['error'] = 'reparent failed: ' + str(error)
            actions.append({'reparent': result})
            continue
    # Soft-wire KitConfig onto CDO when possible
    try:
        cdo = unreal.get_default_object(bp.generated_class())
        if kit_asset and cdo:
            try:
                current_kit = cdo.get_editor_property('KitConfig')
                if object_path(current_kit) != object_path(kit_asset):
                    cdo.set_editor_property('KitConfig', kit_asset)
                    result['changes'].append({'KitConfig': object_path(kit_asset)})
            except Exception as error:
                result['changes'].append({'KitConfig_skip': str(error)})
    except Exception as error:
        result['changes'].append({'cdo_warn': str(error)})
    if result['changes']:
        save(bp)
    result['inheritsNative'] = inherits_native(bp_path, native_path)
    result['parent'] = parent_class_path(bp_path)
    result['saved'] = bool(result['changes'])
    actions.append({'reparent': result})

# Note: ability-set grants are NOT rewritten here — accept native OR BP grants.
# Probe fails loudly if tags/count drift.

print('BWAY_21C_APPLY=' + json.dumps({'actions': actions}))
`;

  const applied = await callEditorPython(client, pyApply);
  log.actions = extractJsonMarker(applied.raw, 'BWAY_21C_APPLY=');
  writeFileSync('AI_Planning/setup_rawlins_21c_apply.json', JSON.stringify(log.actions, null, 2));
  console.log('Apply result — wrote AI_Planning/setup_rawlins_21c_apply.json');

  const after = await callEditorPython(client, pyProbe);
  log.after = extractJsonMarker(after.raw, 'BWAY_21C_PROBE=');
  writeFileSync('AI_Planning/setup_rawlins_21c_probe_after.json', JSON.stringify(log.after, null, 2));
  console.log('Probe after — wrote AI_Planning/setup_rawlins_21c_probe_after.json');

  log.parity = evaluateParity(log.after);
  writeLog('setup_rawlins_21c_parity_log.json', log);

  console.log('Kit:', JSON.stringify(log.after?.kitConfig?.values ?? {}, null, 2));
  console.log('Hero:', JSON.stringify({
    DisplayName: log.after?.hero?.DisplayName,
    MaxHealth: log.after?.hero?.MaxHealth,
    BaseDamage: log.after?.hero?.BaseDamage,
    Armor: log.after?.hero?.Armor,
    MoveSpeed: log.after?.hero?.MoveSpeed,
  }, null, 2));
  console.log('Cage HP:', log.after?.cage?.MaxHealth);
  console.log('Plugins:', log.after?.experience?.plugins);
  console.log('Grants:', (log.after?.abilitySet?.grants ?? []).map((g) => g.inputTag).join(', '));
  console.log('Cooldown GEs:', log.after?.cooldownGEs);

  if (!log.parity.pass) {
    console.error('PARITY FAIL:', log.parity.failures.join('; '));
    await client.close?.();
    process.exit(1);
  } else {
    console.log('PARITY PASS — sheet spot-check ready for PIE / 3/3 cold starts');
    await client.close?.();
    process.exit(0);
  }
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
