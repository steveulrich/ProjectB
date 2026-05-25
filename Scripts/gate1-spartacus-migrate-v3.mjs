import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const UPROJECT = 'E:/Unreal Projects/ProjectB/ProjectB.uproject';

async function callTool(client, name, args) {
  const result = await client.callTool({ name, arguments: args });
  const text = result.content?.map((c) => c.text).join('\n') ?? '';
  let parsed;
  try {
    parsed = JSON.parse(text);
  } catch {
    parsed = text;
  }
  const bridgeOk = parsed?.success !== false && !parsed?.error;
  return { ok: !result.isError && bridgeOk, parsed, raw: text };
}

const migratePy = `
import unreal

SRC_ROOT = '/BreakawayCore/Characters/Heroes/Argus'
DST_ROOT = '/Hero_Spartacus/Characters/Heroes/Argus'
HERO_DA = DST_ROOT + '/DA_BW_HeroData_Argus'
BUILDABLE_SRC = DST_ROOT + '/DA_BW_BuildableData_Argus'
BUILDABLE_FIRE = DST_ROOT + '/DA_BW_Buildable_FireCatapult'
BUILDABLE_SPIRE = DST_ROOT + '/DA_BW_Buildable_DragonSpire'
MANNY_MESH = '/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny'

def log(msg):
    unreal.log(str(msg))

if not unreal.EditorAssetLibrary.does_directory_exist('/Hero_Spartacus'):
    log('ERROR: /Hero_Spartacus mount missing — enable Hero_Spartacus plugin and restart editor')
else:
    rel_paths = unreal.EditorAssetLibrary.list_assets(SRC_ROOT, recursive=True, include_folder=False)
    moved = []
    failed = []
    for src in sorted(rel_paths, key=len, reverse=True):
        dst = src.replace('/BreakawayCore/', '/Hero_Spartacus/')
        if unreal.EditorAssetLibrary.does_asset_exist(dst):
            moved.append('exists ' + dst)
            continue
        ok = unreal.EditorAssetLibrary.rename_asset(src, dst)
        (moved if ok else failed).append(src + ' -> ' + dst)

    for dest_path in [BUILDABLE_FIRE, BUILDABLE_SPIRE]:
        if unreal.EditorAssetLibrary.does_asset_exist(dest_path):
            continue
        if not unreal.EditorAssetLibrary.does_asset_exist(BUILDABLE_SRC):
            failed.append('buildable source missing')
            continue
        if unreal.EditorAssetLibrary.duplicate_asset(BUILDABLE_SRC, dest_path):
            moved.append('dup ' + dest_path)
        else:
            failed.append('dup failed ' + dest_path)

    hero = unreal.load_asset(HERO_DA)
    if hero:
        hero.set_editor_property('DisplayName', unreal.Text.from_string('Spartacus'))
        manny = unreal.load_asset(MANNY_MESH)
        if manny:
            hero.set_editor_property('HeroMesh', manny)
        fire = unreal.load_asset(BUILDABLE_FIRE)
        spire = unreal.load_asset(BUILDABLE_SPIRE)
        buildables = [a for a in [fire, spire] if a]
        hero.set_editor_property('BuildableDataAssets', buildables)
        unreal.EditorAssetLibrary.save_loaded_asset(hero)

    gfd_class = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayGameFeatureData')
    gfd = unreal.EditorAssetLibrary.load_asset('/Hero_Spartacus/Hero_Spartacus')
    if not gfd and gfd_class:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('DataAssetClass', gfd_class)
        gfd = unreal.AssetToolsHelpers.get_asset_tools().create_asset('Hero_Spartacus', '/Hero_Spartacus', gfd_class, factory)
    if gfd and hero:
        gfd.set_editor_property('HeroDataAssets', [unreal.SoftObjectPath(hero.get_path_name())])
        unreal.EditorAssetLibrary.save_loaded_asset(gfd)

    exp = unreal.load_asset('/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic')
    if exp:
        cdo = unreal.get_default_object(exp.generated_class())
        plugins = list(cdo.get_editor_property('GameFeaturesToEnable'))
        if 'Hero_Spartacus' not in plugins:
            plugins.append('Hero_Spartacus')
            cdo.set_editor_property('GameFeaturesToEnable', plugins)
        unreal.EditorAssetLibrary.save_loaded_asset(exp)

    unreal.EditorAssetLibrary.save_directory('/Hero_Spartacus', only_if_is_dirty=False, recursive=True)
    remaining = unreal.EditorAssetLibrary.list_assets(SRC_ROOT, recursive=True, include_folder=False)
    log('moved=' + str(len(moved)) + ' failed=' + str(len(failed)) + ' remaining=' + str(len(remaining)))
    if failed:
        log('failures: ' + str(failed[:10]))
`;

async function sleep(ms) {
  await new Promise((r) => setTimeout(r, ms));
}

async function connectClient() {
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'gate1-restart-migrate', version: '1.0.0' });
  await client.connect(transport);
  return client;
}

async function waitForEditor(client, attempts = 40) {
  for (let i = 0; i < attempts; i++) {
    const status = await callTool(client, 'project', { action: 'get_status' });
    if (status.parsed?.editorConnected) {
      return true;
    }
    await sleep(5000);
  }
  return false;
}

async function main() {
  let client = await connectClient();

  console.log('Restarting editor to pick up Hero_Spartacus plugin + Content mount...');
  const restart = await callTool(client, 'editor', { action: 'restart_editor' });
  console.log(restart.raw.slice(0, 500));
  await client.close();

  await sleep(8000);
  client = await connectClient();

  const ready = await waitForEditor(client);
  if (!ready) {
    console.error('Editor did not reconnect in time.');
    process.exit(1);
  }

  console.log('Editor reconnected. Running migration...');
  const migrate = await callTool(client, 'editor', { action: 'execute_python', code: migratePy });
  console.log(migrate.raw);

  const verify = {
    pluginList: await callTool(client, 'asset', {
      action: 'list',
      directory: '/Hero_Spartacus',
      recursive: true,
    }),
    heroData: await callTool(client, 'asset', {
      action: 'read_properties',
      assetPath: '/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus',
      includeValues: true,
    }),
    remaining: await callTool(client, 'asset', {
      action: 'list',
      directory: '/BreakawayCore/Characters/Heroes/Argus',
      recursive: true,
    }),
  };

  writeFileSync('AI_Planning/gate1_spartacus_migrate_v3.json', JSON.stringify({ migrate, verify }, null, 2));
  console.log('Hero_Spartacus assets:', verify.pluginList.parsed?.assetCount);
  console.log('Remaining Argus:', verify.remaining.parsed?.assetCount);

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
