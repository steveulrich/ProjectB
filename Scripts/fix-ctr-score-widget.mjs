import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const UPROJECT = 'E:/Unreal Projects/ProjectB/ProjectB.uproject';
const ASSET_PATH = '/BreakawayCore/UI/Match/W_BW_CaptureTheRelic_ScoreWidget';

function text(content) {
  return content?.map?.((c) => c.text).join('\n') ?? String(content ?? '');
}

async function callTool(client, name, args) {
  const result = await client.callTool({ name, arguments: args });
  const raw = text(result.content);
  try {
    return JSON.parse(raw);
  } catch {
    return { raw, isError: result.isError };
  }
}

async function main() {
  const log = [];
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'fix-ctr-score-widget', version: '1.0.0' });
  await client.connect(transport);

  log.push({
    step: 'blueprint_read',
    result: await callTool(client, 'blueprint', { action: 'read', assetPath: ASSET_PATH }),
  });

  log.push({
    step: 'move_Team2Pips',
    result: await callTool(client, 'widget', {
      action: 'move_widget',
      assetPath: ASSET_PATH,
      widgetName: 'Team2Pips',
      newParentWidgetName: 'Team2Column',
    }),
  });

  log.push({
    step: 'cdo_bShowRoundTimer',
    result: await callTool(client, 'blueprint', {
      action: 'set_class_default',
      assetPath: ASSET_PATH,
      propertyName: 'bShowRoundTimer',
      value: 'true',
    }),
  });

  log.push({
    step: 'cdo_TimerPollInterval',
    result: await callTool(client, 'blueprint', {
      action: 'set_class_default',
      assetPath: ASSET_PATH,
      propertyName: 'TimerPollInterval',
      value: '1.0',
    }),
  });

  for (const name of ['Text_Team1Score', 'Text_Team2Score', 'Text_Timer']) {
    log.push({
      step: `details_${name}`,
      result: await callTool(client, 'widget', {
        action: 'get_details',
        assetPath: ASSET_PATH,
        widgetName: name,
      }),
    });
  }

  log.push({
    step: 'compile',
    result: await callTool(client, 'blueprint', { action: 'compile', assetPath: ASSET_PATH }),
  });

  log.push({
    step: 'after_tree',
    result: await callTool(client, 'widget', { action: 'read_tree', assetPath: ASSET_PATH }),
  });

  log.push({
    step: 'save',
    result: await callTool(client, 'asset', { action: 'save', assetPath: ASSET_PATH }),
  });

  writeFileSync('AI_Planning/fix_ctr_score_widget_log.json', JSON.stringify(log, null, 2));
  console.log(JSON.stringify(log.at(-2)?.result, null, 2));
  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
