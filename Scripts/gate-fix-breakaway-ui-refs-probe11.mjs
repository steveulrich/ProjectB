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
    unreal.log('[bp-vars] ' + str(msg))

WIDGET = '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button'
asset = unreal.load_asset(WIDGET)
bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)

# Reload to force full load
unreal.EditorAssetLibrary.reload_asset(WIDGET)

# Inspect generated class CDO all editor properties
gen = bp.generated_class()
cdo = unreal.get_default_object(gen)
for prop in ['HeroPortrait', 'Portrait', 'PreviewImage', 'DefaultImage', 'Icon', 'AbilityIcon']:
    try:
        val = cdo.get_editor_property(prop)
        if val:
            log(prop + '=' + (val.get_path_name() if hasattr(val, 'get_path_name') else str(val)))
    except Exception as e:
        log(prop + ' err=' + str(e))

# Try listing blueprint graphs for literal texture nodes
try:
    graph = unreal.BlueprintEditorLibrary.find_event_graph(bp)
    log('event graph=' + str(graph))
except Exception as e:
    log('graph err=' + str(e))

# Search package using EditorActorSubsystem - no

# Use AssetRegistry dependency and try Consolidate redirect approach
registry = unreal.AssetRegistryHelpers.get_asset_registry()
soft = unreal.SoftObjectPath(WIDGET + '.WBP_BW_HeroSelect_Button')
data = registry.get_asset_by_object_path(soft)
log('asset data=' + str(data))

# Try get metadata tags
if data:
    tags = registry.get_asset_tag_values(data)
    hero_tags = {k: v for k, v in tags.items() if 'Hero' in str(v) or 'hero' in str(k).lower()}
    log('hero tags=' + str(list(hero_tags.items())[:10]))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'bp-vars', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
