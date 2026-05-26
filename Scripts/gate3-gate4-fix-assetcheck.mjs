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
BUILDABLE_MESH = '/Game/Weapons/Pistol/Mesh/SK_Pistol.SK_Pistol'
CORE_PLACEHOLDER_DIR = '/BreakawayCore/UI/HeroSelect'
CORE_PLACEHOLDER = CORE_PLACEHOLDER_DIR + '/T_HeroPortrait_Placeholder'

HERO_FIXES = [
    {
        'hero_da': '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver',
        'portrait_dst': '/Hero_Morgan/Characters/Heroes/Hexweaver/hud_headshot_hexweaver',
        'buildable_das': [
            '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_BuildableData_Hexweaver',
            '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_Buildable_ElderStone',
            '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_Buildable_TomeOfFrailty',
        ],
        'remove_assets': [
            '/Hero_Morgan/Characters/Heroes/Hexweaver/Buildable/SKM_BW_hexweaver_buildable',
            '/Hero_Morgan/Characters/Heroes/Hexweaver/Buildable/SK_BW_hexweaver_buildable',
        ],
    },
    {
        'hero_da': '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger',
        'portrait_dst': '/Hero_Rawlins/Characters/Heroes/Gunslinger/hud_headshot_gunslinger',
        'buildable_das': [
            '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_BuildableData_Gunslinger',
            '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_Buildable_Cage',
            '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_Buildable_BoomBox',
        ],
        'remove_assets': [
            '/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable/SKM_BW_gunslinger_buildable',
            '/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable/SK_BW_gunslinger_buildable',
        ],
    },
]

ALONA_BUILDABLE_DAS = [
    '/Hero_Alona/Characters/Heroes/Alona/DA_BW_BuildableData_Alona',
    '/Hero_Alona/Characters/Heroes/Alona/DA_BW_Buildable_SunShrine',
    '/Hero_Alona/Characters/Heroes/Alona/DA_BW_Buildable_Starlight',
]
ALONA_REMOVE = [
    '/Hero_Alona/Characters/Heroes/Alona/Buildable/SKM_BW_alona_buildable',
    '/Hero_Alona/Characters/Heroes/Alona/Buildable/SK_BW_alona_buildable',
]


def log(msg):
    unreal.log('[gate3-gate4-fix] ' + str(msg))


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
    return unreal.load_asset(dst)


def ensure_core_placeholder():
    if not unreal.EditorAssetLibrary.does_directory_exist(CORE_PLACEHOLDER_DIR):
        unreal.EditorAssetLibrary.make_directory(CORE_PLACEHOLDER_DIR)
    tex = dup_if_missing(PORTRAIT_SRC, CORE_PLACEHOLDER)
    if tex:
        unreal.EditorAssetLibrary.save_loaded_asset(tex)
    return tex


def fix_buildable_das(da_paths, mesh):
    count = 0
    for path in da_paths:
        da = unreal.load_asset(path)
        if not da or not mesh:
            continue
        da.set_editor_property('PrimaryBuildableMesh', mesh)
        unreal.EditorAssetLibrary.save_loaded_asset(da)
        count += 1
    return count


def remove_assets(paths):
    removed = []
    for path in paths:
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            ok = unreal.EditorAssetLibrary.delete_asset(path)
            removed.append((path, ok))
    return removed


summary = {'heroes': [], 'alona': {}, 'core_placeholder': None}
placeholder_mesh = unreal.load_asset(BUILDABLE_MESH)
core_portrait = ensure_core_placeholder()
summary['core_placeholder'] = CORE_PLACEHOLDER if core_portrait else None

for cfg in HERO_FIXES:
    result = {'hero_da': cfg['hero_da']}
    portrait = dup_if_missing(PORTRAIT_SRC, cfg['portrait_dst'])
    hero = unreal.load_asset(cfg['hero_da'])
    if hero and portrait:
        hero.set_editor_property('Portrait', portrait)
        unreal.EditorAssetLibrary.save_loaded_asset(hero)
        result['portrait'] = cfg['portrait_dst']
    result['buildables_updated'] = fix_buildable_das(cfg['buildable_das'], placeholder_mesh)
    result['removed'] = remove_assets(cfg['remove_assets'])
    summary['heroes'].append(result)

summary['alona']['buildables_updated'] = fix_buildable_das(ALONA_BUILDABLE_DAS, placeholder_mesh)
summary['alona']['removed'] = remove_assets(ALONA_REMOVE)

for mount in ['/Hero_Morgan', '/Hero_Rawlins', '/Hero_Alona', '/BreakawayCore/UI/HeroSelect']:
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
  const client = new Client({ name: 'gate3-gate4-fix', version: '1.0.0' });
  await client.connect(transport);

  const out = {};
  out.status = await callTool(client, 'project', { action: 'get_status' });
  out.fix = await callTool(client, 'editor', { action: 'execute_python', code: py });

  const heroPaths = [
    '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver',
    '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger',
  ];

  out.heroes = {};
  for (const path of heroPaths) {
    out.heroes[path] = await callTool(client, 'asset', {
      action: 'read_properties',
      assetPath: path,
      includeValues: true,
    });
  }

  for (const path of [
    '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_Buildable_ElderStone',
    '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_Buildable_Cage',
  ]) {
    out[path] = await callTool(client, 'asset', {
      action: 'read_properties',
      assetPath: path,
      propertyName: 'PrimaryBuildableMesh',
      includeValues: true,
    });
  }

  writeFileSync('AI_Planning/gate3_gate4_fix.json', JSON.stringify(out, null, 2));

  console.log('Editor:', out.status.parsed?.editorConnected ?? out.status.raw);
  console.log('Fix log:', out.fix.raw?.slice(-2500));
  for (const path of heroPaths) {
    const props = out.heroes[path]?.parsed?.properties ?? [];
    console.log('\n' + path);
    console.log('  Portrait:', props.find((p) => p.name === 'Portrait')?.value);
  }

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
