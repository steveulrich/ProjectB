import { writeFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

export const PROJECT_ROOT = join(dirname(fileURLToPath(import.meta.url)), '../..');

export const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';

export const UPROJECT = 'E:/Unreal Projects/ProjectB/ProjectB.uproject';

export const TEAM0_COLOR = '(R=0.350000,G=0.650000,B=1.000000,A=1.000000)';
export const TEAM1_COLOR = '(R=1.000000,G=0.350000,B=0.300000,A=1.000000)';
export const LABEL_COLOR = '(R=0.850000,G=0.850000,B=0.850000,A=1.000000)';
export const HEADER_COLOR = '(R=0.780000,G=0.820000,B=0.880000,A=1.000000)';
export const SUBTITLE_COLOR = '(R=0.620000,G=0.680000,B=0.760000,A=1.000000)';
export const MVP_COLOR = '(R=1.000000,G=0.820000,B=0.200000,A=1.000000)';

export function text(content) {
  return content?.map?.((c) => c.text).join('\n') ?? String(content ?? '');
}

export function createMcpClient(name) {
  return (async () => {
    const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
    const { StdioClientTransport } = await import(
      pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
    );
    const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
    const client = new Client({ name, version: '1.0.0' });
    await client.connect(transport);
    return client;
  })();
}

export function makeWidgetApi(client, assetPath, log) {
  async function callWidget(action, args) {
    const result = await client.callTool({
      name: 'widget',
      arguments: { action, assetPath, ...args },
    });
    const raw = text(result.content);
    try {
      return JSON.parse(raw);
    } catch {
      return { raw, isError: result.isError };
    }
  }

  async function callBlueprint(action, args) {
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

  async function ensureWidgetAsset({ name, packagePath, parentClass }) {
    let probe = await callWidget('read_tree', {});
    const missing =
      probe.error ||
      probe.isError ||
      (typeof probe.raw === 'string' && probe.raw.toLowerCase().includes('failed to load'));

    if (missing) {
      const created = await callWidget('create', {
        name,
        packagePath,
        parentClass,
      });
      log.push({ step: `create_asset_${name}`, result: created });
      if (created.error || created.isError) {
        throw new Error(`Failed to create widget asset ${name}: ${JSON.stringify(created)}`);
      }
      probe = await callWidget('read_tree', {});
    }

    log.push({ step: `asset_ready_${name}`, result: probe });
    return probe;
  }

  async function reparent(newParentClass) {
    const res = await callBlueprint('reparent', { assetPath, newParentClass });
    log.push({ step: 'reparent', result: res });
    return res;
  }

  async function removeIfExists(widgetName) {
    const res = await callWidget('remove_widget', { widgetName });
    log.push({ step: `remove_${widgetName}`, result: res });
    return res;
  }

  async function addWidget(widgetClass, widgetName, parentWidgetName) {
    const args = { widgetClass, widgetName };
    if (parentWidgetName) args.parentWidgetName = parentWidgetName;
    const res = await callWidget('add_widget', args);
    log.push({ step: `add_${widgetName}`, result: res });
    if (res.error || res.isError) {
      throw new Error(`Failed to add ${widgetName}: ${JSON.stringify(res)}`);
    }
    return res;
  }

  async function setProp(widgetName, propertyName, value) {
    const res = await callWidget('set_property', {
      widgetName,
      propertyName,
      value: String(value),
    });
    log.push({ step: `${widgetName}_${propertyName}`, result: res });
    return res;
  }

  async function setCanvasCenter(widgetName, width, height) {
    await setProp(widgetName, 'slot.anchors', '0.5,0.5,0.5,0.5');
    await setProp(widgetName, 'slot.alignment', '0.5,0.5');
    await setProp(widgetName, 'slot.size', `${width},${height}`);
    await setProp(widgetName, 'slot.autoSize', 'false');
  }

  async function setCanvasFullScreen(widgetName) {
    await setProp(widgetName, 'slot.anchors', '0,0,1,1');
    await setProp(widgetName, 'slot.alignment', '0,0');
    await setProp(widgetName, 'slot.position', '0,0');
    await setProp(widgetName, 'slot.autoSize', 'false');
  }

  async function styleText(widgetName, { textValue, fontSize, color, justify = 'Center' }) {
    if (textValue != null) {
      await setProp(widgetName, 'text', textValue);
    }
    await setProp(widgetName, 'fontSize', String(fontSize));
    await setProp(widgetName, 'ColorAndOpacity', color);
    await setProp(widgetName, 'Justification', `ETextJustify::${justify}`);
  }

  async function addStatRow(parent, rowName, labelText, team0Name, labelName, team1Name) {
    await addWidget('HorizontalBox', rowName, parent);
    await addWidget('TextBlock', team0Name, rowName);
    await addWidget('TextBlock', labelName, rowName);
    await addWidget('TextBlock', team1Name, rowName);

    await setProp(rowName, 'slot.padding', '0,6,0,6');

    await setProp(team0Name, 'slot.sizeRule', 'fill');
    await setProp(team0Name, 'slot.fillWeight', '1');
    await setProp(team0Name, 'slot.hAlign', 'left');
    await styleText(team0Name, {
      textValue: '0',
      fontSize: 22,
      color: TEAM0_COLOR,
      justify: 'Left',
    });

    await setProp(labelName, 'slot.hAlign', 'center');
    await setProp(labelName, 'slot.padding', '8,0,8,0');
    await styleText(labelName, {
      textValue: labelText,
      fontSize: 16,
      color: LABEL_COLOR,
      justify: 'Center',
    });

    await setProp(team1Name, 'slot.sizeRule', 'fill');
    await setProp(team1Name, 'slot.fillWeight', '1');
    await setProp(team1Name, 'slot.hAlign', 'right');
    await styleText(team1Name, {
      textValue: '0',
      fontSize: 22,
      color: TEAM1_COLOR,
      justify: 'Right',
    });
  }

  async function addStatLabelRow(parent, rowName, labelName, labelText) {
    await addWidget('HorizontalBox', rowName, parent);
    await addWidget('TextBlock', labelName, rowName);
    await setProp(rowName, 'slot.padding', '0,6,0,6');
    await setProp(labelName, 'slot.sizeRule', 'fill');
    await setProp(labelName, 'slot.fillWeight', '1');
    await styleText(labelName, {
      textValue: labelText,
      fontSize: 16,
      color: LABEL_COLOR,
      justify: 'Left',
    });
  }

  async function clearTree() {
    const before = await callWidget('read_tree', {});
    log.push({ step: 'before_tree', result: before });
    const names = collectWidgetNames(before.widgetTree ?? before.result?.widgetTree);
    for (const name of [...names].reverse()) {
      await removeIfExists(name);
    }
    return before;
  }

  async function saveAsset() {
    const save = await client.callTool({
      name: 'asset',
      arguments: { action: 'save', assetPath },
    });
    log.push({ step: 'save', result: text(save.content) });
    return save;
  }

  async function readTree() {
    const after = await callWidget('read_tree', {});
    log.push({ step: 'after_tree', result: after });
    return after;
  }

  return {
    assetPath,
    callWidget,
    callBlueprint,
    ensureWidgetAsset,
    reparent,
    removeIfExists,
    addWidget,
    setProp,
    setCanvasCenter,
    setCanvasFullScreen,
    styleText,
    addStatRow,
    addStatLabelRow,
    clearTree,
    saveAsset,
    readTree,
  };
}

export function collectWidgetNames(node, names = []) {
  if (!node || typeof node !== 'object') return names;
  if (node.name) names.push(node.name);
  for (const child of node.children ?? []) {
    collectWidgetNames(child.value ?? child, names);
  }
  return names;
}

export async function waitForEditorBridge(client, assetPath, log, maxWaitMs = 120000) {
  const started = Date.now();
  while (Date.now() - started < maxWaitMs) {
    const ping = await client.callTool({
      name: 'widget',
      arguments: { action: 'read_tree', assetPath },
    });
    let parsed;
    try {
      parsed = JSON.parse(text(ping.content));
    } catch {
      parsed = { raw: text(ping.content), isError: ping.isError };
    }
    if (parsed.success || parsed.widgetTree) {
      log.push({ step: 'editor_ready', assetPath, result: parsed });
      return true;
    }
    log.push({ step: 'editor_wait', assetPath, result: parsed });
    await new Promise((resolve) => setTimeout(resolve, 3000));
  }
  return false;
}

export function writeLog(filename, log) {
  writeFileSync(join(PROJECT_ROOT, 'AI_Planning', filename), JSON.stringify(log, null, 2));
}
