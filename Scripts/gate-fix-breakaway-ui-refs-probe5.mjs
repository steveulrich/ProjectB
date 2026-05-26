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

def log(msg):
    unreal.log('[ui-tree] ' + str(msg))

WIDGET = '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button'
asset = unreal.load_asset(WIDGET)
bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)

# List BlueprintEditorLibrary methods
bel = [n for n in dir(unreal.BlueprintEditorLibrary) if not n.startswith('_')]
log('BlueprintEditorLibrary=' + str(bel))

# Try subsystems
for cls_name in ['UMGEditorSubsystem', 'WidgetBlueprintEditorUtils', 'WidgetBlueprintExtension', 'AssetEditorSubsystem']:
    cls = getattr(unreal, cls_name, None)
    log(cls_name + '=' + ('yes ' + str([n for n in dir(cls) if not n.startswith('_')][:30]) if cls else 'no'))

# Try accessing widget tree via get_editor_property on bp with suppress
for prop in ['WidgetTree', 'widget_tree', 'BlueprintWidgetTree']:
    try:
        wt = bp.get_editor_property(prop)
        log('prop ' + prop + '=' + str(wt))
    except Exception as e:
        log('prop ' + prop + ' err=' + str(e))

# Try unreal.load_object on subobject path
for sub in ['WidgetTree', 'WidgetTree_0']:
    try:
        obj = unreal.load_object(bp, sub)
        log('load_object ' + sub + '=' + str(obj))
    except Exception as e:
        log('load_object ' + sub + ' err=' + str(e))

# Inspect all subobjects of bp
try:
    subobjects = bp.get_editor_property('subobjects') if False else None
except Exception:
    pass

# Use AssetEditorSubsystem to open and query?
try:
    aes = unreal.get_editor_subsystem(unreal.AssetEditorSubsystem)
    log('AssetEditorSubsystem methods=' + str([n for n in dir(aes) if 'reference' in n.lower() or 'replace' in n.lower()]))
except Exception as e:
    log('aes err=' + str(e))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'ui-tree', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
