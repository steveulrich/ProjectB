import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const py = `
import unreal

def log(msg):
    unreal.log('[enable-plugins] ' + str(msg))

for plugin in ['Hero_Morgan', 'Hero_Rawlins']:
    try:
        unreal.SystemLibrary.execute_console_command(None, 'EnablePlugin ' + plugin)
        log('EnablePlugin ' + plugin)
    except Exception as e:
        log('console failed ' + plugin + ': ' + str(e))

mounts = {}
for m in ['/Hero_Morgan', '/Hero_Rawlins']:
    mounts[m] = unreal.EditorAssetLibrary.does_directory_exist(m)
log('MOUNTS=' + str(mounts))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'enable-hero-plugins', version: '1.0.0' });
  await client.connect(transport);
  const r = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(r.content?.map((c) => c.text).join(''));
  await client.close();
}

main().catch(console.error);
