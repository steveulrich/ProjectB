# Hero Select Staging — Editor Setup

C++ routing is implemented. Complete these Editor steps after recompiling BreakawayCoreRuntime.

## 1. Create `B_BW_Experience_HeroSelectStaging`

**Path:** `/BreakawayCore/Experiences/B_BW_Experience_HeroSelectStaging`

1. Duplicate `B_BW_Experience_Dev` or create new Blueprint parented to `LyraExperienceDefinition`.
2. **GameFeaturesToEnable:** `BreakawayCore` only (no hero GF plugins required for lobby).
3. **Action Sets:** include `LAS_BW_SharedInput` only.
4. **Do NOT add:** `EAS_BW_CaptureTheRelic`, bot spawner, scoring, match HUD, ShooterCore HUD action sets.
5. **GameMode:** `B_BWayGameMode_Default` (or base `B_BWayGameMode`).
6. **GameState:** `BP_BW_GameState` (hero select components come from C++ defaults + `BP_BW_HeroSelectionPhase`).
7. **Do NOT grant** `BW_Phase_HeroSelection` or any other phase abilities — staging starts hero select from C++ on experience load (`HeroSelectStaging` URL option).

## 2. Configure `L_BW_HeroSelect_Staging`

**Path:** `/BreakawayCore/Maps/L_BW_HeroSelect_Staging`

1. World Settings → **GameMode Override:** `B_BWayGameMode_Default`
2. World Settings → **Default Gameplay Experience:** `B_BW_Experience_HeroSelectStaging`
3. Ensure map has **no** relic/goal spawn points (or leave them unused — C++ skips match object spawn on staging).
4. Optional: add camera / lighting / UI backdrop only.

## 3. Configure `BP_BW_HeroSelectionPhase`

On `BP_BW_GameState` → `BP_BW_HeroSelectionPhase` component:

| Property | Value |
|----------|-------|
| `HeroSelectionWidgetClass` | `WBP_BW_HeroSelect` |
| `DefaultHeroData` | e.g. `DA_BW_HeroData_Argus` |
| `PostHeroSelectionMapID` | optional fallback if URL omits target map |
| `PostHeroSelectionExperienceID` | optional fallback if URL omits target experience |
| `bBlockPlayerSpawningUntilComplete` | `true` |

## 4. Create playlist tiles (`UBwayUserFacingExperienceDefinition`)

**Path:** `/BreakawayCore/Playlists/` (one asset per frontend mode)

Create Primary Data Assets with parent class **`BwayUserFacingExperienceDefinition`** (C++), not stock Lyra.

Asset Manager registers these under the **`LyraUserFacingExperienceDefinition`** primary asset type (base-class scan in `DefaultGame.ini`), same pattern as hero data using `PrimaryDataAsset`. The C++ subclass is still used at runtime so `CreateHostingRequest` hero-select routing works.

Example: **Capture The Relic — Quick Play**

| Field | Value |
|-------|-------|
| `MapID` | `L_BW_DevMap` (match destination) |
| `ExperienceID` | `B_BW_Experience_CaptureTheRelic` |
| `bRouteThroughHeroSelectStaging` | `true` |
| `HeroSelectStagingMapID` | leave empty (uses `L_BW_HeroSelect_Staging` default) |
| `HeroSelectStagingExperienceID` | leave empty (uses `B_BW_Experience_HeroSelectStaging` default) |
| `OnlineMode` | per tile (Online / LAN / Offline) |
| `bShowInFrontEnd` | `true` |

Repeat for other modes (Dorado, ranked, casual, etc.) — same staging defaults, different `MapID` / `ExperienceID` / `OnlineMode`.

**PIE/dev tile:** set `bRouteThroughHeroSelectStaging = false` if you want direct map travel without staging.

## 5. Wire frontend experience list

Open the Lyra front-end experience / playlist widget and ensure Breakaway tiles reference your new `BwayUserFacingExperienceDefinition` assets under `/BreakawayCore/Playlists/`.

## 6. Verify

### Production path
1. Launch from frontend → pick a Breakaway mode tile.
2. Confirm travel lands on `L_BW_HeroSelect_Staging` with hero select UI.
3. Lock heroes → seamless travel to match map.
4. Match starts with heroes applied; bots have random picks.

### PIE dev path
1. PIE directly into `L_BW_DevMap?Experience=B_BW_Experience_CaptureTheRelic`.
2. No auto hero select UI.
3. Console: `ShowHeroSelect` → pick → lock → spawn.
4. Bots receive random heroes automatically.

### Logs to expect
```
BwayUserFacingExperienceDefinition: Routing ... through hero select staging
BwayHeroSelectionPhaseComponent: HeroSelectStaging URL option found
BwayHeroSelectionPhaseComponent: Hero selection complete; travelling to match URL
```
