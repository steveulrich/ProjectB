---
name: projectb-sdk
description: "Build a requested programmatic agent integration. Identify Codex versus legacy Cursor runtime before choosing SDK packages, APIs, or deployment behavior."
---

# Agent SDK

For current OpenAI Codex integrations, use the available OpenAI Docs skill or current official Codex documentation to identify the installed SDK and API. The Cursor package names and examples in the legacy reference are not Codex defaults.

Infer language and runtime from the repository; when the choice is reversible, state a reasonable default and continue preparation. Ask only when ambiguity materially changes scope, runtime, or cost.

Use [the legacy Cursor reference](references/legacy-cursor-sdk.md) only for an explicitly identified Cursor-compatible host after verifying its packages, paths, and tool schema. Do not load it for current Codex tasks.
