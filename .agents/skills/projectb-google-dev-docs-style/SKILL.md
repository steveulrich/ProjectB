---
name: projectb-google-dev-docs-style
description: >-
  Write and edit technical documentation following the Google developer
  documentation style guide. Use when drafting or revising READMEs, API docs,
  tutorials, how-to guides, reference pages, release notes, or any developer
  documentation where the user asks for Google style, clear technical writing,
  or editorial consistency.
---

# Google developer documentation style

Apply the [Google developer documentation style guide](https://developers.google.com/style) when writing or editing technical docs for software developers and other technical practitioners.

## Reference hierarchy

Apply rules in this order:

1. **Project-specific style** — team or product overrides win.
2. **This skill** — Google developer documentation conventions.
3. **Third-party references** — only when this skill is silent:
   - Spelling: [Merriam-Webster](https://www.merriam-webster.com/)
   - Nontechnical style: *The Chicago Manual of Style*, 17th ed.
   - Technical style: [Microsoft Writing Style Guide](https://learn.microsoft.com/en-us/style-guide/welcome/)

Guidelines, not laws. Break a rule when doing so improves clarity for your audience — but stay consistent within the document.

## Workflow

1. Identify document type: concept, task/how-to, tutorial, reference, or troubleshooting.
2. Consult [`references/voice-and-tone.md`](references/voice-and-tone.md) when voice, tone, or accessibility guidance is needed for the edit.
3. Preserve coherent document structure; consult [`references/formatting.md`](references/formatting.md) when changing headings or formatting.
4. When writing or changing procedural steps, consult [`references/lists-and-procedures.md`](references/lists-and-procedures.md).
5. When writing or changing links and cross-references, consult [`references/links-and-cross-references.md`](references/links-and-cross-references.md).
6. Consult [`references/word-list-common.md`](references/word-list-common.md) for unresolved terminology choices.
7. Apply the relevant checklist items to the edited content; use the full checklist for a complete new document or publication review.

## Reporting

Return the requested content or a concise change summary. For a full document or editorial review, include applicable details below; omit routine style commentary for targeted edits:

1. **Revised content** (or new draft) in the requested format.
2. **Document type** you assumed (concept, task, reference, etc.).
3. **Notable style choices** — only where a rule was applied non-obviously or broken deliberately for clarity.
4. **Checklist summary** — confirm the pre-publish checklist passed, or list remaining issues.

Do not pad with meta-commentary about the style guide unless the user asks for an editorial review.

## Pre-publish checklist

### Voice and content

- [ ] Conversational and friendly, not frivolous or overly formal
- [ ] Second person (`you`), active voice — actor is clear
- [ ] No pre-announcements ("In this section, we will...")
- [ ] No `please`, `simply`, `easy`, `quickly`, `let's`, or exclamation marks
- [ ] No directional language (`above`, `below`, `click here`) — use `preceding`, `following`, descriptive link text
- [ ] Global audience: no culture-specific references, minimal jargon, acronyms defined on first use
- [ ] Sentences under ~26 words where practical

### Structure and formatting

- [ ] Sentence case for all headings and titles
- [ ] Task headings start with bare infinitive (`Create an instance`); concept headings use noun phrases
- [ ] No skipped heading levels; one `h1` per page
- [ ] **Bold** for UI elements; `` `code font` `` for code, filenames, commands, placeholders
- [ ] Serial (Oxford) commas in lists
- [ ] Numbered lists for sequences; bulleted lists for non-sequential items
- [ ] Conditions before instructions
- [ ] Unambiguous dates (`January 19, 2017` or `2017-01-19`; not `01/19/17`)

### Procedures

- [ ] Location/context before action (`In the console, click...`)
- [ ] Goal before action when helpful (`To create a VM, click...`)
- [ ] Single-step tasks as one bulleted item, not a numbered list of one
- [ ] Optional steps prefixed with `Optional:` (not `(Optional)`)
- [ ] Sub-steps use lowercase letters; sub-sub-steps use lowercase Roman numerals

### Links and media

- [ ] Descriptive link text — never `click here`, `this document`, or raw URLs as link text
- [ ] Cross-reference intro: `For more information, see...` or `For more information about..., see...`
- [ ] Alt text on images; no new information conveyed by images alone
- [ ] Unexpected link behavior explained (downloads, new tab, same-page jump)

## Quick reference

| Element | Rule |
| --- | --- |
| UI labels | **Bold**; add `` `code font` `` if the label is user-entered text |
| Code in prose | `` `backticks` `` for filenames, classes, methods, flags, placeholders |
| Headings | Sentence case; avoid `-ing` as first word |
| Lists | Introduce with a complete sentence; parallel structure across items |
| Links | Match page title or use a descriptive phrase; punctuation outside link |
| Dates | `Month DD, YYYY` in prose; `YYYY-MM-DD` when numeric only |
| Times | 12-hour clock: `3 PM`, `3:45 PM` |
| Ampersands | Use `and` — never `&` in headings or body text (except UI labels that contain `&`) |

## Additional resources

- Voice, tone, and what to avoid: [`references/voice-and-tone.md`](references/voice-and-tone.md)
- Headings, text formatting, code font, dates: [`references/formatting.md`](references/formatting.md)
- Lists and procedures: [`references/lists-and-procedures.md`](references/lists-and-procedures.md)
- Links and cross-references: [`references/links-and-cross-references.md`](references/links-and-cross-references.md)
- Accessibility: [`references/accessibility.md`](references/accessibility.md)
- Common word-list entries: [`references/word-list-common.md`](references/word-list-common.md)
- Official source index: [`references/sources.md`](references/sources.md)
