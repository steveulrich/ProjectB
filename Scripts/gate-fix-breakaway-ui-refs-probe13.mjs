import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const py = `
import unreal

def log(msg):
    unreal.log('[subobj] ' + str(msg))

path = '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button'
asset = unreal.load_asset(path)
bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)
tree = unreal.load_object(bp, 'WidgetTree')

names = ['Image_Portrait', 'LockedIcon', 'Overlay_Unavailable', 'Border_Background', 'SizeBox_368']
for name in names:
    obj = unreal.load_object(tree, name)
    log('load_object tree.' + name + '=' + str(obj))
    obj2 = unreal.load_object(None, path + '.' + path.split('/')[-1] + ':WidgetTree.' + name)
    log('load_object full ' + name + '=' + str(obj2))
    if obj2 and obj2.get_class().get_name() == 'Image':
        brush = obj2.get_editor_property('Brush')
        tex = brush.get_editor_property('ResourceObject')
        log('  tex=' + (tex.get_path_name() if tex else None))

# also try all subobject names from bp read - iterate load_object on tree with common prefixes
for i in range(20):
    obj = unreal.load_object(tree, 'Image_' + str(i))
    if obj:
        log('found Image_' + str(i))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'subobj', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
