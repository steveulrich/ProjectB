import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const py = `
import unreal

WIDGET = '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button'
asset = unreal.load_asset(WIDGET)
bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)
tree = unreal.load_object(bp, 'WidgetTree')

def log(msg):
    unreal.log('[root-probe] ' + str(msg))

for name in ['RootWidget', 'root_widget', 'Root']:
    try:
        obj = tree.get_editor_property(name)
        log('prop ' + name + '=' + str(obj))
    except Exception as e:
        log('prop ' + name + ' err=' + str(e))

for name in ['RootWidget', 'Root']:
    obj = unreal.load_object(tree, name)
    log('load_object ' + name + '=' + str(obj))

# list all subobjects by scanning package
pkg = tree.get_outer()
log('tree outer=' + str(tree.get_outer()))
log('tree full=' + tree.get_full_name())

# try all_objects or get_objects_with_outer
try:
    objs = unreal.EditorUtilityLibrary.get_selected_assets()
except Exception:
    pass

# Asset registry searchable names on package?
registry = unreal.AssetRegistryHelpers.get_asset_registry()
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'root-probe', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
