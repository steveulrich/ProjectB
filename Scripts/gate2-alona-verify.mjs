import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

async function callTool(client, name, args) {
  const result = await client.callTool({ name, arguments: args });
  const text = result.content?.map((c) => c.text).join('\n') ?? '';
  try {
    return JSON.parse(text);
  } catch {
    return text;
  }
}

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'gate2-verify', version: '1' });
  await client.connect(transport);

  const out = {
    heroData: await callTool(client, 'asset', {
      action: 'read_properties',
      assetPath: '/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona',
      includeValues: true,
    }),
    buildableSun: await callTool(client, 'asset', {
      action: 'read_properties',
      assetPath: '/Hero_Alona/Characters/Heroes/Alona/DA_BW_Buildable_SunShrine',
      includeValues: true,
    }),
    pluginList: await callTool(client, 'asset', {
      action: 'list',
      directory: '/Hero_Alona',
      recursive: true,
    }),
    remainingAlona: await callTool(client, 'asset', {
      action: 'list',
      directory: '/BreakawayCore/Characters/Heroes/Alona',
      recursive: true,
    }),
    heroSearch: await callTool(client, 'asset', {
      action: 'search',
      query: 'Alona',
      directory: '/Hero_Alona/Characters/Heroes',
      maxResults: 5,
    }),
  };

  writeFileSync('AI_Planning/gate2_alona_verify.json', JSON.stringify(out, null, 2));
  console.log(JSON.stringify({
    pluginAssets: out.pluginList?.assetCount,
    remainingBC: out.remainingAlona?.assetCount,
    displayName: out.heroData?.properties?.find((p) => p.name === 'DisplayName')?.value,
    buildables: out.heroData?.properties?.find((p) => p.name === 'BuildableDataAssets')?.value,
    buildableActorClass: out.buildableSun?.properties?.find((p) => p.name === 'BuildableActorClass')?.value,
  }, null, 2));

  await client.close();
}

main();
