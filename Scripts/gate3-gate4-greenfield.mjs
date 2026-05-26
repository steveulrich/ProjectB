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

TEMPLATE_ROOT = '/Hero_Alona/Characters/Heroes/Alona'
PORTRAIT_SRC = TEMPLATE_ROOT + '/hud_headshot_sunpriestess'
BUILDABLE_MESH = '/Game/Weapons/Pistol/Mesh/SK_Pistol.SK_Pistol'
MANNY_MESH = '/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny'
EXP_PATH = '/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic'
GFD_CLASS = '/Script/BreakawayCoreRuntime.BwayGameFeatureData'

HEROES = [
    {
        'mount': '/Hero_Morgan',
        'plugin': 'Hero_Morgan',
        'folder': 'Hexweaver',
        'hero_da': 'DA_BW_HeroData_Hexweaver',
        'portrait': 'hud_headshot_hexweaver',
        'ability_set': 'DA_BW_AbilitySet_Hexweaver',
        'buildable_bp': 'BP_BW_Hexweaver_Buildable',
        'buildable_base': 'DA_BW_BuildableData_Hexweaver',
        'buildables': ['DA_BW_Buildable_ElderStone', 'DA_BW_Buildable_TomeOfFrailty'],
        'display_name': 'Morgan Le Fay',
        'hero_class_name': 'Support',
    },
    {
        'mount': '/Hero_Rawlins',
        'plugin': 'Hero_Rawlins',
        'folder': 'Gunslinger',
        'hero_da': 'DA_BW_HeroData_Gunslinger',
        'portrait': 'hud_headshot_gunslinger',
        'ability_set': 'DA_BW_AbilitySet_Gunslinger',
        'buildable_bp': 'BP_BW_Gunslinger_Buildable',
        'buildable_base': 'DA_BW_BuildableData_Gunslinger',
        'buildables': ['DA_BW_Buildable_Cage', 'DA_BW_Buildable_BoomBox'],
        'display_name': 'Rawlins',
        'hero_class_name': 'Assassin',
    },
]
def log(msg):
    unreal.log('[gate3-gate4] ' + str(msg))

def ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)

def dup_if_missing(src, dst):
    if unreal.EditorAssetLibrary.does_asset_exist(dst):
        return True
    if not unreal.EditorAssetLibrary.does_asset_exist(src):
        log('MISSING template: ' + src)
        return False
    ok = unreal.EditorAssetLibrary.duplicate_asset(src, dst)
    if not ok:
        log('FAILED duplicate ' + src + ' -> ' + dst)
    return ok

