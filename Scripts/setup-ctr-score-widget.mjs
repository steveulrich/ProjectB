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

async function callWidget(client, action, args) {
  const result = await client.callTool({
    name: 'widget',
    arguments: { action, ...args },
  });
  const raw = text(result.content);
  try {
    return JSON.parse(raw);
  } catch {
    return { raw, isError: result.isError };
  }
}

async function callBlueprint(client, action, args) {
  const result = await client.callTool({
    name: 'blueprint',
    arguments: { action, ...args },
  });
  const raw = text(result.content);
  try {
    return JSON.parse(raw);
  } catch {
    return { raw, isError: result.isError };
  }
}

function collectWidgetNames(node, names = []) {
  if (!node || typeof node !== 'object') return names;
  if (node.name) names.push(node.name);
  for (const child of node.children ?? []) {
    collectWidgetNames(child.value ?? child, names);
  }
  return names;
}

async function removeIfExists(client, name) {
  const res = await callWidget(client, 'remove_widget', { assetPath: ASSET_PATH, widgetName: name });
  return res;
}

async function addWidget(client, widgetClass, widgetName, parentWidgetName) {
  const args = { assetPath: ASSET_PATH, widgetClass, widgetName };
  if (parentWidgetName) args.parentWidgetName = parentWidgetName;
  return callWidget(client, 'add_widget', args);
}

async function setProp(client, widgetName, propertyName, value) {
  return callWidget(client, 'set_property', {
    assetPath: ASSET_PATH,
    widgetName,
    propertyName,
    value: String(value),
  });
}

