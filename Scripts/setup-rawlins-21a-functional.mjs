/**
 * Step 21a — Wire Rawlins (Hero_Rawlins / Gunslinger) hero DA, ability set grants, kit config, GFD, and experience plugin enable.
 *
 * Requires Unreal Editor open with MCP bridge (ue-mcp) AFTER HeroRawlinsRuntime compiles.
 * Idempotent: safe to re-run.
 *
 * Usage:
 *   node Scripts/setup-rawlins-21a-functional.mjs
 *   node Scripts/setup-rawlins-21a-functional.mjs --probe-only
 */
import { createMcpClient, text, writeLog } from './lib/bway-widget-setup.mjs';

const PROBE_ONLY = process.argv.includes('--probe-only');

const PATHS = {
  heroRoot: '/Hero_Rawlins/Characters/Heroes/Gunslinger',
  kitRoot: '/Hero_Rawlins/Kit',
  heroData: '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger',
  abilitySet: '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_AbilitySet_Gunslinger',
  kitConfig: '/Hero_Rawlins/Kit/DA_BW_RawlinsKitConfig',
  gfd: '/Hero_Rawlins/Hero_Rawlins',
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
};

const GRANTS = [
  { tag: 'InputTag.Ability.Primary', ability: NATIVE.primary },
  { tag: 'InputTag.Ability.Ability4', ability: NATIVE.doubleDown },
  { tag: 'InputTag.Ability.Ability1', ability: NATIVE.powerShot },
  { tag: 'InputTag.Ability.Ability2', ability: NATIVE.slideShot },
  { tag: 'InputTag.Ability.Ability3', ability: NATIVE.blazingBarrage },
];

const PY_HELPERS = `
import unreal
import json
import re

def exists(p):
    return unreal.EditorAssetLibrary.does_asset_exist(p)

def save(asset):
    if asset:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)

def tag_name(tag):
    if not tag:
        return ''
    try:
        return str(tag.get_editor_property('TagName'))
    except Exception:
        pass
    try:
        exported = tag.export_text() or ''
        m = re.search(r'TagName=\\"([^\\"]+)\\"', exported)
        if m:
            return m.group(1)
    except Exception:
        pass
    return str(tag)

def ability_set_grants(path):
    if not path or not exists(path):
        return {'exists': False, 'path': path}
    aset = unreal.load_asset(path)
    grants = []
    for g in aset.get_editor_property('GrantedGameplayAbilities') or []:
        ab = g.get_editor_property('Ability')
        tag = g.get_editor_property('InputTag')
        grants.append({
            'ability': ab.get_path_name() if ab else None,
            'inputTag': tag_name(tag),
        })
    return {'exists': True, 'path': path, 'grants': grants}

def experience_plugins(path):
    if not exists(path):
        return {'exists': False}
    exp = unreal.load_asset(path)
    cdo = unreal.get_default_object(exp.generated_class()) if hasattr(exp, 'generated_class') else exp
    plugins = list(cdo.get_editor_property('GameFeaturesToEnable') or [])
    return {'exists': True, 'plugins': plugins}

def hero_summary(path):
    if not path or not exists(path):
        return {'exists': False, 'path': path}
    hero = unreal.load_asset(path)
    out = {'exists': True, 'path': path}
    try:
        out['DisplayName'] = str(hero.get_editor_property('DisplayName'))
    except Exception as e:
        out['DisplayName'] = 'error:' + str(e)
    try:
        stats = hero.get_editor_property('HeroStats')
        out['MaxHealth'] = float(stats.get_editor_property('MaxHealth'))
        out['BaseDamage'] = float(stats.get_editor_property('BaseDamage'))
        out['Armor'] = float(stats.get_editor_property('Armor'))
        out['MoveSpeed'] = float(stats.get_editor_property('MoveSpeed'))
    except Exception as e:
        out['statsError'] = str(e)
    try:
        attr = hero.get_editor_property('AttributeSetClass')
        out['AttributeSetClass'] = attr.get_path_name() if attr else None
    except Exception as e:
        out['AttributeSetClass'] = 'error:' + str(e)
    try:
        sets = hero.get_editor_property('AbilitySets') or []
        out['AbilitySets'] = [s.get_path_name() for s in sets if s]
    except Exception as e:
        out['AbilitySets'] = 'error:' + str(e)
    try:
        buildable = hero.get_editor_property('BuildableDataAsset')
        out['BuildableDataAsset'] = buildable.get_path_name() if buildable else None
    except Exception as e:
        out['BuildableDataAsset'] = 'error:' + str(e)
    try:
        out['HeroClass'] = str(hero.get_editor_property('HeroClass'))
    except Exception as e:
        out['HeroClass'] = 'error:' + str(e)
    try:
        out['bUseCustomClassName'] = bool(hero.get_editor_property('bUseCustomClassName'))
        out['CustomClassName'] = str(hero.get_editor_property('CustomClassName'))
    except Exception as e:
        out['CustomClassName'] = 'error:' + str(e)
    return out

def kit_summary(path):
    if not path or not exists(path):
        return {'exists': False, 'path': path}
    kit = unreal.load_asset(path)
    out = {'exists': True, 'path': path}
    for prop in [
        'PrimaryBaseDamage', 'PrimaryDamageScaling', 'PrimaryShotCount', 'PrimaryShotInterval',
        'DoubleDownCooldown', 'DoubleDownDashDistance', 'DoubleDownDashDuration',
        'PowerShotBaseDamage', 'PowerShotDamageScaling', 'PowerShotCooldown',
        'SlideShotBaseDamage', 'SlideShotDamageScaling', 'SlideShotCooldown',
        'BarrageBaseDamagePerBullet', 'BarrageDamageScaling', 'BarrageShotCount', 'BarrageCooldown',
        'JailMaxHealth', 'JailTriggerRadius', 'JailCaptureRadius', 'JailCageRadius'
    ]:
        try:
            out[prop] = float(kit.get_editor_property(prop))
        except Exception as e:
            out[prop] = 'error:' + str(e)
    return out
`;

