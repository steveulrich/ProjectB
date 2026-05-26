import { Handle, Position, type NodeProps } from '@xyflow/react';
import type { FlowNodeData, NodeType } from '../types';

export const NODE_TYPE_COLORS: Record<NodeType, { bg: string; border: string; text: string }> = {
  map: { bg: '#1e3a5f', border: '#3b82f6', text: '#dbeafe' },
  experience: { bg: '#4c1d95', border: '#8b5cf6', text: '#ede9fe' },
  class: { bg: '#14532d', border: '#22c55e', text: '#dcfce7' },
  component: { bg: '#134e4a', border: '#14b8a6', text: '#ccfbf1' },
  widget: { bg: '#7c2d12', border: '#f97316', text: '#ffedd5' },
  asset: { bg: '#374151', border: '#9ca3af', text: '#f3f4f6' },
  subsystem: { bg: '#312e81', border: '#6366f1', text: '#e0e7ff' },
  urlparam: { bg: '#713f12', border: '#eab308', text: '#fef9c3' },
  plugin: { bg: '#831843', border: '#ec4899', text: '#fce7f3' },
  event: { bg: '#7f1d1d', border: '#ef4444', text: '#fee2e2' },
};

export const LANE_COLORS: Record<string, string> = {
  lyra: '#3b82f6',
  breakaway: '#14b8a6',
  heroes: '#ec4899',
  dev: '#eab308',
};

export const EDGE_TYPE_COLORS: Record<string, string> = {
  sequence: '#94a3b8',
  calls: '#60a5fa',
  loads: '#a78bfa',
  travels: '#fbbf24',
};

export function FlowMapNode({ data, selected }: NodeProps) {
  const nodeData = data as unknown as FlowNodeData;
  const colors = NODE_TYPE_COLORS[nodeData.type];
  const laneColor = LANE_COLORS[nodeData.lane];

  return (
    <>
      <Handle
        type="target"
        position={Position.Left}
        className="flow-handle flow-handle--target"
      />
      <div
        className={`flow-node ${selected ? 'flow-node--selected' : ''}`}
        style={{
          background: colors.bg,
          borderColor: selected ? '#ffffff' : colors.border,
          boxShadow: selected ? `0 0 0 2px ${laneColor}` : undefined,
        }}
      >
        <div className="flow-node__type" style={{ color: colors.border }}>
          {nodeData.type}
        </div>
        <div className="flow-node__title" style={{ color: colors.text }}>
          {nodeData.title}
        </div>
        {nodeData.className && (
          <div className="flow-node__class">{nodeData.className}</div>
        )}
      </div>
      <Handle
        type="source"
        position={Position.Right}
        className="flow-handle flow-handle--source"
      />
    </>
  );
}

export const nodeTypes = {
  flowNode: FlowMapNode,
};
