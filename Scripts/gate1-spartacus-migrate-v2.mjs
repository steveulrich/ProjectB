import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const UPROJECT = 'E:/Unreal Projects/ProjectB/ProjectB.uproject';

async function callTool(client, name, args) {
  const result = await client.callTool({ name, arguments: args });
  const text = result.content?.map((c) => c.text).join('\n') ?? '';
  let parsed;
  try {
    parsed = JSON.parse(text);
  } catch {
    parsed = text;
  }
  const bridgeOk = parsed?.success !== false && !parsed?.error;
  return { ok: !result.isError && bridgeOk, parsed, raw: text };
}

const migratePy = `
import unreal

SRC_ROOT = '/BreakawayCore/Characters/Heroes/Argus'
DST_ROOT = '/Hero_Spartacus/Characters/Heroes/Argus'
HERO_DA = DST_ROOT + '/DA_BW_HeroData_Argus'
BUILDABLE_SRC = DST_ROOT + '/DA_BW_BuildableData_Argus'
BUILDABLE_FIRE = DST_ROOT + '/DA_BW_Buildable_FireCatapult'
BUILDABLE_SPIRE = DST_ROOT + '/DA_BW_Buildable_DragonSpire'
MANNY_MESH = '/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny'

def log(msg):
    unreal.log(str(msg))

def move_asset(name):
    src = SRC_ROOT + '/' + name if '/' not in name else name
    if src.startswith(SRC_ROOT + '/' + SRC_ROOT):
        src = src.replace(SRC_ROOT + '/', SRC_ROOT + '/', 1)
    dst = src.replace('/BreakawayCore/', '/Hero_Spartacus/')
    if not unreal.EditorAssetLibrary.does_asset_exist(src):
        return False, 'missing:' + src
    if unreal.EditorAssetLibrary.does_asset_exist(dst):
        return True, 'exists:' + dst
    ok = unreal.EditorAssetLibrary.rename_asset(src, dst)
    return ok, src + ' -> ' + dst

asset_names = [
    'Abilities/GameplayEffects/GE_BW_Spartacus_DefensiveStance',
    'Abilities/GameplayEffects/GE_BW_Spartacus_GladiatorsLeap_Damage',
    'Abilities/GameplayEffects/GE_BW_Spartacus_ShieldBash_Stun',
    'Abilities/GameplayEffects/GE_BW_Spartacus_WarCry_Buff',
    'Abilities/GA_BW_Spartacus_DefensiveStance',
    'Abilities/GA_BW_Spartacus_GladiatorsLeap',
    'Abilities/GA_BW_Spartacus_ShieldBash',
    'Abilities/GA_BW_Spartacus_WarCry',
    'OldAbilities/Icons/icon_argus_ability1',
    'OldAbilities/Icons/ICON_Character_Argus',
    'OldAbilities/Icons/icon_magnus_ability2',
    'OldAbilities/Icons/icon_magnus_ability3',
    'OldAbilities/Icons/icon_magnus_basicattack',
    'OldAbilities/Icons/icon_magnus_buildable1',
    'OldAbilities/Icons/icon_magnus_buildable2',
    'OldAbilities/Icons/icon_magnus_defense',
    'UI/icon_argus_ability1',
    'UI/ICON_Character_Argus',
    'UI/icon_magnus_ability2',
    'UI/icon_magnus_ability3',
    'UI/icon_magnus_basicattack',
    'UI/icon_magnus_buildable1',
    'UI/icon_magnus_buildable2',
    'UI/icon_magnus_defense',
    'B_BW_Character_Argus',
    'DA_BW_AbilitySet_Argus',
    'DA_BW_BuildableData_Argus',
    'DA_BW_HeroData_Argus',
    'hud_headshot_argus',
    'hud_headshot_magnus',
    'PD_BW_Argus',
]

moved = []
failed = []
for rel in sorted(asset_names, key=len, reverse=True):
    ok, info = move_asset(rel)
    (moved if ok else failed).append(info)

# Duplicate buildables
for dest_name, path in [('FireCatapult', BUILDABLE_FIRE), ('DragonSpire', BUILDABLE_SPIRE)]:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        log('Buildable exists: ' + path)
        continue
    if not unreal.EditorAssetLibrary.does_asset_exist(BUILDABLE_SRC):
        failed.append('buildable source missing: ' + BUILDABLE_SRC)
        continue
    dup = unreal.EditorAssetLibrary.duplicate_asset(BUILDABLE_SRC, path)
    if dup:
        moved.append('dup ' + path)
    else:
        failed.append('dup failed ' + path)

# Fix hero DA
hero = unreal.load_asset(HERO_DA)
if hero:
    hero.set_editor_property('DisplayName', unreal.Text.from_string('Spartacus'))
    manny = unreal.load_asset(MANNY_MESH)
    if manny:
        hero.set_editor_property('HeroMesh', manny)
    fire = unreal.load_asset(BUILDABLE_FIRE)
    spire = unreal.load_asset(BUILDABLE_SPIRE)
    buildables = [a for a in [fire, spire] if a]
    hero.set_editor_property('BuildableDataAssets', buildables)
    unreal.EditorAssetLibrary.save_loaded_asset(hero)
    log('Hero DA fixed: Spartacus / buildables=' + str(len(buildables)))
else:
    failed.append('Hero DA missing at ' + HERO_DA)

# GFD
package = '/Hero_Spartacus'
gfd_path = package + '/Hero_Spartacus'
gfd_class = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayGameFeatureData')
gfd = unreal.EditorAssetLibrary.load_asset(gfd_path)
if not gfd and gfd_class:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('DataAssetClass', gfd_class)
    gfd = unreal.AssetToolsHelpers.get_asset_tools().create_asset('Hero_Spartacus', package, gfd_class, factory)
if gfd and hero:
    gfd.set_editor_property('HeroDataAssets', [unreal.SoftObjectPath(hero.get_path_name())])
    unreal.EditorAssetLibrary.save_loaded_asset(gfd)
    log('GFD saved')

# Experience GF enable (idempotent)
exp = unreal.load_asset('/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic')
if exp:
    cdo = unreal.get_default_object(exp.generated_class())
    plugins = list(cdo.get_editor_property('GameFeaturesToEnable'))
    if 'Hero_Spartacus' not in plugins:
        plugins.append('Hero_Spartacus')
        cdo.set_editor_property('GameFeaturesToEnable', plugins)
    unreal.EditorAssetLibrary.save_loaded_asset(exp)
    log('Experience GameFeaturesToEnable updated')

unreal.EditorAssetLibrary.save_directory('/Hero_Spartacus', only_if_is_dirty=False, recursive=True)

result = {
    'moved_count': len(moved),
    'failed_count': len(failed),
    'moved': moved[:5],
    'failed': failed,
    'hero_exists': unreal.EditorAssetLibrary.does_asset_exist(HERO_DA),
    'remaining_src': unreal.EditorAssetLibrary.list_assets(SRC_ROOT, recursive=True, include_folder=False),
}
log('MIGRATION_RESULT=' + str(result))
`;

