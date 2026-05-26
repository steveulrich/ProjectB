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
    unreal.log('[verify-ui] ' + str(msg))

registry = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions()
opts.include_hard_package_references = True
opts.include_soft_package_references = True
opts.include_searchable_names = False

WIDGETS = [
    '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button.WBP_BW_HeroSelect_Button',
    '/BreakawayCore/UI/HeroSelect/WBP_BW_AbilityIcon.WBP_BW_AbilityIcon',
    '/BreakawayCore/UI/WBP_BW_CoreAbilitySlot.WBP_BW_CoreAbilitySlot',
    '/BreakawayCore/Dev/WBP_BW_HeroSpawnerWidget.WBP_BW_HeroSpawnerWidget',
]

for soft_path in WIDGETS:
    data = registry.get_asset_by_object_path(unreal.SoftObjectPath(soft_path))
    deps = registry.get_dependencies(str(data.package_name), opts)
    hero_deps = [str(d) for d in deps if '/Hero_' in str(d)]
    log(soft_path + ' hero_deps=' + str(hero_deps))

# Probe ShooterCTF and Alona illegal refs
PROBES = [
    '/ShooterCTF/Blueprint/B_BallGoalPad',
    '/ShooterCTF/Blueprint/B_GrantBallPad',
    '/Hero_Alona/Characters/Heroes/Alona/Abilities/PrimaryAttack/B_BW_WeaponInstance_Alona',
    '/Hero_Alona/Characters/Heroes/Alona/Abilities/PrimaryAttack/WeaponPickupData_BW_Pistol',
]
for path in PROBES:
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        log(path + ' MISSING')
        continue
    asset = unreal.load_asset(path)
    cls = asset.get_class().get_name()
    log(path + ' class=' + cls)
    if cls.endswith('Blueprint'):
        bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)
        try:
            parent = bp.get_editor_property('ParentClass')
            log('  parent=' + str(parent.get_path_name() if parent else None))
        except Exception as e:
            log('  parent err=' + str(e))
    else:
        for prop in ['WeaponDefinition', 'InstanceType', 'WeaponInstanceClass', 'PickupDefine']:
            try:
                val = asset.get_editor_property(prop)
                if val:
                    log('  ' + prop + '=' + (val.get_path_name() if hasattr(val, 'get_path_name') else str(val)))
            except Exception:
                pass
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'verify-ui', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  writeFileSync('AI_Planning/gate_verify_ui_refs.txt', fix.content?.map((c) => c.text).join('\n') ?? '');
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
