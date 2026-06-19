import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const ASSET_PATH = '/BreakawayCore/UI/Match/W_BW_CaptureTheRelic_ScoreWidget';

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'save-ctr-score-widget', version: '1.0.0' });
  await client.connect(transport);

  const compile = await client.callTool({
    name: 'blueprint',
    arguments: { action: 'compile', assetPath: ASSET_PATH },
  });
  const save = await client.callTool({
    name: 'asset',
    arguments: { action: 'save', assetPath: ASSET_PATH },
  });

  console.log(compile.content?.map((c) => c.text).join('\n'));
  console.log(save.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
