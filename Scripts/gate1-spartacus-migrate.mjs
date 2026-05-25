import { readFileSync, writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const UPROJECT = 'E:/Unreal Projects/ProjectB/ProjectB.uproject';
const SRC_ROOT = '/BreakawayCore/Characters/Heroes/Argus';
const DST_ROOT = '/Hero_Spartacus/Characters/Heroes/Argus';
const HERO_DA = `${DST_ROOT}/DA_BW_HeroData_Argus`;
const BUILDABLE_SRC = `${DST_ROOT}/DA_BW_BuildableData_Argus`;
const BUILDABLE_FIRE = `${DST_ROOT}/DA_BW_Buildable_FireCatapult`;
const BUILDABLE_SPIRE = `${DST_ROOT}/DA_BW_Buildable_DragonSpire`;
const MANNY_MESH = '/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny';

async function callTool(client, name, args) {
  const result = await client.callTool({ name, arguments: args });
  const text = result.content?.map((c) => c.text).join('\n') ?? '';
  let parsed;
  try {
    parsed = JSON.parse(text);
  } catch {
    parsed = text;
  }
  return { ok: !result.isError, parsed, raw: text };
}

function stripAssetSuffix(path) {
  const dot = path.lastIndexOf('.');
  return dot > path.lastIndexOf('/') ? path.slice(0, dot) : path;
}

async function main() {
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'gate1-spartacus-migrate', version: '1.0.0' });
  await client.connect(transport);

  const log = { startedAt: new Date().toISOString(), steps: [] };
  const step = (name, result) => {
    log.steps.push({ name, ok: result.ok, result: result.parsed ?? result.raw?.slice?.(0, 500) });
    console.log(`${result.ok ? 'OK' : 'FAIL'}: ${name}`);
    if (!result.ok) {
      console.log(typeof result.parsed === 'object' ? JSON.stringify(result.parsed, null, 2) : result.raw);
    }
  };

  // 1) Enable Hero_Spartacus plugin + ensure mount
  const enablePy = `
import unreal
subsystem = unreal.get_editor_subsystem(unreal.PluginsEditorSubsystem)
if subsystem:
    subsystem.set_plugin_enabled('Hero_Spartacus', True)
    unreal.log('Hero_Spartacus enabled')
else:
    unreal.log_error('PluginsEditorSubsystem unavailable')
`;
  step('enable_plugin', await callTool(client, 'editor', { action: 'execute_python', code: enablePy }));

  // 2) List and move Argus assets (deepest paths first)
  const list = await callTool(client, 'asset', {
    action: 'list',
    directory: SRC_ROOT,
    recursive: true,
  });
  const assets = list.parsed?.assets ?? [];
  assets.sort((a, b) => b.path.length - a.path.length);

  for (const asset of assets) {
    const src = asset.path;
    const dst = src.replace('/BreakawayCore/', '/Hero_Spartacus/');
    const moveResult = await callTool(client, 'asset', {
      action: 'move',
      sourcePath: src,
      destinationPath: dst,
    });
    step(`move:${asset.name}`, moveResult);
    if (!moveResult.ok) break;
  }

  // 3) Duplicate legacy buildable into two O2 DAs
  for (const [label, dest] of [
    ['FireCatapult', BUILDABLE_FIRE],
    ['DragonSpire', BUILDABLE_SPIRE],
  ]) {
    step(
      `duplicate_buildable:${label}`,
      await callTool(client, 'asset', {
        action: 'duplicate',
        sourcePath: BUILDABLE_SRC,
        destinationPath: dest,
      })
    );
  }

  // 4) Fix hero DA (DisplayName, mesh, buildable array)
  const heroFixPy = `
import unreal
hero_path = '${HERO_DA}'
hero = unreal.load_asset(hero_path)
if not hero:
    unreal.log_error('Hero DA missing: ' + hero_path)
else:
    hero.set_editor_property('DisplayName', unreal.Text.from_string('Spartacus'))
    manny = unreal.load_asset('${MANNY_MESH}')
    if manny:
        hero.set_editor_property('HeroMesh', manny)
    fire = unreal.load_asset('${BUILDABLE_FIRE}')
    spire = unreal.load_asset('${BUILDABLE_SPIRE}')
    buildables = []
    if fire:
        buildables.append(fire)
    if spire:
        buildables.append(spire)
    hero.set_editor_property('BuildableDataAssets', buildables)
    unreal.EditorAssetLibrary.save_loaded_asset(hero)
    unreal.log('Hero DA updated: Spartacus, mesh, buildables=' + str(len(buildables)))
`;
  step('fix_hero_da', await callTool(client, 'editor', { action: 'execute_python', code: heroFixPy }));

  // 5) Create Hero_Spartacus GameFeatureData (Lyra root naming convention)
  const gfdPy = `
import unreal
package = '/Hero_Spartacus'
asset_name = 'Hero_Spartacus'
existing = unreal.EditorAssetLibrary.load_asset(package + '/' + asset_name)
gfd_class = unreal.load_class(None, '/Script/BreakawayCoreRuntime.BwayGameFeatureData')
if existing:
    gfd = existing
    unreal.log('Reusing existing GFD')
elif gfd_class:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('DataAssetClass', gfd_class)
    gfd = unreal.AssetToolsHelpers.get_asset_tools().create_asset(asset_name, package, gfd_class, factory)
else:
    gfd = None
    unreal.log_error('BwayGameFeatureData class not found')
if gfd:
    hero = unreal.load_asset('${HERO_DA}')
    if hero:
        gfd.set_editor_property('HeroDataAssets', [unreal.SoftObjectPath(hero.get_path_name())])
    unreal.EditorAssetLibrary.save_loaded_asset(gfd)
    unreal.log('GFD saved with hero ref')
`;
  step('create_gfd', await callTool(client, 'editor', { action: 'execute_python', code: gfdPy }));

  // 6) Add Hero_Spartacus to CaptureTheRelic experience
  const expPy = `
import unreal
exp = unreal.load_asset('/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic')
if not exp:
    unreal.log_error('Experience asset not found')
else:
    cdo = unreal.get_default_object(exp.generated_class())
    plugins = list(cdo.get_editor_property('GameFeaturesToEnable'))
    if 'Hero_Spartacus' not in plugins:
        plugins.append('Hero_Spartacus')
        cdo.set_editor_property('GameFeaturesToEnable', plugins)
        unreal.EditorAssetLibrary.save_loaded_asset(exp)
        unreal.log('Added Hero_Spartacus to GameFeaturesToEnable')
    else:
        unreal.log('Hero_Spartacus already in GameFeaturesToEnable')
`;
  step('experience_gf_enable', await callTool(client, 'editor', { action: 'execute_python', code: expPy }));

  // 7) Save all dirty packages
  step('save_all', await callTool(client, 'asset', { action: 'save' }));

  // 8) Verify
  const verify = {};
  verify.pluginAssets = await callTool(client, 'asset', {
    action: 'search',
    query: '',
    directory: '/Hero_Spartacus',
    maxResults: 50,
  });
  verify.heroData = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: HERO_DA,
    includeValues: true,
  });
  verify.remainingArgus = await callTool(client, 'asset', {
    action: 'list',
    directory: SRC_ROOT,
    recursive: true,
  });
  log.verify = verify;

  writeFileSync('AI_Planning/gate1_spartacus_migrate.json', JSON.stringify(log, null, 2));
  console.log('Wrote AI_Planning/gate1_spartacus_migrate.json');
  console.log(
    'Hero_Spartacus assets:',
    verify.pluginAssets.parsed?.resultCount ?? verify.pluginAssets.parsed
  );
  console.log(
    'Remaining BreakawayCore Argus assets:',
    verify.remainingArgus.parsed?.assetCount ?? verify.remainingArgus.parsed
  );

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
