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
    unreal.log('[ui-api] ' + str(msg))

names = [n for n in dir(unreal) if any(k in n for k in ['Reference', 'Replace', 'Consolidate', 'Redirector', 'WidgetBlueprint', 'BlueprintEditor'])]
log('NAMES=' + str(sorted(names)))

WIDGET = '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button'
asset = unreal.load_asset(WIDGET)
if asset:
    bp_asset = None
    try:
        bp_asset = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)
        log('bp_asset=' + str(bp_asset))
    except Exception as e:
        log('get_blueprint_asset err=' + str(e))

registry = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions()
opts.include_hard_package_references = True
opts.include_soft_package_references = True
opts.include_searchable_names = False
data = registry.get_asset_by_object_path(WIDGET + '.WBP_BW_HeroSelect_Button')
if data:
    deps = registry.get_dependencies(str(data.package_name), opts)
    hero_deps = [str(d) for d in deps if '/Hero_' in str(d)]
    log('hero_deps=' + str(hero_deps))

# Try reading all CDO properties recursively for soft object paths
if asset:
    gen = asset.generated_class()
    cdo = unreal.get_default_object(gen)
    found = []
    for prop in cdo.get_class().get_properties():
        try:
            val = cdo.get_editor_property(prop.get_name())
            if val and hasattr(val, 'get_path_name'):
                p = val.get_path_name()
                if '/Hero_' in p:
                    found.append((prop.get_name(), p))
        except Exception:
            pass
    log('cdo_hero_refs=' + str(found))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'ui-api', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
