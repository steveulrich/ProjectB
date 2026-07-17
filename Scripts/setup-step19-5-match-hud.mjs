/**
 * Core Loop Step 19.5 — match HUD gameplay strip.
 *
 * Requires:
 *   1. BreakawayCoreRuntime recompiled after pulling the Step 19.5 C++ classes.
 *   2. Unreal Editor open with the ue-mcp bridge connected.
 *
 * Idempotent: widget trees are rebuilt and EAS/input rows are de-duplicated.
 *
 * Usage:
 *   node Scripts/setup-step19-5-match-hud.mjs
 *   node Scripts/setup-step19-5-match-hud.mjs --probe-only
 */
import {
  collectWidgetNames,
  createMcpClient,
  makeWidgetApi,
  text,
  waitForEditorBridge,
  writeLog,
} from './lib/bway-widget-setup.mjs';

const PROBE_ONLY = process.argv.includes('--probe-only');
const REPAIR_PORTRAIT_ONLY = process.argv.includes('--repair-portrait');
const INSPECT_PORTRAIT_ONLY = process.argv.includes('--inspect-portrait');
const INSPECT_LAYOUT_ONLY = process.argv.includes('--inspect-layout');
const PORTRAITS_ONLY = process.argv.includes('--portraits-only');
const FINISH_WIRING_ONLY = process.argv.includes('--finish-wiring');
const VALIDATE_ONLY = process.argv.includes('--validate');

const PATHS = {
  layout: '/BreakawayCore/UI/Match/WBP_BW_MatchHUDLayout',
  abilitySlot: '/BreakawayCore/UI/Match/W_BW_MatchAbilitySlot',
  abilityBar: '/BreakawayCore/UI/Match/W_BW_AbilityBar',
  gold: '/BreakawayCore/UI/Match/W_BW_GoldWidget',
  portraitSlot: '/BreakawayCore/UI/Match/W_BW_TeamPortraitSlot',
  portraitsFriendly: '/BreakawayCore/UI/Match/W_BW_TeamPortraitsWidget',
  portraitsEnemy: '/BreakawayCore/UI/Match/W_BW_TeamPortraitsWidget_Enemy',
  // Legacy alias kept for --inspect-portrait / --repair-portrait flags.
  portraits: '/BreakawayCore/UI/Match/W_BW_TeamPortraitsWidget',
  eas: '/BreakawayCore/Experiences/EAS_BW_CaptureTheRelic',
  las: '/BreakawayCore/Experiences/LAS_BW_MatchHUD',
  experience: '/BreakawayCore/Experiences/B_BW_Experience_Dev',
  relicAbilitySet:
    '/BreakawayCore/GameModes/BallMode/Abilities/BallHandlingAbilities/AS_BW_BallAbilities',
  relicImc: '/BreakawayCore/Input/Mappings/IMC_BW_RelicAbilities',
  inputConfig: '/BreakawayCore/Input/DA_BW_InputData_Humanoid',
  portraitPlaceholder: '/BreakawayCore/UI/HeroSelect/T_HeroPortrait_Placeholder',
};

const WIDGET_SPECS = {
  abilitySlot: {
    assetPath: PATHS.abilitySlot,
    name: 'W_BW_MatchAbilitySlot',
    packagePath: '/BreakawayCore/UI/Match',
    parentClass: '/Script/BreakawayCoreRuntime.BwayMatchAbilitySlotWidget',
  },
  abilityBar: {
    assetPath: PATHS.abilityBar,
    name: 'W_BW_AbilityBar',
    packagePath: '/BreakawayCore/UI/Match',
    parentClass: '/Script/BreakawayCoreRuntime.BwayAbilityBarHUDWidget',
  },
  gold: {
    assetPath: PATHS.gold,
    name: 'W_BW_GoldWidget',
    packagePath: '/BreakawayCore/UI/Match',
    parentClass: '/Script/BreakawayCoreRuntime.BwayCurrencyDisplayWidget',
  },
  portraitSlot: {
    assetPath: PATHS.portraitSlot,
    name: 'W_BW_TeamPortraitSlot',
    packagePath: '/BreakawayCore/UI/Match',
    parentClass: '/Script/BreakawayCoreRuntime.BwayTeamPortraitSlotWidget',
  },
  portraitsFriendly: {
    assetPath: PATHS.portraitsFriendly,
    name: 'W_BW_TeamPortraitsWidget',
    packagePath: '/BreakawayCore/UI/Match',
    parentClass: '/Script/BreakawayCoreRuntime.BwayTeamPortraitsHUDWidget',
  },
  portraitsEnemy: {
    assetPath: PATHS.portraitsEnemy,
    name: 'W_BW_TeamPortraitsWidget_Enemy',
    packagePath: '/BreakawayCore/UI/Match',
    parentClass: '/Script/BreakawayCoreRuntime.BwayTeamPortraitsHUDWidget',
  },
};

