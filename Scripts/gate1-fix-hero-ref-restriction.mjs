import { writeFileSync } from 'node:fs';
import { pathToFileURL } from 'node:url';

const UE_MCP_ROOT =
  'C:/Users/Steve/AppData/Roaming/npm/node_modules/ue-mcp/node_modules/@modelcontextprotocol/sdk';
const { Client } = await import(pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/index.js`).href);
const { StdioClientTransport } = await import(
  pathToFileURL(`${UE_MCP_ROOT}/dist/esm/client/stdio.js`).href
);

const PHASE_BP = '/BreakawayCore/Experiences/Phases/BP_BW_HeroSelectionPhase';

const py = `
import unreal

bp = unreal.load_asset('${PHASE_BP}')
if not bp:
    unreal.log_error('BP not found')
else:
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('DefaultHeroData', None)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    val = cdo.get_editor_property('DefaultHeroData')
    unreal.log('DefaultHeroData cleared: ' + str(val.is_null() if hasattr(val, 'is_null') else val))
`;

async function main() {
  const transport = new StdioClientTransport({
    command: 'npx',
    args: ['ue-mcp', 'E:/Unreal Projects/ProjectB/ProjectB.uproject'],
  });
  const client = new Client({ name: 'fix-ref', version: '1' });
  await client.connect(transport);

  const fix = await client.callTool({ name: 'editor', arguments: { action: 'execute_python', code: py } });
  console.log(fix.content?.map((c) => c.text).join(''));

  const props = await client.callTool({
    name: 'asset',
    arguments: {
      action: 'read_properties',
      assetPath: PHASE_BP,
      propertyName: 'DefaultHeroData',
      includeValues: true,
    },
  });
  console.log('DefaultHeroData after fix:', props.content?.map((c) => c.text).join(''));

  writeFileSync('AI_Planning/gate1_ref_fix.txt', fix.content?.map((c) => c.text).join('\n') ?? '');
  await client.close();
}

main();
