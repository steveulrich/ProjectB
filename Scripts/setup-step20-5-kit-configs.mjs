/**
 * Step 20.5 — Create/seed Argus + Alona kit configs; soft-bind combat abilities.
 * Probe also verifies canonical Korryn kit (read-only). Does not change grants/inputs/buildables.
 *
 * Requires Unreal Editor open with MCP bridge AFTER BreakawayCoreRuntime + HeroAlonaRuntime compile.
 * Idempotent: safe to re-run.
 *
 * Usage:
 *   node Scripts/setup-step20-5-kit-configs.mjs
 *   node Scripts/setup-step20-5-kit-configs.mjs --probe-only
 */
import { createMcpClient, text, writeLog } from './lib/bway-widget-setup.mjs';

const PROBE_ONLY = process.argv.includes('--probe-only');

const PATHS = {
  argusKitRoot: '/Hero_Spartacus/Kit',
  alonaKitRoot: '/Hero_Alona/Kit',
  argusKit: '/Hero_Spartacus/Kit/DA_BW_ArgusKitConfig',
  alonaKit: '/Hero_Alona/Kit/DA_BW_AlonaKitConfig',
  korrynKit: '/Hero_Morgan/Kit/DA_BW_KorrynKitConfig',
  korrynDuplicate: '/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_KorrynKitConfig',
};

const NATIVE = {
  argusKitClass: '/Script/BreakawayCoreRuntime.BwayArgusKitConfig',
  alonaKitClass: '/Script/HeroAlonaRuntime.BwayAlonaKitConfig',
  korrynKitClass: '/Script/HeroMorganRuntime.BwayKorrynKitConfig',
  argusAbilities: [
    '/Script/BreakawayCoreRuntime.BwayGameplayAbility_MeleePrimary',
    '/Script/BreakawayCoreRuntime.BwayGameplayAbility_ArgusSlide',
    '/Script/BreakawayCoreRuntime.BwayGameplayAbility_NoRetreat',
    '/Script/BreakawayCoreRuntime.BwayGameplayAbility_ForGlory',
    '/Script/BreakawayCoreRuntime.BwayGameplayAbility_Retribution',
  ],
  alonaAbilities: [
    '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaPrimary',
    '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaSunsGrace',
    '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaRadiance',
    '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaSunBurst',
    '/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaBlessing',
  ],
};

const BP_PATHS = {
  argus: [
    '/Hero_Spartacus/Characters/Heroes/Argus/Abilities/GA_BW_Spartacus_PrimaryAttack',
    '/Hero_Spartacus/Characters/Heroes/Argus/Abilities/GA_BW_Spartacus_Slide',
    '/Hero_Spartacus/Characters/Heroes/Argus/Abilities/GA_BW_Spartacus_NoRetreat',
    '/Hero_Spartacus/Characters/Heroes/Argus/Abilities/GA_BW_Spartacus_ForGlory',
    '/Hero_Spartacus/Characters/Heroes/Argus/Abilities/GA_BW_Spartacus_Retribution',
  ],
  alona: [
    '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_PrimaryAttack',
    '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_SunsGrace',
    '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_Radiance',
    '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_Sunburst',
    '/Hero_Alona/Characters/Heroes/Alona/Abilities/GA_BW_Alona_Blessing',
  ],
};

const ARGUS_DEFAULTS = {
  PrimaryBaseDamage: 10.0,
  PrimaryDamageScaling: 0.4,
  PrimaryTraceRadius: 50.0,
  PrimaryTraceDistance: 175.0,
  SlideDashDistance: 900.0,
  SlideDashDuration: 0.35,
  SlideCooldown: 18.0,
  NoRetreatChargeDistance: 1000.0,
  NoRetreatChargeDuration: 0.4,
  NoRetreatTraceRadius: 120.0,
  NoRetreatBaseDamage: 2.0,
  NoRetreatDamageScaling: 0.4,
  NoRetreatKnockbackStrength: 1200.0,
  NoRetreatKnockbackUpward: 250.0,
  NoRetreatCooldown: 12.0,
  ForGloryTraceRadius: 80.0,
  ForGloryTraceDistance: 200.0,
  ForGloryBaseDamage: 2.0,
  ForGloryDamageScaling: 0.4,
  ForGloryKnockbackStrength: 1600.0,
  ForGloryKnockbackUpward: 350.0,
  ForGloryCooldown: 25.0,
  RetributionTraceRadius: 90.0,
  RetributionTraceDistance: 220.0,
  RetributionUppercutBaseDamage: 10.0,
  RetributionDownSmashBaseDamage: 20.0,
  RetributionDamageScaling: 0.5,
  RetributionUppercutDelay: 0.05,
  RetributionDownSmashDelay: 0.35,
  RetributionUppercutKnockback: 400.0,
  RetributionUppercutUpward: 700.0,
  RetributionDownSmashKnockback: 900.0,
  RetributionDownSmashDownward: 400.0,
  RetributionCooldown: 30.0,
};

