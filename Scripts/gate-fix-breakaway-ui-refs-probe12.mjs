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
    unreal.log('[cdo-widgets] ' + str(msg))

WIDGETS = [
    ('/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button', ['Image_Portrait', 'LockedIcon', 'Overlay_Unavailable']),
    ('/BreakawayCore/UI/HeroSelect/WBP_BW_AbilityIcon', ['AbilityIcon', 'LockedIcon', 'XIcon', 'UltimateIndicator']),
    ('/BreakawayCore/UI/WBP_BW_CoreAbilitySlot', []),
    ('/BreakawayCore/Dev/WBP_BW_HeroSpawnerWidget', []),
]

placeholder = unreal.load_asset('/BreakawayCore/UI/HeroSelect/T_HeroPortrait_Placeholder')
ability_ph = unreal.load_asset('/BreakawayCore/UI/Icons/T_AbilityIcon_Placeholder') if unreal.EditorAssetLibrary.does_asset_exist('/BreakawayCore/UI/Icons/T_AbilityIcon_Placeholder') else placeholder

for path, names in WIDGETS:
    asset = unreal.load_asset(path)
    bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)
    cdo = unreal.get_default_object(bp.generated_class())
    log('=== ' + path)
    # scan all properties on cdo that are Image-like
    if not names:
        names = []
        for prop in ['Image_Portrait', 'PortraitImage', 'HeroPortrait', 'AbilityIcon', 'IconImage', 'Icon', 'Portrait']:
            try:
                val = cdo.get_editor_property(prop)
                if val:
                    names.append(prop)
            except Exception:
                pass
    for prop in names:
        try:
            widget = cdo.get_editor_property(prop)
            if not widget:
                continue
            cls = widget.get_class().get_name()
            log(prop + ' class=' + cls)
            if cls in ['Image', 'CommonLazyImage']:
                brush = widget.get_editor_property('Brush')
                tex = brush.get_editor_property('ResourceObject')
                if tex:
                    log('  tex=' + tex.get_path_name())
                    if '/Hero_' in tex.get_path_name():
                        repl = ability_ph if 'Ability' in prop or 'Icon' in prop else placeholder
                        brush.set_editor_property('ResourceObject', repl)
                        widget.set_editor_property('Brush', brush)
                        bp.modify()
                        log('  REPLACED with ' + repl.get_path_name())
        except Exception as e:
            log(prop + ' err=' + str(e))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'cdo-widgets', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
