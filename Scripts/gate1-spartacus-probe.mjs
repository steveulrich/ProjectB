import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT = 'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href);

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
  const client = new Client({ name: 'gate1-spartacus', version: '1.0.0' });
  await client.connect(transport);

  const out = {};

  out.project = await callTool(client, 'project', { action: 'get_status' });

  out.argusSearch = await callTool(client, 'asset', {
    action: 'search',
    query: 'Argus',
    directory: '/BreakawayCore/Characters/Heroes/Argus',
    maxResults: 100,
  });

  out.heroData = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: '/BreakawayCore/Characters/Heroes/Argus/DA_BW_HeroData_Argus',
    includeValues: true,
  });

  out.abilitySet = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: '/BreakawayCore/Characters/Heroes/Argus/DA_BW_AbilitySet_Argus',
    includeValues: true,
  });

  out.buildable = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: '/BreakawayCore/Characters/Heroes/Argus/DA_BW_BuildableData_Argus',
    includeValues: true,
  });

  out.spartacusPluginSearch = await callTool(client, 'asset', {
    action: 'search',
    query: '',
    directory: '/Hero_Spartacus',
    maxResults: 20,
  });

  out.experienceDeps = await callTool(client, 'blueprint', {
    action: 'get_dependencies',
    assetPath: '/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic',
  });

  // Probe asset actions available via invalid call message or list tools
  out.moveProbe = await callTool(client, 'asset', {
    action: 'list',
    directory: '/BreakawayCore/Characters/Heroes/Argus',
    maxResults: 50,
  });

  writeFileSync('AI_Planning/gate1_spartacus_probe.json', JSON.stringify(out, null, 2));
  console.log('Wrote AI_Planning/gate1_spartacus_probe.json');
  console.log('Argus assets:', out.argusSearch.parsed?.resultCount ?? out.argusSearch.parsed);
  console.log('Hero_Spartacus assets:', out.spartacusPluginSearch.parsed?.resultCount ?? out.spartacusPluginSearch.parsed);

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
