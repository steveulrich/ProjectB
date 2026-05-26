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

registry = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions()
opts.include_hard_package_references = True
opts.include_soft_package_references = True
opts.include_searchable_names = False
opts.include_manage_set_identifiers = False

def log(msg):
    unreal.log('[ui-deps] ' + str(msg))

summary = {}
for path in WIDGETS:
    data = registry.get_asset_by_object_path(path + '.' + path.split('/')[-1])
    pkg = data.package_name if data else None
    deps = []
    if pkg:
        dep_names = registry.get_dependencies(str(pkg), opts)
        for dep in dep_names:
            if dep.startswith('/Hero_'):
                deps.append(str(dep))
    summary[path] = {'package': str(pkg), 'hero_deps': deps}

# Try WidgetBlueprintEditor utilities
util_names = [n for n in dir(unreal) if 'Widget' in n and ('Edit' in n or 'Blueprint' in n)]
summary['_widget_utils'] = util_names[:40]

log('SUMMARY=' + str(summary))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'ui-deps', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
