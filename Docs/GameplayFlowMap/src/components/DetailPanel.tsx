import { useState, type ReactNode } from 'react';
import type { FlowNode, FlowMapConfig } from '../types';

interface DetailPanelProps {
  node: FlowNode | null;
  config: FlowMapConfig;
  onClose: () => void;
}

function CopyButton({ text, label }: { text: string; label: string }) {
  const [copied, setCopied] = useState(false);

  async function handleCopy() {
    await navigator.clipboard.writeText(text);
    setCopied(true);
    window.setTimeout(() => setCopied(false), 1500);
  }

  return (
    <button type="button" className="btn btn--small" onClick={handleCopy}>
      {copied ? 'Copied!' : label}
    </button>
  );
}

function CollapsibleSection({
  title,
  defaultOpen = false,
  children,
}: {
  title: string;
  defaultOpen?: boolean;
  children: ReactNode;
}) {
  const [open, setOpen] = useState(defaultOpen);

  return (
    <section className="detail-section">
      <button
        type="button"
        className="detail-section__toggle"
        onClick={() => setOpen((value) => !value)}
        aria-expanded={open}
      >
        <span>{open ? '▾' : '▸'}</span>
        {title}
      </button>
      {open && <div className="detail-section__body">{children}</div>}
    </section>
  );
}

export function DetailPanel({ node, config, onClose }: DetailPanelProps) {
  if (!node) {
    return (
      <aside className="detail-panel detail-panel--empty">
        <p>Select a node to see details.</p>
        <p className="muted">Use search to jump to a class, asset, or URL param.</p>
      </aside>
    );
  }

  const githubUrl =
    config.repoBaseUrl && node.filePath
      ? `${config.repoBaseUrl}/${node.filePath}`
      : null;

  return (
    <aside className="detail-panel">
      <header className="detail-panel__header">
        <div>
          <span className={`lane-badge lane-badge--${node.lane}`}>{node.lane}</span>
          <span className="type-badge">{node.type}</span>
        </div>
        <button type="button" className="btn btn--icon" onClick={onClose} aria-label="Close">
          ×
        </button>
      </header>

      <h2>{node.title}</h2>
      <p className="detail-panel__summary">{node.summary}</p>

      {node.className && (
        <p className="detail-panel__meta">
          <strong>Class:</strong> <code>{node.className}</code>
        </p>
      )}

      {node.filePath && (
        <div className="detail-panel__actions">
          <CopyButton text={node.filePath} label="Copy path" />
          {githubUrl && (
            <a className="btn btn--small btn--link" href={githubUrl} target="_blank" rel="noreferrer">
              View on GitHub
            </a>
          )}
        </div>
      )}

      {node.callChain && node.callChain.length > 0 && (
        <CollapsibleSection title="Call chain">
          <ol className="detail-list">
            {node.callChain.map((item) => (
              <li key={item}>
                <code>{item}</code>
              </li>
            ))}
          </ol>
        </CollapsibleSection>
      )}

      {node.assets && node.assets.length > 0 && (
        <CollapsibleSection title="UE assets">
          <ul className="detail-list">
            {node.assets.map((asset) => (
              <li key={asset}>
                <code>{asset}</code>
                <span className="muted block">Open in Content Browser</span>
              </li>
            ))}
          </ul>
        </CollapsibleSection>
      )}

      {(node.urlParams || node.tags) && (
        <CollapsibleSection title="URL params / tags">
          {node.urlParams && node.urlParams.length > 0 && (
            <>
              <p className="detail-subheading">URL params</p>
              <ul className="detail-list">
                {node.urlParams.map((param) => (
                  <li key={param}>
                    <code>{param}</code>
                  </li>
                ))}
              </ul>
            </>
          )}
          {node.tags && node.tags.length > 0 && (
            <>
              <p className="detail-subheading">Tags</p>
              <div className="tag-row">
                {node.tags.map((tag) => (
                  <span key={tag} className="tag">
                    {tag}
                  </span>
                ))}
              </div>
            </>
          )}
        </CollapsibleSection>
      )}

      {node.docLinks && node.docLinks.length > 0 && (
        <CollapsibleSection title="Docs">
          <ul className="detail-list">
            {node.docLinks.map((doc) => (
              <li key={doc.path}>
                <span>{doc.title}</span>
                <code className="block">{doc.path}</code>
              </li>
            ))}
          </ul>
        </CollapsibleSection>
      )}
    </aside>
  );
}
