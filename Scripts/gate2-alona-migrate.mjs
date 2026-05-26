import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const py = `
import unreal

SRC = '/BreakawayCore/Characters/Heroes/Alona'
DST_ROOT = '/Hero_Alona/Characters/Heroes/Alona'
HERO_DA = DST_ROOT + '/DA_BW_HeroData_Alona'
BUILDABLE_SRC = DST_ROOT + '/DA_BW_BuildableData_Alona'
BUILDABLE_BP = DST_ROOT + '/Buildable/BP_BW_Alona_Buildable'
BUILDABLE_MESH = DST_ROOT + '/Buildable/SKM_BW_alona_buildable'

def log(msg):
    unreal.log(str(msg))

if not unreal.EditorAssetLibrary.does_directory_exist('/Hero_Alona'):
    log('ERROR: /Hero_Alona mount missing — enable Hero_Alona in uproject and restart editor')
else:
    moved = 0
    failed = []
    for src in sorted(unreal.EditorAssetLibrary.list_assets(SRC, recursive=True, include_folder=False), key=len, reverse=True):
        dst = src.replace('/BreakawayCore/', '/Hero_Alona/')
        if unreal.EditorAssetLibrary.does_asset_exist(dst):
            continue
        ok = unreal.EditorAssetLibrary.rename_asset(src, dst)
        if ok:
            moved += 1
        else:
            failed.append(src)

    # Remove misplaced legacy GFD under hero folder; recreate at plugin root.
    legacy_gfd = DST_ROOT + '/Hero_Alona'
    if unreal.EditorAssetLibrary.does_asset_exist(legacy_gfd):
        unreal.EditorAssetLibrary.delete_asset(legacy_gfd)
        log('deleted legacy GFD at hero folder')

    for name in ['DA_BW_Buildable_SunShrine', 'DA_BW_Buildable_Starlight']:
        path = DST_ROOT + '/' + name
        if not unreal.EditorAssetLibrary.does_asset_exist(path) and unreal.EditorAssetLibrary.does_asset_exist(BUILDABLE_SRC):
            unreal.EditorAssetLibrary.duplicate_asset(BUILDABLE_SRC, path)

    buildable_bp = unreal.load_asset(BUILDABLE_BP)
    buildable_class = buildable_bp.generated_class() if buildable_bp else None
    buildable_mesh = unreal.load_asset(BUILDABLE_MESH)
    if buildable_bp and buildable_class:
        cdo = unreal.get_default_object(buildable_class)
        cdo.set_editor_property('bPersistsBetweenRounds', True)
        unreal.EditorAssetLibrary.save_loaded_asset(buildable_bp)

    for da_path in [BUILDABLE_SRC, DST_ROOT + '/DA_BW_Buildable_SunShrine', DST_ROOT + '/DA_BW_Buildable_Starlight']:
        da = unreal.load_asset(da_path)
        if not da:
            continue
        if buildable_class:
            da.set_editor_property('BuildableActorClass', buildable_class)
        if buildable_mesh:
            da.set_editor_property('PrimaryBuildableMesh', buildable_mesh)
        unreal.EditorAssetLibrary.save_loaded_asset(da)

    hero = unreal.load_asset(HERO_DA)
    if hero:
        shrine = unreal.load_asset(DST_ROOT + '/DA_BW_Buildable_SunShrine')
        star = unreal.load_asset(DST_ROOT + '/DA_BW_Buildable_Starlight')
        buildables = [a for a in [shrine, star] if a]
        hero.set_editor_property('BuildableDataAssets', buildables)
        unreal.EditorAssetLibrary.save_loaded_asset(hero)
        log('hero buildables=' + str(len(buildables)))

    gfd_class = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayGameFeatureData')
    gfd = unreal.EditorAssetLibrary.load_asset('/Hero_Alona/Hero_Alona')
    if not gfd and gfd_class:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('DataAssetClass', gfd_class)
        gfd = unreal.AssetToolsHelpers.get_asset_tools().create_asset('Hero_Alona', '/Hero_Alona', gfd_class, factory)
    if gfd and hero:
        gfd.set_editor_property('HeroDataAssets', [hero])
        unreal.EditorAssetLibrary.save_loaded_asset(gfd)

    exp = unreal.load_asset('/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic')
    if exp:
        cdo = unreal.get_default_object(exp.generated_class())
        plugins = list(cdo.get_editor_property('GameFeaturesToEnable'))
        if 'Hero_Alona' not in plugins:
            plugins.append('Hero_Alona')
            cdo.set_editor_property('GameFeaturesToEnable', plugins)
        unreal.EditorAssetLibrary.save_loaded_asset(exp)

    # Delete any stale redirectors left in BreakawayCore
    for stale in unreal.EditorAssetLibrary.list_assets(SRC, recursive=True, include_folder=False):
        unreal.EditorAssetLibrary.delete_asset(stale)

    unreal.EditorAssetLibrary.save_directory('/Hero_Alona', only_if_is_dirty=False, recursive=True)
    remaining = len(unreal.EditorAssetLibrary.list_assets(SRC, recursive=True, include_folder=False))
    dest = len(unreal.EditorAssetLibrary.list_assets('/Hero_Alona', recursive=True, include_folder=False))
    log('moved=' + str(moved) + ' failed=' + str(len(failed)) + ' remaining=' + str(remaining) + ' dest=' + str(dest))
    if failed:
        log('failures: ' + str(failed[:10]))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'gate2-alona-migrate', version: '1.0.0' });
  await client.connect(transport);

  const migrate = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(migrate.content?.map((c) => c.text).join(''));

  const verify = {
    heroData: await client.callTool({
      name: 'asset',
      arguments: {
        action: 'read_properties',
        assetPath: '/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona',
        includeValues: true,
      },
    }),
    remaining: await client.callTool({
      name: 'asset',
      arguments: { action: 'list', directory: '/BreakawayCore/Characters/Heroes/Alona', recursive: true },
    }),
    pluginList: await client.callTool({
      name: 'asset',
      arguments: { action: 'list', directory: '/Hero_Alona', recursive: true },
    }),
  };

  writeFileSync('AI_Planning/gate2_alona_migrate.json', JSON.stringify({ migrate: migrate.raw, verify }, null, 2));

  const props = verify.heroData.parsed?.properties ?? [];
  console.log('Hero_Alona assets:', verify.pluginList.parsed?.assetCount);
  console.log('Remaining BreakawayCore Alona:', verify.remaining.parsed?.assetCount);
  console.log('BuildableDataAssets:', props.find((p) => p.name === 'BuildableDataAssets')?.value);

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