const ALONA_DEFAULTS = {
  PrimaryBaseDamage: 28.0,
  PrimaryDamageScaling: 0.25,
  PrimaryProjectileSpeed: 3200.0,
  PrimaryProjectileLifeSpan: 3.0,
  PrimarySpawnForwardOffset: 80.0,
  SunsGraceTeleportDistance: 750.0,
  SunsGraceInvulnerabilityDuration: 0.25,
  SunsGraceCooldown: 22.0,
  RadianceActiveDuration: 4.0,
  RadianceHealTickInterval: 1.0,
  RadianceHealPerTick: 20.0,
  RadianceStrengthDivisor: 52.0,
  RadianceMaxHealMultiplier: 1.75,
  RadianceMaxTargetRange: 2500.0,
  RadianceMaxRayDistance: 400.0,
  RadianceCooldown: 8.0,
  SunBurstRadius: 350.0,
  SunBurstBaseDamage: 30.0,
  SunBurstDamageScaling: 0.4,
  SunBurstKnockbackStrength: 1400.0,
  SunBurstKnockbackUpward: 400.0,
  SunBurstCooldown: 20.0,
  BlessingForwardPlacementDistance: 400.0,
  BlessingZoneRadius: 400.0,
  BlessingZoneDuration: 5.0,
  BlessingInitialHeal: 70.0,
  BlessingHealPerSecond: 35.0,
  BlessingCooldown: 25.0,
};

const PY_HELPERS = `
import unreal
import json

def exists(p):
    return unreal.EditorAssetLibrary.does_asset_exist(p)

def save(asset):
    if asset:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)

def object_path(value):
    if not value:
        return None
    try:
        return value.get_path_name()
    except Exception:
        return str(value)

def kit_summary(path, props):
    if not path or not exists(path):
        return {'exists': False, 'path': path}
    kit = unreal.load_asset(path)
    out = {
        'exists': True,
        'path': path,
        'class': object_path(kit.get_class()) if kit else None,
    }
    try:
        out['primaryAssetId'] = str(kit.get_primary_asset_id())
    except Exception as e:
        out['primaryAssetId'] = 'error:' + str(e)
    for prop in props:
        try:
            out[prop] = float(kit.get_editor_property(prop))
        except Exception as e:
            out[prop] = 'error:' + str(e)
    return out

def soft_kit_on_cdo(cls_path):
    cls = unreal.load_class(None, cls_path)
    if not cls:
        return {'class': cls_path, 'loaded': False}
    try:
        cdo = unreal.get_default_object(cls)
        kit = cdo.get_editor_property('KitConfig')
        return {
            'class': cls_path,
            'loaded': True,
            'kitConfig': object_path(kit) if kit else None,
            'kitConfigSoft': str(kit) if kit is not None else None,
        }
    except Exception as e:
        return {'class': cls_path, 'loaded': True, 'error': str(e)}

def soft_kit_on_bp(bp_path):
    if not exists(bp_path):
        return {'path': bp_path, 'exists': False}
    try:
        bp = unreal.load_asset(bp_path)
        generated = bp.generated_class() if bp else None
        cdo = unreal.get_default_object(generated) if generated else None
        if not cdo:
            return {'path': bp_path, 'exists': True, 'error': 'missing CDO'}
        kit = cdo.get_editor_property('KitConfig')
        return {
            'path': bp_path,
            'exists': True,
            'kitConfig': object_path(kit) if kit else None,
            'kitConfigSoft': str(kit) if kit is not None else None,
        }
    except Exception as e:
        return {'path': bp_path, 'exists': True, 'error': str(e)}

def ensure_dir(path, actions):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)
        actions.append({'mkdir': path})

def create_or_get_kit(asset_path, package_path, asset_name, class_path, actions):
    if exists(asset_path):
        actions.append({'kit_exists': asset_path})
        return asset_path
    kit_cls = unreal.load_class(None, class_path)
    if not kit_cls:
        actions.append({'create_kit_fail': asset_path, 'error': 'missing class ' + class_path})
        return None
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('DataAssetClass', kit_cls)
    created = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name, package_path, kit_cls, factory)
    save(created)
    actions.append({'create_kit': asset_path, 'ok': bool(created)})
    return asset_path if created else None

def seed_kit(asset_path, defaults, actions):
    if not asset_path or not exists(asset_path):
        actions.append({'seed_fail': asset_path, 'error': 'missing'})
        return
    kit = unreal.load_asset(asset_path)
    for key, value in defaults.items():
        try:
            kit.set_editor_property(key, value)
        except Exception as e:
            actions.append({'kit_prop_fail': key, 'error': str(e)})
    save(kit)
    actions.append({'kit_seeded': asset_path, 'count': len(defaults)})

def wire_kit_to_native(cls_paths, kit_path, actions):
    if not kit_path or not exists(kit_path):
        return
    kit = unreal.load_asset(kit_path)
    for cls_path in cls_paths:
        cls = unreal.load_class(None, cls_path)
        if not cls:
            actions.append({'kit_wire_native_skip': cls_path, 'error': 'missing class'})
            continue
        try:
            cdo = unreal.get_default_object(cls)
            cdo.set_editor_property('KitConfig', kit)
            actions.append({'kit_wired_native': cls_path})
        except Exception as e:
            actions.append({'kit_wire_native_skip': cls_path, 'error': str(e)})

def wire_kit_to_bps(bp_paths, kit_path, actions):
    if not kit_path or not exists(kit_path):
        return
    kit = unreal.load_asset(kit_path)
    for bp_path in bp_paths:
        if not exists(bp_path):
            actions.append({'kit_wire_bp_skip': bp_path, 'error': 'missing bp'})
            continue
        try:
            bp = unreal.load_asset(bp_path)
            generated = bp.generated_class()
            cdo = unreal.get_default_object(generated) if generated else None
            if not cdo:
                actions.append({'kit_wire_bp_skip': bp_path, 'error': 'missing CDO'})
                continue
            cdo.set_editor_property('KitConfig', kit)
            save(bp)
            actions.append({'kit_wired_bp': bp_path})
        except Exception as e:
            actions.append({'kit_wire_bp_skip': bp_path, 'error': str(e)})
`;

