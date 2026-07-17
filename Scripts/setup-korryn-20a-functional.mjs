/**
 * Step 20a — Wire Korryn (Hero_Morgan / Hexweaver) hero DA, ability set grants, kit config, GFD, and experience plugin enable.
 *
 * Requires Unreal Editor open with MCP bridge (ue-mcp) AFTER HeroMorganRuntime compiles.
 * Idempotent: safe to re-run.
 *
 * Usage:
 *   node Scripts/setup-korryn-20a-functional.mjs
 *   node Scripts/setup-korryn-20a-functional.mjs --probe-only
 */
import { createMcpClient, text, writeLog } from './lib/bway-widget-setup.mjs';

const PROBE_ONLY = process.argv.includes('--probe-only');

const PATHS = {
  heroRoot: '/Hero_Morgan/Characters/Heroes/Hexweaver',
  kitRoot: '/Hero_Morgan/Kit',
  heroData: '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver',
  abilitySet: '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_AbilitySet_Hexweaver',
  kitConfig: '/Hero_Morgan/Kit/DA_BW_KorrynKitConfig',
  gfd: '/Hero_Morgan/Hero_Morgan',
  experienceDev: '/BreakawayCore/Experiences/B_BW_Experience_Dev',
};

const NATIVE = {
  primary: '/Script/HeroMorganRuntime.BwayGameplayAbility_KorrynPrimary',
  flock: '/Script/HeroMorganRuntime.BwayGameplayAbility_KorrynFlock',
  burden: '/Script/HeroMorganRuntime.BwayGameplayAbility_KorrynBurdenOfSin',
  circle: '/Script/HeroMorganRuntime.BwayGameplayAbility_KorrynCircleOfSpite',
  aura: '/Script/HeroMorganRuntime.BwayGameplayAbility_KorrynAuraOfSilence',
  attributeSet: '/Script/BreakawayCoreRuntime.BwayHeroAttributeSet',
  kitConfigClass: '/Script/HeroMorganRuntime.BwayKorrynKitConfig',
};