const FRAME_COLOR = '(R=0.090000,G=0.110000,B=0.160000,A=0.960000)';
const PANEL_COLOR = '(R=0.025000,G=0.035000,B=0.055000,A=0.880000)';
const GOLD_COLOR = '(R=1.000000,G=0.780000,B=0.160000,A=1.000000)';
const FRIENDLY_COLOR = '(R=0.250000,G=0.600000,B=1.000000,A=1.000000)';
const ENEMY_COLOR = '(R=1.000000,G=0.320000,B=0.150000,A=1.000000)';

function parseToolResult(result) {
  const raw = text(result.content);
  try {
    return { value: JSON.parse(raw), raw, isError: result.isError };
  } catch {
    return { value: raw, raw, isError: result.isError };
  }
}

async function executePython(client, code) {
  return parseToolResult(
    await client.callTool({
      name: 'editor',
      arguments: { action: 'execute_python', code },
    })
  );
}

function widgetTree(result) {
  return result?.widgetTree ?? result?.result?.widgetTree ?? result?.value?.widgetTree;
}

function findWidgetByClass(node, className) {
  if (!node || typeof node !== 'object') return null;
  if (node.class === className) return node;
  for (const child of node.children ?? []) {
    const match = findWidgetByClass(child.value ?? child, className);
    if (match) return match;
  }
  return null;
}

function isFailure(result) {
  if (!result) return true;
  if (result.success === false) return true;
  if (result.error || result.isError) return true;
  const raw = typeof result.raw === 'string' ? result.raw.toLowerCase() : '';
  return raw.includes('failed') || raw.includes('error');
}

async function setGameplayTag(api, widgetName, propertyName, tagName) {
  const candidates = [`(TagName="${tagName}")`, tagName];
  for (const value of candidates) {
    const result = await api.callWidget('set_property', {
      widgetName,
      propertyName,
      value,
    });
    if (!isFailure(result)) {
      return result;
    }
  }
  throw new Error(`Could not set ${widgetName}.${propertyName} to ${tagName}`);
}

async function ensureWidgetParent(client, log, spec) {
  const result = await executePython(
    client,
    `
import unreal
import json

asset_path = '${spec.assetPath}'
parent_path = '${spec.parentClass}'
blueprint = unreal.load_asset(asset_path)
parent_class = unreal.load_class(None, parent_path)
if not blueprint or not parent_class:
    raise RuntimeError('Missing widget Blueprint or native parent: ' + asset_path + ' -> ' + parent_path)

current_parent = getattr(blueprint, 'parent_class', None)
if current_parent != parent_class:
    unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent_class)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint)

print('BWAY_STEP19_5_PARENT=' + json.dumps({
    'asset': asset_path,
    'parent': parent_path,
    'reparented': current_parent != parent_class,
}))
`
  );
  log.push({ step: `ensure_parent_${spec.name}`, result });
  if (result.isError || result.raw.includes('Traceback')) {
    throw new Error(
      `Failed to parent ${spec.assetPath}; recompile BreakawayCoreRuntime first: ${result.raw}`
    );
  }
}

async function prepareWidget(client, log, spec) {
  const api = makeWidgetApi(client, spec.assetPath, log);
  await api.ensureWidgetAsset(spec);
  await ensureWidgetParent(client, log, spec);

  const before = await api.readTree();
  const names = new Set(collectWidgetNames(widgetTree(before)));
  const addWidget = api.addWidget;
  api.addWidget = async (widgetClass, widgetName, parentWidgetName) => {
    if (names.has(widgetName)) {
      return { success: true, existing: true, widgetName };
    }

    const result = await addWidget(widgetClass, widgetName, parentWidgetName);
    names.add(widgetName);
    return result;
  };

  return api;
}

