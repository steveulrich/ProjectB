# ProjectB — Gameplay Flow Map

Interactive graph of the full ProjectB gameplay flow: engine launch → Lyra frontend → hero-select staging → Capture-the-Relic match → post-game return, plus hero plugins and dev/PIE paths.

## Quick start

```bash
cd Docs/GameplayFlowMap
npm install
npm run dev
```

Open the URL Vite prints (usually `http://localhost:5173`).

## Features

- **Single canvas** with ~80 nodes covering production, match internals, hero plugins, and dev paths
- **Swim lanes:** Lyra Shell · BreakawayCore · Hero Plugins · Dev / PIE
- **Layer toggles:** Boot & Frontend, Match Systems, Hero Plugins, GAS & Phases, Dev / PIE
- **Edge filters:** Sequence, Travel (default on); Calls, Loads (toggle on for debugging)
- **Search:** classes, file paths, UE assets, URL params, edge labels — press `/` to focus
- **Detail panel:** summary, file path (copy + optional GitHub), call chain, assets, docs links

## Configuration

Edit `flow-map.config.json` to enable GitHub links in the detail panel:

```json
{
  "repoBaseUrl": "https://github.com/YourOrg/ProjectB/blob/main"
}
```

Leave `repoBaseUrl` empty to use copy-path only.

## Updating the map

Graph data lives in `src/data/flow-graph.yaml`. When you change a major gameplay flow in C++, update the relevant nodes/edges in the same PR.

### YAML schema (summary)

```yaml
nodes:
  - id: unique-id
    type: class | component | map | experience | widget | ...
    lane: lyra | breakaway | heroes | dev
    layers: [core, match, hero, gas, dev]
    journeyOrder: 0   # left → right on canvas
    title: Display name
    summary: One-line description
    className: Optional
    filePath: Repo-relative path
    assets: [/Game/...]
    urlParams: [SkipHeroSelection=1]
    callChain: [FunctionA, FunctionB]
    docLinks: [{ title, path }]

edges:
  - from: node-id
    to: node-id
    type: sequence | calls | loads | travels
    label: Edge label
```

## Build

```bash
npm run build
npm run preview
```

## Source docs

Seeded from:

- `Plugins/GameFeatures/BreakawayCore/Docs/SYSTEMS_INDEX.md`
- `Plugins/GameFeatures/BreakawayCore/Docs/MatchFlow_and_Phases.md`
- `Plugins/GameFeatures/BreakawayCore/Docs/Lyra_Integration.md`
- `Plugins/GameFeatures/BreakawayCore/Docs/HeroSelect_Staging_Setup.md`
- `Plugins/GameFeatures/Heroes/README.md`
