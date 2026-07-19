/**
 * Step 21b — Wire Rawlins Jail floor trap + per-victim cage editor assets.
 *
 * Requires Unreal Editor open with MCP bridge (ue-mcp) AFTER HeroRawlinsRuntime compiles
 * (ABwayRawlinsJailBuildable / ABwayRawlinsJailCage must be available).
 * Idempotent: safe to re-run.
 *
 * Usage:
 *   node Scripts/setup-rawlins-21b-jail.mjs
 *   node Scripts/setup-rawlins-21b-jail.mjs --probe-only
 */
import { writeFileSync } from 'node:fs';
import { createMcpClient, text, writeLog } from './lib/bway-widget-setup.mjs';

const PROBE_ONLY = process.argv.includes('--probe-only');

const PATHS = {
  heroRoot: '/Hero_Rawlins/Characters/Heroes/Gunslinger',
  buildableDir: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable',
  abilitiesDir: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities',
  kitRoot: '/Hero_Rawlins/Kit',
  heroData: '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger',
  abilitySet: '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_AbilitySet_Gunslinger',
  kitConfig: '/Hero_Rawlins/Kit/DA_BW_RawlinsKitConfig',
  jailDA: '/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_BuildableData_Gunslinger',
  trapBP: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable/BP_BW_Gunslinger_Buildable',
  cageBP: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Buildable/BP_BW_Gunslinger_JailCage',
  placeGA: '/Hero_Rawlins/Characters/Heroes/Gunslinger/Abilities/GA_BW_Rawlins_BuildablePlacement',
};

const NATIVE = {
  trapActor: '/Script/HeroRawlinsRuntime.BwayRawlinsJailBuildable',
  cageActor: '/Script/HeroRawlinsRuntime.BwayRawlinsJailCage',
  placeAbility: '/Script/BreakawayCoreRuntime.BwayGameplayAbility_PlaceBuildable',
  buildableDA: '/Script/BreakawayCoreRuntime.BwayBuildableDataAsset',
  kitConfigClass: '/Script/HeroRawlinsRuntime.BwayRawlinsKitConfig',
};

const EXPECTED_COMBAT_TAGS = [
  'InputTag.Ability.Primary',
  'InputTag.Ability.Ability4',
  'InputTag.Ability.Ability1',
  'InputTag.Ability.Ability2',
  'InputTag.Ability.Ability3',
];