async function setupAbilitySlot(client, log) {
  const api = await prepareWidget(client, log, WIDGET_SPECS.abilitySlot);

  await api.addWidget('SizeBox', 'RootSizeBox', null);
  await api.addWidget('Border', 'SlotFrame', 'RootSizeBox');
  await api.addWidget('Overlay', 'SlotOverlay', 'SlotFrame');
  await api.addWidget('Image', 'Image_Icon', 'SlotOverlay');
  await api.addWidget('Image', 'Image_RelicFrame', 'SlotOverlay');
  await api.addWidget('Image', 'Image_DisabledOverlay', 'SlotOverlay');
  await api.addWidget('TextBlock', 'Text_AbilityName', 'SlotOverlay');
  await api.addWidget('TextBlock', 'Text_Keybind', 'SlotOverlay');
  await api.addWidget('TextBlock', 'Text_State', 'SlotOverlay');

  for (const [property, value] of [
    ['WidthOverride', '82'],
    ['HeightOverride', '96'],
    ['bOverride_WidthOverride', 'true'],
    ['bOverride_HeightOverride', 'true'],
  ]) {
    await api.setProp('RootSizeBox', property, value);
  }
  await api.setProp('SlotFrame', 'Background', FRAME_COLOR);
  await api.setProp('SlotFrame', 'Padding', '4,4,4,4');

  await api.setProp('Image_Icon', 'slot.padding', '5,5,5,24');
  await api.setProp(
    'Image_Icon',
    'ColorAndOpacity',
    '(R=0.200000,G=0.450000,B=0.850000,A=1.000000)'
  );

  await api.setProp(
    'Image_RelicFrame',
    'ColorAndOpacity',
    '(R=0.100000,G=0.850000,B=1.000000,A=0.220000)'
  );
  await api.setProp('Image_RelicFrame', 'Visibility', 'ESlateVisibility::Collapsed');

  await api.setProp(
    'Image_DisabledOverlay',
    'ColorAndOpacity',
    '(R=0.000000,G=0.000000,B=0.000000,A=0.700000)'
  );
  await api.setProp('Image_DisabledOverlay', 'Visibility', 'ESlateVisibility::Collapsed');

  await api.setProp('Text_AbilityName', 'slot.hAlign', 'center');
  await api.setProp('Text_AbilityName', 'slot.vAlign', 'top');
  await api.setProp('Text_AbilityName', 'slot.padding', '2,2,2,0');
  await api.styleText('Text_AbilityName', {
    textValue: 'ABILITY',
    fontSize: 9,
    color: '(R=0.900000,G=0.920000,B=1.000000,A=1.000000)',
    justify: 'Center',
  });

  await api.setProp('Text_Keybind', 'slot.hAlign', 'center');
  await api.setProp('Text_Keybind', 'slot.vAlign', 'bottom');
  await api.setProp('Text_Keybind', 'slot.padding', '0,0,0,2');
  await api.styleText('Text_Keybind', {
    textValue: '—',
    fontSize: 17,
    color: '(R=1.000000,G=1.000000,B=1.000000,A=1.000000)',
    justify: 'Center',
  });

  await api.setProp('Text_State', 'slot.hAlign', 'center');
  await api.setProp('Text_State', 'slot.vAlign', 'center');
  await api.styleText('Text_State', {
    textValue: '—',
    fontSize: 12,
    color: '(R=0.950000,G=0.950000,B=0.950000,A=1.000000)',
    justify: 'Center',
  });
  await api.setProp('Text_State', 'Visibility', 'ESlateVisibility::Collapsed');

  await api.saveAsset();
  return api.readTree();
}

async function setupAbilityBar(client, log) {
  const api = await prepareWidget(client, log, WIDGET_SPECS.abilityBar);

  await api.addWidget('SizeBox', 'RootSizeBox', null);
  await api.addWidget('Border', 'AbilityBarFrame', 'RootSizeBox');
  await api.addWidget('HorizontalBox', 'AbilitySlots', 'AbilityBarFrame');

  for (const [property, value] of [
    ['WidthOverride', '600'],
    ['HeightOverride', '106'],
    ['bOverride_WidthOverride', 'true'],
    ['bOverride_HeightOverride', 'true'],
  ]) {
    await api.setProp('RootSizeBox', property, value);
  }
  await api.setProp('AbilityBarFrame', 'Background', PANEL_COLOR);
  await api.setProp('AbilityBarFrame', 'Padding', '8,5,8,5');
  await api.setProp('AbilitySlots', 'slot.hAlign', 'center');
  await api.setProp('AbilitySlots', 'slot.vAlign', 'center');

  await api.saveAsset();
  return api.readTree();
}

async function setupGold(client, log) {
  const api = await prepareWidget(client, log, WIDGET_SPECS.gold);

  await api.addWidget('SizeBox', 'RootSizeBox', null);
  await api.addWidget('Border', 'GoldFrame', 'RootSizeBox');
  await api.addWidget('HorizontalBox', 'GoldRow', 'GoldFrame');
  await api.addWidget('TextBlock', 'Text_CurrencySymbol', 'GoldRow');
  await api.addWidget('Image', 'Image_CurrencyIcon', 'GoldRow');
  await api.addWidget('TextBlock', 'Text_Amount', 'GoldRow');

  for (const [property, value] of [
    ['WidthOverride', '150'],
    ['HeightOverride', '54'],
    ['bOverride_WidthOverride', 'true'],
    ['bOverride_HeightOverride', 'true'],
  ]) {
    await api.setProp('RootSizeBox', property, value);
  }
  await api.setProp('GoldFrame', 'Background', PANEL_COLOR);
  await api.setProp('GoldFrame', 'Padding', '10,6,10,6');
  await api.setProp('GoldRow', 'slot.hAlign', 'center');
  await api.setProp('GoldRow', 'slot.vAlign', 'center');

  await api.setProp('Text_CurrencySymbol', 'slot.padding', '0,0,6,0');
  await api.styleText('Text_CurrencySymbol', {
    textValue: 'G',
    fontSize: 20,
    color: GOLD_COLOR,
    justify: 'Center',
  });

  await api.setProp('Image_CurrencyIcon', 'Visibility', 'ESlateVisibility::Collapsed');
  await api.setProp('Image_CurrencyIcon', 'slot.padding', '0,0,6,0');
  await api.setProp('Image_CurrencyIcon', 'DesiredSizeOverride', '(X=24.000000,Y=24.000000)');
  await api.setProp('Image_CurrencyIcon', 'bOverride_DesiredSizeOverride', 'true');

  await api.styleText('Text_Amount', {
    textValue: '0',
    fontSize: 24,
    color: '(R=1.000000,G=0.950000,B=0.720000,A=1.000000)',
    justify: 'Center',
  });

  await api.saveAsset();
  return api.readTree();
}

