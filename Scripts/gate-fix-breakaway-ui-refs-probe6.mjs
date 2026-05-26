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
    unreal.log('[ui-walk] ' + str(msg))

WIDGET = '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect_Button'
asset = unreal.load_asset(WIDGET)
bp = unreal.BlueprintEditorLibrary.get_blueprint_asset(asset)
tree = unreal.load_object(bp, 'WidgetTree')

log('tree=' + str(tree))
log('tree props=' + str([n for n in dir(tree) if not n.startswith('_')][:40]))

root = tree.get_editor_property('RootWidget')
log('root=' + str(root) + ' class=' + root.get_class().get_name())

def walk(widget, depth=0):
    items = []
    if not widget:
        return items
    name = widget.get_name()
    cls = widget.get_class().get_name()
    tex = None
    if cls == 'Image':
        brush = widget.get_editor_property('Brush')
        tex = brush.get_editor_property('ResourceObject')
        if tex:
            tex = tex.get_path_name()
    items.append({'name': name, 'class': cls, 'tex': tex, 'depth': depth})
    # children via slot content on panel widgets
    for prop in ['Content', 'content']:
        try:
            child = widget.get_editor_property(prop)
            if child:
                items.extend(walk(child, depth + 1))
        except Exception:
            pass
    # NamedSlot / Overlay slots
    for prop_name in ['Slots', 'slots']:
        try:
            slots = widget.get_editor_property(prop_name)
            if slots:
                for slot in slots:
                    for slot_prop in ['Content', 'content']:
                        try:
                            child = slot.get_editor_property(slot_prop)
                            if child:
                                items.extend(walk(child, depth + 1))
                        except Exception:
                            pass
        except Exception:
            pass
    return items

widgets = walk(root)
log('widgets=' + str(widgets))

# Test setting brush on image with hero texture
placeholder = unreal.load_asset('/BreakawayCore/UI/HeroSelect/T_HeroPortrait_Placeholder')
for w in widgets:
    if w['tex'] and '/Hero_' in w['tex']:
        log('FOUND illegal tex on ' + w['name'] + ': ' + w['tex'])
        # locate widget object again
        target = root
        # naive: search all objects in tree by name - use walk to return objects
`;

def walk_objs(widget, depth=0):
    items = []
    if not widget:
        return items
    items.append(widget)
    for prop in ['Content', 'content']:
        try:
            child = widget.get_editor_property(prop)
            if child:
                items.extend(walk_objs(child, depth + 1))
        except Exception:
            pass
    for prop_name in ['Slots', 'slots']:
        try:
            slots = widget.get_editor_property(prop_name)
            if slots:
                for slot in slots:
                    for slot_prop in ['Content', 'content']:
                        try:
                            child = slot.get_editor_property(slot_prop)
                            if child:
                                items.extend(walk_objs(child, depth + 1))
                        except Exception:
                            pass
        except Exception:
            pass
    return items

all_widgets = walk_objs(root)
changed = []
for widget in all_widgets:
    if widget.get_class().get_name() != 'Image':
        continue
    brush = widget.get_editor_property('Brush')
    tex = brush.get_editor_property('ResourceObject')
    if tex and '/Hero_' in tex.get_path_name():
        brush.set_editor_property('ResourceObject', placeholder)
        widget.set_editor_property('Brush', brush)
        changed.append(widget.get_name())

if changed:
    bp.modify()
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
log('changed=' + str(changed))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'ui-walk', version: '1' });
  await client.connect(transport);
  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join('\n'));
  await client.close();
}

main();
