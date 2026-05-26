import Fuse from 'fuse.js';
import type { FlowEdge, FlowNode } from '../types';

export interface SearchDocument {
  id: string;
  title: string;
  summary: string;
  className: string;
  filePath: string;
  assets: string;
  urlParams: string;
  tags: string;
  edgeLabels: string;
  lane: string;
  node: FlowNode;
}

export function buildSearchIndex(
  nodes: FlowNode[],
  edges: FlowEdge[],
): Fuse<SearchDocument> {
  const edgeLabelsByNode = new Map<string, string[]>();

  for (const edge of edges) {
    for (const nodeId of [edge.from, edge.to]) {
      const existing = edgeLabelsByNode.get(nodeId) ?? [];
      existing.push(edge.label);
      edgeLabelsByNode.set(nodeId, existing);
    }
  }

  const documents: SearchDocument[] = nodes.map((node) => ({
    id: node.id,
    title: node.title,
    summary: node.summary,
    className: node.className ?? '',
    filePath: node.filePath ?? '',
    assets: (node.assets ?? []).join(' '),
    urlParams: (node.urlParams ?? []).join(' '),
    tags: (node.tags ?? []).join(' '),
    edgeLabels: (edgeLabelsByNode.get(node.id) ?? []).join(' '),
    lane: node.lane,
    node,
  }));

  return new Fuse(documents, {
    keys: [
      { name: 'title', weight: 0.25 },
      { name: 'summary', weight: 0.15 },
      { name: 'className', weight: 0.2 },
      { name: 'filePath', weight: 0.15 },
      { name: 'assets', weight: 0.1 },
      { name: 'urlParams', weight: 0.1 },
      { name: 'tags', weight: 0.05 },
      { name: 'edgeLabels', weight: 0.1 },
    ],
    threshold: 0.38,
    includeMatches: true,
    ignoreLocation: true,
  });
}