async function setupPortraitSlot(client, log) {
  const api = await prepareWidget(client, log, WIDGET_SPECS.portraitSlot);

  await api.addWidget('SizeBox', 'RootSizeBox', null);
  await api.addWidget('Overlay', 'SlotOverlay', 'RootSizeBox');
  await api.addWidget('Border', 'Frame', 'SlotOverlay');
  await api.addWidget('Image', 'Image_Portrait', 'SlotOverlay');
  await api.addWidget('Image', 'Image_DeathOverlay', 'SlotOverlay');
  await api.addWidget('TextBlock', 'Text_RelicBadge', 'SlotOverlay');

  for (const [property, value] of [
    ['WidthOverride', '76'],
    ['HeightOverride', '76'],
    ['bOverride_WidthOverride', 'true'],
    ['bOverride_HeightOverride', 'true'],
  ]) {
    await api.setProp('RootSizeBox', property, value);
  }

  await api.setProp('Frame', 'Background', FRAME_COLOR);
  await api.setProp('Image_Portrait', 'slot.padding', '5,5,5,5');
  await api.setProp(
    'Image_Portrait',
    'ColorAndOpacity',
    '(R=0.400000,G=0.450000,B=0.550000,A=0.800000)'
  );
  await api.setProp(
    'Image_DeathOverlay',
    'ColorAndOpacity',
    '(R=0.000000,G=0.000000,B=0.000000,A=0.580000)'
  );
  await api.setProp('Image_DeathOverlay', 'Visibility', 'ESlateVisibility::Collapsed');
  await api.setProp('Text_RelicBadge', 'slot.hAlign', 'center');
  await api.setProp('Text_RelicBadge', 'slot.vAlign', 'bottom');
  await api.setProp('Text_RelicBadge', 'slot.padding', '2,0,2,3');
  await api.styleText('Text_RelicBadge', {
    textValue: 'RELIC',
    fontSize: 9,
    color: GOLD_COLOR,
    justify: 'Center',
  });
  await api.setProp('Text_RelicBadge', 'Visibility', 'ESlateVisibility::Collapsed');

  await api.saveAsset();
  return api.readTree();
}

function clearWidgetTreePython(assetPath) {
  return `
import unreal
import json

asset_path = '${assetPath}'
blueprint = unreal.load_asset(asset_path)
if not blueprint:
    raise RuntimeError('Missing widget Blueprint: ' + asset_path)

widget_tree = unreal.load_object(blueprint, 'WidgetTree')
if not widget_tree:
    raise RuntimeError('Missing WidgetTree on ' + asset_path)

removed = []
root = widget_tree.root_widget
if root is not None:
    all_widgets = list(widget_tree.get_all_widgets() or [])
    for widget in all_widgets:
        name = widget.get_name()
        if widget is root:
            continue
        try:
            widget_tree.remove_widget(widget)
            removed.append(name)
        except Exception:
            pass
    try:
        widget_tree.remove_widget(root)
        removed.append(root.get_name())
    except Exception:
        try:
            widget_tree.root_widget = None
            removed.append(root.get_name())
        except Exception:
            pass

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
unreal.EditorAssetLibrary.save_loaded_asset(blueprint)
print('BWAY_STEP19_5_CLEAR_TREE=' + json.dumps({'asset': asset_path, 'removed': removed}))
`;
}

async function setupSingleTeamPortraits(client, log, spec, displaySlotIndex, labelText, labelColor) {
  // Dual-team BindWidgets were removed from C++; clearing the old tree is now safe.
  const existing = await executePython(
    client,
    `
import unreal
print('EXISTS=' + str(bool(unreal.load_asset('${spec.assetPath}'))))
`
  );
  if (existing.raw.includes('EXISTS=True')) {
    const clearResult = await executePython(client, clearWidgetTreePython(spec.assetPath));
    log.push({ step: `clear_${spec.name}`, result: clearResult });
    if (clearResult.isError || clearResult.raw.includes('Traceback')) {
      console.log(`Portrait clear failed for ${spec.name}; continuing with non-destructive add.`);
    }
  }

  const api = await prepareWidget(client, log, spec);

  await api.addWidget('VerticalBox', 'RootVertical', null);
  await api.addWidget('TextBlock', 'Text_TeamLabel', 'RootVertical');
  await api.addWidget('HorizontalBox', 'TeamPortraitsRow', 'RootVertical');

  await api.setProp('Text_TeamLabel', 'slot.padding', '0,0,0,4');
  await api.styleText('Text_TeamLabel', {
    textValue: labelText,
    fontSize: 12,
    color: labelColor,
    justify: displaySlotIndex === 0 ? 'Left' : 'Right',
  });
  await api.setProp('TeamPortraitsRow', 'slot.padding', '0,0,0,0');

  await api.saveAsset();
  return api.readTree();
}

