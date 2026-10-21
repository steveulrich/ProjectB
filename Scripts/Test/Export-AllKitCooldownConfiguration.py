"""Read authored cooldowns without saving assets. Run only in an owned, fresh editor.

exec(open(r'<project>/Scripts/Test/Export-AllKitCooldownConfiguration.py').read())
export_configuration(r'<successful package-result.json>', r'<fresh output.json>')

This export is configuration evidence, never gameplay or visual acceptance.
"""
import datetime
import hashlib
import json
from pathlib import Path
import unreal


CASES = [
    ("Argus", "Spartacus", "ForGlory", "ForGloryCooldown", "for_glory_cooldown"),
    ("Argus", "Spartacus", "NoRetreat", "NoRetreatCooldown", "no_retreat_cooldown"),
    ("Argus", "Spartacus", "Retribution", "RetributionCooldown", "retribution_cooldown"),
    ("Argus", "Spartacus", "ArgusSlide", "SlideCooldown", "slide_cooldown"),
    ("Rawlins", "Rawlins", "RawlinsPowerShot", "PowerShotCooldown", "power_shot_cooldown"),
    ("Rawlins", "Rawlins", "RawlinsDoubleDown", "DoubleDownCooldown", "double_down_cooldown"),
    ("Rawlins", "Rawlins", "RawlinsSlideShot", "SlideShotCooldown", "slide_shot_cooldown"),
    ("Rawlins", "Rawlins", "RawlinsBlazingBarrage", "BarrageCooldown", "barrage_cooldown"),
    ("Alona", "Alona", "AlonaRadiance", "RadianceCooldown", "radiance_cooldown"),
    ("Alona", "Alona", "AlonaBlessing", "BlessingCooldown", "blessing_cooldown"),
    ("Alona", "Alona", "AlonaSunBurst", "SunBurstCooldown", "sun_burst_cooldown"),
    ("Alona", "Alona", "AlonaSunsGrace", "SunsGraceCooldown", "suns_grace_cooldown"),
    ("Korryn", "Morgan", "KorrynBurdenOfSin", "BurdenCooldown", "burden_cooldown"),
    ("Korryn", "Morgan", "KorrynCircleOfSpite", "CircleCooldown", "circle_cooldown"),
    ("Korryn", "Morgan", "KorrynAuraOfSilence", "AuraCooldown", "aura_cooldown"),
    ("Korryn", "Morgan", "KorrynFlock", "FlockCooldown", "flock_cooldown"),
]


def export_configuration(package_result, output):
    root = Path(unreal.Paths.project_dir()).resolve()
    destination = Path(output).resolve()
    if not destination.is_relative_to(root / "Saved") or destination.exists():
        raise RuntimeError("Use a fresh output beneath project Saved")
    package_path = Path(package_result).resolve()
    package = json.loads(package_path.read_text(encoding="utf-8-sig"))
    if package.get("status") != "passed" or package.get("exitCode") != 0:
        raise RuntimeError("Require a successful immutable package record")
    digest = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
    if digest(package["sourceManifest"]) != package["sourceManifestSha256"].upper():
        raise RuntimeError("Package source manifest changed")
    dirty = {p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}
    assets, configs, rows = [], {}, []
    editor_assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)

    def load(asset_path, relative_path):
        if asset_path in dirty:
            raise RuntimeError("Unsaved asset: " + asset_path)
        unreal.AssetRegistryHelpers.get_asset_registry().scan_paths_synchronous(
            [asset_path.rsplit("/", 1)[0]], force_rescan=True)
        asset = editor_assets.load_asset(asset_path)
        if not asset:
            raise RuntimeError("Missing asset: " + asset_path)
        assets.append({"path": relative_path, "asset": asset_path, "sha256": digest(root / relative_path)})
        return asset

    for index, (hero, plugin, family, field, python_field) in enumerate(CASES):
        if hero not in configs:
            configs[hero] = load(f"/Hero_{plugin}/Kit/DA_BW_{hero}KitConfig",
                f"Plugins/GameFeatures/Heroes/{plugin}/Content/Kit/DA_BW_{hero}KitConfig.uasset")
        base = float(configs[hero].get_editor_property(python_field))
        if not 0 < base <= 120:
            raise RuntimeError(f"Unbounded configured cooldown: {hero}/{field}={base}")
        rows.append({"case": index, "hero": hero, "nativeClass": "UBwayGameplayAbility_" + family,
                     "cooldownField": field, "baseDuration": base})
    catalog = load("/BreakawayCore/Economy/DA_BW_MatchUpgrades",
                   "Plugins/GameFeatures/BreakawayCore/Content/Economy/DA_BW_MatchUpgrades.uasset")
    entries = [x for x in catalog.get_editor_property("upgrades")
               if str(x.get_editor_property("id")) == "CooldownReduction"]
    if len(entries) != 1:
        raise RuntimeError("Require one CooldownReduction upgrade")
    rank = entries[0].get_editor_property("ranks")[0]
    bonus = float(rank.get_editor_property("magnitude"))
    effect = entries[0].get_editor_property("effect_class").get_path_name()
    if not 0 < bonus < 0.5 or not effect.endswith(".BwayUpgradeEffect_CooldownReduction"):
        raise RuntimeError("Unexpected rank-one cooldown upgrade")
    record = {"schema": "allkit-cooldown-config-1", "status": "validated-authored-config",
              "scope": "Fresh read-only authored asset export; cooked/runtime agreement still required",
              "createdUtc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "candidate": package["candidate"], "effectiveSourceIdentity": package["effectiveSourceIdentity"],
              "packageResult": str(package_path), "packageResultSHA256": digest(package_path),
              "sourceManifestSHA256": package["sourceManifestSha256"], "abilityCount": 16,
              "abilities": rows, "assets": assets, "cooldownReduction": bonus,
              "upgradeRank": 1, "upgradeCost": int(rank.get_editor_property("cost")), "effectClass": effect}
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    unreal.log(f"AllKitCooldownConfiguration: PASS abilities=16 assets=5 evidence={destination}")
