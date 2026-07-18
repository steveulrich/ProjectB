/**
 * Step 20b — Wire Korryn Cursed Ward buildable editor assets.
 *
 * Requires Unreal Editor open with MCP bridge (ue-mcp) AFTER HeroMorganRuntime compiles
 * (ABwayKorrynCursedWardBuildable must be available).
 * Idempotent: safe to re-run.
 *
 * Usage:
 *   node Scripts/setup-korryn-20b-cursed-ward.mjs
 *   node Scripts/setup-korryn-20b-cursed-ward.mjs --probe-only
 */
import { writeFileSync } from 'node:fs';
import { createMcpClient, text, writeLog } from './lib/bway-widget-setup.mjs';

const PROBE_ONLY = process.argv.includes('--probe-only');

const PATHS = {
  heroRoot: '/Hero_Morgan/Characters/Heroes/Hexweaver',
  buildableDir: '/Hero_Morgan/Characters/Heroes/Hexweaver/Buildable',
  abilitiesDir: '/Hero_Morgan/Characters/Heroes/Hexweaver/Abilities',
  kitRoot: '/Hero_Morgan/Kit',
  heroData: '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver',
  abilitySet: '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_AbilitySet_Hexweaver',
  kitConfig: '/Hero_Morgan/Kit/DA_BW_KorrynKitConfig',
  wardDA: '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_BuildableData_Hexweaver',
  wardBP: '/Hero_Morgan/Characters/Heroes/Hexweaver/Buildable/BP_BW_Hexweaver_Buildable',
  placeGA: '/Hero_Morgan/Characters/Heroes/Hexweaver/Abilities/GA_BW_Korryn_BuildablePlacement',
};

const NATIVE = {
  wardActor: '/Script/HeroMorganRuntime.BwayKorrynCursedWardBuildable',
  placeAbility: '/Script/BreakawayCoreRuntime.BwayGameplayAbility_PlaceBuildable',
  buildableDA: '/Script/BreakawayCoreRuntime.BwayBuildableDataAsset',
  kitConfigClass: '/Script/HeroMorganRuntime.BwayKorrynKitConfig',
};

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
    return {'exists': True, 'path': path, 'grants': grants}

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

def ward_da_summary(path):
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

