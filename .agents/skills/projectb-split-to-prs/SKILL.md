---
name: projectb-split-to-prs
description: >-
  Split current work into small reviewable PRs. Use when the user asks to split
  a chat, set of changes, branch, or PR.
---
# Split to PRs

Turn one pile of work into a few small PRs.

## Hard rules

- Prepare a recoverable snapshot, isolated local branches/commits, reviewable diffs, and relevant validation within the requested split. Obtain any still-missing approval before pushing or opening PRs; existing explicit publication approval remains valid within its scope.
- Never discard user work. No destructive git commands (`reset --hard`, `clean -fdx`, branch deletion, force-push, history rewrite) without explicit approval.
- Always save a recoverable snapshot before moving work around. This often starts from dirty work on `main`, so do not assume there is already a safe branch.
- Stage only named files or hunks. No `git add .` / `git add -A`.

## 1. Check the state

Compare the current work to the repo's default branch, including committed and uncommitted changes. Summarize the real slices you see, and use the chat history to recover intent.

Before proposing slices, find ownership signals for the touched paths (`CODEOWNERS`, nested ownership files, `tools/ownership/PRODUCTOWNERS`, or repo equivalents) and use them to identify natural reviewer boundaries.

## 2. Propose the split

Use judgment on detail. Usually PR titles are enough. Add a one-line scope note only when a title is unclear. Show a Mermaid diagram when there are multiple slices.

Optimize for reviewer-aligned PRs with minimal unrelated diff: split independent owners or concerns, keep tightly coupled changes together, and when stacking is necessary, order foundations before consumers.

Default to independent PRs off the default branch. Stack PRs only when the dependency is real.

Prepare the local split and validation before requesting any missing publication approval. Present concrete branch diffs and proposed PR titles so that approval is the final step before publication. If the user requested planning only, return the plan without modifying Git state.

## 3. Execute the split

- If there is uncommitted work, save a recoverable snapshot without changing the working tree:

  ```bash
  SHA=$(git stash create "pre-split")
  if [ -n "$SHA" ]; then
    git update-ref "refs/backup/pre-split-$(date +%s)" "$SHA"
  fi
  ```

- For each requested slice, use an isolated branch from the right base, stage and commit only planned files or hunks, and validate the resulting diff. Preserve the starting checkout and unrelated work. Push and open the PR once publication is authorized; do not ask again for approval already provided.

## 4. Report back

Keep it short: PR titles and URLs, plus anything left on the starting branch or working tree. Do not delete the backup ref or original branch unless the user asks.
