/// <reference types="vite/client" />

declare module '*.yaml' {
  import type { FlowGraph } from './types';
  const content: FlowGraph;
  export default content;
}

declare module '../flow-map.config.json' {
  import type { FlowMapConfig } from './types';
  const config: FlowMapConfig;
  export default config;
}

declare module '../../flow-map.config.json' {
  import type { FlowMapConfig } from './types';
  const config: FlowMapConfig;
  export default config;
}
