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

SRC = '/BreakawayCore/Characters/Heroes/Argus'
for src in unreal.EditorAssetLibrary.list_assets(SRC, recursive=True, include_folder=False):
    dst = src.replace('/BreakawayCore/', '/Hero_Spartacus/')
    unreal.log('move ' + str(unreal.EditorAssetLibrary.rename_asset(src, dst)) + ' ' + src)

BUILDABLE_SRC = '/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_BuildableData_Argus'
for name in ['DA_BW_Buildable_FireCatapult', 'DA_BW_Buildable_DragonSpire']:
    path = '/Hero_Spartacus/Characters/Heroes/Argus/' + name
    if not unreal.EditorAssetLibrary.does_asset_exist(path) and unreal.EditorAssetLibrary.does_asset_exist(BUILDABLE_SRC):
        unreal.EditorAssetLibrary.duplicate_asset(BUILDABLE_SRC, path)

hero = unreal.load_asset('/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus')
if hero:
    hero.set_editor_property('DisplayName', 'Spartacus')
    manny = unreal.load_asset('/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny')
    if manny:
        hero.set_editor_property('HeroMesh', manny)
    fire = unreal.load_asset('/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_Buildable_FireCatapult')
    spire = unreal.load_asset('/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_Buildable_DragonSpire')
    bs = [a for a in [fire, spire] if a]
    hero.set_editor_property('BuildableDataAssets', bs)
    unreal.EditorAssetLibrary.save_loaded_asset(hero)
    unreal.log('hero fixed buildables=' + str(len(bs)))

gfd_class = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayGameFeatureData')
gfd = unreal.EditorAssetLibrary.load_asset('/Hero_Spartacus/Hero_Spartacus')
if not gfd and gfd_class:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('DataAssetClass', gfd_class)
    gfd = unreal.AssetToolsHelpers.get_asset_tools().create_asset('Hero_Spartacus', '/Hero_Spartacus', gfd_class, factory)
if gfd and hero:
    gfd.set_editor_property('HeroDataAssets', [unreal.SoftObjectPath(hero.get_path_name())])
    unreal.EditorAssetLibrary.save_loaded_asset(gfd)

remaining = len(unreal.EditorAssetLibrary.list_assets(SRC, recursive=True, include_folder=False))
dest = len(unreal.EditorAssetLibrary.list_assets('/Hero_Spartacus', recursive=True, include_folder=False))
unreal.log('remaining=' + str(remaining) + ' dest=' + str(dest))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'fix', version: '1' });
  await client.connect(transport);

  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join(''));

  const verify = await client.callTool({
    name: 'asset',
    arguments: {
      action: 'read_properties',
      assetPath: '/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus',
      includeValues: true,
    },
  });
  console.log(verify.content?.map((c) => c.text).join(''));

  const remaining = await client.callTool({
    name: 'asset',
    arguments: { action: 'list', directory: '/BreakawayCore/Characters/Heroes/Argus', recursive: true },
  });
  console.log('remaining:', remaining.content?.map((c) => c.text).join(''));

  writeFileSync('AI_Planning/gate1_fix_result.txt', fix.content?.map((c) => c.text).join('\n') ?? '');
  await client.close();
}

main();
