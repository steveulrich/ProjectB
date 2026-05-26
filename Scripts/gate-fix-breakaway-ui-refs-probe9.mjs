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
    unreal.log('[gc-probe] ' + str(msg))

WIDGET = '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button'
asset = unreal.load_asset(WIDGET)
bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)
tree = unreal.load_object(bp, 'WidgetTree')

images = []
for obj in gc.get_objects():
    try:
        if not obj:
            continue
        outer = obj.get_outer()
        if outer != tree and outer != bp:
            continue
        cls = obj.get_class().get_name()
        if cls in ['Image', 'CommonLazyImage', 'WidgetTree']:
            images.append((obj.get_name(), cls, obj.get_full_name()))
    except Exception:
        pass

log('images=' + str(images))

# Try call_method GetAllWidgets on tree
try:
    res = tree.call_method('GetAllWidgets')
    log('GetAllWidgets=' + str(res))
except Exception as e:
    log('GetAllWidgets err=' + str(e))

try:
    res = tree.call_method('FindWidget', 'PortraitImage')
    log('FindWidget PortraitImage=' + str(res))
except Exception as e:
    log('FindWidget err=' + str(e))

# If we found an image with hero tex, try replace
placeholder = unreal.load_asset('/BreakawayCore/UI/HeroSelect/T_HeroPortrait_Placeholder')
for obj in gc.get_objects():
    try:
        if obj.get_class().get_name() != 'Image':
            continue
        if obj.get_outer() != tree and obj.get_typed_outer(unreal.WidgetTree) != tree:
            continue
        brush = obj.get_editor_property('Brush')
        tex = brush.get_editor_property('ResourceObject')
        if tex and '/Hero_' in tex.get_path_name():
            log('FOUND ' + obj.get_name() + ' -> ' + tex.get_path_name())
            brush.set_editor_property('ResourceObject', placeholder)
            obj.set_editor_property('Brush', brush)
            bp.modify()
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            unreal.EditorAssetLibrary.save_loaded_asset(bp)
            log('REPLACED ' + obj.get_name())
    except Exception as e:
        pass
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'gc-probe', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
