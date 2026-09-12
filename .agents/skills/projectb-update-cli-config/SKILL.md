---
name: projectb-update-cli-config
description: "Change requested CLI settings after identifying the actual host and configuration. Use current Codex schemas for Codex and legacy Cursor schemas only for Cursor."
metadata:
  surfaces:
    - cli
---

# CLI Configuration

Resolve the current host and its active configuration before editing. For Codex, use current host documentation and its resolved config.toml paths. Preserve unrelated configuration and existing sandbox, credential, and approval boundaries; change only the settings requested.

Use [the legacy Cursor reference](references/legacy-cursor-cli-config.md) only for an explicitly identified Cursor-compatible host after verifying its packages, paths, and tool schema. Do not load it for current Codex tasks.
