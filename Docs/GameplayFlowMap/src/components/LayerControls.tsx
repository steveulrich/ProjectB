import type { EdgeType, LayerConfig, NodeLayer } from '../types';
import { EDGE_TYPE_COLORS } from './FlowMapNode';

interface LayerControlsProps {
  nodeLayers: LayerConfig[];
  visibleNodeLayers: Set<NodeLayer>;
  onToggleNodeLayer: (layer: NodeLayer) => void;
  visibleEdgeTypes: Set<EdgeType>;
  onToggleEdgeType: (type: EdgeType) => void;
}

const EDGE_TYPES: Array<{ id: EdgeType; label: string }> = [
  { id: 'sequence', label: 'Sequence' },
  { id: 'travels', label: 'Travel' },
  { id: 'calls', label: 'Calls' },
  { id: 'loads', label: 'Loads' },
];

export function LayerControls({
  nodeLayers,
  visibleNodeLayers,
  onToggleNodeLayer,
  visibleEdgeTypes,
  onToggleEdgeType,
}: LayerControlsProps) {
  return (
    <div className="layer-controls">
      <div className="layer-controls__group">
        <h3>Node layers</h3>
        {nodeLayers.map((layer) => (
          <label key={layer.id} className="layer-toggle">
            <input
              type="checkbox"
              checked={visibleNodeLayers.has(layer.id)}
              onChange={() => onToggleNodeLayer(layer.id)}
            />
            {layer.label}
          </label>
        ))}
      </div>

      <div className="layer-controls__group">
        <h3>Edge types</h3>
        {EDGE_TYPES.map((edgeType) => (
          <label key={edgeType.id} className="layer-toggle">
            <input
              type="checkbox"
              checked={visibleEdgeTypes.has(edgeType.id)}
              onChange={() => onToggleEdgeType(edgeType.id)}
            />
            <span
              className="edge-swatch"
              style={{ background: EDGE_TYPE_COLORS[edgeType.id] }}
            />
            {edgeType.label}
          </label>
        ))}
      </div>
    </div>
  );
}
