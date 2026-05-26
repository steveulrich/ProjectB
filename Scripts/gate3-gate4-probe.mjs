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
  try {
    return { ok: !result.isError, parsed: JSON.parse(text), raw: text };
  } catch {
    return { ok: !result.isError, parsed: text, raw: text };
  }
}

async function main() {
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'gate3-gate4-probe', version: '1.0.0' });
  await client.connect(transport);

  const py = `
import unreal
mounts = {}
for m in ['/Hero_Morgan', '/Hero_Rawlins', '/Hero_Alona']:
    mounts[m] = unreal.EditorAssetLibrary.does_directory_exist(m)
unreal.log('MOUNTS=' + str(mounts))
`;

  const out = {};
  out.status = await callTool(client, 'project', { action: 'get_status' });
  out.mounts = await callTool(client, 'editor', { action: 'execute_python', code: py });
  out.morgan = await callTool(client, 'asset', { action: 'list', directory: '/Hero_Morgan', recursive: true });
  out.rawlins = await callTool(client, 'asset', { action: 'list', directory: '/Hero_Rawlins', recursive: true });

  writeFileSync('AI_Planning/gate3_gate4_probe.json', JSON.stringify(out, null, 2));
  console.log(JSON.stringify(out, null, 2));

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
