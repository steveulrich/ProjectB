/**
 * Step 19a — Wire Alona hero DA, ability set grants, GFD, and experience plugin enable.
 *
 * Requires Unreal Editor open with MCP bridge (ue-mcp) AFTER HeroAlonaRuntime compiles.
 * Idempotent: safe to re-run.
 *
 * Usage:
 *   node Scripts/setup-alona-19a-functional.mjs
 *   node Scripts/setup-alona-19a-functional.mjs --probe-only
 */
import { createMcpClient, text, writeLog } from './lib/bway-widget-setup.mjs';

const PROBE_ONLY = process.argv.includes('--probe-only');

const PATHS = {
  heroRoot: '/Hero_Alona/Characters/Heroes/Alona',
  legacyRoot: '/BreakawayCore/Characters/Heroes/Alona',
  heroData: '/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona',
  legacyHeroData: '/BreakawayCore/Characters/Heroes/Alona/DA_BW_HeroData_Alona',
  abilitySet: '/Hero_Alona/Characters/Heroes/Alona/DA_BW_AbilitySet_Alona',
  legacyAbilitySet: '/BreakawayCore/Characters/Heroes/Alona/DA_BW_AbilitySet_Alona',
  gfd: '/Hero_Alona/Hero_Alona',
  experienceDev: '/BreakawayCore/Experiences/B_BW_Experience_Dev',
};

const NATIVE = {
  primary: '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaPrimary',
  sunsGrace: '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaSunsGrace',
  radiance: '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaRadiance',
  sunBurst: '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaSunBurst',
  blessing: '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaBlessing',
  attributeSet: '/Script/BreakawayCoreRuntime.BwayHeroAttributeSet',
};