async function main() {
  const log = [];
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'setup-ctr-score-widget', version: '1.0.0' });
  await client.connect(transport);

  const status = await client.callTool({ name: 'project', arguments: { action: 'get_status' } });
  log.push({ step: 'status', result: text(status.content) });

  const before = await callWidget(client, 'read_tree', { assetPath: ASSET_PATH });
  log.push({ step: 'before_tree', result: before });

  // Clear existing tree by removing all widgets except we'll rebuild from scratch.
  const existingNames = collectWidgetNames(before.widgetTree ?? before.result?.widgetTree);
  // Remove children first (deepest first by reversing discovery order)
  for (const name of [...existingNames].reverse()) {
    const removed = await removeIfExists(client, name);
    log.push({ step: `remove_${name}`, result: removed });
  }

  // Build hierarchy per MatchUI_Setup.md
  const steps = [
    ['SizeBox', 'RootSizeBox', null],
    ['VerticalBox', 'RootVBox', 'RootSizeBox'],
    ['TextBlock', 'Text_Timer', 'RootVBox'],
    ['HorizontalBox', 'ScoreRow', 'RootVBox'],
    ['VerticalBox', 'Team1Column', 'ScoreRow'],
    ['TextBlock', 'Text_Team1Score', 'Team1Column'],
    ['HorizontalBox', 'Team1Pips', 'Team1Column'],
    ['Image', 'Image_CenterRelicIcon', 'ScoreRow'],
    ['VerticalBox', 'Team2Column', 'ScoreRow'],
    ['TextBlock', 'Text_Team2Score', 'Team2Column'],
    ['HorizontalBox', 'Team2Pips', 'ScoreRow'],
  ];

  for (const [cls, name, parent] of steps) {
    const res = await addWidget(client, cls, name, parent);
    log.push({ step: `add_${name}`, result: res });
    if (res.error || res.isError) {
      throw new Error(`Failed to add ${name}: ${JSON.stringify(res)}`);
    }
  }

  // Root SizeBox 600x120
  for (const [prop, val] of [
    ['WidthOverride', '600'],
    ['HeightOverride', '120'],
    ['bOverride_WidthOverride', 'true'],
    ['bOverride_HeightOverride', 'true'],
  ]) {
    log.push({
      step: `RootSizeBox_${prop}`,
      result: await setProp(client, 'RootSizeBox', prop, val),
    });
  }

  // Score row alignment
  log.push({
    step: 'ScoreRow_slot',
    result: await setProp(client, 'ScoreRow', 'slot.hAlign', 'center'),
  });

  // Timer text defaults
  for (const [prop, val] of [
    ['text', '1:30'],
    ['fontSize', '24'],
    ['ColorAndOpacity', '(R=1.000000,G=1.000000,B=1.000000,A=1.000000)'],
    ['Justification', 'ETextJustify::Center'],
  ]) {
    log.push({
      step: `Text_Timer_${prop}`,
      result: await setProp(client, 'Text_Timer', prop, val),
    });
  }
  log.push({
    step: 'Text_Timer_slot',
    result: await setProp(client, 'Text_Timer', 'slot.hAlign', 'center'),
  });

  // Team 1 score — blue (0.2, 0.5, 1.0)
  for (const [prop, val] of [
    ['text', '0'],
    ['fontSize', '52'],
    ['ColorAndOpacity', '(R=0.200000,G=0.500000,B=1.000000,A=1.000000)'],
    ['Justification', 'ETextJustify::Center'],
  ]) {
    log.push({
      step: `Text_Team1Score_${prop}`,
      result: await setProp(client, 'Text_Team1Score', prop, val),
    });
  }
  log.push({
    step: 'Text_Team1Score_slot',
    result: await setProp(client, 'Text_Team1Score', 'slot.hAlign', 'center'),
  });

  // Team 2 score — orange (1.0, 0.4, 0.1)
  for (const [prop, val] of [
    ['text', '0'],
    ['fontSize', '52'],
    ['ColorAndOpacity', '(R=1.000000,G=0.400000,B=0.100000,A=1.000000)'],
    ['Justification', 'ETextJustify::Center'],
  ]) {
    log.push({
      step: `Text_Team2Score_${prop}`,
      result: await setProp(client, 'Text_Team2Score', prop, val),
    });
  }
  log.push({
    step: 'Text_Team2Score_slot',
    result: await setProp(client, 'Text_Team2Score', 'slot.hAlign', 'center'),
  });

  // Column sizing — fill equally
  for (const col of ['Team1Column', 'Team2Column']) {
    log.push({
      step: `${col}_slot`,
      result: await setProp(client, col, 'slot.sizeRule', 'fill'),
    });
  }

  // Center relic icon placeholder sizing
  for (const [prop, val] of [
    ['slot.padding', '8,0,8,0'],
    ['DesiredSizeOverride', '(X=48.000000,Y=48.000000)'],
    ['bOverride_DesiredSizeOverride', 'true'],
  ]) {
    log.push({
      step: `Image_CenterRelicIcon_${prop}`,
      result: await setProp(client, 'Image_CenterRelicIcon', prop, val),
    });
  }

  // Hide pip rows for now (optional polish — keep collapsed)
  for (const pip of ['Team1Pips', 'Team2Pips']) {
    log.push({
      step: `${pip}_visibility`,
      result: await setProp(client, pip, 'Visibility', 'ESlateVisibility::Collapsed'),
    });
  }

  // Class defaults: bShowRoundTimer=true, TimerPollInterval=1.0
  for (const [prop, val] of [
    ['bShowRoundTimer', 'true'],
    ['TimerPollInterval', '1.0'],
  ]) {
    log.push({
      step: `cdo_${prop}`,
      result: await callBlueprint(client, 'set_cdo_property', {
        assetPath: ASSET_PATH,
        propertyName: prop,
        value: val,
      }),
    });
  }

  const after = await callWidget(client, 'read_tree', { assetPath: ASSET_PATH });
  log.push({ step: 'after_tree', result: after });

  // Save asset
  const save = await client.callTool({
    name: 'asset',
    arguments: { action: 'save', assetPath: ASSET_PATH },
  });
  log.push({ step: 'save', result: text(save.content) });

  writeFileSync('AI_Planning/setup_ctr_score_widget_log.json', JSON.stringify(log, null, 2));
  console.log(JSON.stringify(after, null, 2));
  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