const PY_HELPERS = `
import unreal
import json
import re

def exists(p):
    return unreal.EditorAssetLibrary.does_asset_exist(p)

def save(asset):
    if asset:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)

def tag_name(tag):
    if not tag:
        return ''
    try:
        return str(tag.get_editor_property('TagName'))
    except Exception:
        pass
    try:
        exported = tag.export_text() or ''
        m = re.search(r'TagName=\\"([^\\"]+)\\"', exported)
        if m:
            return m.group(1)
    except Exception:
        pass
    return str(tag)

def make_tag(tag_str):
    tag = unreal.GameplayTag()
    try:
        tag.set_editor_property('TagName', tag_str)
        return tag
    except Exception:
        pass
    try:
        tag.import_text('(TagName="%s")' % tag_str)
        return tag
    except Exception:
        return tag

def load_bp_class(path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if cls:
        return cls
    return unreal.load_class(None, path + '_C')

def ability_set_grants(path):
    if not path or not exists(path):
        return {'exists': False, 'path': path}
    aset = unreal.load_asset(path)
    grants = []
    for g in aset.get_editor_property('GrantedGameplayAbilities') or []:
        ab = g.get_editor_property('Ability')
        tag = g.get_editor_property('InputTag')
        grants.append({
            'ability': ab.get_path_name() if ab else None,
            'inputTag': tag_name(tag),
        })
    return {'exists': True, 'path': path, 'grants': grants, 'grant_count': len(grants)}

def hero_summary(path):
    if not path or not exists(path):
        return {'exists': False, 'path': path}
    hero = unreal.load_asset(path)
    out = {'exists': True, 'path': path}
    try:
        out['DisplayName'] = str(hero.get_editor_property('DisplayName'))
    except Exception as e:
        out['DisplayName'] = 'error:' + str(e)
    try:
        buildable = hero.get_editor_property('BuildableDataAsset')
        out['BuildableDataAsset'] = buildable.get_path_name() if buildable else None
    except Exception as e:
        out['BuildableDataAsset'] = 'error:' + str(e)
    return out

def jail_da_summary(path):
    if not exists(path):
        return {'exists': False, 'path': path}
    da = unreal.load_asset(path)
    cls = da.get_editor_property('BuildableActorClass')
    mesh = da.get_editor_property('PrimaryBuildableMesh')
    return {
        'exists': True,
        'path': path,
        'buildableActorClass': cls.get_path_name() if cls else None,
        'cost': float(da.get_editor_property('Cost') or 0),
        'maxActive': int(da.get_editor_property('MaxActiveBuildablesPerPlayer') or 0),
        'mesh': mesh.get_path_name() if mesh else None,
    }

def parent_of_bp(path):
    if not exists(path):
        return None
    bp = unreal.load_asset(path)
    if not bp:
        return None
    try:
        cdo = unreal.get_default_object(bp.generated_class())
        return cdo.get_class().get_path_name() if cdo else None
    except Exception as e:
        return 'error:' + str(e)

def bp_cdo_summary(path, props):
    if not exists(path):
        return {'exists': False, 'path': path}
    bp = unreal.load_asset(path)
    out = {'exists': True, 'path': path}
    try:
        cdo = unreal.get_default_object(bp.generated_class())
        out['parent'] = cdo.get_class().get_path_name()
        for prop in props:
            try:
                val = cdo.get_editor_property(prop)
                if prop == 'bPersistsBetweenRounds':
                    out[prop] = bool(val)
                elif prop == 'JailCageClass':
                    out[prop] = val.get_path_name() if val else None
                elif prop == 'KitConfig':
                    out[prop] = val.get_path_name() if val else None
                else:
                    out[prop] = float(val)
            except Exception as e:
                out[prop] = 'error:' + str(e)
        for prop in ['HealthSet', 'MeshComponent', 'AbilitySystemComponent', 'HealthComponent']:
            try:
                val = cdo.get_editor_property(prop)
                out[prop] = 'ok' if val is not None else 'NULL'
            except Exception as e:
                out[prop] = 'error:' + str(e)
    except Exception as e:
        out['error'] = str(e)
    return out

def kit_summary(path):
    if not path or not exists(path):
        return {'exists': False, 'path': path}
    kit = unreal.load_asset(path)
    out = {'exists': True, 'path': path}
    for prop in [
        'JailMaxHealth', 'JailTriggerRadius', 'JailCaptureRadius', 'JailCageRadius',
        'BarrageCooldown', 'DoubleDownCooldown', 'PowerShotCooldown', 'SlideShotCooldown'
    ]:
        try:
            out[prop] = float(kit.get_editor_property(prop))
        except Exception as e:
            out[prop] = 'error:' + str(e)
    return out
`;

async function callEditorPython(client, code) {
  const result = await client.callTool({
    name: 'editor',
    arguments: { action: 'execute_python', code },
  });
  const raw = text(result.content);
  return { ok: !result.isError, raw };
}

function extractJsonMarker(raw, marker) {
  const idx = raw.indexOf(marker);
  if (idx < 0) {
    return { raw: raw.slice(0, 4000) };
  }
  const rest = raw.slice(idx + marker.length);
  const brace = rest.indexOf('{');
  if (brace < 0) {
    return { raw: rest.slice(0, 4000) };
  }
  let body = rest.slice(brace);
  try {
    return JSON.parse(body.trim());
  } catch {
    /* continue */
  }
  const end = body.lastIndexOf('}');
  if (end > 0) {
    try {
      return JSON.parse(body.slice(0, end + 1));
    } catch {
      /* ignore */
    }
  }
  return { raw: body.slice(0, 4000) };
}

