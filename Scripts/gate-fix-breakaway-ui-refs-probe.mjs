import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const UPROJECT = 'E:/Unreal Projects/ProjectB/ProjectB.uproject';

const py = `
import unreal

WIDGETS = [
    '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button',
    '/BreakawayCore/UI/HeroSelect/WBP_BW_AbilityIcon',
    '/BreakawayCore/UI/WBP_BW_CoreAbilitySlot',
    '/BreakawayCore/Dev/WBP_BW_HeroSpawnerWidget',
]

def log(msg):
    unreal.log('[ui-ref-probe] ' + str(msg))

def get_brush_texture(widget):
    try:
        brush = widget.get_editor_property('brush')
        if brush:
            return brush.get_editor_property('resource_object')
    except Exception as e:
        pass
    return None

def probe_widget(path):
    result = {'path': path, 'widgets': [], 'cdo_props': []}
    asset = unreal.load_asset(path)
    if not asset:
        result['error'] = 'not found'
        return result

    tree = asset.get_editor_property('widget_tree')
    if tree:
        root = tree.get_editor_property('root_widget')
        if root:
            stack = [root]
            while stack:
                w = stack.pop()
                tex = get_brush_texture(w)
                if tex:
                    result['widgets'].append({
                        'name': w.get_name(),
                        'class': w.get_class().get_name(),
                        'texture': tex.get_path_name(),
                    })
                try:
                    children = w.get_editor_property('slots')
                except Exception:
                    children = None
                if children:
                    for slot in children:
                        child = slot.get_editor_property('content')
                        if child:
                            stack.append(child)
                try:
                    for child in w.get_children_widgets():
                        stack.append(child)
                except Exception:
                    pass

    bp_gen = asset.generated_class()
    if bp_gen:
        cdo = unreal.get_default_object(bp_gen)
        for prop in ['HeroPortrait', 'Portrait', 'AbilityIcon', 'Icon', 'DefaultPortrait']:
            try:
                val = cdo.get_editor_property(prop)
                if val:
                    result['cdo_props'].append({'prop': prop, 'value': str(val.get_path_name() if hasattr(val, 'get_path_name') else val)})
            except Exception:
                pass

    return result

summary = []
for w in WIDGETS:
    summary.append(probe_widget(w))

log('SUMMARY=' + str(summary))
`;

async function main() {
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'ui-ref-probe', version: '1.0.0' });
  await client.connect(transport);

  const status = await client.callTool({ name: 'project', arguments: { action: 'get_status' } });
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });

  const out = {
    status: status.content?.map((c) => c.text).join('\n'),
    probe: fix.content?.map((c) => c.text).join('\n'),
  };
  writeFileSync('AI_Planning/ui_ref_probe.json', JSON.stringify(out, null, 2));
  console.log(out.status);
  console.log(out.probe?.slice(-4000));
  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