const ARGUS_PROBE_PROPS = Object.keys(ARGUS_DEFAULTS)
  .filter((k) =>
    [
      'PrimaryBaseDamage',
      'SlideCooldown',
      'NoRetreatCooldown',
      'ForGloryCooldown',
      'RetributionCooldown',
      'NoRetreatBaseDamage',
      'RetributionUppercutBaseDamage',
      'RetributionDownSmashBaseDamage',
    ].includes(k),
  );

const ALONA_PROBE_PROPS = Object.keys(ALONA_DEFAULTS).filter((k) =>
  [
    'PrimaryBaseDamage',
    'SunsGraceCooldown',
    'RadianceCooldown',
    'SunBurstCooldown',
    'BlessingCooldown',
    'RadianceHealPerTick',
    'BlessingInitialHeal',
    'BlessingHealPerSecond',
  ].includes(k),
);

const KORRYN_PROBE_PROPS = [
  'PrimaryBaseDamage',
  'FlockCooldown',
  'BurdenCooldown',
  'CircleCooldown',
  'AuraCooldown',
];

function pyList(values) {
  return values.map((v) => `'${v}'`).join(', ');
}

function pyDefaults(obj) {
  return Object.entries(obj)
    .map(([k, v]) => `'${k}': ${typeof v === 'number' ? v : JSON.stringify(v)}`)
    .join(',\n    ');
}