async function setupPortraits(client, log) {
  await setupPortraitSlot(client, log);
  await setupSingleTeamPortraits(
    client,
    log,
    WIDGET_SPECS.portraitsFriendly,
    0,
    'FRIENDLY TEAM',
    FRIENDLY_COLOR
  );
  await setupSingleTeamPortraits(
    client,
    log,
    WIDGET_SPECS.portraitsEnemy,
    1,
    'ENEMY TEAM',
    ENEMY_COLOR
  );
  return { friendly: PATHS.portraitsFriendly, enemy: PATHS.portraitsEnemy };
}

async function setupLayoutExtensionPoints(client, log) {
  const api = makeWidgetApi(client, PATHS.layout, log);
  const before = await api.readTree();
  const root = widgetTree(before);
  const names = collectWidgetNames(root);
  const canvasName = findWidgetByClass(root, 'CanvasPanel')?.name;
  if (!canvasName) {
    throw new Error(`Could not determine layout CanvasPanel for ${PATHS.layout}`);
  }

  const specs = [
    {
      name: 'ExtensionPoint_Team1Portraits',
      tag: 'HUD.Slot.TeamPortraits.Friendly',
      anchors: '0,0,0,0',
      alignment: '0,0',
      position: '24,18',
      size: '340,100',
    },
    {
      name: 'ExtensionPoint_Team2Portraits',
      tag: 'HUD.Slot.TeamPortraits.Enemy',
      anchors: '1,0,1,0',
      alignment: '1,0',
      position: '-24,18',
      size: '340,100',
    },
    {
      name: 'AbilityBarExtensionPoint',
      tag: 'HUD.Slot.AbilityBar',
      anchors: '0.5,1,0.5,1',
      alignment: '0.5,1',
      position: '0,-18',
      size: '620,112',
    },
    {
      name: 'GoldExtensionPoint',
      tag: 'HUD.Slot.Gold',
      anchors: '0.5,1,0.5,1',
      alignment: '0.5,1',
      position: '-390,-28',
      size: '160,60',
    },
  ];

  for (const spec of specs) {
    if (!names.includes(spec.name)) {
      await api.addWidget('/Script/UIExtension.UIExtensionPointWidget', spec.name, canvasName);
    }
    await setGameplayTag(api, spec.name, 'ExtensionPointTag', spec.tag);
    await api.setProp(spec.name, 'slot.anchors', spec.anchors);
    await api.setProp(spec.name, 'slot.alignment', spec.alignment);
    await api.setProp(spec.name, 'slot.position', spec.position);
    await api.setProp(spec.name, 'slot.size', spec.size);
    await api.setProp(spec.name, 'slot.autoSize', 'false');
  }

  await api.saveAsset();
  return api.readTree();
}

function auditPython() {
  return `
import unreal
import json
import re

def tag_name(tag):
    if not tag:
        return ''
    try:
        return str(tag.get_editor_property('TagName'))
    except Exception:
        pass
    try:
        exported = tag.export_text() or ''
        match = re.search(r'TagName="([^"]+)"', exported)
        return match.group(1) if match else exported
    except Exception:
        return str(tag)

def path_name(obj):
    try:
        return obj.get_path_name() if obj else None
    except Exception:
        return str(obj)

report = {
    'assets': {},
    'relicGrants': [],
    'relicMappings': [],
    'inputTags': [],
    'easRows': [],
    'lasLayouts': [],
    'experienceActionSets': [],
}
for path in [
    '${PATHS.relicAbilitySet}',
    '${PATHS.relicImc}',
    '${PATHS.inputConfig}',
    '${PATHS.eas}',
    '${PATHS.las}',
    '${PATHS.experience}',
    '${PATHS.layout}',
]:
    report['assets'][path] = unreal.EditorAssetLibrary.does_asset_exist(path)

ability_set = unreal.load_asset('${PATHS.relicAbilitySet}')
if ability_set:
    for grant in ability_set.get_editor_property('GrantedGameplayAbilities') or []:
        report['relicGrants'].append({
            'ability': path_name(grant.get_editor_property('Ability')),
            'inputTag': tag_name(grant.get_editor_property('InputTag')),
        })

imc = unreal.load_asset('${PATHS.relicImc}')
if imc:
    exported = imc.get_editor_property('default_key_mappings').export_text()
    for action, key in re.findall(r'Action="([^"]+)".*?Key=([A-Za-z0-9_]+)', exported):
        report['relicMappings'].append({'action': action, 'key': key})

input_config = unreal.load_asset('${PATHS.inputConfig}')
if input_config:
    for entry in input_config.get_editor_property('AbilityInputActions') or []:
        report['inputTags'].append({
            'action': path_name(entry.get_editor_property('InputAction')),
            'inputTag': tag_name(entry.get_editor_property('InputTag')),
        })

eas = unreal.load_asset('${PATHS.eas}')
if eas:
    for action in eas.get_editor_property('Actions') or []:
        if action and action.get_class().get_name() == 'GameFeatureAction_AddWidgets':
            for row in action.get_editor_property('Widgets') or []:
                report['easRows'].append({
                    'slot': tag_name(row.get_editor_property('SlotID')),
                    'widget': path_name(row.get_editor_property('WidgetClass')),
                })

las = unreal.load_asset('${PATHS.las}')
if las:
    for action in las.get_editor_property('Actions') or []:
        if action and action.get_class().get_name() == 'GameFeatureAction_AddWidgets':
            for row in action.get_editor_property('Layout') or []:
                report['lasLayouts'].append({
                    'layer': tag_name(row.get_editor_property('LayerID')),
                    'layout': path_name(row.get_editor_property('LayoutClass')),
                })

experience_class = unreal.EditorAssetLibrary.load_blueprint_class('${PATHS.experience}')
experience = unreal.get_default_object(experience_class) if experience_class else unreal.load_asset('${PATHS.experience}')
if experience:
    report['experienceActionSets'] = [
        path_name(action_set)
        for action_set in (experience.get_editor_property('ActionSets') or [])
    ]

print('BWAY_STEP19_5_AUDIT=' + json.dumps(report))
`;
}