async function main() {
  const client = await createMcpClient('setup-rawlins-21a');
  const log = { probe: null, actions: [], after: null };

  const pyProbe = `
${PY_HELPERS}
report = {
  'hero': hero_summary('${PATHS.heroData}'),
  'abilitySet': ability_set_grants('${PATHS.abilitySet}'),
  'kitConfig': kit_summary('${PATHS.kitConfig}'),
  'gfdExists': exists('${PATHS.gfd}'),
  'pluginRootExists': unreal.EditorAssetLibrary.does_directory_exist('/Hero_Rawlins'),
  'experience': experience_plugins('${PATHS.experienceDev}'),
  'nativeClasses': {
    k: bool(unreal.load_class(None, v)) for k, v in {
      'primary': '${NATIVE.primary}',
      'doubleDown': '${NATIVE.doubleDown}',
      'powerShot': '${NATIVE.powerShot}',
      'slideShot': '${NATIVE.slideShot}',
      'blazingBarrage': '${NATIVE.blazingBarrage}',
      'attributeSet': '${NATIVE.attributeSet}',
      'kitConfigClass': '${NATIVE.kitConfigClass}',
    }.items()
  },
}
print('BWAY_JSON:' + json.dumps(report))
`;

  const probeResult = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: pyProbe } });
  log.probe = text(probeResult.content);
  writeLog('setup_rawlins_21a_probe.json', log);

  if (PROBE_ONLY) {
    console.log(log.probe);
    await client.close?.();
    return;
  }

  const grantEntries = GRANTS.map(
    (g) => `{'tag': '${g.tag}', 'ability': '${g.ability}'}`
  ).join(',\n  ');

  const pyFix = `
${PY_HELPERS}
actions = []

# Ensure directories
for d in ['/Hero_Rawlins', '/Hero_Rawlins/Characters', '/Hero_Rawlins/Characters/Heroes', '${PATHS.heroRoot}', '${PATHS.kitRoot}']:
    if not unreal.EditorAssetLibrary.does_directory_exist(d):
        unreal.EditorAssetLibrary.make_directory(d)
        actions.append({'mkdir': d})

# Create kit config if missing
kit_path = '${PATHS.kitConfig}' if exists('${PATHS.kitConfig}') else None
if not kit_path:
    factory = unreal.DataAssetFactory()
    kit_cls = unreal.load_class(None, '${NATIVE.kitConfigClass}')
    if kit_cls:
        factory.set_editor_property('DataAssetClass', kit_cls)
        created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'DA_BW_RawlinsKitConfig', '${PATHS.kitRoot}', kit_cls, factory)
        save(created)
        kit_path = '${PATHS.kitConfig}'
        actions.append({'create_kit_config': kit_path, 'ok': bool(created)})
    else:
        actions.append({'create_kit_config': False, 'error': 'missing BwayRawlinsKitConfig'})
else:
    actions.append({'kit_config_exists': kit_path})

# Seed kit config sheet defaults (always refresh magnitudes for idempotent 21a)
if kit_path and exists(kit_path):
    kit = unreal.load_asset(kit_path)
    defaults = {
        'PrimaryBaseDamage': 1.0,
        'PrimaryDamageScaling': 0.3,
        'PrimaryProjectileSpeed': 3600.0,
        'PrimaryProjectileLifeSpan': 1.5,
        'PrimarySpawnForwardOffset': 80.0,
        'PrimaryShotCount': 2,
        'PrimaryShotInterval': 0.12,
        'DoubleDownDashDistance': 900.0,
        'DoubleDownDashDuration': 0.35,
        'DoubleDownCooldown': 14.0,
        'PowerShotBaseDamage': 30.0,
        'PowerShotDamageScaling': 0.65,
        'PowerShotTraceRadius': 100.0,
        'PowerShotTraceDistance': 900.0,
        'PowerShotKnockbackStrength': 1600.0,
        'PowerShotKnockbackUpward': 250.0,
        'PowerShotCooldown': 8.0,
        'SlideShotSlideDistance': 1000.0,
        'SlideShotSlideDuration': 0.4,
        'SlideShotTraceRadius': 120.0,
        'SlideShotBaseDamage': 10.0,
        'SlideShotDamageScaling': 0.6,
        'SlideShotLaunchStrength': 400.0,
        'SlideShotLaunchUpward': 900.0,
        'SlideShotCooldown': 18.0,
        'BarrageBaseDamagePerBullet': 4.0,
        'BarrageDamageScaling': 0.3,
        'BarrageShotCount': 12,
        'BarrageShotInterval': 0.1,
        'BarrageProjectileSpeed': 3600.0,
        'BarrageProjectileLifeSpan': 1.5,
        'BarrageSpawnForwardOffset': 80.0,
        'BarrageJuggleUpward': 350.0,
        'BarrageCooldown': 25.0,
        # 21b Jail fields (safe on 21a reruns)
        'JailMaxHealth': 450.0,
        'JailTriggerRadius': 300.0,
        'JailCaptureRadius': 450.0,
        'JailCageRadius': 120.0,
    }
    for k, v in defaults.items():
        try:
            kit.set_editor_property(k, v)
        except Exception as e:
            actions.append({'kit_prop_fail': k, 'error': str(e)})
    save(kit)
    actions.append({'kit_config_seeded': kit_path})

# Create ability set if missing
set_path = '${PATHS.abilitySet}' if exists('${PATHS.abilitySet}') else None
if not set_path:
    factory = unreal.DataAssetFactory()
    lyra_set_cls = unreal.load_class(None, '/Script/LyraGame.LyraAbilitySet')
    if lyra_set_cls:
        factory.set_editor_property('DataAssetClass', lyra_set_cls)
        created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'DA_BW_AbilitySet_Gunslinger', '${PATHS.heroRoot}', lyra_set_cls, factory)
        save(created)
        set_path = '${PATHS.abilitySet}'
        actions.append({'create_ability_set': set_path, 'ok': bool(created)})
    else:
        actions.append({'create_ability_set': False, 'error': 'missing LyraAbilitySet'})

# Create hero DA if missing
hero_path = '${PATHS.heroData}' if exists('${PATHS.heroData}') else None
if not hero_path:
    factory = unreal.DataAssetFactory()
    hero_cls = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayHeroDataAsset')
    if hero_cls:
        factory.set_editor_property('DataAssetClass', hero_cls)
        created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'DA_BW_HeroData_Gunslinger', '${PATHS.heroRoot}', hero_cls, factory)
        save(created)
        hero_path = '${PATHS.heroData}'
        actions.append({'create_hero_da': hero_path, 'ok': bool(created)})
    else:
        actions.append({'create_hero_da': False, 'error': 'missing BwayHeroDataAsset'})

# Configure ability set grants. Preserve PlaceBuildable (InputTag.Ability.Buildable) for future 21b.
if set_path and exists(set_path):
    try:
        aset = unreal.load_asset(set_path)
        preserved_buildable = None
        for g in aset.get_editor_property('GrantedGameplayAbilities') or []:
            if tag_name(g.get_editor_property('InputTag')) == 'InputTag.Ability.Buildable':
                preserved_buildable = g
                break

        new_grants = []
        exports = []
        for entry in [${grantEntries}]:
            cls = unreal.load_class(None, entry['ability'])
            if not cls:
                actions.append({'grant_fail': entry, 'error': 'missing class'})
                continue
            exp = '(Ability="%s",AbilityLevel=1,InputTag=(TagName="%s"))' % (entry['ability'], entry['tag'])
            g = unreal.LyraAbilitySet_GameplayAbility()
            imported = g.import_text(exp)
            if not imported:
                actions.append({'grant_fail': entry, 'error': 'import_text failed', 'export': exp})
                continue
            new_grants.append(g)
            exports.append(exp)
            actions.append({'grant': entry})

        if preserved_buildable:
            new_grants.append(preserved_buildable)
            actions.append({'preserved_buildable_grant': True})

        aset.set_editor_property('GrantedGameplayAbilities', new_grants)
        save(aset)
        actions.append({'ability_set_saved': set_path, 'count': len(new_grants), 'exports': exports})
    except Exception as e:
        actions.append({'ability_set_error': str(e)})

# Configure hero DA
if hero_path and exists(hero_path):
    hero = unreal.load_asset(hero_path)
    hero.set_editor_property('DisplayName', unreal.Text('Rawlins'))
    try:
        hero.set_editor_property('HeroClass', unreal.HeroClass.FIGHTER)
        hero.set_editor_property('bUseCustomClassName', True)
        hero.set_editor_property('CustomClassName', unreal.Text('Shooter'))
        actions.append({'HeroClass': 'Shooter (custom)'})
    except Exception as e:
        actions.append({'HeroClass': 'skip', 'error': str(e)})

    stats = hero.get_editor_property('HeroStats')
    stats.set_editor_property('MaxHealth', 400.0)
    stats.set_editor_property('BaseDamage', 60.0)
    stats.set_editor_property('Armor', 0.0)
    stats.set_editor_property('MoveSpeed', 10.5)
    hero.set_editor_property('HeroStats', stats)

    attr = unreal.load_class(None, '${NATIVE.attributeSet}')
    if attr:
        hero.set_editor_property('AttributeSetClass', attr)

    if set_path and exists(set_path):
        aset = unreal.load_asset(set_path)
        hero.set_editor_property('AbilitySets', [aset])

    # Preserve canonical Gunslinger / Jail buildable on script reruns (21b+).
    # Clear only stale / non-Jail buildables when 21b has not wired yet.
    try:
        existing_buildable = hero.get_editor_property('BuildableDataAsset')
        existing_path = existing_buildable.get_path_name() if existing_buildable else None
        preserve = False
        if existing_path:
            lower = existing_path.lower()
            if ('jail' in lower) or ('gunslinger' in lower and 'buildable' in lower):
                preserve = True
        if preserve:
            actions.append({'buildable_data_asset_preserved_jail': existing_path})
        else:
            hero.set_editor_property('BuildableDataAsset', None)
            actions.append({'buildable_data_asset_cleared': existing_path})
    except Exception as e:
        actions.append({'buildable_probe': str(e)})

    save(hero)
    actions.append({'hero_configured': hero_path})

# Soft-wire kit config onto ability CDOs when possible
if kit_path and exists(kit_path):
    kit = unreal.load_asset(kit_path)
    for cls_path in ['${NATIVE.primary}', '${NATIVE.doubleDown}', '${NATIVE.powerShot}', '${NATIVE.slideShot}', '${NATIVE.blazingBarrage}']:
        cls = unreal.load_class(None, cls_path)
        if not cls:
            continue
        try:
            cdo = unreal.get_default_object(cls)
            cdo.set_editor_property('KitConfig', kit)
            actions.append({'kit_wired_to': cls_path})
        except Exception as e:
            actions.append({'kit_wire_skip': cls_path, 'error': str(e)})

# Ensure GFD
gfd_cls = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayGameFeatureData')
gfd = unreal.EditorAssetLibrary.load_asset('${PATHS.gfd}') if exists('${PATHS.gfd}') else None
if not gfd and gfd_cls:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('DataAssetClass', gfd_cls)
    gfd = unreal.AssetToolsHelpers.get_asset_tools().create_asset('Hero_Rawlins', '/Hero_Rawlins', gfd_cls, factory)
    actions.append({'create_gfd': bool(gfd)})
if gfd and hero_path and exists(hero_path):
    hero = unreal.load_asset(hero_path)
    gfd.set_editor_property('HeroDataAssets', [hero])
    save(gfd)
    actions.append({'gfd_wired': '${PATHS.gfd}'})

# Enable Hero_Rawlins on B_BW_Experience_Dev (append; do not replace existing)
exp = unreal.load_asset('${PATHS.experienceDev}')
if exp:
    cdo = unreal.get_default_object(exp.generated_class()) if hasattr(exp, 'generated_class') else exp
    plugins = list(cdo.get_editor_property('GameFeaturesToEnable') or [])
    changed = False
    for required in ['BreakawayCore', 'Hero_Spartacus', 'Hero_Alona', 'Hero_Morgan', 'Hero_Rawlins']:
        if required not in plugins:
            plugins.append(required)
            changed = True
            actions.append({'experience_add': required})
    if changed:
        cdo.set_editor_property('GameFeaturesToEnable', plugins)
        save(exp)
        actions.append({'experience_saved': True, 'plugins': plugins})
    else:
        actions.append({'experience_ok': plugins})
else:
    actions.append({'experience_missing': '${PATHS.experienceDev}'})

print('BWAY_JSON:' + json.dumps({'actions': actions}))
`;

  const fixResult = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: pyFix } });
  log.actions = text(fixResult.content);

  const afterResult = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: pyProbe } });
  log.after = text(afterResult.content);
  writeLog('setup_rawlins_21a_functional_log.json', log);
  console.log(JSON.stringify(log, null, 2));
  await client.close?.();
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
