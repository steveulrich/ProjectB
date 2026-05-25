import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT = 'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href);

const UPROJECT = 'E:/Unreal Projects/ProjectB/ProjectB.uproject';

const ASSETS = [
  '/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic',
  '/BreakawayCore/Experiences/B_BW_Experience_Dev',
  '/BreakawayCore/GameModes/B_BWayGameMode_Default',
  '/BreakawayCore/GameModes/BP_BW_GameState',
  '/BreakawayCore/Characters/DA_BW_PawnData_Humanoid',
  '/BreakawayCore/Characters/Heroes/Argus/DA_BW_HeroData_Argus',
  '/BreakawayCore/Characters/Heroes/Alona/DA_BW_HeroData_Alona',
  '/BreakawayCore/GameModes/BallMode/Relic/DA_BW_RelicSettings_Default',
  '/BreakawayCore/Experiences/LAS_BW_SharedInput',
  '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect',
  '/BreakawayCore/UI/WBP_BW_CoreHUD',
  '/BreakawayCore/UI/WBP_BW_ResultsWidget',
  '/BreakawayCore/Experiences/Phases/BW_Phase_Warmup',
  '/BreakawayCore/Experiences/Phases/BW_Phase_HeroSelection',
  '/BreakawayCore/Experiences/Phases/BW_Phase_Playing',
  '/BreakawayCore/Experiences/Phases/BW_Phase_PostRound',
  '/BreakawayCore/Experiences/Phases/BW_Phase_PostGame',
];

const BLUEPRINTS = [
  '/BreakawayCore/GameModes/B_BWayGameMode_Default',
  '/BreakawayCore/GameModes/BP_BW_GameState',
  '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect',
  '/BreakawayCore/UI/WBP_BW_CoreHUD',
];

const MAPS = ['/BreakawayCore/Maps/L_BW_DevMap', '/BreakawayCore/Maps/L_BW_Dorado'];

async function callTool(client, name, args) {
  const result = await client.callTool({ name, arguments: args });
  const text = result.content?.map((c) => c.text).join('\n') ?? JSON.stringify(result);
  let parsed;
  try {
    parsed = JSON.parse(text);
  } catch {
    parsed = text;
  }
  return { ok: !result.isError, parsed, raw: text.slice(0, 12000) };
}

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', UPROJECT],
  });

  const client = new Client({ name: 'audit-breakaway', version: '1.0.0' });
  await client.connect(transport);

  const report = {
    generatedAt: new Date().toISOString(),
    status: null,
    assets: {},
    blueprints: {},
    heroes: null,
    heroSearch: {},
    maps: {},
  };

  report.status = await callTool(client, 'project', { action: 'get_status' });

  for (const assetPath of ASSETS) {
    report.assets[assetPath] = {
      properties: await callTool(client, 'asset', {
        action: 'read_properties',
        assetPath,
        includeValues: true,
      }),
    };
  }

  for (const assetPath of BLUEPRINTS) {
    report.blueprints[assetPath] = await callTool(client, 'blueprint', {
      action: 'read',
      assetPath,
      includeComponentProperties: true,
    });
  }

  for (const mapPath of MAPS) {
    await callTool(client, 'level', { action: 'load', levelPath: mapPath });
    report.maps[mapPath] = {
      spawnPoints: await callTool(client, 'level', {
        action: 'get_actors_by_class',
        className: 'BwaySpawnPoint',
        world: 'editor',
      }),
      goalVolumes: await callTool(client, 'level', {
        action: 'get_actors_by_class',
        className: 'BwayGoalVolume',
        world: 'editor',
      }),
      bwayGoals: await callTool(client, 'level', {
        action: 'get_actors_by_class',
        className: 'Bway_Goal',
        world: 'editor',
      }),
    };
  }

  report.heroes = await callTool(client, 'asset', {
    action: 'list',
    directory: '/BreakawayCore/Characters/Heroes',
    recursive: true,
    maxResults: 500,
  });

  for (const query of ['Morgan', 'Rawlins', 'Argus', 'Alona', 'HeroData', 'Buildable']) {
    report.heroSearch[query] = await callTool(client, 'asset', {
      action: 'search',
      query,
      directory: '/BreakawayCore/Characters/Heroes',
      maxResults: 50,
    });
  }

  writeFileSync('AI_Planning/ue_mcp_audit_raw.json', JSON.stringify(report, null, 2));
  console.log('Wrote AI_Planning/ue_mcp_audit_raw.json');

  await client.close();
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