def setup_hero(cfg):
    mount = cfg['mount']
    folder = cfg['folder']
    root = mount + '/Characters/Heroes/' + folder
    buildable_dir = root + '/Buildable'
    results = {'plugin': cfg['plugin'], 'mount_ok': False, 'hero_da': None, 'buildables': 0, 'gfd': False, 'errors': []}

    if not unreal.EditorAssetLibrary.does_directory_exist(mount):
        results['errors'].append('mount missing — enable plugin in uproject and restart editor')
        return results
    results['mount_ok'] = True

    ensure_dir(root)
    ensure_dir(buildable_dir)

    hero_path = root + '/' + cfg['hero_da']
    portrait_path = root + '/' + cfg['portrait']
    ability_path = root + '/' + cfg['ability_set']
    bp_path = buildable_dir + '/' + cfg['buildable_bp']
    base_da_path = root + '/' + cfg['buildable_base']

    dup_if_missing(TEMPLATE_ROOT + '/DA_BW_HeroData_Alona', hero_path)
    dup_if_missing(PORTRAIT_SRC, portrait_path)
    dup_if_missing(TEMPLATE_ROOT + '/DA_BW_AbilitySet_Alona', ability_path)
    dup_if_missing(TEMPLATE_ROOT + '/Buildable/BP_BW_Alona_Buildable', bp_path)
    dup_if_missing(TEMPLATE_ROOT + '/DA_BW_BuildableData_Alona', base_da_path)

    buildable_da_paths = []
    for name in cfg['buildables']:
        path = root + '/' + name
        dup_if_missing(base_da_path, path)
        buildable_da_paths.append(path)

    buildable_bp = unreal.load_asset(bp_path)
    buildable_class = buildable_bp.generated_class() if buildable_bp else None
    buildable_mesh = unreal.load_asset(BUILDABLE_MESH)
    manny = unreal.load_asset(MANNY_MESH)

    if buildable_bp and buildable_class:
        cdo = unreal.get_default_object(buildable_class)
        cdo.set_editor_property('bPersistsBetweenRounds', True)
        unreal.EditorAssetLibrary.save_loaded_asset(buildable_bp)

    for da_path in [base_da_path] + buildable_da_paths:
        da = unreal.load_asset(da_path)
        if not da:
            continue
        if buildable_class:
            da.set_editor_property('BuildableActorClass', buildable_class)
        if buildable_mesh:
            da.set_editor_property('PrimaryBuildableMesh', buildable_mesh)
        unreal.EditorAssetLibrary.save_loaded_asset(da)

    hero = unreal.load_asset(hero_path)
    ability_set = unreal.load_asset(ability_path)
    buildable_objs = [unreal.load_asset(p) for p in buildable_da_paths]
    buildable_objs = [b for b in buildable_objs if b]

    portrait = unreal.load_asset(portrait_path)

    if hero:
        hero.set_editor_property('DisplayName', cfg['display_name'])
        if portrait:
            hero.set_editor_property('Portrait', portrait)
        if manny:
            hero.set_editor_property('HeroMesh', manny)
        if ability_set:
            hero.set_editor_property('AbilitySets', [ability_set])
        if buildable_objs:
            hero.set_editor_property('BuildableDataAssets', buildable_objs)
            hero.set_editor_property('BuildableDataAsset', buildable_objs[0])
        unreal.EditorAssetLibrary.save_loaded_asset(hero)
        results['hero_da'] = hero_path
        results['buildables'] = len(buildable_objs)

    gfd_class = unreal.load_class(None, GFD_CLASS)
    gfd_path = mount + '/' + cfg['plugin']
    gfd = unreal.EditorAssetLibrary.load_asset(gfd_path)
    if not gfd and gfd_class:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('DataAssetClass', gfd_class)
        gfd = unreal.AssetToolsHelpers.get_asset_tools().create_asset(cfg['plugin'], mount, gfd_class, factory)
    if gfd and hero:
        gfd.set_editor_property('HeroDataAssets', [hero])
        unreal.EditorAssetLibrary.save_loaded_asset(gfd)
        results['gfd'] = True

    unreal.EditorAssetLibrary.save_directory(mount, only_if_is_dirty=False, recursive=True)
    results['asset_count'] = len(unreal.EditorAssetLibrary.list_assets(mount, recursive=True, include_folder=False))
    return results

summary = []
for cfg in HEROES:
    summary.append(setup_hero(cfg))

exp = unreal.load_asset(EXP_PATH)
if exp:
    cdo = unreal.get_default_object(exp.generated_class())
    plugins = list(cdo.get_editor_property('GameFeaturesToEnable'))
    changed = False
    for cfg in HEROES:
        if cfg['plugin'] not in plugins:
            plugins.append(cfg['plugin'])
            changed = True
    if changed:
        cdo.set_editor_property('GameFeaturesToEnable', plugins)
    unreal.EditorAssetLibrary.save_loaded_asset(exp)
    log('experience plugins=' + str(plugins))

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
  const client = new Client({ name: 'gate3-gate4-greenfield', version: '1.0.0' });
  await client.connect(transport);

  const out = {};
  out.status = await callTool(client, 'project', { action: 'get_status' });
  out.setup = await callTool(client, 'editor', { action: 'execute_python', code: py });

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

  out.morganList = await callTool(client, 'asset', {
    action: 'list',
    directory: '/Hero_Morgan',
    recursive: true,
  });
  out.rawlinsList = await callTool(client, 'asset', {
    action: 'list',
    directory: '/Hero_Rawlins',
    recursive: true,
  });
  out.heroSearch = await callTool(client, 'asset', {
    action: 'search',
    query: 'HeroData',
    classNames: ['BwayHeroDataAsset'],
  });

  writeFileSync('AI_Planning/gate3_gate4_greenfield.json', JSON.stringify(out, null, 2));

  console.log('Editor:', out.status.parsed?.editorConnected ?? out.status.raw);
  console.log('Setup log:', out.setup.raw?.slice(-2000));
  for (const path of heroPaths) {
    const props = out.heroes[path]?.parsed?.properties ?? [];
    console.log('\n' + path);
    console.log('  DisplayName:', props.find((p) => p.name === 'DisplayName')?.value);
    console.log('  BuildableDataAssets:', props.find((p) => p.name === 'BuildableDataAssets')?.value);
  }
  console.log('\nHero_Morgan assets:', out.morganList.parsed?.assetCount);
  console.log('Hero_Rawlins assets:', out.rawlinsList.parsed?.assetCount);
  console.log('HeroData search hits:', out.heroSearch.parsed?.assets?.length ?? out.heroSearch.raw);

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
