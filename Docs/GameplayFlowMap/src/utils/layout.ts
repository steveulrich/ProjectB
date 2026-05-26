import dagre from '@dagrejs/dagre';
import type { FlowEdge, FlowGraph, FlowNode, Lane, NodeLayer, EdgeType } from '../types';

export const NODE_WIDTH = 200;
export const NODE_HEIGHT = 76;

const LANE_Y: Record<Lane, number> = {
  lyra: 80,
  breakaway: 520,
  heroes: 960,
  dev: 1320,
};

const LANE_BAND_HEIGHT = 380;
const COLUMN_X_THRESHOLD = 140;
const ROW_GAP = 96;

/** Edge types used to compute left→right reference-tree positions. */
const LAYOUT_EDGE_TYPES = new Set<EdgeType>(['sequence', 'travels', 'calls']);

export function computeGraphLayout(
  nodes: FlowNode[],
  edges: FlowEdge[],
): Map<string, { x: number; y: number }> {
  const positions = new Map<string, { x: number; y: number }>();
  if (nodes.length === 0) {
    return positions;
  }

  const nodeIds = new Set(nodes.map((node) => node.id));
  const graph = new dagre.graphlib.Graph();
  graph.setDefaultEdgeLabel(() => ({}));
  graph.setGraph({
    rankdir: 'LR',
    nodesep: 90,
    ranksep: 200,
    edgesep: 80,
    marginx: 60,
    marginy: 60,
  });

  for (const node of nodes) {
    graph.setNode(node.id, { width: NODE_WIDTH, height: NODE_HEIGHT });
  }

  const layoutEdges = edges.filter(
    (edge) =>
      nodeIds.has(edge.from) &&
      nodeIds.has(edge.to) &&
      LAYOUT_EDGE_TYPES.has(edge.type),
  );

  for (const edge of layoutEdges) {
    if (!graph.hasEdge(edge.from, edge.to)) {
      graph.setEdge(edge.from, edge.to);
    }
  }

  dagre.layout(graph);

  const dagrePositions = new Map<string, { x: number; y: number }>();
  for (const node of nodes) {
    const dagreNode = graph.node(node.id);
    if (dagreNode) {
      dagrePositions.set(node.id, {
        x: dagreNode.x - NODE_WIDTH / 2,
        y: dagreNode.y - NODE_HEIGHT / 2,
      });
    }
  }

  // Re-stack within swim lanes so lanes stay readable (reference-viewer rows).
  for (const lane of Object.keys(LANE_Y) as Lane[]) {
    const laneNodes = nodes.filter((node) => node.lane === lane);
    if (laneNodes.length === 0) {
      continue;
    }

    laneNodes.sort((a, b) => {
      const posA = dagrePositions.get(a.id);
      const posB = dagrePositions.get(b.id);
      const xA = posA?.x ?? a.journeyOrder * 220;
      const xB = posB?.x ?? b.journeyOrder * 220;
      if (Math.abs(xA - xB) > COLUMN_X_THRESHOLD) {
        return xA - xB;
      }
      const yA = posA?.y ?? 0;
      const yB = posB?.y ?? 0;
      return yA - yB;
    });

    let columnX = -Infinity;
    let rowInColumn = 0;

    for (const node of laneNodes) {
      const dagrePos = dagrePositions.get(node.id);
      const x = dagrePos?.x ?? node.journeyOrder * 220 + 60;

      if (x - columnX > COLUMN_X_THRESHOLD) {
        columnX = x;
        rowInColumn = 0;
      } else {
        rowInColumn += 1;
      }

      positions.set(node.id, {
        x: columnX,
        y: LANE_Y[lane] + rowInColumn * ROW_GAP,
      });
    }
  }

  // Fallback for any node missed above
  for (const node of nodes) {
    if (!positions.has(node.id)) {
      positions.set(node.id, {
        x: node.journeyOrder * 220 + 60,
        y: LANE_Y[node.lane],
      });
    }
  }

  return positions;
}

export function computeNodePosition(
  node: FlowNode,
  layout: Map<string, { x: number; y: number }>,
): { x: number; y: number } {
  return layout.get(node.id) ?? { x: node.journeyOrder * 220, y: LANE_Y[node.lane] };
}

export function getLaneBands(): Array<{ lane: Lane; y: number; height: number; label: string }> {
  return [
    { lane: 'lyra', y: LANE_Y.lyra - 40, height: LANE_BAND_HEIGHT, label: 'Lyra Shell' },
    { lane: 'breakaway', y: LANE_Y.breakaway - 40, height: LANE_BAND_HEIGHT, label: 'BreakawayCore' },
    { lane: 'heroes', y: LANE_Y.heroes - 40, height: LANE_BAND_HEIGHT, label: 'Hero Plugins' },
    { lane: 'dev', y: LANE_Y.dev - 40, height: LANE_BAND_HEIGHT, label: 'Dev / PIE' },
  ];
}

export function filterNodesByLayers(
  nodes: FlowNode[],
  visibleLayers: Set<NodeLayer>,
): FlowNode[] {
  return nodes.filter((node) => node.layers.some((layer) => visibleLayers.has(layer)));
}

export function getDefaultVisibleLayers(graph: FlowGraph): Set<NodeLayer> {
  return new Set(
    graph.layers.filter((layer) => layer.defaultVisible).map((layer) => layer.id),
  );
}

export function getGraphBounds(
  nodes: FlowNode[],
  layout: Map<string, { x: number; y: number }>,
): { width: number; height: number } {
  if (nodes.length === 0) {
    return { width: 2400, height: 1700 };
  }

  const positions = nodes.map((node) => computeNodePosition(node, layout));
  const maxX = Math.max(...positions.map((p) => p.x)) + NODE_WIDTH + 120;
  const maxY = Math.max(...positions.map((p) => p.y)) + NODE_HEIGHT + 120;
  return { width: Math.max(maxX, 2400), height: Math.max(maxY, 1700) };
}