const GRANTS = [
  { tag: 'InputTag.Ability.Primary', ability: NATIVE.primary },
  { tag: 'InputTag.Ability.Ability4', ability: NATIVE.flock },
  { tag: 'InputTag.Ability.Ability1', ability: NATIVE.burden },
  { tag: 'InputTag.Ability.Ability2', ability: NATIVE.circle },
  { tag: 'InputTag.Ability.Ability3', ability: NATIVE.aura },
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

def make_tag(tag_str):
    tag = unreal.GameplayTag()
    try:
        tag.set_editor_property('TagName', tag_str)
        return tag
    except Exception:
        pass
    try:
        tag.import_text('(TagName="%s")' % tag_str)
    except Exception:
        pass
    return tag

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
    return out

def kit_summary(path):
    if not path or not exists(path):
        return {'exists': False, 'path': path}
    kit = unreal.load_asset(path)
    out = {'exists': True, 'path': path}
    for prop in [
        'PrimaryBaseDamage', 'FlockCooldown', 'BurdenCooldown', 'CircleCooldown',
        'AuraCooldown', 'CircleRadius', 'AuraRadius', 'BurdenSlowMultiplier',
        'CircleSlowMultiplier', 'CircleIncomingDamageMultiplier'
    ]:
        try:
            out[prop] = float(kit.get_editor_property(prop))
        except Exception as e:
            out[prop] = 'error:' + str(e)
    return out
`;

async function main() {
  const client = await createMcpClient('setup-korryn-20a');
  const log = { probe: null, actions: [], after: null };

  const pyProbe = `
${PY_HELPERS}
report = {
  'hero': hero_summary('${PATHS.heroData}'),
  'abilitySet': ability_set_grants('${PATHS.abilitySet}'),
  'kitConfig': kit_summary('${PATHS.kitConfig}'),
  'gfdExists': exists('${PATHS.gfd}'),
  'pluginRootExists': unreal.EditorAssetLibrary.does_directory_exist('/Hero_Morgan'),
  'experience': experience_plugins('${PATHS.experienceDev}'),
  'nativeClasses': {
    k: bool(unreal.load_class(None, v)) for k, v in {
      'primary': '${NATIVE.primary}',
      'flock': '${NATIVE.flock}',
      'burden': '${NATIVE.burden}',
      'circle': '${NATIVE.circle}',
      'aura': '${NATIVE.aura}',
      'attributeSet': '${NATIVE.attributeSet}',
      'kitConfigClass': '${NATIVE.kitConfigClass}',
    }.items()
  },
}
print('BWAY_JSON:' + json.dumps(report))
`;

  const probeResult = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: pyProbe } });
  log.probe = text(probeResult.content);
  writeLog('setup_korryn_20a_probe.json', log);

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
for d in ['/Hero_Morgan', '/Hero_Morgan/Characters', '/Hero_Morgan/Characters/Heroes', '${PATHS.heroRoot}', '${PATHS.kitRoot}']:
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
            'DA_BW_KorrynKitConfig', '${PATHS.kitRoot}', kit_cls, factory)
        save(created)
        kit_path = '${PATHS.kitConfig}'
        actions.append({'create_kit_config': kit_path, 'ok': bool(created)})
    else:
        actions.append({'create_kit_config': False, 'error': 'missing BwayKorrynKitConfig'})
else:
    actions.append({'kit_config_exists': kit_path})

# Seed kit config sheet defaults (always refresh magnitudes for idempotent 20a)
if kit_path and exists(kit_path):
    kit = unreal.load_asset(kit_path)
    defaults = {
        'PrimaryBaseDamage': 22.0,
        'PrimaryDamageScaling': 0.3,
        'PrimaryProjectileSpeed': 2800.0,
        'PrimaryProjectileLifeSpan': 3.0,
        'ArmorShredPerHit': -2.0,
        'ArmorShredDuration': 2.0,
        'ArmorShredMaxStacks': 5,
        'FlockDuration': 1.5,
        'FlockCooldown': 22.0,
        'FlockHoverSpeed': 600.0,
        'BurdenBaseDamage': 33.0,
        'BurdenDamageScaling': 0.3,
        'BurdenCooldown': 14.0,
        'BurdenSlowDuration': 2.0,
        'BurdenSlowMultiplier': 0.5,
        'BurdenConeRange': 900.0,
        'BurdenConeHalfAngleDegrees': 35.0,
        'CircleCooldown': 20.0,
        'CircleDuration': 5.0,
        'CircleRadius': 700.0,
        'CircleSlowMultiplier': 0.85,
        'CircleIncomingDamageMultiplier': 1.35,
        'CircleSpawnForwardOffset': 400.0,
        'AuraBaseDamage': 13.0,
        'AuraDamageScaling': 0.5,
        'AuraCooldown': 30.0,
        'AuraRadius': 800.0,
        'AuraSilenceDuration': 5.0,
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
            'DA_BW_AbilitySet_Hexweaver', '${PATHS.heroRoot}', lyra_set_cls, factory)
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
            'DA_BW_HeroData_Hexweaver', '${PATHS.heroRoot}', hero_cls, factory)
        save(created)
        hero_path = '${PATHS.heroData}'
        actions.append({'create_hero_da': hero_path, 'ok': bool(created)})
    else:
        actions.append({'create_hero_da': False, 'error': 'missing BwayHeroDataAsset'})

# Configure ability set grants. Preserve PlaceBuildable (InputTag.Ability.Buildable) for future 20b.
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
    hero.set_editor_property('DisplayName', unreal.Text('Korryn'))
    try:
        hero.set_editor_property('HeroClass', unreal.HeroClass.SUPPORT)
        actions.append({'HeroClass': 'Support'})
    except Exception as e:
        try:
            hero.set_editor_property('HeroClass', unreal.HeroClass.SUPPORT)
        except Exception:
            pass
        actions.append({'HeroClass': 'skip', 'error': str(e)})

    stats = hero.get_editor_property('HeroStats')
    stats.set_editor_property('MaxHealth', 335.0)
    stats.set_editor_property('BaseDamage', 40.0)
    stats.set_editor_property('Armor', 0.0)
    stats.set_editor_property('MoveSpeed', 10.0)
    hero.set_editor_property('HeroStats', stats)

    attr = unreal.load_class(None, '${NATIVE.attributeSet}')
    if attr:
        hero.set_editor_property('AttributeSetClass', attr)

    if set_path and exists(set_path):
        aset = unreal.load_asset(set_path)
        hero.set_editor_property('AbilitySets', [aset])

    # Clear stale Elder Stone / non-Cursed-Ward buildable for 20a.
    # Preserve a future canonical Cursed Ward grant on script reruns (path contains CursedWard).
    try:
        existing_buildable = hero.get_editor_property('BuildableDataAsset')
        existing_path = existing_buildable.get_path_name() if existing_buildable else None
        if existing_path and ('CursedWard' in existing_path or 'Cursed_Ward' in existing_path):
            actions.append({'buildable_data_asset_preserved_cursed_ward': existing_path})
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
    for cls_path in ['${NATIVE.primary}', '${NATIVE.flock}', '${NATIVE.burden}', '${NATIVE.circle}', '${NATIVE.aura}']:
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
    gfd = unreal.AssetToolsHelpers.get_asset_tools().create_asset('Hero_Morgan', '/Hero_Morgan', gfd_cls, factory)
    actions.append({'create_gfd': bool(gfd)})
if gfd and hero_path and exists(hero_path):
    hero = unreal.load_asset(hero_path)
    gfd.set_editor_property('HeroDataAssets', [hero])
    save(gfd)
    actions.append({'gfd_wired': '${PATHS.gfd}'})

# Enable Hero_Morgan on B_BW_Experience_Dev
exp = unreal.load_asset('${PATHS.experienceDev}')
if exp:
    cdo = unreal.get_default_object(exp.generated_class()) if hasattr(exp, 'generated_class') else exp
    plugins = list(cdo.get_editor_property('GameFeaturesToEnable') or [])
    changed = False
    for required in ['BreakawayCore', 'Hero_Spartacus', 'Hero_Alona', 'Hero_Morgan']:
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
  writeLog('setup_korryn_20a_functional_log.json', log);
  console.log(JSON.stringify(log, null, 2));
  await client.close?.();
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
