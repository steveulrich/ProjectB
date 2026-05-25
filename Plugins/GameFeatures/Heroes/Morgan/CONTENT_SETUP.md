# Morgan Le Fay — Content Setup

Controller/debuff hero. Implement abilities as BP `ULyraGameplayAbility` or new C++ subclasses.

## Design Spec Abilities

- **Q:** Burden of Sin (slow projectile)
- **E:** Circle of Spite (hex zone)
- **Ultimate:** Aura of Silence (AOE silence + damage)

## Buildables

- Elder Stone (trap/explosion charge)
- Tome of Frailty (`ATurretBase` — attack debuff)

## Hero Data

`DA_BW_Hero_Morgan` — Support/Controller class, two entries in `BuildableDataAssets`.

Register via `GFD_Hero_Morgan` (`UBwayGameFeatureData`).

See [Breakaway_Reborn_Design_Spec.md](../../../AI_Planning/Breakaway_Reborn_Design_Spec.md).