def ward_cdo_summary(path):
    if not exists(path):
        return {'exists': False, 'path': path}
    bp = unreal.load_asset(path)
    out = {'exists': True, 'path': path}
    try:
        cdo = unreal.get_default_object(bp.generated_class())
        out['parent'] = cdo.get_class().get_path_name()
        for prop in ['MaxHealth', 'SlowRadius', 'SlowMultiplier', 'BuildTime', 'bPersistsBetweenRounds']:
            try:
                val = cdo.get_editor_property(prop)
                out[prop] = float(val) if prop != 'bPersistsBetweenRounds' else bool(val)
            except Exception as e:
                out[prop] = 'error:' + str(e)
        # Critical: reparented Hexweaver BP previously lost HealthSet/MeshComponent UObject refs.
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
        'BurdenBaseDamage', 'BurdenDamageScaling', 'BurdenCooldown', 'BurdenSlowDuration', 'BurdenSlowMultiplier',
        'CircleCooldown', 'CircleDuration', 'CircleRadius', 'CircleSlowMultiplier', 'CircleIncomingDamageMultiplier',
        'AuraBaseDamage', 'AuraDamageScaling', 'AuraCooldown', 'AuraRadius', 'AuraSilenceDuration',
        'FlockCooldown', 'WardMaxHealth', 'WardSlowRadius', 'WardSlowMultiplier'
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
  const client = await createMcpClient('setup-korryn-20b');
  const log = { probe: null, actions: null, after: null };

  const pyProbe = `
${PY_HELPERS}
report = {
  'hero': hero_summary('${PATHS.heroData}'),
  'abilitySet': ability_set_grants('${PATHS.abilitySet}'),
  'wardDA': ward_da_summary('${PATHS.wardDA}'),
  'wardCDO': ward_cdo_summary('${PATHS.wardBP}'),
  'kitConfig': kit_summary('${PATHS.kitConfig}'),
  'parents': {
    'wardBP': parent_of_bp('${PATHS.wardBP}'),
    'placeGA': parent_of_bp('${PATHS.placeGA}'),
  },
  'exists': {
    'wardBP': exists('${PATHS.wardBP}'),
    'placeGA': exists('${PATHS.placeGA}'),
    'kitConfig': exists('${PATHS.kitConfig}'),
  },
  'nativeClasses': {
    'wardActor': bool(unreal.load_class(None, '${NATIVE.wardActor}')),
    'placeAbility': bool(unreal.load_class(None, '${NATIVE.placeAbility}')),
    'buildableDA': bool(unreal.load_class(None, '${NATIVE.buildableDA}')),
  },
}
print('BWAY_20B_PROBE=' + json.dumps(report))
`;

  const probeResult = await callEditorPython(client, pyProbe);
  log.probe = extractJsonMarker(probeResult.raw, 'BWAY_20B_PROBE=');
  writeFileSync('AI_Planning/setup_korryn_20b_probe.json', JSON.stringify(log.probe, null, 2));
  console.log('Probe OK — wrote AI_Planning/setup_korryn_20b_probe.json');

  if (PROBE_ONLY) {
    writeLog('setup_korryn_20b_cursed_ward_log.json', log);
    await client.close?.();
    return;
  }

  const pyApply = `
${PY_HELPERS}

WARD_PARENT = '${NATIVE.wardActor}'
PLACE_PARENT = '${NATIVE.placeAbility}'
BUILDABLE_DA_CLS = '${NATIVE.buildableDA}'
TAG_BUILDABLE = 'InputTag.Ability.Buildable'

actions = []

def ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)
        actions.append({'mkdir': path})

for d in [
    '/Hero_Morgan',
    '/Hero_Morgan/Characters',
    '/Hero_Morgan/Characters/Heroes',
    '${PATHS.heroRoot}',
    '${PATHS.buildableDir}',
    '${PATHS.abilitiesDir}',
    '${PATHS.kitRoot}',
]:
    ensure_dir(d)

ward_parent_cls = unreal.load_class(None, WARD_PARENT)
place_parent_cls = unreal.load_class(None, PLACE_PARENT)
buildable_da_cls = unreal.load_class(None, BUILDABLE_DA_CLS)

if not ward_parent_cls:
    actions.append({'error': 'missing ABwayKorrynCursedWardBuildable — recompile HeroMorganRuntime first'})
    print('BWAY_20B_APPLY=' + json.dumps({'actions': actions}))
else:
    # --- Seed kit config ward + parity fields ---
    kit_path = '${PATHS.kitConfig}'
    if not exists(kit_path):
        kit_cls = unreal.load_class(None, '${NATIVE.kitConfigClass}')
        if kit_cls:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property('DataAssetClass', kit_cls)
            created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                'DA_BW_KorrynKitConfig', '${PATHS.kitRoot}', kit_cls, factory)
            save(created)
            actions.append({'create_kit_config': bool(created)})

    if exists(kit_path):
        kit = unreal.load_asset(kit_path)
        defaults = {
            'BurdenBaseDamage': 33.0,
            'BurdenDamageScaling': 0.3,
            'BurdenCooldown': 14.0,
            'BurdenSlowDuration': 2.0,
            'BurdenSlowMultiplier': 0.5,
            'CircleCooldown': 20.0,
            'CircleDuration': 5.0,
            'CircleRadius': 700.0,
            'CircleSlowMultiplier': 0.85,
            'CircleIncomingDamageMultiplier': 1.35,
            'AuraBaseDamage': 13.0,
            'AuraDamageScaling': 0.5,
            'AuraCooldown': 30.0,
            'AuraRadius': 800.0,
            'AuraSilenceDuration': 5.0,
            'FlockCooldown': 22.0,
            'WardMaxHealth': 600.0,
            'WardSlowRadius': 600.0,
            'WardSlowMultiplier': 0.5,
        }
        for k, v in defaults.items():
            try:
                kit.set_editor_property(k, v)
            except Exception as e:
                actions.append({'kit_prop_fail': k, 'error': str(e)})
        save(kit)
        actions.append({'kit_config_seeded': kit_path})

    # --- Ensure ward actor BP ---
    # Reparenting the legacy Hexweaver BP left HealthSet/MeshComponent UPROPERTY refs NULL
    # (hard crash in ABuildableActor::InitializeAbilitySystem). Recreate when corrupt.
    ward_bp_path = '${PATHS.wardBP}'
    ward_bp_ok = False
    if exists(ward_bp_path):
        try:
            bp = unreal.load_asset(ward_bp_path)
            cdo = unreal.get_default_object(bp.generated_class())
            hs = cdo.get_editor_property('HealthSet')
            mesh = cdo.get_editor_property('MeshComponent')
            ward_bp_ok = (hs is not None) and (mesh is not None)
            actions.append({'ward_bp_healthset': 'ok' if hs else 'NULL', 'ward_bp_mesh': 'ok' if mesh else 'NULL'})
        except Exception as e:
            actions.append({'ward_bp_probe_fail': str(e)})
            ward_bp_ok = False

    if exists(ward_bp_path) and not ward_bp_ok:
        broken = '${PATHS.buildableDir}/BP_BW_Hexweaver_Buildable_Broken'
        if exists(broken):
            unreal.EditorAssetLibrary.delete_asset(broken)
        ok = unreal.EditorAssetLibrary.rename_asset(ward_bp_path, broken)
        actions.append({'quarantine_corrupt_ward_bp': bool(ok), 'to': broken})

    if not exists(ward_bp_path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('ParentClass', ward_parent_cls)
        created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'BP_BW_Hexweaver_Buildable', '${PATHS.buildableDir}', unreal.Blueprint, factory)
        save(created)
        actions.append({'create_fresh_ward_bp': bool(created)})

    if exists(ward_bp_path) and ward_parent_cls:
        bp = unreal.load_asset(ward_bp_path)
        # Only reparent if already parented incorrectly AND subobjects are healthy.
        # Fresh create already has the correct parent — avoid reparent corruption.
        try:
            cdo = unreal.get_default_object(bp.generated_class())
            hs = cdo.get_editor_property('HealthSet')
            if hs is None:
                actions.append({'warn': 'fresh BP still missing HealthSet — compile BP in editor'})
            else:
                actions.append({'ward_bp_subobjects_ok': True})
        except Exception as e:
            actions.append({'ward_bp_verify_fail': str(e)})

        try:
            cdo = unreal.get_default_object(bp.generated_class())
            cdo.set_editor_property('bPersistsBetweenRounds', True)
            cdo.set_editor_property('BuildTime', 0.0)
            try:
                cdo.set_editor_property('MaxHealth', 600.0)
                cdo.set_editor_property('SlowRadius', 600.0)
                cdo.set_editor_property('SlowMultiplier', 0.5)
            except Exception as e:
                actions.append({'cdo_config_warn': str(e)})
            if exists(kit_path):
                try:
                    cdo.set_editor_property('KitConfig', unreal.load_asset(kit_path))
                except Exception as e:
                    actions.append({'ward_kit_wire_warn': str(e)})
            save(bp)
            actions.append({'ward_cdo_configured': True})
        except Exception as e:
            actions.append({'ward_cdo_fail': str(e)})

    # --- Ensure buildable data asset ---
    ward_da_path = '${PATHS.wardDA}'
    if not exists(ward_da_path) and buildable_da_cls:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('DataAssetClass', buildable_da_cls)
        created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'DA_BW_BuildableData_Hexweaver', '${PATHS.heroRoot}', buildable_da_cls, factory)
        save(created)
        actions.append({'create_ward_da': bool(created)})

    ward_bp_cls = load_bp_class(ward_bp_path) if exists(ward_bp_path) else None

    if exists(ward_da_path):
        da = unreal.load_asset(ward_da_path)
        if ward_bp_cls:
            da.set_editor_property('BuildableActorClass', ward_bp_cls)
        da.set_editor_property('Cost', 0.0)
        da.set_editor_property('MaxActiveBuildablesPerPlayer', 3)
        save(da)
        actions.append({'ward_da_wired': ward_da_path, 'actor': ward_bp_path if ward_bp_cls else None})

    # --- Ensure PlaceBuildable BP ---
    place_path = '${PATHS.placeGA}'
    if not exists(place_path) and place_parent_cls:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('ParentClass', place_parent_cls)
        created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'GA_BW_Korryn_BuildablePlacement', '${PATHS.abilitiesDir}', unreal.Blueprint, factory)
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
                display.set_editor_property('AbilityName', unreal.Text('Cursed Ward'))
                display.set_editor_property('Description', unreal.Text('Place a ward that slows enemies in its radius.'))
                cdo.set_editor_property('DisplayData', display)
            except Exception as e:
                actions.append({'place_display_warn': str(e)})
            save(bp)
            actions.append({'place_ga_configured': True})
        except Exception as e:
            actions.append({'place_ga_cdo_fail': str(e)})

    # --- Wire hero DA ---
    if exists('${PATHS.heroData}') and exists(ward_da_path):
        hero = unreal.load_asset('${PATHS.heroData}')
        da = unreal.load_asset(ward_da_path)
        hero.set_editor_property('BuildableDataAsset', da)
        try:
            hero.set_editor_property('DisplayName', unreal.Text('Korryn'))
        except Exception:
            pass
        save(hero)
        actions.append({'hero_buildable': ward_da_path})

    # --- Append PlaceBuildable grant (preserve five combat grants) ---
    if exists('${PATHS.abilitySet}') and exists(place_path):
        aset = unreal.load_asset('${PATHS.abilitySet}')
        place_cls = load_bp_class(place_path)
        if place_cls:
            grants = list(aset.get_editor_property('GrantedGameplayAbilities') or [])
            updated = False
            new_grants = []
            for g in grants:
                if tag_name(g.get_editor_property('InputTag')) == TAG_BUILDABLE:
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
            aset.set_editor_property('GrantedGameplayAbilities', new_grants)
            save(aset)
            actions.append({'ability_set_saved': True, 'grant_count': len(new_grants)})
        else:
            actions.append({'place_class_missing': place_path})

    unreal.EditorAssetLibrary.save_directory('/Hero_Morgan', only_if_is_dirty=False, recursive=True)
    print('BWAY_20B_APPLY=' + json.dumps({'actions': actions}))
`;

  const applyResult = await callEditorPython(client, pyApply);
  log.actions = extractJsonMarker(applyResult.raw, 'BWAY_20B_APPLY=');
  writeFileSync('AI_Planning/setup_korryn_20b_apply.json', JSON.stringify(log.actions, null, 2));
  console.log('Apply result — AI_Planning/setup_korryn_20b_apply.json');

  const afterResult = await callEditorPython(client, pyProbe);
  log.after = extractJsonMarker(afterResult.raw, 'BWAY_20B_PROBE=');
  writeFileSync('AI_Planning/setup_korryn_20b_probe_after.json', JSON.stringify(log.after, null, 2));
  console.log('Probe after — AI_Planning/setup_korryn_20b_probe_after.json');

  writeLog('setup_korryn_20b_cursed_ward_log.json', log);

  const after = log.after;
  if (after?.abilitySet?.grants) {
    console.log('Korryn grants:', after.abilitySet.grants.map((g) => g.inputTag).join(', '));
  }
  if (after?.hero) {
    console.log('Hero buildable:', after.hero.BuildableDataAsset);
  }
  if (after?.wardDA) {
    console.log('Ward DA actor:', after.wardDA.buildableActorClass, 'maxActive:', after.wardDA.maxActive, 'cost:', after.wardDA.cost);
  }
  if (after?.wardCDO) {
    console.log('Ward CDO:', after.wardCDO.MaxHealth, after.wardCDO.SlowRadius, after.wardCDO.SlowMultiplier);
  }
  console.log('Next: PIE listen server Hero=Korryn — press 1, LMB confirm; spot-check 20c kit values.');

  await client.close?.();
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
