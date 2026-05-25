import { writeFileSync, readFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT = 'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href);

const UPROJECT = 'E:/Unreal Projects/ProjectB/ProjectB.uproject';

async function callTool(client, name, args) {
  const result = await client.callTool({ name, arguments: args });
  const text = result.content?.map((c) => c.text).join('\n') ?? '';
  try {
    return { ok: !result.isError, parsed: JSON.parse(text), raw: text };
  } catch {
    return { ok: !result.isError, parsed: text, raw: text };
  }
}

async function main() {
  const transport = new StdioClientTransport({ command: 'npx', args: ['ue-mcp', UPROJECT] });
  const client = new Client({ name: 'audit-breakaway-2', version: '1.0.0' });
  await client.connect(transport);

  const extra = {};

  extra.experienceCaptureCDO = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: '/BreakawayCore/Experiences/B_BW_Experience_CaptureTheRelic.B_BW_Experience_CaptureTheRelic_C',
    includeValues: true,
  });

  extra.experienceDevCDO = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: '/BreakawayCore/Experiences/B_BW_Experience_Dev.B_BW_Experience_Dev_C',
    includeValues: true,
  });

  extra.gameModeDefaultCDO = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: '/BreakawayCore/GameModes/B_BWayGameMode_Default.B_BWayGameMode_Default_C',
    includeValues: true,
  });

  extra.gameModeBaseCDO = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: '/BreakawayCore/GameModes/B_BWayGameMode.B_BWayGameMode_C',
    includeValues: true,
  });

  extra.gameStateCDO = await callTool(client, 'asset', {
    action: 'read_properties',
    assetPath: '/BreakawayCore/GameModes/BP_BW_GameState.BP_BW_GameState_C',
    includeValues: true,
  });

  for (const phase of ['Warmup', 'HeroSelection', 'Playing', 'PostRound', 'PostGame']) {
    extra[`phase_${phase}`] = await callTool(client, 'asset', {
      action: 'read_properties',
      assetPath: `/BreakawayCore/Experiences/Phases/BW_Phase_${phase}.BW_Phase_${phase}_C`,
      includeValues: true,
    });
  }

  for (const widget of [
    '/BreakawayCore/UI/HeroSelect/WBP_BW_HeroSelect',
    '/BreakawayCore/UI/WBP_BW_CoreHUD',
    '/BreakawayCore/UI/WBP_BW_ResultsWidget',
  ]) {
    extra[`bp_${widget.split('/').pop()}`] = await callTool(client, 'blueprint', {
      action: 'read',
      assetPath: widget,
    });
  }

  for (const map of ['/BreakawayCore/Maps/L_BW_DevMap', '/BreakawayCore/Maps/L_BW_Dorado']) {
    await callTool(client, 'level', { action: 'load', levelPath: map });
    extra[`outliner_${map.split('/').pop()}`] = await callTool(client, 'level', { action: 'get_outliner', limit: 500 });
  }

  for (const q of ['Morgan', 'Rawlins', 'HeroData', 'DA_BW_Hero']) {
    extra[`search_${q}`] = await callTool(client, 'asset', {
      action: 'search',
      query: q,
      directory: '/BreakawayCore',
      maxResults: 30,
    });
  }

  extra.heroDataAssets = await callTool(client, 'asset', {
    action: 'search',
    query: 'HeroData',
    directory: '/BreakawayCore/Characters/Heroes',
    maxResults: 50,
  });

  const existing = JSON.parse(readFileSync('AI_Planning/ue_mcp_audit_raw.json', 'utf8'));
  writeFileSync('AI_Planning/ue_mcp_audit_raw.json', JSON.stringify({ ...existing, extra }, null, 2));
  writeFileSync('AI_Planning/ue_mcp_audit_extra.json', JSON.stringify(extra, null, 2));
  console.log('Wrote ue_mcp_audit_extra.json');

  await client.close();
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
