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

stale = [
 '/BreakawayCore/Characters/Heroes/Argus/DA_BW_HeroData_Argus',
 '/BreakawayCore/Characters/Heroes/Argus/hud_headshot_magnus',
]
for p in stale:
    if unreal.EditorAssetLibrary.does_asset_exist(p):
        ok = unreal.EditorAssetLibrary.delete_asset(p)
        unreal.log('delete ' + p + ' -> ' + str(ok))

hero = unreal.load_asset('/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus')
gfd_class = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayGameFeatureData')
gfd = unreal.EditorAssetLibrary.load_asset('/Hero_Spartacus/Hero_Spartacus')
if not gfd and gfd_class:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('DataAssetClass', gfd_class)
    gfd = unreal.AssetToolsHelpers.get_asset_tools().create_asset('Hero_Spartacus', '/Hero_Spartacus', gfd_class, factory)
if gfd and hero:
    gfd.set_editor_property('HeroDataAssets', [hero])
    unreal.EditorAssetLibrary.save_loaded_asset(gfd)
    unreal.log('GFD HeroDataAssets set')

remaining = unreal.EditorAssetLibrary.list_assets('/BreakawayCore/Characters/Heroes/Argus', recursive=True, include_folder=False)
dest = unreal.EditorAssetLibrary.list_assets('/Hero_Spartacus', recursive=True, include_folder=False)
unreal.log('remaining=' + str(len(remaining)) + ' dest=' + str(len(dest)))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'cleanup', version: '1' });
  await client.connect(transport);
  const out = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(out.content?.map((c) => c.text).join(''));
  const remaining = await client.callTool({
    name: 'asset',
    arguments: { action: 'list', directory: '/BreakawayCore/Characters/Heroes/Argus', recursive: true },
  });
  const dest = await client.callTool({
    name: 'asset',
    arguments: { action: 'list', directory: '/Hero_Spartacus', recursive: true },
  });
  console.log('remaining Argus:', remaining.content?.map((c) => c.text).join(''));
  console.log('Hero_Spartacus count:', dest.content?.map((c) => c.text).join('').match(/assetCount": (\d+)/)?.[1]);
  writeFileSync('AI_Planning/gate1_cleanup.txt', out.content?.map((c) => c.text).join('\n') ?? '');
  await client.close();
}

main();
