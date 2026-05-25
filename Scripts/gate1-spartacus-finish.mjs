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
  return { ok: !result.isError, parsed, raw: text };
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

def make_text(s):
    return unreal.KismetTextLibrary.make_literal_text(s)

if not unreal.EditorAssetLibrary.does_directory_exist('/Hero_Spartacus'):
    log('ERROR: /Hero_Spartacus mount missing')
else:
    rel_paths = unreal.EditorAssetLibrary.list_assets(SRC_ROOT, recursive=True, include_folder=False)
    moved = []
    failed = []
    for src in sorted(rel_paths, key=len, reverse=True):
        dst = src.replace('/BreakawayCore/', '/Hero_Spartacus/')
        if unreal.EditorAssetLibrary.does_asset_exist(dst):
            moved.append('exists ' + dst)
            continue
        ok = unreal.EditorAssetLibrary.rename_asset(src, dst)
        (moved if ok else failed).append(src + ' -> ' + dst)

    for dest_path in [BUILDABLE_FIRE, BUILDABLE_SPIRE]:
        if unreal.EditorAssetLibrary.does_asset_exist(dest_path):
            continue
        if not unreal.EditorAssetLibrary.does_asset_exist(BUILDABLE_SRC):
            failed.append('buildable source missing')
            continue
        if unreal.EditorAssetLibrary.duplicate_asset(BUILDABLE_SRC, dest_path):
            moved.append('dup ' + dest_path)
        else:
            failed.append('dup failed ' + dest_path)

    hero = unreal.load_asset(HERO_DA)
    if hero:
        hero.set_editor_property('DisplayName', make_text('Spartacus'))
        manny = unreal.load_asset(MANNY_MESH)
        if manny:
            hero.set_editor_property('HeroMesh', manny)
        fire = unreal.load_asset(BUILDABLE_FIRE)
        spire = unreal.load_asset(BUILDABLE_SPIRE)
        buildables = [a for a in [fire, spire] if a]
        hero.set_editor_property('BuildableDataAssets', buildables)
        unreal.EditorAssetLibrary.save_loaded_asset(hero)
        log('Hero DA fixed')

    gfd_class = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayGameFeatureData')
    gfd = unreal.EditorAssetLibrary.load_asset('/Hero_Spartacus/Hero_Spartacus')
    if not gfd and gfd_class:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('DataAssetClass', gfd_class)
        gfd = unreal.AssetToolsHelpers.get_asset_tools().create_asset('Hero_Spartacus', '/Hero_Spartacus', gfd_class, factory)
    if gfd and hero:
        gfd.set_editor_property('HeroDataAssets', [unreal.SoftObjectPath(hero.get_path_name())])
        unreal.EditorAssetLibrary.save_loaded_asset(gfd)
        log('GFD saved')

    exp = unreal.load_asset('/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic')
    if exp:
        cdo = unreal.get_default_object(exp.generated_class())
        plugins = list(cdo.get_editor_property('GameFeaturesToEnable'))
        if 'Hero_Spartacus' not in plugins:
            plugins.append('Hero_Spartacus')
            cdo.set_editor_property('GameFeaturesToEnable', plugins)
        unreal.EditorAssetLibrary.save_loaded_asset(exp)

    unreal.EditorAssetLibrary.save_directory('/Hero_Spartacus', only_if_is_dirty=False, recursive=True)
    remaining = unreal.EditorAssetLibrary.list_assets(SRC_ROOT, recursive=True, include_folder=False)
    dest_count = len(unreal.EditorAssetLibrary.list_assets('/Hero_Spartacus', recursive=True, include_folder=False))
    log('moved=' + str(len(moved)) + ' failed=' + str(len(failed)) + ' remaining=' + str(len(remaining)) + ' dest=' + str(dest_count))
    if failed:
        log('failures: ' + str(failed[:15]))
`;

async function main() {
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'gate1-finish', version: '1.0.0' });
  await client.connect(transport);

  const migrate = await callTool(client, 'editor', { action: 'execute_python', code: migratePy });
  console.log(migrate.raw);

  const verify = {
    pluginList: await callTool(client, 'asset', {
      action: 'list',
      directory: '/Hero_Spartacus',
      recursive: true,
    }),
    heroData: await callTool(client, 'asset', {
      action: 'read_properties',
      assetPath: '/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus',
      includeValues: true,
    }),
    remaining: await callTool(client, 'asset', {
      action: 'list',
      directory: '/BreakawayCore/Characters/Heroes/Argus',
      recursive: true,
    }),
  };

  writeFileSync('AI_Planning/gate1_spartacus_finish.json', JSON.stringify({ migrate, verify }, null, 2));
  console.log('Hero_Spartacus assets:', verify.pluginList.parsed?.assetCount);
  console.log('Remaining Argus:', verify.remaining.parsed?.assetCount);
  const props = verify.heroData.parsed?.properties ?? [];
  console.log('DisplayName:', props.find((p) => p.name === 'DisplayName')?.value);
  console.log('BuildableDataAssets:', props.find((p) => p.name === 'BuildableDataAssets')?.value);
  console.log('HeroMesh:', props.find((p) => p.name === 'HeroMesh')?.value);

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
