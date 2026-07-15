/**
 * Step 19b — Wire Alona Sun Shrine buildable editor assets.
 *
 * Requires Unreal Editor open with MCP bridge (ue-mcp) AFTER HeroAlonaRuntime compiles
 * (ABwayAlonaSunShrineBuildable must be available).
 * Idempotent: safe to re-run.
 *
 * Usage:
 *   node Scripts/setup-alona-19b-sun-shrine.mjs
 *   node Scripts/setup-alona-19b-sun-shrine.mjs --probe-only
 */
import { writeFileSync } from 'node:fs';
import { createMcpClient, text, writeLog } from './lib/bway-widget-setup.mjs';

const PROBE_ONLY = process.argv.includes('--probe-only');

const PATHS = {
  heroRoot: '/Hero_Alona/Characters/Heroes/Alona',
  buildablesDir: '/Hero_Alona/Characters/Heroes/Alona/Buildables',
  abilitiesDir: '/Hero_Alona/Characters/Heroes/Alona/Abilities',
  heroData: '/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona',
  abilitySet: '/Hero_Alona/Characters/Heroes/Alona/DA_BW_AbilitySet_Alona',
  shrineDA: '/Hero_Alona/Characters/Heroes/Alona/Buildables/DA_BW_BuildableData_SunShrine',
  shrineBP: '/Hero_Alona/Characters/Heroes/Alona/Buildables/BP_BW_Buildable_SunShrine',
  placeGA: '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_BuildablePlacement',
  // Legacy migration names (reuse if present)
  legacyShrineDA: '/Hero_Alona/Characters/Heroes/Alona/DA_BW_Buildable_SunShrine',
  legacyBuildableDA: '/Hero_Alona/Characters/Heroes/Alona/DA_BW_BuildableData_Alona',
  legacyBP: '/Hero_Alona/Characters/Heroes/Alona/Buildable/BP_BW_Alona_Buildable',
  legacyMesh: '/Hero_Alona/Characters/Heroes/Alona/Buildable/SKM_BW_alona_buildable',
};

