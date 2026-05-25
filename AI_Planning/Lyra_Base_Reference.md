# Lyra Base Reference (ProjectB)

What Lyra provides vs what BreakawayCore adds — onboarding for new contributors.

## Lyra Modules (ProjectB)

| Path | Module |
|------|--------|
| `Source/LyraGame/` | Main game runtime |
| `Source/LyraEditor/` | Editor utilities |

## Lyra Responsibilities

### Boot & Shell

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

### Teams & Social

- `ULyraTeamSubsystem`, `ILyraTeamAgentInterface`
- Session plugins (Steam/EOS) — unchanged by Breakaway

### Assets

- **`ULyraAssetManager`** — primary asset scanning (Breakaway paths added in config)

## Lyra Sample Game Features (Reference Only)

| Plugin | Use in ProjectB |
|--------|-----------------|
| ShooterCore | Phase patterns, shooter UX samples |
| ShooterCTF | Spawn tag patterns (legacy coupling) |
| ShooterMaps / ShooterExplorer | Lyra sample content |
| TopDownArena | Unrelated sample mode |

## BreakawayCore Responsibilities

Everything in [SYSTEMS_INDEX.md](../Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md):

- Capture-the-Relic rules, rounds, scoring
- Hero selection, 4-hero roster content
- Relic, buildables, gold economy
- Custom movement (slide), Spartacus C++ abilities

## Integration Rule

**Never replace Lyra front-end or experience manager.** Breakaway injects via:

1. Game Feature plugin load
2. Experience asset selecting Breakaway game mode / pawn data
3. GameState components for match logic

Deep dive: [Lyra_Integration.md](../Plugins/GameFeatures/BreakawayCore/Docs/Lyra_Integration.md)
