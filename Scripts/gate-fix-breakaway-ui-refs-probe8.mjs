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
    unreal.log('[tree-methods] ' + str(msg))

WIDGET = '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button'
asset = unreal.load_asset(WIDGET)
bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)
tree = unreal.load_object(bp, 'WidgetTree')

log('WidgetTree methods=' + str([n for n in dir(tree) if not n.startswith('_')]))

# Try call_method on tree
for method in ['get_all_widgets', 'GetAllWidgets', 'find_widget', 'FindWidget']:
    if hasattr(tree, method):
        log('has ' + method)

# Iterate objects in package with Widget/Image class
pkg_name = str(bp.get_package().get_name())
registry = unreal.AssetRegistryHelpers.get_asset_registry()
filter = unreal.ARFilter()
filter.package_names = [pkg_name]
assets = registry.get_assets(filter)
log('package asset count=' + str(len(assets)))
for a in assets[:20]:
    log('asset ' + str(a.asset_name) + ' class=' + str(a.asset_class_path))

# Use get_objects_with_outer
try:
    arr = unreal.EditorFilterLibrary.by_class(unreal.load_class(None, '/Script/UMG.Image'), unreal.EditorFilterLibrary.get_all_level_actors()) if False else None
except Exception as e:
    log('filter err=' + str(e))

# Try BlueprintEditorLibrary.call_method
for m in ['GetAllWidgets', 'get_all_widgets']:
    try:
        res = unreal.BlueprintEditorLibrary.call_method(tree, m)
        log('call_method ' + m + '=' + str(res))
    except Exception as e:
        log('call_method ' + m + ' err=' + str(e))

# Search all objects owned by bp using asset utilities
try:
    subsys = unreal.get_editor_subsystem(unreal.SubobjectDataSubsystem)
    log('SubobjectDataSubsystem=' + str(subsys))
except Exception as e:
    log('subobject err=' + str(e))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'tree-methods', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