function wiringPython() {
  return `
import unreal
import json
import re
import copy

def tag_name(tag):
    if not tag:
        return ''
    try:
        return str(tag.get_editor_property('TagName'))
    except Exception:
        pass
    try:
        exported = tag.export_text() or ''
        match = re.search(r'TagName="([^"]+)"', exported)
        return match.group(1) if match else exported
    except Exception:
        return str(tag)

def make_tag(name):
    tag = unreal.GameplayTag()
    tag.import_text(name)
    return tag

def class_path(asset_path):
    name = asset_path.rsplit('/', 1)[-1]
    return asset_path + '.' + name + '_C'

result = {'compiled': [], 'inputRowsAdded': [], 'easRows': [], 'errors': []}

widget_paths = [
    '${PATHS.portraitSlot}',
    '${PATHS.abilitySlot}',
    '${PATHS.abilityBar}',
    '${PATHS.gold}',
    '${PATHS.portraitsFriendly}',
    '${PATHS.portraitsEnemy}',
    '${PATHS.layout}',
]
for path in widget_paths:
    bp = unreal.load_asset(path)
    if not bp:
        result['errors'].append('Missing widget BP: ' + path)
        continue
    try:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
        result['compiled'].append(path)
    except Exception as exc:
        result['errors'].append('Compile failed ' + path + ': ' + str(exc))

# Assign the runtime-created slot visual class.
ability_bar_bp = unreal.load_asset('${PATHS.abilityBar}')
ability_slot_class = unreal.load_class(None, class_path('${PATHS.abilitySlot}'))
if ability_bar_bp and ability_slot_class:
    try:
        ability_bar_cdo = unreal.get_default_object(ability_bar_bp.generated_class())
        ability_bar_cdo.set_editor_property('AbilitySlotWidgetClass', ability_slot_class)
        unreal.EditorAssetLibrary.save_loaded_asset(ability_bar_bp)
    except Exception as exc:
        result['errors'].append('AbilitySlotWidgetClass: ' + str(exc))
else:
    result['errors'].append('Could not resolve ability bar or slot generated class')

# Wire Friendly / Enemy portrait rows: DisplaySlotIndex + shared slot class + placeholder.
portrait_slot_class = unreal.load_class(None, class_path('${PATHS.portraitSlot}'))
placeholder = unreal.load_asset('${PATHS.portraitPlaceholder}')
portrait_configs = [
    ('${PATHS.portraitsFriendly}', 0),
    ('${PATHS.portraitsEnemy}', 1),
]
if not portrait_slot_class:
    result['errors'].append('Missing generated portrait slot class')
for portraits_path, display_slot in portrait_configs:
    portraits_bp = unreal.load_asset(portraits_path)
    if not portraits_bp:
        result['errors'].append('Missing portraits BP: ' + portraits_path)
        continue
    try:
        portraits_cdo = unreal.get_default_object(portraits_bp.generated_class())
        portraits_cdo.set_editor_property('PortraitSlotWidgetClass', portrait_slot_class)
        portraits_cdo.set_editor_property('DisplaySlotIndex', display_slot)
        portraits_cdo.set_editor_property('PortraitRefreshInterval', 0.5)
        if placeholder:
            portraits_cdo.set_editor_property('EmptyPortraitTexture', placeholder)
        unreal.EditorAssetLibrary.save_loaded_asset(portraits_bp)
    except Exception as exc:
        result['errors'].append('Portrait defaults ' + portraits_path + ': ' + str(exc))

if placeholder and portrait_slot_class:
    try:
        slot_bp = unreal.load_asset('${PATHS.portraitSlot}')
        if slot_bp:
            slot_cdo = unreal.get_default_object(slot_bp.generated_class())
            slot_cdo.set_editor_property('EmptyPortraitTexture', placeholder)
            unreal.EditorAssetLibrary.save_loaded_asset(slot_bp)
    except Exception as exc:
        result['errors'].append('Portrait slot EmptyPortraitTexture: ' + str(exc))

# Ensure the Lyra input config can translate each real relic grant tag back to its InputAction.
desired_input_rows = [
    ('InputTag.Relic.Throw', '/BreakawayCore/Input/Actions/IA_BW_Relic_Throw'),
    ('InputTag.Relic.Dash', '/BreakawayCore/Input/Actions/IA_BW_Relic_Dash'),
    ('InputTag.Relic.Pass', '/BreakawayCore/Input/Actions/IA_BW_Relic_Pass'),
    ('InputTag.Relic.Drop', '/BreakawayCore/Input/Actions/IA_BW_Relic_Drop'),
]
input_config = unreal.load_asset('${PATHS.inputConfig}')
if input_config:
    rows = list(input_config.get_editor_property('AbilityInputActions') or [])
    for input_tag, action_path in desired_input_rows:
        action = unreal.load_asset(action_path)
        if not action:
            result['errors'].append('Missing real relic InputAction: ' + action_path)
            continue
        existing_row = next(
            (row for row in rows if tag_name(row.get_editor_property('InputTag')) == input_tag),
            None
        )
        if existing_row:
            try:
                if existing_row.get_editor_property('InputAction') != action:
                    existing_row.set_editor_property('InputAction', action)
                    result['inputRowsAdded'].append({
                        'tag': input_tag,
                        'action': action_path,
                        'operation': 'corrected',
                    })
            except Exception as exc:
                result['errors'].append('Input row correction ' + input_tag + ': ' + str(exc))
            continue
        try:
            row = unreal.LyraInputAction()
            row.set_editor_property('InputAction', action)
            row.set_editor_property('InputTag', make_tag(input_tag))
            rows.append(row)
            result['inputRowsAdded'].append({
                'tag': input_tag,
                'action': action_path,
                'operation': 'added',
            })
        except Exception as exc:
            result['errors'].append('Input row ' + input_tag + ': ' + str(exc))
    input_config.set_editor_property('AbilityInputActions', rows)
    unreal.EditorAssetLibrary.save_loaded_asset(input_config)
else:
    result['errors'].append('Missing input config ${PATHS.inputConfig}')

# Preserve existing AddWidgets rows, replacing AbilityBar/Gold + Friendly/Enemy portrait rows.
# Also strip the legacy dual-team HUD.Slot.TeamPortraits tag (caused double injection).
desired_eas_rows = {
    'HUD.Slot.AbilityBar': class_path('${PATHS.abilityBar}'),
    'HUD.Slot.Gold': class_path('${PATHS.gold}'),
    'HUD.Slot.TeamPortraits.Friendly': class_path('${PATHS.portraitsFriendly}'),
    'HUD.Slot.TeamPortraits.Enemy': class_path('${PATHS.portraitsEnemy}'),
}
obsolete_eas_slots = {'HUD.Slot.TeamPortraits'}
eas = unreal.load_asset('${PATHS.eas}')
add_widgets_action = None
if eas:
    for action in eas.get_editor_property('Actions') or []:
        if action and action.get_class().get_name() == 'GameFeatureAction_AddWidgets':
            add_widgets_action = action
            break

if add_widgets_action:
    existing = list(add_widgets_action.get_editor_property('Widgets') or [])
    retained = []
    for row in existing:
        slot = tag_name(row.get_editor_property('SlotID'))
        if slot in desired_eas_rows or slot in obsolete_eas_slots:
            continue
        retained.append(row)
    for slot_name, generated_class_path in desired_eas_rows.items():
        widget_class = unreal.load_class(None, generated_class_path)
        if not widget_class:
            result['errors'].append('Missing generated widget class: ' + generated_class_path)
            continue
        if not existing:
            result['errors'].append('EAS Widgets array has no FLyraHUDElementEntry template row')
            continue
        try:
            try:
                row = type(existing[0])()
            except Exception:
                row = copy.copy(existing[0])
            row.set_editor_property('WidgetClass', widget_class)
            row.set_editor_property('SlotID', make_tag(slot_name))
            retained.append(row)
            result['easRows'].append({'slot': slot_name, 'widget': generated_class_path})
        except Exception as exc:
            result['errors'].append('EAS row ' + slot_name + ': ' + str(exc))
    add_widgets_action.set_editor_property('Widgets', retained)
    unreal.EditorAssetLibrary.save_loaded_asset(eas)
else:
    result['errors'].append('EAS has no GameFeatureAction_AddWidgets action')

if result['errors']:
    raise RuntimeError(json.dumps(result))

print('BWAY_STEP19_5_WIRING=' + json.dumps(result))
`;
}

