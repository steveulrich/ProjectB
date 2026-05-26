export type Lane = 'lyra' | 'breakaway' | 'heroes' | 'dev';
export type NodeType =
  | 'map'
  | 'experience'
  | 'class'
  | 'component'
  | 'widget'
  | 'asset'
  | 'subsystem'
  | 'urlparam'
  | 'plugin'
  | 'event';
export type EdgeType = 'sequence' | 'calls' | 'loads' | 'travels';
export type NodeLayer = 'core' | 'match' | 'hero' | 'dev' | 'gas';

export interface DocLink {
  title: string;
  path: string;
}

export interface FlowNode {
  id: string;
  type: NodeType;
  lane: Lane;
  layers: NodeLayer[];
  journeyOrder: number;
  title: string;
  summary: string;
  className?: string;
  filePath?: string;
  assets?: string[];
  urlParams?: string[];
  tags?: string[];
  callChain?: string[];
  docLinks?: DocLink[];
}

export interface FlowEdge {
  from: string;
  to: string;
  type: EdgeType;
  label: string;
}

export interface LayerConfig {
  id: NodeLayer;
  label: string;
  defaultVisible: boolean;
}

export interface FlowGraph {
  version: number;
  title: string;
  description: string;
  layers: LayerConfig[];
  nodes: FlowNode[];
  edges: FlowEdge[];
}

export interface FlowMapConfig {
  repoBaseUrl: string;
  branch: string;
  projectName: string;
}

export interface FlowNodeData extends FlowNode {
  label: string;
}
