"""Author the initial match shop catalog. Values are provisional, not reference tuning."""
import unreal


def create_upgrade_catalog():
    path = "/BreakawayCore/Economy/DA_BW_MatchUpgrades"
    catalog = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if catalog is None:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.BwayUpgradeCatalog)
        catalog = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "DA_BW_MatchUpgrades", "/BreakawayCore/Economy", unreal.BwayUpgradeCatalog, factory)
    if catalog is None:
        raise RuntimeError("Catalog creation failed")
    definitions = []
    for identifier, name, description, effect, amounts in [
        ("AttackStrength", "Attack strength", "Increase attack strength. Total rank bonuses: +5 / +10 / +15 / +20.", "BwayUpgradeEffect_AttackStrength", [5, 10, 15, 20]),
        ("Armor", "Armor", "Reduce incoming damage with armor. Total rank bonuses: +3 / +6 / +9 / +12 armor.", "BwayUpgradeEffect_Armor", [3, 6, 9, 12]),
    ]:
        entry = unreal.BwayUpgradeDefinition()
        entry.set_editor_property("id", identifier)
        entry.set_editor_property("display_name", name)
        entry.set_editor_property("description", description)
        entry.set_editor_property("effect_class", unreal.load_class(None, "/Script/BreakawayCoreRuntime." + effect))
        ranks = []
        for price, amount in zip([75, 125, 175, 225], amounts):
            rank = unreal.BwayUpgradeRank()
            rank.set_editor_property("cost", price)
            rank.set_editor_property("magnitude", amount)
            ranks.append(rank)
        entry.set_editor_property("ranks", ranks)
        definitions.append(entry)
    catalog.set_editor_property("max_owned_upgrades", 4)
    catalog.set_editor_property("upgrades", definitions)
    if not unreal.EditorAssetLibrary.save_loaded_asset(catalog):
        raise RuntimeError("Catalog save failed")
    print("SHOP_CATALOG_AUTHORED", catalog.get_path_name(), len(definitions))


create_upgrade_catalog()
del create_upgrade_catalog