async function main() {
  const log = [];
  const client = await createMcpClient('setup-step19-5-match-hud');

  try {
    const status = await client.callTool({
      name: 'project',
      arguments: { action: 'get_status' },
    });
    log.push({ step: 'status', result: text(status.content) });

    const ready = await waitForEditorBridge(client, PATHS.layout, log);
    if (!ready) {
      throw new Error('Editor bridge not reachable. Open Unreal Editor and re-run the script.');
    }

    if (VALIDATE_ONLY) {
      const validation = await client.callTool({
        name: 'editor',
        arguments: { action: 'validate_assets', directory: '/BreakawayCore/UI/Match' },
      });
      console.log(text(validation.content));
      if (validation.isError) {
        throw new Error(`Step 19.5 asset validation failed: ${text(validation.content)}`);
      }
      return;
    }

    if (INSPECT_PORTRAIT_ONLY) {
      const portraitApi = makeWidgetApi(client, PATHS.portraits, log);
      const tree = await portraitApi.readTree();
      console.log(JSON.stringify(tree, null, 2));
      return;
    }

    if (INSPECT_LAYOUT_ONLY) {
      const layoutApi = makeWidgetApi(client, PATHS.layout, log);
      const tree = await layoutApi.readTree();
      console.log(JSON.stringify(tree, null, 2));
      return;
    }

    if (REPAIR_PORTRAIT_ONLY) {
      const compile = await client.callTool({
        name: 'blueprint',
        arguments: { action: 'compile', assetPath: PATHS.portraits },
      });
      log.push({ step: 'repair_portrait_compile', result: text(compile.content) });
      if (compile.isError) {
        throw new Error(`Portrait Blueprint repair compile failed: ${text(compile.content)}`);
      }

      const save = await client.callTool({
        name: 'asset',
        arguments: { action: 'save', assetPath: PATHS.portraits },
      });
      log.push({ step: 'repair_portrait_save', result: text(save.content) });
      if (save.isError) {
        throw new Error(`Portrait Blueprint repair save failed: ${text(save.content)}`);
      }

      const portraitApi = makeWidgetApi(client, PATHS.portraits, log);
      const tree = await portraitApi.readTree();
      writeLog('setup_step19_5_match_hud_log.json', log);
      console.log(JSON.stringify(tree, null, 2));
      return;
    }

    if (PORTRAITS_ONLY) {
      const tree = await setupPortraits(client, log);
      writeLog('setup_step19_5_match_hud_log.json', log);
      console.log(JSON.stringify(tree, null, 2));
      return;
    }

    // Mandatory pre-authoring audit: real carrier grants and active IMC mappings.
    const beforeAudit = await executePython(client, auditPython());
    log.push({ step: 'audit_before', result: beforeAudit });
    console.log('Step 19.5 audit complete.');

    if (PROBE_ONLY) {
      writeLog('setup_step19_5_match_hud_log.json', log);
      console.log(beforeAudit.raw);
      return;
    }

    if (FINISH_WIRING_ONLY) {
      log.push({
        step: 'layout_extension_points',
        result: await setupLayoutExtensionPoints(client, log),
      });
      console.log('Step 19.5 layout extension points complete.');

      const wiring = await executePython(client, wiringPython());
      log.push({ step: 'wiring', result: wiring });
      if (wiring.isError || wiring.raw.includes('Traceback')) {
        throw new Error(`Step 19.5 asset wiring reported errors: ${wiring.raw}`);
      }
      console.log('Step 19.5 asset wiring complete.');

      const afterAudit = await executePython(client, auditPython());
      log.push({ step: 'audit_after', result: afterAudit });
      writeLog('setup_step19_5_match_hud_log.json', log);
      console.log(afterAudit.raw);
      return;
    }

    log.push({ step: 'ability_slot', result: await setupAbilitySlot(client, log) });
    console.log('Step 19.5 ability-slot widget complete.');
    log.push({ step: 'ability_bar', result: await setupAbilityBar(client, log) });
    console.log('Step 19.5 ability-bar widget complete.');
    log.push({ step: 'gold', result: await setupGold(client, log) });
    console.log('Step 19.5 gold widget complete.');
    log.push({ step: 'portraits', result: await setupPortraits(client, log) });
    console.log('Step 19.5 portrait widget complete.');
    log.push({
      step: 'layout_extension_points',
      result: await setupLayoutExtensionPoints(client, log),
    });
    console.log('Step 19.5 layout extension points complete.');

    const wiring = await executePython(client, wiringPython());
    log.push({ step: 'wiring', result: wiring });
    if (wiring.isError || wiring.raw.includes('Traceback')) {
      throw new Error(`Step 19.5 asset wiring reported errors: ${wiring.raw}`);
    }
    console.log('Step 19.5 asset wiring complete.');

    const afterAudit = await executePython(client, auditPython());
    log.push({ step: 'audit_after', result: afterAudit });
    writeLog('setup_step19_5_match_hud_log.json', log);

    console.log(afterAudit.raw);
    console.log('Step 19.5 match HUD setup complete.');
  } finally {
    await Promise.race([
      client.close(),
      new Promise((resolve) => setTimeout(resolve, 2000)),
    ]);
  }
}

main()
  .then(() => process.exit(0))
  .catch((error) => {
    console.error(error);
    process.exit(1);
  });
