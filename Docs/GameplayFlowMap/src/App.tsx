import { useCallback, useEffect, useMemo, useState } from 'react';
import {
  Background,
  Controls,
  MiniMap,
  ReactFlow,
  MarkerType,
  Position,
  type Edge,
  type Node,
  useReactFlow,
  ReactFlowProvider,
} from '@xyflow/react';
import '@xyflow/react/dist/style.css';

import flowGraph from './data/flow-graph.yaml';
import flowMapConfig from '../flow-map.config.json';
import { DetailPanel } from './components/DetailPanel';
import { LayerControls } from './components/LayerControls';
import { nodeTypes, EDGE_TYPE_COLORS, LANE_COLORS } from './components/FlowMapNode';
import { SearchBar } from './components/SearchBar';
import type { EdgeType, FlowEdge, FlowNode, NodeLayer } from './types';
import {
  computeGraphLayout,
  computeNodePosition,
  filterNodesByLayers,
  getDefaultVisibleLayers,
  getGraphBounds,
  getLaneBands,
  NODE_HEIGHT,
  NODE_WIDTH,
} from './utils/layout';
import { buildSearchIndex } from './utils/search';

/** Show reference chains by default — like UE Reference Viewer. */
const DEFAULT_EDGE_TYPES = new Set<EdgeType>(['sequence', 'travels', 'calls']);

