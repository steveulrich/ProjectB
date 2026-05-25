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
  return result.content?.map((c) => c.text).join('\n') ?? '';
}

const py = `
import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.scan_paths_synchronous(['/Hero_Spartacus'], True)
checks = [
 '/BreakawayCore/Characters/Heroes/Argus/DA_BW_HeroData_Argus',
 '/Hero_Spartacus',
]
for p in checks:
    unreal.log(p + ' dir=' + str(unreal.EditorAssetLibrary.does_directory_exist(p)) + ' asset=' + str(unreal.EditorAssetLibrary.does_asset_exist(p)))
try:
    names = unreal.PluginBlueprintLibrary.get_enabled_plugin_names()
    hero = [n for n in names if 'Hero' in n or 'Spartacus' in n]
    unreal.log('hero plugins enabled: ' + str(hero))
except Exception as e:
    unreal.log('plugin query failed: ' + str(e))
`;

async function main() {
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'diag', version: '1' });
  await client.connect(transport);
  const out = await callTool(client, 'editor', { action: 'execute_python', code: py });
  writeFileSync('AI_Planning/gate1_plugin_diag.txt', out);
  console.log(out);
  await client.close();
}

main();
