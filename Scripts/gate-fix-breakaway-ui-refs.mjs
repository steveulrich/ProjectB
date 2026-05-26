import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const UPROJECT = 'E:/Unreal Projects/ProjectB/ProjectB.uproject';

const py = `
import unreal

PORTRAIT_SRC = '/Hero_Alona/Characters/Heroes/Alona/hud_headshot_sunpriestess'
ABILITY_ICON_SRC = '/Hero_Spartacus/Characters/Heroes/Argus/OldAbilities/Icons/icon_argus_ability1'
BUILDABLE_ICON_SRC = '/Hero_Spartacus/Characters/Heroes/Argus/OldAbilities/Icons/icon_magnus_buildable2'

CORE_UI_DIR = '/BreakawayCore/UI/HeroSelect'
CORE_ICON_DIR = '/BreakawayCore/UI/Icons'
PORTRAIT_PLACEHOLDER = CORE_UI_DIR + '/T_HeroPortrait_Placeholder'
ABILITY_PLACEHOLDER = CORE_ICON_DIR + '/T_AbilityIcon_Placeholder'
BUILDABLE_PLACEHOLDER = CORE_ICON_DIR + '/T_BuildableIcon_Placeholder'

WIDGET_BPS = [
    '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button',
    '/BreakawayCore/UI/HeroSelect/WBP_BW_AbilityIcon',
    '/BreakawayCore/UI/WBP_BW_CoreAbilitySlot',
    '/BreakawayCore/Dev/WBP_BW_HeroSpawnerWidget',
]

HERO_PREFIXES = ('/Hero_Spartacus/', '/Hero_Alona/', '/Hero_Morgan/', '/Hero_Rawlins/')

# Fallback widget names if map parsing fails
FALLBACK_WIDGET_NAMES = {
    '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button': [
        'SizeBox_368', 'Overlay_133', 'Border_Background', 'Image_Portrait',
        'Border_Highlight', 'StatusIndicator', 'LockedIcon', 'Overlay_Unavailable',
    ],
    '/BreakawayCore/UI/HeroSelect/WBP_BW_AbilityIcon': [
        'SelectionBorder', 'UltimateIndicator', 'BackgroundBorder', 'Overlay_26',
        'AbilityIcon', 'LockedIcon', 'XIcon',
    ],
    '/BreakawayCore/UI/WBP_BW_CoreAbilitySlot': [
        'AbilityIcon', 'IconImage', 'Image_Icon', 'Image_Ability',
    ],
    '/BreakawayCore/Dev/WBP_BW_HeroSpawnerWidget': [
        'Image_Portrait', 'PortraitImage', 'HeroPortrait', 'Image_Hero',
    ],
}


def log(msg):
    unreal.log('[gate-fix-ui-refs] ' + str(msg))


def dup_if_missing(src, dst):
    if unreal.EditorAssetLibrary.does_asset_exist(dst):
        return unreal.load_asset(dst)
    if not unreal.EditorAssetLibrary.does_asset_exist(src):
        log('MISSING source: ' + src)
        return None
    ok = unreal.EditorAssetLibrary.duplicate_asset(src, dst)
    if not ok:
        log('FAILED duplicate ' + src + ' -> ' + dst)
        return None
    tex = unreal.load_asset(dst)
    if tex:
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex


def ensure_placeholders():
    for d in [CORE_UI_DIR, CORE_ICON_DIR]:
        if not unreal.EditorAssetLibrary.does_directory_exist(d):
            unreal.EditorAssetLibrary.make_directory(d)
    return {
        'portrait': dup_if_missing(PORTRAIT_SRC, PORTRAIT_PLACEHOLDER),
        'ability': dup_if_missing(ABILITY_ICON_SRC, ABILITY_PLACEHOLDER),
        'buildable': dup_if_missing(BUILDABLE_ICON_SRC, BUILDABLE_PLACEHOLDER),
    }


def is_hero_path(path):
    return any(prefix in path for prefix in HERO_PREFIXES)


def pick_replacement(tex_path, placeholders):
    lower = tex_path.lower()
    if 'buildable' in lower:
        return placeholders['buildable']
    if 'ability' in lower or 'icon_argus' in lower or 'icon_' in lower:
        return placeholders['ability']
    return placeholders['portrait']


def get_widget_names(bp, path):
    names = []
    try:
        mapping = bp.get_editor_property('WidgetVariableNameToGuidMap')
        if mapping:
            for key in mapping:
                names.append(str(key))
    except Exception:
        pass
    if not names:
        names = list(FALLBACK_WIDGET_NAMES.get(path, []))
    return names


def replace_tree_textures(bp, path, placeholders):
    tree = unreal.load_object(bp, 'WidgetTree')
    if not tree:
        return []
    changed = []
    for name in get_widget_names(bp, path):
        widget = unreal.load_object(tree, name)
        if not widget:
            continue
        if widget.get_class().get_name() not in ['Image', 'CommonLazyImage']:
            continue
        brush = widget.get_editor_property('Brush')
        tex = brush.get_editor_property('ResourceObject')
        if not tex:
            continue
        tex_path = tex.get_path_name()
        if not is_hero_path(tex_path):
            continue
        replacement = pick_replacement(tex_path, placeholders)
        if not replacement:
            continue
        brush.set_editor_property('ResourceObject', replacement)
        widget.set_editor_property('Brush', brush)
        changed.append({'widget': name, 'from': tex_path, 'to': replacement.get_path_name()})
    return changed


def replace_cdo_texture_refs(bp, placeholders):
    gen = bp.generated_class()
    if not gen:
        return []
    cdo = unreal.get_default_object(gen)
    changed = []
    for prop_name in ['HeroPortrait', 'Portrait', 'AbilityIcon', 'Icon', 'DefaultPortrait', 'PreviewPortrait']:
        try:
            val = cdo.get_editor_property(prop_name)
        except Exception:
            continue
        if not val or not hasattr(val, 'get_path_name'):
            continue
        tex_path = val.get_path_name()
        if not is_hero_path(tex_path):
            continue
        replacement = pick_replacement(tex_path, placeholders)
        if replacement:
            cdo.set_editor_property(prop_name, replacement)
            changed.append({'prop': prop_name, 'from': tex_path, 'to': replacement.get_path_name()})
    return changed


def fix_widget(path, placeholders):
    result = {'path': path, 'tree_changes': [], 'cdo_changes': [], 'saved': False}
    asset = unreal.load_asset(path)
    if not asset:
        result['error'] = 'not found'
        return result
    bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)
    result['tree_changes'] = replace_tree_textures(bp, path, placeholders)
    result['cdo_changes'] = replace_cdo_texture_refs(bp, placeholders)
    if result['tree_changes'] or result['cdo_changes']:
        bp.modify()
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
        result['saved'] = True
    return result


placeholders = ensure_placeholders()
summary = {
    'placeholders': {
        'portrait': PORTRAIT_PLACEHOLDER if placeholders['portrait'] else None,
        'ability': ABILITY_PLACEHOLDER if placeholders['ability'] else None,
        'buildable': BUILDABLE_PLACEHOLDER if placeholders['buildable'] else None,
    },
    'widgets': [],
}

for path in WIDGET_BPS:
    summary['widgets'].append(fix_widget(path, placeholders))

for mount in ['/BreakawayCore/UI', '/BreakawayCore/Dev']:
    if unreal.EditorAssetLibrary.does_directory_exist(mount):
        unreal.EditorAssetLibrary.save_directory(mount, only_if_is_dirty=False, recursive=True)

log('SUMMARY=' + str(summary))
`;

async function callTool(client, name, args) {
  const result = await client.callTool({ name, arguments: args });
  const text = result.content?.map((c) => c.text).join('\n') ?? '';
  try {
    return { ok: !result.isError, parsed: JSON.parse(text), raw: text, isError: result.isError };
  } catch {
    return { ok: !result.isError, parsed: text, raw: text, isError: result.isError };
  }
}

async function main() {
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'gate-fix-ui-refs', version: '1.0.0' });
  await client.connect(transport);

  const out = {};
  out.status = await callTool(client, 'project', { action: 'get_status' });
  out.fix = await callTool(client, 'editor', { action: 'execute_python', code: py });

  writeFileSync('AI_Planning/gate_fix_ui_refs.json', JSON.stringify(out, null, 2));
  console.log('Editor:', out.status.parsed?.editorConnected ?? out.status.raw);
  console.log('Fix log:', out.fix.raw?.slice(-5000));

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
