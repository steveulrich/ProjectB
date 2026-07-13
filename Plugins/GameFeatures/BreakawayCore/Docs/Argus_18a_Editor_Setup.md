# Step 18a — Argus Functional (Editor Setup)

C++ landed in `BreakawayCoreRuntime` (hero stats, mesh swap, `BwayGameplayAbility_MeleePrimary`). Complete these Editor steps after recompiling.

**Related:** [Argus_18b_Editor_Setup.md](./Argus_18b_Editor_Setup.md) · [Buildable_System.md](./Buildable_System.md) · [CONTENT_SETUP.md](../../Heroes/Spartacus/CONTENT_SETUP.md)

## Ability ownership (canonical)

| Scope | Ability set | Grants |
|-------|-------------|--------|
| **Common (all heroes)** | `DA_BW_AbilitySet_Humanoid` | Left Shift movement slide · RMB Request Relic · LMB Confirm placement · RMB Cancel placement |
| **Per-hero** | `DA_BW_AbilitySet_Argus` (etc.) | LMB Primary · F / Q / E / R combat · key **1** PlaceBuildable |
| **Hero DA** | `DA_BW_HeroData_Argus` | `BuildableDataAsset` (which buildable spawns — empty until 18b) |

**Do not** put PlaceBuildable, Confirm, or Cancel on the wrong set. **Do not** put Request Relic or movement slide on the per-hero set.

## Input layout (all heroes)

| Key | Input tag | Scope | Notes |
|-----|-----------|-------|-------|
| **LMB** | `InputTag.Ability.Primary` | Per-hero | Primary attack |
| **RMB** | `InputTag.Ability.RelicRequest` | **Common** | **Request Relic** — open-for-relic / pass-me status |
| **F** | `InputTag.Ability.Ability4` | Per-hero | Hero **defense / dodge** (Argus sheet name: **Slide**) |
| **Q** | `InputTag.Ability.Ability1` | Per-hero | Ability 1 |
| **E** | `InputTag.Ability.Ability2` | Per-hero | Ability 2 |
| **R** | `InputTag.Ability.Ability3` | Per-hero | Ability 3 (ultimate) |
| **Left Shift** | `InputTag.Ability.Slide` | **Common** | Base **movement slide** — all heroes |
| **1** | `InputTag.Ability.Buildable` | **Per-hero** | PlaceBuildable BP child on hero ability set; hero DA `BuildableDataAsset` selects spawn class |

**Stats sheet vs keys:** The stats sheet labels dodge under an **RMB** column (design slot). In-game, **`InputTag.Ability.Ability4`** is on **F**. **RMB** is reserved for common **Request Relic**.

**Do not** use `InputTag.Ability.Defense` — use **`InputTag.Ability.Ability4`**.

**Do not** bind `InputTag.Ability.Slide` to RMB or F — that tag is Left Shift movement slide only.

## 1. Enable Hero_Spartacus

1. **Edit → Plugins → Hero Spartacus** → Enabled → restart if prompted.
2. Open **`B_BW_Experience_Dev`** → **GameFeaturesToEnable** → add **`Hero_Spartacus`**.
3. Confirm `Config/DefaultGame.ini` scans `/Hero_Spartacus/Characters/Heroes` under `HeroDataAsset`.

## 2. Configure `DA_BW_HeroData_Argus`

**Path:** `/BreakawayCore/Characters/Heroes/Argus/DA_BW_HeroData_Argus` (or `/Hero_Spartacus/...` after migration)

| Field | Value |
|-------|--------|
| **DisplayName** | `Argus` (Section 3 official UI name) |
| **HeroStats → MaxHealth** | `500` |
| **HeroStats → BaseDamage** | `50` (Attack Str) |
| **HeroStats → Armor** | `3` |
| **HeroStats → MoveSpeed** | `10` (sheet rating → 600 uu/s in C++) |
| **AttributeSetClass** | `BwayHeroAttributeSet` |
| **HeroMesh** | Argus/Spartacus skeletal mesh (**not** Alona placeholder) |
| **AnimationBP** | Matching AnimBP (**required** — fixes T-pose) |

**Buildables:** leave `BuildableDataAsset` empty for 18a — Siege Engine lands in **Step 18b**.

## 3. Common abilities (humanoid set)

On **`DA_BW_AbilitySet_Humanoid`**:

