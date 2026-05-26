import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const py = `
import gc
import unreal

def log(msg):
    unreal.log('[find-probe] ' + str(msg))

WIDGET = '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button'
asset = unreal.load_asset(WIDGET)
bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)
tree = unreal.load_object(bp, 'WidgetTree')

for name in ['PortraitImage', 'Image_0', 'Image', 'HeroPortrait', 'IconImage']:
    try:
        res = tree.call_method('FindWidget', (name,))
        log('FindWidget(' + name + ')=' + str(res))
    except Exception as e:
        log('FindWidget(' + name + ') err=' + str(e))

# gc by package path substring
hits = []
for obj in gc.get_objects():
    try:
        if obj.get_class().get_name() != 'Image':
            continue
        fn = obj.get_full_name()
        if 'WBP_BW_HeroSelect_Button' in fn:
            brush = obj.get_editor_property('Brush')
            tex = brush.get_editor_property('ResourceObject')
            tex_path = tex.get_path_name() if tex else None
            hits.append((obj.get_name(), fn, tex_path))
    except Exception:
        pass
log('gc hits=' + str(hits))

# any object in bp package with hero texture reference
pkg_path = '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button'
hero_hits = []
for obj in gc.get_objects():
    try:
        fn = obj.get_full_name()
        if pkg_path not in fn:
            continue
        if obj.get_class().get_name() != 'Image':
            continue
        brush = obj.get_editor_property('Brush')
        tex = brush.get_editor_property('ResourceObject')
        if tex:
            hero_hits.append((obj.get_name(), tex.get_path_name()))
    except Exception:
        pass
log('hero_hits=' + str(hero_hits))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'find-probe', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
