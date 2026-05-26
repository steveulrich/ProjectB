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
  const client = new Client({ name: 'gate2-alona-probe', version: '1.0.0' });
  await client.connect(transport);

  const out = {};
  out.status = await callTool(client, 'project', { action: 'get_status' });
  out.alonaSearch = await callTool(client, 'asset', {
    action: 'list',
    directory: '/BreakawayCore/Characters/Heroes/Alona',
    recursive: true,
  });
  out.heroData = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: '/BreakawayCore/Characters/Heroes/Alona/DA_BW_HeroData_Alona',
    includeValues: true,
  });
  out.buildable = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: '/BreakawayCore/Characters/Heroes/Alona/DA_BW_BuildableData_Alona',
    includeValues: true,
  });
  out.pluginSearch = await callTool(client, 'asset', {
    action: 'list',
    directory: '/Hero_Alona',
    recursive: true,
  });

  writeFileSync('AI_Planning/gate2_alona_probe.json', JSON.stringify(out, null, 2));
  console.log('Alona assets:', out.alonaSearch.parsed?.assetCount ?? out.alonaSearch.parsed);
  console.log('Hero_Alona assets:', out.pluginSearch.parsed?.assetCount ?? out.pluginSearch.parsed);
  console.log('Wrote AI_Planning/gate2_alona_probe.json');

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