| Input tag | Ability | Key | Notes |
|-----------|---------|-----|-------|
| `InputTag.Ability.Slide` | `BwayGameplayAbility_Slide` → slide BP | **Left Shift** | Not blocked when relic carrier |
| `InputTag.Ability.RelicRequest` | Request Relic GA | **RMB** | Reparent to `BwayGameplayAbility_RelicRequest` in 18b |

Confirm / Cancel placement GAs are added in **18b** (same humanoid set). **Do not** add PlaceBuildable here.

## 4. Ability set `DA_BW_AbilitySet_Argus`

Parent: **`LyraAbilitySet`**. Wire combat kit; add PlaceBuildable in **18b** (row shown for planning):

| Slot | Input tag | Sheet ability | C++ parent (create BP child) | 18a stand-in |
|------|-----------|---------------|------------------------------|--------------|
| LMB | `InputTag.Ability.Primary` | Primary Attack | `BwayGameplayAbility_MeleePrimary` → `GA_Argus_Primary` | — |
| F | `InputTag.Ability.Ability4` | **Slide** (defense dash) | `BwayGameplayAbility_GladiatorsLeap` → temp dash BP | Replace in 18c |
| Q | `InputTag.Ability.Ability1` | No Retreat | `BwayGameplayAbility_ShieldBash` → temp BP | Replace in 18c |
| E | `InputTag.Ability.Ability2` | For Glory | `BwayGameplayAbility_WarCry` → temp BP | Replace in 18c |
| R | `InputTag.Ability.Ability3` | Retribution | `BwayGameplayAbility_DefensiveStance` → temp BP | Replace in 18c |
| 1 | `InputTag.Ability.Buildable` | Siege Engine | `BwayGameplayAbility_PlaceBuildable` → `GA_BW_Spartacus_BuildablePlacement` | Step **18b** |

**Each combat BP:** assign a **Cooldown Gameplay Effect** or abilities fire with no CD.

**Relic carrier:** LMB / F / Q / E / R (`BwayGameplayAbility_Base`) are **blocked** while carrying. Left Shift slide, RMB Request Relic, and key-1 PlaceBuildable remain usable (`UBwayGameplayAbility` / Lyra, not `Base`).

## 5. Input data

On **`DA_BW_InputData_Humanoid`**:

| Key | Tag |
|-----|-----|
| LMB | `InputTag.Ability.Primary` |
| RMB | `InputTag.Ability.RelicRequest` |
| F | `InputTag.Ability.Ability4` |
| Q | `InputTag.Ability.Ability1` |
| E | `InputTag.Ability.Ability2` |
| R | `InputTag.Ability.Ability3` |
| Left Shift | `InputTag.Ability.Slide` |
| 1 | `InputTag.Ability.Buildable` |

Dual LMB/RMB Confirm/Cancel tags are added in **18b** (keep Primary / RelicRequest; **add** second tags).

## 6. PIE test

```
L_BW_DevMap?Experience=B_BW_Experience_Dev&Hero=Argus&NumBots=7
```

**Listen server**, 1 player.

### Pass checklist (18a)

- [ ] Single visible mesh (no humanoid stack); AnimBP playing (not T-pose)
- [ ] Log: `BwayHeroStatsLibrary: Applied stats … HP 500 …`
- [ ] LMB / F / Q / E / R activate; **Left Shift** movement slide works
- [ ] **F** defense dash activates (Argus sheet "Slide")
- [ ] **RMB** Request Relic toggles open-for-relic status
- [ ] Relic carrier blocks LMB / F / Q / E / R; **Left Shift** slide and **RMB** Request Relic still work
- [ ] Relic pickup / score regression still OK
- [ ] **3/3** cold-start PIE runs

Buildable placement is **not** required for 18a pass (no Siege Engine until 18b). Confirm Slide + Request Relic live on **`DA_BW_AbilitySet_Humanoid`**; PlaceBuildable is **not** on humanoid.

## Deferred to 18b / 18c

| Item | Step |
|------|------|
| Siege Engine data + actor + PlaceBuildable grant | 18b — [Argus_18b_Editor_Setup.md](./Argus_18b_Editor_Setup.md) |
| Confirm / Cancel + dual LMB/RMB input tags | 18b |
| Sheet-accurate ability kits + parity | 18c |
| In-match ability bar UI | 22 |