const GRANTS = [
  { tag: 'InputTag.Ability.Primary', ability: NATIVE.primary },
  { tag: 'InputTag.Ability.Ability4', ability: NATIVE.sunsGrace },
  { tag: 'InputTag.Ability.Ability1', ability: NATIVE.radiance },
  { tag: 'InputTag.Ability.Ability2', ability: NATIVE.sunBurst },
  { tag: 'InputTag.Ability.Ability3', ability: NATIVE.blessing },
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
        return tag
    except Exception:
        return tag

def resolve_hero_path():
    if exists('${PATHS.heroData}'):
        return '${PATHS.heroData}'
    if exists('${PATHS.legacyHeroData}'):
        return '${PATHS.legacyHeroData}'
    return None

def resolve_set_path():
    if exists('${PATHS.abilitySet}'):
        return '${PATHS.abilitySet}'
    if exists('${PATHS.legacyAbilitySet}'):
        return '${PATHS.legacyAbilitySet}'
    return None

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
    return out
`;

async function main() {
  const client = await createMcpClient('setup-alona-19a');
  const log = { probe: null, actions: [], after: null };

  const pyProbe = `
${PY_HELPERS}
report = {
  'hero': hero_summary(resolve_hero_path()),
  'abilitySet': ability_set_grants(resolve_set_path()),
  'gfdExists': exists('${PATHS.gfd}'),
  'pluginRootExists': unreal.EditorAssetLibrary.does_directory_exist('/Hero_Alona'),
  'experience': experience_plugins('${PATHS.experienceDev}'),
  'nativeClasses': {
    k: bool(unreal.load_class(None, v)) for k, v in {
      'primary': '${NATIVE.primary}',
      'sunsGrace': '${NATIVE.sunsGrace}',
      'radiance': '${NATIVE.radiance}',
      'sunBurst': '${NATIVE.sunBurst}',
      'blessing': '${NATIVE.blessing}',
      'attributeSet': '${NATIVE.attributeSet}',
    }.items()
  },
}
print('BWAY_JSON:' + json.dumps(report))
`;

  const probeResult = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: pyProbe } });
  log.probe = text(probeResult.content);
  writeLog('setup_alona_19a_probe.json', log);

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
for d in ['/Hero_Alona', '/Hero_Alona/Characters', '/Hero_Alona/Characters/Heroes', '${PATHS.heroRoot}']:
    if not unreal.EditorAssetLibrary.does_directory_exist(d):
        unreal.EditorAssetLibrary.make_directory(d)
        actions.append({'mkdir': d})

hero_path = resolve_hero_path()
set_path = resolve_set_path()

# Create ability set if missing
if not set_path:
    factory = unreal.DataAssetFactory()
    lyra_set_cls = unreal.load_class(None, '/Script/LyraGame.LyraAbilitySet')
    if lyra_set_cls:
        factory.set_editor_property('DataAssetClass', lyra_set_cls)
        created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'DA_BW_AbilitySet_Alona', '${PATHS.heroRoot}', lyra_set_cls, factory)
        save(created)
        set_path = '${PATHS.abilitySet}'
        actions.append({'create_ability_set': set_path, 'ok': bool(created)})
    else:
        actions.append({'create_ability_set': False, 'error': 'missing LyraAbilitySet'})

# Create hero DA if missing
if not hero_path:
    factory = unreal.DataAssetFactory()
    hero_cls = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayHeroDataAsset')
    if hero_cls:
        factory.set_editor_property('DataAssetClass', hero_cls)
        created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'DA_BW_HeroData_Alona', '${PATHS.heroRoot}', hero_cls, factory)
        save(created)
        hero_path = '${PATHS.heroData}'
        actions.append({'create_hero_da': hero_path, 'ok': bool(created)})
    else:
        actions.append({'create_hero_da': False, 'error': 'missing BwayHeroDataAsset'})

# Configure ability set grants via import_text (Ability cannot be set_editor_property on instances).
# Preserve any existing PlaceBuildable grant (InputTag.Ability.Buildable) so re-running 19a after 19b is safe.
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
            # Native C++ SoftClassPath: /Script/Module.ClassName (no _C suffix)
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
    hero.set_editor_property('DisplayName', unreal.Text('Alona'))
    try:
        hero.set_editor_property('HeroClass', unreal.HeroClass.SUPPORT)
        actions.append({'HeroClass': 'Support'})
    except Exception as e:
        actions.append({'HeroClass': 'skip', 'error': str(e)})

    stats = hero.get_editor_property('HeroStats')
    stats.set_editor_property('MaxHealth', 350.0)
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

    # Do not clear BuildableDataAsset — 19b may have wired Sun Shrine; re-running 19a must preserve it.
    try:
        existing_buildable = hero.get_editor_property('BuildableDataAsset')
        actions.append({
            'buildable_data_asset_preserved': existing_buildable.get_path_name() if existing_buildable else None
        })
    except Exception as e:
        actions.append({'buildable_probe': str(e)})

    save(hero)
    actions.append({'hero_configured': hero_path})

# Ensure GFD
gfd_cls = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayGameFeatureData')
gfd = unreal.EditorAssetLibrary.load_asset('${PATHS.gfd}') if exists('${PATHS.gfd}') else None
if not gfd and gfd_cls:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('DataAssetClass', gfd_cls)
    gfd = unreal.AssetToolsHelpers.get_asset_tools().create_asset('Hero_Alona', '/Hero_Alona', gfd_cls, factory)
    actions.append({'create_gfd': bool(gfd)})
if gfd and hero_path and exists(hero_path):
    hero = unreal.load_asset(hero_path)
    gfd.set_editor_property('HeroDataAssets', [hero])
    save(gfd)
    actions.append({'gfd_wired': '${PATHS.gfd}'})

# Enable Hero_Alona on B_BW_Experience_Dev (keep BreakawayCore + Hero_Spartacus)
exp = unreal.load_asset('${PATHS.experienceDev}')
if exp:
    cdo = unreal.get_default_object(exp.generated_class()) if hasattr(exp, 'generated_class') else exp
    plugins = list(cdo.get_editor_property('GameFeaturesToEnable') or [])
    changed = False
    for required in ['BreakawayCore', 'Hero_Spartacus', 'Hero_Alona']:
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
  writeLog('setup_alona_19a_functional_log.json', log);
  console.log(JSON.stringify(log, null, 2));
  await client.close?.();
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
