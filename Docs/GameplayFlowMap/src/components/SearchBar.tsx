import { useEffect, useMemo, useState } from 'react';
import type Fuse from 'fuse.js';
import type { SearchDocument } from '../utils/search';

interface SearchBarProps {
  fuse: Fuse<SearchDocument>;
  onSelect: (nodeId: string) => void;
}

export function SearchBar({ fuse, onSelect }: SearchBarProps) {
  const [query, setQuery] = useState('');
  const [focused, setFocused] = useState(false);

  const results = useMemo(() => {
    if (!query.trim()) {
      return [];
    }
    return fuse.search(query.trim()).slice(0, 12);
  }, [fuse, query]);

  useEffect(() => {
    function onKeyDown(event: KeyboardEvent) {
      if (event.key === '/' && document.activeElement?.tagName !== 'INPUT') {
        event.preventDefault();
        document.getElementById('flow-search')?.focus();
      }
    }
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, []);

  return (
    <div className="search-bar">
      <input
        id="flow-search"
        type="search"
        placeholder="Search classes, assets, params, calls… (/ to focus)"
        value={query}
        onChange={(event) => setQuery(event.target.value)}
        onFocus={() => setFocused(true)}
        onBlur={() => window.setTimeout(() => setFocused(false), 150)}
      />
      {focused && results.length > 0 && (
        <ul className="search-results">
          {results.map((result) => (
            <li key={result.item.id}>
              <button
                type="button"
                onMouseDown={() => {
                  onSelect(result.item.id);
                  setQuery('');
                  setFocused(false);
                }}
              >
                <span className="search-results__title">{result.item.title}</span>
                <span className={`lane-badge lane-badge--${result.item.lane}`}>
                  {result.item.lane}
                </span>
                {result.item.className && (
                  <span className="search-results__meta">{result.item.className}</span>
                )}
              </button>
            </li>
          ))}
        </ul>
      )}
    </div>
  );
}