const NATIVE = {
  shrineActor: '/Script/HeroAlonaRuntime.BwayAlonaSunShrineBuildable',
  placeAbility: '/Script/BreakawayCoreRuntime.BwayGameplayAbility_PlaceBuildable',
  buildableDA: '/Script/BreakawayCoreRuntime.BwayBuildableDataAsset',
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

def shrine_da_summary(path):
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
  // Prefer first complete JSON object from the marker line / block
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
  const client = await createMcpClient('setup-alona-19b');
  const log = { probe: null, actions: null, after: null };

  const pyProbe = `
${PY_HELPERS}
report = {
  'hero': hero_summary('${PATHS.heroData}'),
  'abilitySet': ability_set_grants('${PATHS.abilitySet}'),
  'shrineDA': shrine_da_summary('${PATHS.shrineDA}'),
  'legacyShrineDA': shrine_da_summary('${PATHS.legacyShrineDA}'),
  'legacyBuildableDA': shrine_da_summary('${PATHS.legacyBuildableDA}'),
  'parents': {
    'shrineBP': parent_of_bp('${PATHS.shrineBP}'),
    'legacyBP': parent_of_bp('${PATHS.legacyBP}'),
    'placeGA': parent_of_bp('${PATHS.placeGA}'),
  },
  'exists': {
    'shrineBP': exists('${PATHS.shrineBP}'),
    'placeGA': exists('${PATHS.placeGA}'),
    'legacyBP': exists('${PATHS.legacyBP}'),
    'legacyMesh': exists('${PATHS.legacyMesh}'),
  },
  'nativeClasses': {
    'shrineActor': bool(unreal.load_class(None, '${NATIVE.shrineActor}')),
    'placeAbility': bool(unreal.load_class(None, '${NATIVE.placeAbility}')),
    'buildableDA': bool(unreal.load_class(None, '${NATIVE.buildableDA}')),
  },
}
print('BWAY_19B_PROBE=' + json.dumps(report))
`;

  const probeResult = await callEditorPython(client, pyProbe);
  log.probe = extractJsonMarker(probeResult.raw, 'BWAY_19B_PROBE=');
  writeFileSync('AI_Planning/setup_alona_19b_probe.json', JSON.stringify(log.probe, null, 2));
  console.log('Probe OK — wrote AI_Planning/setup_alona_19b_probe.json');

  if (PROBE_ONLY) {
    writeLog('setup_alona_19b_sun_shrine_log.json', log);
    await client.close?.();
    return;
  }

  const pyApply = `
${PY_HELPERS}

SHRINE_PARENT = '${NATIVE.shrineActor}'
PLACE_PARENT = '${NATIVE.placeAbility}'
BUILDABLE_DA_CLS = '${NATIVE.buildableDA}'
TAG_BUILDABLE = 'InputTag.Ability.Buildable'

actions = []

def ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)
        actions.append({'mkdir': path})

for d in [
    '/Hero_Alona',
    '/Hero_Alona/Characters',
    '/Hero_Alona/Characters/Heroes',
    '${PATHS.heroRoot}',
    '${PATHS.buildablesDir}',
    '${PATHS.abilitiesDir}',
]:
    ensure_dir(d)

shrine_parent_cls = unreal.load_class(None, SHRINE_PARENT)
place_parent_cls = unreal.load_class(None, PLACE_PARENT)
buildable_da_cls = unreal.load_class(None, BUILDABLE_DA_CLS)

if not shrine_parent_cls:
    actions.append({'error': 'missing ABwayAlonaSunShrineBuildable — recompile HeroAlonaRuntime first'})
    print('BWAY_19B_APPLY=' + json.dumps({'actions': actions}))
else:
    # --- Ensure shrine actor BP ---
    shrine_bp_path = '${PATHS.shrineBP}'
    if not exists(shrine_bp_path):
        # Prefer reparent/migrate legacy BP if present
        if exists('${PATHS.legacyBP}'):
            ok = unreal.EditorAssetLibrary.duplicate_asset('${PATHS.legacyBP}', shrine_bp_path)
            actions.append({'duplicate_legacy_bp': bool(ok), 'from': '${PATHS.legacyBP}'})
        else:
            factory = unreal.BlueprintFactory()
            factory.set_editor_property('ParentClass', shrine_parent_cls)
            created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                'BP_BW_Buildable_SunShrine', '${PATHS.buildablesDir}', unreal.Blueprint, factory)
            save(created)
            actions.append({'create_shrine_bp': bool(created)})

    if exists(shrine_bp_path) and shrine_parent_cls:
        bp = unreal.load_asset(shrine_bp_path)
        try:
            unreal.BlueprintEditorLibrary.reparent_blueprint(bp, shrine_parent_cls)
            save(bp)
            actions.append({'reparent_shrine_bp': SHRINE_PARENT})
        except Exception as e:
            actions.append({'reparent_shrine_bp_fail': str(e)})

        try:
            cdo = unreal.get_default_object(bp.generated_class())
            cdo.set_editor_property('bPersistsBetweenRounds', True)
            cdo.set_editor_property('BuildTime', 0.0)
            try:
                cdo.set_editor_property('MaxHealth', 750.0)
                cdo.set_editor_property('HealRadius', 500.0)
                cdo.set_editor_property('HealPerSecond', 35.0)
            except Exception as e:
                actions.append({'cdo_config_warn': str(e)})
            save(bp)
            actions.append({'shrine_cdo_configured': True})
        except Exception as e:
            actions.append({'shrine_cdo_fail': str(e)})

    # --- Ensure buildable data asset ---
    shrine_da_path = '${PATHS.shrineDA}'
    if not exists(shrine_da_path) and buildable_da_cls:
        src = None
        if exists('${PATHS.legacyShrineDA}'):
            src = '${PATHS.legacyShrineDA}'
        elif exists('${PATHS.legacyBuildableDA}'):
            src = '${PATHS.legacyBuildableDA}'
        if src:
            ok = unreal.EditorAssetLibrary.duplicate_asset(src, shrine_da_path)
            actions.append({'duplicate_legacy_da': bool(ok), 'from': src})
        else:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property('DataAssetClass', buildable_da_cls)
            created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                'DA_BW_BuildableData_SunShrine', '${PATHS.buildablesDir}', buildable_da_cls, factory)
            save(created)
            actions.append({'create_shrine_da': bool(created)})

    shrine_bp_cls = load_bp_class(shrine_bp_path) if exists(shrine_bp_path) else None
    mesh = unreal.load_asset('${PATHS.legacyMesh}') if exists('${PATHS.legacyMesh}') else None

    if exists(shrine_da_path):
        da = unreal.load_asset(shrine_da_path)
        if shrine_bp_cls:
            da.set_editor_property('BuildableActorClass', shrine_bp_cls)
        da.set_editor_property('Cost', 0.0)
        da.set_editor_property('MaxActiveBuildablesPerPlayer', 3)
        if mesh:
            try:
                da.set_editor_property('PrimaryBuildableMesh', mesh)
            except Exception as e:
                actions.append({'mesh_warn': str(e)})
        save(da)
        actions.append({'shrine_da_wired': shrine_da_path, 'actor': shrine_bp_path if shrine_bp_cls else None})

    # --- Ensure PlaceBuildable BP ---
    place_path = '${PATHS.placeGA}'
    if not exists(place_path) and place_parent_cls:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('ParentClass', place_parent_cls)
        created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'GA_BW_Alona_BuildablePlacement', '${PATHS.abilitiesDir}', unreal.Blueprint, factory)
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
                display.set_editor_property('AbilityName', unreal.Text('Sun Shrine'))
                display.set_editor_property('Description', unreal.Text('Place a healing shrine that regenerates allies in its radius.'))
                cdo.set_editor_property('DisplayData', display)
            except Exception as e:
                actions.append({'place_display_warn': str(e)})
            save(bp)
            actions.append({'place_ga_configured': True})
        except Exception as e:
            actions.append({'place_ga_cdo_fail': str(e)})

    # --- Wire hero DA ---
    if exists('${PATHS.heroData}') and exists(shrine_da_path):
        hero = unreal.load_asset('${PATHS.heroData}')
        da = unreal.load_asset(shrine_da_path)
        hero.set_editor_property('BuildableDataAsset', da)
        try:
            hero.set_editor_property('DisplayName', unreal.Text('Alona'))
        except Exception:
            pass
        save(hero)
        actions.append({'hero_buildable': shrine_da_path})

    # --- Append PlaceBuildable grant on ability set (preserve combat grants) ---
    if exists('${PATHS.abilitySet}') and exists(place_path):
        aset = unreal.load_asset('${PATHS.abilitySet}')
        place_cls = load_bp_class(place_path)
        if place_cls:
            grants = list(aset.get_editor_property('GrantedGameplayAbilities') or [])
            updated = False
            new_grants = []
            for g in grants:
                if tag_name(g.get_editor_property('InputTag')) == TAG_BUILDABLE:
                    # Rebuild via import_text for SoftClassPath reliability
                    exp = '(Ability="%s_C",AbilityLevel=1,InputTag=(TagName="%s"))' % (place_path, TAG_BUILDABLE)
                    # Prefer generated class path
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

    unreal.EditorAssetLibrary.save_directory('/Hero_Alona', only_if_is_dirty=False, recursive=True)
    print('BWAY_19B_APPLY=' + json.dumps({'actions': actions}))
`;

  const applyResult = await callEditorPython(client, pyApply);
  log.actions = extractJsonMarker(applyResult.raw, 'BWAY_19B_APPLY=');
  writeFileSync('AI_Planning/setup_alona_19b_apply.json', JSON.stringify(log.actions, null, 2));
  console.log('Apply result — AI_Planning/setup_alona_19b_apply.json');

  const afterResult = await callEditorPython(client, pyProbe);
  log.after = extractJsonMarker(afterResult.raw, 'BWAY_19B_PROBE=');
  writeFileSync('AI_Planning/setup_alona_19b_probe_after.json', JSON.stringify(log.after, null, 2));
  console.log('Probe after — AI_Planning/setup_alona_19b_probe_after.json');

  writeLog('setup_alona_19b_sun_shrine_log.json', log);

  const after = log.after;
  if (after?.abilitySet?.grants) {
    console.log('Alona grants:', after.abilitySet.grants.map((g) => g.inputTag).join(', '));
  }
  if (after?.hero) {
    console.log('Hero buildable:', after.hero.BuildableDataAsset);
  }
  if (after?.shrineDA) {
    console.log('Shrine DA actor:', after.shrineDA.buildableActorClass, 'maxActive:', after.shrineDA.maxActive);
  }
  console.log('Next: recompile if needed, then PIE listen server Hero=Alona — press 1, LMB confirm.');

  await client.close?.();
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