async function main() {
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'gate1-spartacus-migrate-v2', version: '1.0.0' });
  await client.connect(transport);

  const migrate = await callTool(client, 'editor', { action: 'execute_python', code: migratePy });
  console.log(migrate.raw);

  const verify = {};
  verify.pluginList = await callTool(client, 'asset', {
    action: 'list',
    directory: '/Hero_Spartacus',
    recursive: true,
  });
  verify.heroData = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: '/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus',
    includeValues: true,
  });
  verify.remaining = await callTool(client, 'asset', {
    action: 'list',
    directory: '/BreakawayCore/Characters/Heroes/Argus',
    recursive: true,
  });

  const out = { migrate, verify };
  writeFileSync('AI_Planning/gate1_spartacus_migrate_v2.json', JSON.stringify(out, null, 2));
  console.log('Hero_Spartacus assetCount:', verify.pluginList.parsed?.assetCount);
  console.log('Remaining Argus assetCount:', verify.remaining.parsed?.assetCount);
  if (verify.heroData.parsed?.properties) {
    const dn = verify.heroData.parsed.properties.find((p) => p.name === 'DisplayName');
    const ba = verify.heroData.parsed.properties.find((p) => p.name === 'BuildableDataAssets');
    console.log('DisplayName:', dn?.value);
    console.log('BuildableDataAssets:', ba?.value);
  }

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