async function main() {
  const client = await createMcpClient('setup-rawlins-21b');
  const log = { probe: null, actions: null, after: null };

  const pyProbe = `
${PY_HELPERS}
report = {
  'hero': hero_summary('${PATHS.heroData}'),
  'abilitySet': ability_set_grants('${PATHS.abilitySet}'),
  'jailDA': jail_da_summary('${PATHS.jailDA}'),
  'trapCDO': bp_cdo_summary('${PATHS.trapBP}', ['TriggerRadius', 'CaptureRadius', 'BuildTime', 'bPersistsBetweenRounds', 'JailCageClass', 'KitConfig']),
  'cageCDO': bp_cdo_summary('${PATHS.cageBP}', ['MaxHealth', 'CageRadius', 'BuildTime', 'bPersistsBetweenRounds', 'KitConfig']),
  'kitConfig': kit_summary('${PATHS.kitConfig}'),
  'parents': {
    'trapBP': parent_of_bp('${PATHS.trapBP}'),
    'cageBP': parent_of_bp('${PATHS.cageBP}'),
    'placeGA': parent_of_bp('${PATHS.placeGA}'),
  },
  'exists': {
    'trapBP': exists('${PATHS.trapBP}'),
    'cageBP': exists('${PATHS.cageBP}'),
    'placeGA': exists('${PATHS.placeGA}'),
    'kitConfig': exists('${PATHS.kitConfig}'),
    'jailDA': exists('${PATHS.jailDA}'),
  },
  'nativeClasses': {
    'trapActor': bool(unreal.load_class(None, '${NATIVE.trapActor}')),
    'cageActor': bool(unreal.load_class(None, '${NATIVE.cageActor}')),
    'placeAbility': bool(unreal.load_class(None, '${NATIVE.placeAbility}')),
    'buildableDA': bool(unreal.load_class(None, '${NATIVE.buildableDA}')),
  },
}
print('BWAY_21B_PROBE=' + json.dumps(report))
`;

  const probeResult = await callEditorPython(client, pyProbe);
  log.probe = extractJsonMarker(probeResult.raw, 'BWAY_21B_PROBE=');
  writeFileSync('AI_Planning/setup_rawlins_21b_probe.json', JSON.stringify(log.probe, null, 2));
  console.log('Probe OK — wrote AI_Planning/setup_rawlins_21b_probe.json');

  if (PROBE_ONLY) {
    writeLog('setup_rawlins_21b_jail_log.json', log);
    await client.close?.();
    return;
  }

  const pyApply = `
${PY_HELPERS}

TRAP_PARENT = '${NATIVE.trapActor}'
CAGE_PARENT = '${NATIVE.cageActor}'
PLACE_PARENT = '${NATIVE.placeAbility}'
BUILDABLE_DA_CLS = '${NATIVE.buildableDA}'
TAG_BUILDABLE = 'InputTag.Ability.Buildable'
EXPECTED_COMBAT = ${JSON.stringify(EXPECTED_COMBAT_TAGS)}

actions = []

def ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)
        actions.append({'mkdir': path})

for d in [
    '/Hero_Rawlins',
    '/Hero_Rawlins/Characters',
    '/Hero_Rawlins/Characters/Heroes',
    '${PATHS.heroRoot}',
    '${PATHS.buildableDir}',
    '${PATHS.abilitiesDir}',
    '${PATHS.kitRoot}',
]:
    ensure_dir(d)

trap_parent_cls = unreal.load_class(None, TRAP_PARENT)
cage_parent_cls = unreal.load_class(None, CAGE_PARENT)
place_parent_cls = unreal.load_class(None, PLACE_PARENT)
buildable_da_cls = unreal.load_class(None, BUILDABLE_DA_CLS)

if not trap_parent_cls or not cage_parent_cls:
    actions.append({
        'error': 'missing Jail native classes — recompile HeroRawlinsRuntime first',
        'trap': bool(trap_parent_cls),
        'cage': bool(cage_parent_cls),
    })
    print('BWAY_21B_APPLY=' + json.dumps({'actions': actions}))
else:
    # --- Seed kit Jail radii ---
    kit_path = '${PATHS.kitConfig}'
    if not exists(kit_path):
        kit_cls = unreal.load_class(None, '${NATIVE.kitConfigClass}')
        if kit_cls:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property('DataAssetClass', kit_cls)
            created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                'DA_BW_RawlinsKitConfig', '${PATHS.kitRoot}', kit_cls, factory)
            save(created)
            actions.append({'create_kit_config': bool(created)})

    if exists(kit_path):
        kit = unreal.load_asset(kit_path)
        defaults = {
            'JailMaxHealth': 450.0,
            'JailTriggerRadius': 300.0,
            'JailCaptureRadius': 450.0,
            'JailCageRadius': 120.0,
        }
        for k, v in defaults.items():
            try:
                kit.set_editor_property(k, v)
            except Exception as e:
                actions.append({'kit_prop_fail': k, 'error': str(e)})
        save(kit)
        actions.append({'kit_config_seeded': kit_path})

    def ensure_healthy_bp(bp_path, parent_cls, asset_name, broken_suffix, configure_fn):
        bp_ok = False
        if exists(bp_path):
            try:
                bp = unreal.load_asset(bp_path)
                cdo = unreal.get_default_object(bp.generated_class())
                hs = cdo.get_editor_property('HealthSet')
                mesh = cdo.get_editor_property('MeshComponent')
                bp_ok = (hs is not None) and (mesh is not None)
                actions.append({asset_name + '_healthset': 'ok' if hs else 'NULL', asset_name + '_mesh': 'ok' if mesh else 'NULL'})
            except Exception as e:
                actions.append({asset_name + '_probe_fail': str(e)})
                bp_ok = False

        if exists(bp_path) and not bp_ok:
            broken = '${PATHS.buildableDir}/' + asset_name + broken_suffix
            if exists(broken):
                unreal.EditorAssetLibrary.delete_asset(broken)
            ok = unreal.EditorAssetLibrary.rename_asset(bp_path, broken)
            actions.append({'quarantine_corrupt_' + asset_name: bool(ok), 'to': broken})

        if not exists(bp_path):
            factory = unreal.BlueprintFactory()
            factory.set_editor_property('ParentClass', parent_cls)
            created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                asset_name, '${PATHS.buildableDir}', unreal.Blueprint, factory)
            save(created)
            actions.append({'create_fresh_' + asset_name: bool(created)})

        if exists(bp_path) and parent_cls:
            bp = unreal.load_asset(bp_path)
            try:
                cdo = unreal.get_default_object(bp.generated_class())
                hs = cdo.get_editor_property('HealthSet')
                if hs is None:
                    actions.append({'warn': asset_name + ' still missing HealthSet — compile BP in editor'})
                else:
                    actions.append({asset_name + '_subobjects_ok': True})
            except Exception as e:
                actions.append({asset_name + '_verify_fail': str(e)})
            try:
                configure_fn(bp)
                save(bp)
                actions.append({asset_name + '_cdo_configured': True})
            except Exception as e:
                actions.append({asset_name + '_cdo_fail': str(e)})

    def configure_cage(bp):
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('bPersistsBetweenRounds', False)
        cdo.set_editor_property('BuildTime', 0.0)
        try:
            cdo.set_editor_property('MaxHealth', 450.0)
            cdo.set_editor_property('CageRadius', 120.0)
        except Exception as e:
            actions.append({'cage_cdo_config_warn': str(e)})
        if exists(kit_path):
            try:
                cdo.set_editor_property('KitConfig', unreal.load_asset(kit_path))
            except Exception as e:
                actions.append({'cage_kit_wire_warn': str(e)})

    ensure_healthy_bp(
        '${PATHS.cageBP}',
        cage_parent_cls,
        'BP_BW_Gunslinger_JailCage',
        '_Broken',
        configure_cage)

    cage_bp_cls = load_bp_class('${PATHS.cageBP}') if exists('${PATHS.cageBP}') else None

    def configure_trap(bp):
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property('bPersistsBetweenRounds', True)
        cdo.set_editor_property('BuildTime', 0.0)
        try:
            cdo.set_editor_property('TriggerRadius', 300.0)
            cdo.set_editor_property('CaptureRadius', 450.0)
            cdo.set_editor_property('EffectRadius', 300.0)
        except Exception as e:
            actions.append({'trap_cdo_config_warn': str(e)})
        if cage_bp_cls:
            try:
                cdo.set_editor_property('JailCageClass', cage_bp_cls)
            except Exception as e:
                actions.append({'trap_cage_class_warn': str(e)})
        if exists(kit_path):
            try:
                cdo.set_editor_property('KitConfig', unreal.load_asset(kit_path))
            except Exception as e:
                actions.append({'trap_kit_wire_warn': str(e)})

    ensure_healthy_bp(
        '${PATHS.trapBP}',
        trap_parent_cls,
        'BP_BW_Gunslinger_Buildable',
        '_Broken',
        configure_trap)

    # --- Ensure buildable data asset (floor trap class) ---
    jail_da_path = '${PATHS.jailDA}'
    if not exists(jail_da_path) and buildable_da_cls:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('DataAssetClass', buildable_da_cls)
        created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'DA_BW_BuildableData_Gunslinger', '${PATHS.heroRoot}', buildable_da_cls, factory)
        save(created)
        actions.append({'create_jail_da': bool(created)})

    trap_bp_cls = load_bp_class('${PATHS.trapBP}') if exists('${PATHS.trapBP}') else None

    if exists(jail_da_path):
        da = unreal.load_asset(jail_da_path)
        if trap_bp_cls:
            da.set_editor_property('BuildableActorClass', trap_bp_cls)
        da.set_editor_property('Cost', 0.0)
        da.set_editor_property('MaxActiveBuildablesPerPlayer', 3)
        save(da)
        actions.append({'jail_da_wired': jail_da_path, 'actor': '${PATHS.trapBP}' if trap_bp_cls else None})

    # --- Ensure PlaceBuildable BP ---
    place_path = '${PATHS.placeGA}'
    if not exists(place_path) and place_parent_cls:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('ParentClass', place_parent_cls)
        created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'GA_BW_Rawlins_BuildablePlacement', '${PATHS.abilitiesDir}', unreal.Blueprint, factory)
        save(created)
        actions.append({'create_place_ga': bool(created)})

    if exists(place_path) and place_parent_cls:
        bp = unreal.load_asset(place_path)
        try:
            unreal.BlueprintEditorLibrary.reparent_blueprint(bp, place_parent_cls)
            save(bp)
            actions.append({'reparent_place_ga': PLACE_PARENT})
        except Exception as e:
            actions.append({'reparent_place_ga_fail': str(e)})

        try:
            cdo = unreal.get_default_object(bp.generated_class())
            try:
                display = cdo.get_editor_property('DisplayData')
                display.set_editor_property('AbilityName', unreal.Text('Jail'))
                display.set_editor_property('Description', unreal.Text('Place a one-shot floor trap that cages nearby enemies.'))
                cdo.set_editor_property('DisplayData', display)
            except Exception as e:
                actions.append({'place_display_warn': str(e)})
            save(bp)
            actions.append({'place_ga_configured': True})
        except Exception as e:
            actions.append({'place_ga_cdo_fail': str(e)})

    # --- Wire hero DA ---
    if exists('${PATHS.heroData}') and exists(jail_da_path):
        hero = unreal.load_asset('${PATHS.heroData}')
        da = unreal.load_asset(jail_da_path)
        hero.set_editor_property('BuildableDataAsset', da)
        try:
            hero.set_editor_property('DisplayName', unreal.Text('Rawlins'))
        except Exception:
            pass
        save(hero)
        actions.append({'hero_buildable': jail_da_path})

    # --- Append PlaceBuildable grant (preserve five combat grants) ---
    if exists('${PATHS.abilitySet}') and exists(place_path):
        aset = unreal.load_asset('${PATHS.abilitySet}')
        place_cls = load_bp_class(place_path)
        if place_cls:
            grants = list(aset.get_editor_property('GrantedGameplayAbilities') or [])
            updated = False
            new_grants = []
            combat_seen = set()
            for g in grants:
                tag = tag_name(g.get_editor_property('InputTag'))
                if tag in EXPECTED_COMBAT:
                    combat_seen.add(tag)
                if tag == TAG_BUILDABLE:
                    gen = place_cls.get_path_name()
                    exp = '(Ability="%s",AbilityLevel=1,InputTag=(TagName="%s"))' % (gen, TAG_BUILDABLE)
                    entry = unreal.LyraAbilitySet_GameplayAbility()
                    if entry.import_text(exp):
                        new_grants.append(entry)
                        updated = True
                        actions.append({'updated_buildable_grant': gen})
                    else:
                        g.set_editor_property('Ability', place_cls)
                        g.set_editor_property('AbilityLevel', 1)
                        new_grants.append(g)
                        updated = True
                        actions.append({'updated_buildable_grant_set_prop': True})
                else:
                    new_grants.append(g)
            if not updated:
                gen = place_cls.get_path_name()
                exp = '(Ability="%s",AbilityLevel=1,InputTag=(TagName="%s"))' % (gen, TAG_BUILDABLE)
                entry = unreal.LyraAbilitySet_GameplayAbility()
                if entry.import_text(exp):
                    new_grants.append(entry)
                    actions.append({'added_buildable_grant': gen})
                else:
                    entry = unreal.LyraAbilitySet_GameplayAbility()
                    entry.set_editor_property('Ability', place_cls)
                    entry.set_editor_property('AbilityLevel', 1)
                    entry.set_editor_property('InputTag', make_tag(TAG_BUILDABLE))
                    new_grants.append(entry)
                    actions.append({'added_buildable_grant_set_prop': True})
            missing_combat = [t for t in EXPECTED_COMBAT if t not in combat_seen]
            if missing_combat:
                actions.append({'warn_missing_combat_grants': missing_combat})
            aset.set_editor_property('GrantedGameplayAbilities', new_grants)
            save(aset)
            actions.append({'ability_set_saved': True, 'grant_count': len(new_grants)})
        else:
            actions.append({'place_class_missing': place_path})

    unreal.EditorAssetLibrary.save_directory('/Hero_Rawlins', only_if_is_dirty=False, recursive=True)
    print('BWAY_21B_APPLY=' + json.dumps({'actions': actions}))
`;

  const applyResult = await callEditorPython(client, pyApply);
  log.actions = extractJsonMarker(applyResult.raw, 'BWAY_21B_APPLY=');
  writeFileSync('AI_Planning/setup_rawlins_21b_apply.json', JSON.stringify(log.actions, null, 2));
  console.log('Apply result — AI_Planning/setup_rawlins_21b_apply.json');

  const afterResult = await callEditorPython(client, pyProbe);
  log.after = extractJsonMarker(afterResult.raw, 'BWAY_21B_PROBE=');
  writeFileSync('AI_Planning/setup_rawlins_21b_probe_after.json', JSON.stringify(log.after, null, 2));
  console.log('Probe after — AI_Planning/setup_rawlins_21b_probe_after.json');

  writeLog('setup_rawlins_21b_jail_log.json', log);

  const after = log.after;
  if (after?.abilitySet?.grants) {
    console.log('Rawlins grants:', after.abilitySet.grants.map((g) => g.inputTag).join(', '));
  }
  if (after?.hero) {
    console.log('Hero buildable:', after.hero.BuildableDataAsset);
  }
  if (after?.jailDA) {
    console.log('Jail DA actor:', after.jailDA.buildableActorClass, 'maxActive:', after.jailDA.maxActive, 'cost:', after.jailDA.cost);
  }
  if (after?.trapCDO) {
    console.log('Trap CDO:', after.trapCDO.TriggerRadius, after.trapCDO.CaptureRadius, after.trapCDO.JailCageClass);
  }
  if (after?.cageCDO) {
    console.log('Cage CDO:', after.cageCDO.MaxHealth, after.cageCDO.CageRadius, after.cageCDO.bPersistsBetweenRounds);
  }
  console.log('Next: PIE listen server Hero=Rawlins — press 1, LMB confirm; walk enemy into trap; break cages.');

  await client.close?.();
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
