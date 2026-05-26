import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const py = `
import unreal

WIDGETS = [
    '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button',
    '/BreakawayCore/UI/HeroSelect/WBP_BW_AbilityIcon',
    '/BreakawayCore/UI/WBP_BW_CoreAbilitySlot',
    '/BreakawayCore/Dev/WBP_BW_HeroSpawnerWidget',
]

def log(msg):
    unreal.log('[ui-ref-probe2] ' + str(msg))

def try_props(obj, names):
    out = {}
    for n in names:
        try:
            out[n] = str(obj.get_editor_property(n))
        except Exception as e:
            out[n] = 'ERR:' + str(e)
    return out

summary = []
for path in WIDGETS:
    asset = unreal.load_asset(path)
    entry = {'path': path, 'class': asset.get_class().get_name() if asset else None}
    if asset:
        entry['props'] = try_props(asset, ['WidgetTree', 'widget_tree', 'ParentClass', 'BlueprintGeneratedClass'])
        wt = None
        for key in ['WidgetTree', 'widget_tree']:
            try:
                wt = asset.get_editor_property(key)
                if wt:
                    entry['tree_class'] = wt.get_class().get_name()
                    break
            except Exception:
                pass
        if wt:
            try:
                root = wt.get_editor_property('RootWidget')
                entry['root'] = root.get_name() if root else None
            except Exception as e:
                entry['root_err'] = str(e)
        gen = asset.generated_class()
        if gen:
            cdo = unreal.get_default_object(gen)
            entry['cdo_class'] = cdo.get_class().get_name()
            for prop in ['HeroPortrait', 'Portrait', 'AbilityIcon', 'Icon', 'DefaultPortrait', 'PreviewPortrait']:
                try:
                    val = cdo.get_editor_property(prop)
                    if val:
                        entry['cdo_' + prop] = val.get_path_name() if hasattr(val, 'get_path_name') else str(val)
                except Exception:
                    pass
    summary.append(entry)

log('SUMMARY=' + str(summary))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'ui-ref-probe2', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  writeFileSync('AI_Planning/ui_ref_probe2.txt', fix.content?.map((c) => c.text).join('\n') ?? '');
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
