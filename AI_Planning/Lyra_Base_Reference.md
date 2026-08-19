# Lyra base reference (ProjectB)

What Lyra provides versus what BreakawayCore adds. Use this as onboarding for new contributors.

**Last reviewed:** August 19, 2026  
**Planning hub:** [Planning docs](./README.md)

## Lyra modules (ProjectB)

| Path | Module |
|------|--------|
| `Source/LyraGame/` | Main game runtime |
| `Source/LyraEditor/` | Editor utilities |

## Lyra responsibilities

### Boot and shell

- `ULyraGameInstance`, front-end map `L_LyraFrontEnd`
- `B_LyraFrontEnd_Experience` — menu, playlists, sessions
- `B_LyraUIPolicy` — CommonUI layer routing

### Experiences

- `ULyraExperienceDefinition` — game mode, pawn data, plugins, actions
- `ULyraExperienceManagerComponent` — async load on GameState
- `ULyraUserFacingExperienceDefinition` — playlist entries

### GAS

- `ULyraAbilitySystemComponent`, `ULyraAbilitySet`
- Health/combat attribute sets, `ULyraGameplayAbility`
- `ULyraGamePhaseSubsystem` — phase tags gate gameplay

### Teams and social

- `ULyraTeamSubsystem`, `ILyraTeamAgentInterface`
- Session plugins (Steam/EOS) — unchanged by Breakaway

### Assets

- **`ULyraAssetManager`** — primary asset scanning (Breakaway paths added in config)

## Lyra sample game features (reference only)

| Plugin | Use in ProjectB |
|--------|-----------------|
| ShooterCore | Phase patterns, shooter UX samples |
| ShooterCTF | Spawn tag patterns (legacy coupling) |
| ShooterMaps / ShooterExplorer | Lyra sample content |
| TopDownArena | Unrelated sample mode |

## BreakawayCore responsibilities

Everything in [BreakawayCore systems index](../Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md):

- Capture-the-Relic rules, rounds, scoring
- Hero selection (staging + direct PIE)
- Four-hero roster content (Argus, Alona, Korryn, Rawlins) in per-hero Game Feature plugins
- Relic, buildables, gold economy
- Custom movement (slide)

## Integration rule

**Never replace Lyra front-end or experience manager.** Breakaway injects via:

1. Game Feature plugin load
2. Experience asset selecting Breakaway game mode / pawn data
3. GameState components for match logic

Deep dive: [Lyra_Integration.md](../Plugins/GameFeatures/BreakawayCore/Docs/Lyra_Integration.md)