async function main() {
  const client = await createMcpClient('setup-step20-5-kit-configs');
  const log = { probe: null, actions: null, after: null };

  const pyProbe = `
${PY_HELPERS}
report = {
  'kits': {
    'argus': kit_summary('${PATHS.argusKit}', [${pyList(ARGUS_PROBE_PROPS)}]),
    'alona': kit_summary('${PATHS.alonaKit}', [${pyList(ALONA_PROBE_PROPS)}]),
    'korryn': kit_summary('${PATHS.korrynKit}', [${pyList(KORRYN_PROBE_PROPS)}]),
  },
  'korrynDuplicate': {
    'path': '${PATHS.korrynDuplicate}',
    'exists': exists('${PATHS.korrynDuplicate}'),
  },
  'dirs': {
    'argusKit': unreal.EditorAssetLibrary.does_directory_exist('${PATHS.argusKitRoot}'),
    'alonaKit': unreal.EditorAssetLibrary.does_directory_exist('${PATHS.alonaKitRoot}'),
  },
  'nativeClasses': {
    k: bool(unreal.load_class(None, v)) for k, v in {
      'argusKit': '${NATIVE.argusKitClass}',
      'alonaKit': '${NATIVE.alonaKitClass}',
      'korrynKit': '${NATIVE.korrynKitClass}',
      'meleePrimary': '${NATIVE.argusAbilities[0]}',
      'argusSlide': '${NATIVE.argusAbilities[1]}',
      'alonaPrimary': '${NATIVE.alonaAbilities[0]}',
      'alonaBlessing': '${NATIVE.alonaAbilities[4]}',
    }.items()
  },
  'argusNativeWiring': [soft_kit_on_cdo(p) for p in [${pyList(NATIVE.argusAbilities)}]],
  'alonaNativeWiring': [soft_kit_on_cdo(p) for p in [${pyList(NATIVE.alonaAbilities)}]],
  'argusBpWiring': [soft_kit_on_bp(p) for p in [${pyList(BP_PATHS.argus)}]],
  'alonaBpWiring': [soft_kit_on_bp(p) for p in [${pyList(BP_PATHS.alona)}]],
}
print('BWAY_JSON:' + json.dumps(report))
`;

  const probeResult = await client.callTool({
    name: 'editor',
    arguments: { action: 'execute_python', code: pyProbe },
  });
  log.probe = text(probeResult.content);
  writeLog('setup_step20_5_kit_configs_probe.json', log);

  if (PROBE_ONLY) {
    console.log(log.probe);
    await client.close?.();
    return;
  }

  const pyFix = `
${PY_HELPERS}
actions = []

# Directories
for d in [
  '/Hero_Spartacus', '/Hero_Spartacus/Kit',
  '/Hero_Alona', '/Hero_Alona/Kit',
]:
    ensure_dir(d, actions)

argus_kit = create_or_get_kit(
  '${PATHS.argusKit}', '${PATHS.argusKitRoot}', 'DA_BW_ArgusKitConfig',
  '${NATIVE.argusKitClass}', actions)
alona_kit = create_or_get_kit(
  '${PATHS.alonaKit}', '${PATHS.alonaKitRoot}', 'DA_BW_AlonaKitConfig',
  '${NATIVE.alonaKitClass}', actions)

seed_kit(argus_kit, {
    ${pyDefaults(ARGUS_DEFAULTS)}
}, actions)
seed_kit(alona_kit, {
    ${pyDefaults(ALONA_DEFAULTS)}
}, actions)

wire_kit_to_native([${pyList(NATIVE.argusAbilities)}], argus_kit, actions)
wire_kit_to_native([${pyList(NATIVE.alonaAbilities)}], alona_kit, actions)
wire_kit_to_bps([${pyList(BP_PATHS.argus)}], argus_kit, actions)
wire_kit_to_bps([${pyList(BP_PATHS.alona)}], alona_kit, actions)

# Remove approved duplicate only after canonical Korryn kit is verified.
if exists('${PATHS.korrynKit}') and exists('${PATHS.korrynDuplicate}'):
    deleted = unreal.EditorAssetLibrary.delete_asset('${PATHS.korrynDuplicate}')
    actions.append({'delete_korryn_duplicate': '${PATHS.korrynDuplicate}', 'ok': bool(deleted)})
elif exists('${PATHS.korrynDuplicate}') and not exists('${PATHS.korrynKit}'):
    actions.append({
      'delete_korryn_duplicate_skipped': '${PATHS.korrynDuplicate}',
      'reason': 'canonical kit missing',
    })
else:
    actions.append({'korryn_duplicate_absent_or_canonical_ok': True})

print('BWAY_JSON:' + json.dumps({'actions': actions}))
`;

  const fixResult = await client.callTool({
    name: 'editor',
    arguments: { action: 'execute_python', code: pyFix },
  });
  log.actions = text(fixResult.content);

  const afterResult = await client.callTool({
    name: 'editor',
    arguments: { action: 'execute_python', code: pyProbe },
  });
  log.after = text(afterResult.content);
  writeLog('setup_step20_5_kit_configs_log.json', log);
  console.log(JSON.stringify(log, null, 2));
  await client.close?.();
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