function FlowMapCanvas() {
  const { setCenter, fitView } = useReactFlow();
  const [visibleNodeLayers, setVisibleNodeLayers] = useState<Set<NodeLayer>>(() =>
    getDefaultVisibleLayers(flowGraph),
  );
  const [visibleEdgeTypes, setVisibleEdgeTypes] = useState<Set<EdgeType>>(
    () => new Set(DEFAULT_EDGE_TYPES),
  );
  const [selectedNode, setSelectedNode] = useState<FlowNode | null>(null);
  const [selectedEdgeId, setSelectedEdgeId] = useState<string | null>(null);
  const [showEdgeLabels, setShowEdgeLabels] = useState(false);

  const visibleNodes = useMemo(
    () => filterNodesByLayers(flowGraph.nodes, visibleNodeLayers),
    [visibleNodeLayers],
  );

  const visibleNodeIds = useMemo(
    () => new Set(visibleNodes.map((node) => node.id)),
    [visibleNodes],
  );

  const layout = useMemo(
    () => computeGraphLayout(visibleNodes, flowGraph.edges),
    [visibleNodes],
  );

  const reactFlowNodes: Node[] = useMemo(
    () =>
      visibleNodes.map((node) => ({
        id: node.id,
        type: 'flowNode',
        position: computeNodePosition(node, layout),
        sourcePosition: Position.Right,
        targetPosition: Position.Left,
        data: { ...node, label: node.title },
      })),
    [visibleNodes, layout],
  );

  const reactFlowEdges: Edge[] = useMemo(
    () =>
      flowGraph.edges
        .filter(
          (edge: FlowEdge) =>
            visibleNodeIds.has(edge.from) &&
            visibleNodeIds.has(edge.to) &&
            visibleEdgeTypes.has(edge.type),
        )
        .map((edge: FlowEdge) => {
          const color = EDGE_TYPE_COLORS[edge.type];
          const edgeId = `${edge.from}→${edge.to}→${edge.type}→${edge.label}`;
          const isSelected = selectedEdgeId === edgeId;

          return {
            id: edgeId,
            source: edge.from,
            target: edge.to,
            label: showEdgeLabels || isSelected ? edge.label : undefined,
            type: 'smoothstep',
            animated: edge.type === 'travels',
            selectable: true,
            interactionWidth: 24,
            markerEnd: {
              type: MarkerType.ArrowClosed,
              width: 18,
              height: 18,
              color,
            },
            style: {
              stroke: color,
              strokeWidth: isSelected ? 3 : edge.type === 'sequence' ? 2.5 : 2,
              opacity: edge.type === 'loads' ? 0.75 : 1,
            },
            labelStyle: { fill: '#e2e8f0', fontSize: 11, fontWeight: 600 },
            labelBgStyle: { fill: '#0f172a', fillOpacity: 0.92 },
            labelBgPadding: [5, 8] as [number, number],
            labelBgBorderRadius: 4,
            data: { edgeType: edge.type, label: edge.label },
          };
        }),
    [visibleEdgeTypes, visibleNodeIds, selectedEdgeId, showEdgeLabels],
  );

  const searchFuse = useMemo(
    () => buildSearchIndex(flowGraph.nodes, flowGraph.edges),
    [],
  );

  const laneBands = useMemo(() => getLaneBands(), []);
  const graphBounds = useMemo(() => getGraphBounds(visibleNodes, layout), [visibleNodes, layout]);

  useEffect(() => {
    const timer = window.setTimeout(() => {
      fitView({ padding: 0.08, duration: 300 });
    }, 50);
    return () => window.clearTimeout(timer);
  }, [fitView, visibleNodes.length, visibleEdgeTypes]);

  const focusNode = useCallback(
    (nodeId: string) => {
      const node = flowGraph.nodes.find((item: FlowNode) => item.id === nodeId);
      if (!node) {
        return;
      }

      if (!visibleNodeIds.has(nodeId)) {
        setVisibleNodeLayers((current) => {
          const next = new Set(current);
          for (const layer of node.layers) {
            next.add(layer);
          }
          return next;
        });
      }

      const position = computeNodePosition(node, layout);
      setSelectedNode(node);
      setSelectedEdgeId(null);
      window.setTimeout(() => {
        setCenter(position.x + NODE_WIDTH / 2, position.y + NODE_HEIGHT / 2, {
          zoom: 0.85,
          duration: 500,
        });
      }, node.layers.some((layer: NodeLayer) => !visibleNodeLayers.has(layer)) ? 150 : 0);
    },
    [layout, setCenter, visibleNodeIds, visibleNodeLayers],
  );

  function toggleNodeLayer(layer: NodeLayer) {
    setVisibleNodeLayers((current) => {
      const next = new Set(current);
      if (next.has(layer)) {
        next.delete(layer);
      } else {
        next.add(layer);
      }
      return next;
    });
  }

  function toggleEdgeType(type: EdgeType) {
    setVisibleEdgeTypes((current) => {
      const next = new Set(current);
      if (next.has(type)) {
        next.delete(type);
      } else {
        next.add(type);
      }
      return next;
    });
  }

  return (
    <div className="app-shell">
      <header className="app-header">
        <div>
          <h1>{flowMapConfig.projectName}</h1>
          <p>{flowGraph.description}</p>
        </div>
        <SearchBar fuse={searchFuse} onSelect={focusNode} />
      </header>

      <div className="app-body">
        <aside className="sidebar">
          <LayerControls
            nodeLayers={flowGraph.layers}
            visibleNodeLayers={visibleNodeLayers}
            onToggleNodeLayer={toggleNodeLayer}
            visibleEdgeTypes={visibleEdgeTypes}
            onToggleEdgeType={toggleEdgeType}
          />

          <div className="layer-controls__group">
            <h3>Display</h3>
            <label className="layer-toggle">
              <input
                type="checkbox"
                checked={showEdgeLabels}
                onChange={() => setShowEdgeLabels((value) => !value)}
              />
              Show all edge labels
            </label>
            <p className="muted hint">
              Click an arrow to highlight its label. Toggle Loads for asset/plugin fan-out links.
            </p>
          </div>

          <div className="legend">
            <h3>Swim lanes</h3>
            {laneBands.map((band) => (
              <div key={band.lane} className="legend-row">
                <span className={`lane-badge lane-badge--${band.lane}`}>{band.label}</span>
              </div>
            ))}
            <p className="muted">
              {visibleNodes.length} nodes · {reactFlowEdges.length} edges visible
            </p>
          </div>
        </aside>

        <main className="graph-panel">
          <div
            className="lane-background"
            style={{ width: graphBounds.width, height: graphBounds.height }}
          >
            {laneBands.map((band) => (
              <div
                key={band.lane}
                className={`lane-band lane-band--${band.lane}`}
                style={{ top: band.y, height: band.height }}
              >
                <span>{band.label}</span>
              </div>
            ))}
          </div>

          <ReactFlow
            nodes={reactFlowNodes}
            edges={reactFlowEdges}
            nodeTypes={nodeTypes}
            minZoom={0.05}
            maxZoom={1.5}
            nodesConnectable={false}
            elementsSelectable
            onNodeClick={(_, node) => {
              const data = node.data as unknown as FlowNode;
              setSelectedNode(data);
              setSelectedEdgeId(null);
            }}
            onEdgeClick={(_, edge) => {
              setSelectedEdgeId(edge.id);
              setSelectedNode(null);
            }}
            onPaneClick={() => {
              setSelectedNode(null);
              setSelectedEdgeId(null);
            }}
          >
            <Background color="#1e293b" gap={24} />
            <Controls showInteractive={false} />
            <MiniMap
              nodeColor={(node) => {
                const data = node.data as unknown as FlowNode;
                return LANE_COLORS[data.lane] ?? '#64748b';
              }}
              maskColor="rgba(15, 23, 42, 0.75)"
              pannable
              zoomable
            />
          </ReactFlow>
        </main>

        <DetailPanel
          node={selectedNode}
          config={flowMapConfig}
          onClose={() => setSelectedNode(null)}
        />
      </div>
    </div>
  );
}

export default function App() {
  return (
    <ReactFlowProvider>
      <FlowMapCanvas />
    </ReactFlowProvider>
  );
}
