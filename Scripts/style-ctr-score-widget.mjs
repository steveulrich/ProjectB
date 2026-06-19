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

const py = `
import unreal

ASSET = '${ASSET_PATH}'

def slate(r, g, b, a=1.0):
    return unreal.SlateColor(specified_color=unreal.LinearColor(r, g, b, a))

asset = unreal.load_asset(ASSET)
bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)
tree = unreal.load_object(bp, 'WidgetTree')

def load_widget(name):
    return unreal.load_object(tree, name)

styling = {
    'Text_Team1Score': {
        'color': slate(0.2, 0.5, 1.0),
        'justify': unreal.TextJustify.CENTER,
        'font_size': 52,
        'text': '0',
    },
    'Text_Team2Score': {
        'color': slate(1.0, 0.4, 0.1),
        'justify': unreal.TextJustify.CENTER,
        'font_size': 52,
        'text': '0',
    },
    'Text_Timer': {
        'color': slate(1.0, 1.0, 1.0),
        'justify': unreal.TextJustify.CENTER,
        'font_size': 24,
        'text': '1:30',
    },
}

results = []
for widget_name, cfg in styling.items():
    w = load_widget(widget_name)
    if not w:
        results.append({'name': widget_name, 'error': 'not found'})
        continue
    w.set_editor_property('ColorAndOpacity', cfg['color'])
    w.set_editor_property('Justification', cfg['justify'])
    font = w.get_editor_property('Font')
    font.size = cfg['font_size']
    w.set_editor_property('Font', font)
    w.set_editor_property('Text', cfg['text'])
    w.set_editor_property('Visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    results.append({'name': widget_name, 'ok': True})

# Ensure pip rows collapsed, center icon visible
for pip_name in ['Team1Pips', 'Team2Pips']:
    pip = load_widget(pip_name)
    if pip:
        pip.set_editor_property('Visibility', unreal.SlateVisibility.COLLAPSED)

icon = load_widget('Image_CenterRelicIcon')
if icon:
    icon.set_editor_property('Visibility', unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
    results.append({'name': 'Image_CenterRelicIcon', 'ok': True})

bp.modify()
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
unreal.EditorAssetLibrary.save_loaded_asset(bp)
unreal.log('[ctr-score-style] ' + str(results))
`;

function text(content) {
  return content?.map?.((c) => c.text).join('\n') ?? String(content ?? '');
}

async function main() {
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'style-ctr-score-widget', version: '1.0.0' });
  await client.connect(transport);

  const style = await client.callTool({
    name: 'editor',
    arguments: { action: 'execute_python', code: py },
  });

  const details1 = await client.callTool({
    name: 'widget',
    arguments: {
      action: 'get_details',
      assetPath: ASSET_PATH,
      widgetName: 'Text_Team1Score',
    },
  });

  const details2 = await client.callTool({
    name: 'widget',
    arguments: {
      action: 'get_details',
      assetPath: ASSET_PATH,
      widgetName: 'Text_Team2Score',
    },
  });

  const out = {
    style: text(style.content),
    team1: text(details1.content),
    team2: text(details2.content),
  };
  writeFileSync('AI_Planning/style_ctr_score_widget_log.json', JSON.stringify(out, null, 2));
  console.log(out.team1);
  console.log(out.team2);
  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
