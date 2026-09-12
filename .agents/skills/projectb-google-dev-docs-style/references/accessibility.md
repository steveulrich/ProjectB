# Accessibility

Source: [Write accessible documentation](https://developers.google.com/style/accessibility)

Accessibility improvements help all readers, not only those using assistive technology.

## Language and readability

- Avoid ableist language and biased terms about disability.
- Use clear, direct language — no double negatives.
- Keep sentences under ~26 words where practical.
- Break up walls of text with headings and lists.
- Put key information in the first sentence of each paragraph.
- Left-align text — do not center or full-justify.
- Define acronyms on first use.
- Avoid exclamation marks, semicolons, and question marks when possible — some screen readers skip them.

## Headings

- Use a logical heading hierarchy (`h1` → `h2` → `h3`); never skip levels.
- Every heading must have associated content — no empty headings.
- Use descriptive, unique headings for navigation.
- Do not use heading levels for visual styling — use CSS.

## Links

- Meaningful link text that works out of context.
- Avoid adjacent links — separate with text if needed.
- Explain unexpected behavior (downloads, new tabs, same-page jumps).
- Do not use `click here` or `read this document`.

## Directional language

Do not orient the reader with `above`, `below`, `left`, `right`, or `right-hand side`.

| Recommended | Not recommended |
| --- | --- |
| In the preceding diagram,... | In the diagram above,... |
| In the following sections,... | In the sections below,... |
| Click **Menu**. | Click the button with three lines. |
| Click **Save**. | Click the green button. |

Use `preceding`, `following`, or `earlier` for document position.

If a UI element is hard to find, provide a screenshot with alt text.

## Lists and procedures

- One action per step in procedures.
- Use lists instead of comma-separated action chains in paragraphs.
- Introduce tables and interactive elements in the text before they appear.

## Images

- Provide `alt` text that summarizes intent; use empty alt for decorative images.
- Do not convey new information in images alone — always provide equivalent text.
- Do not use images of text, code, or terminal output — use real text.
- Prefer SVG over PNG when available.

## Tables

- Introduce tables in preceding text.
- Use `th` for first row and first column headings.
- Do not merge cells (`colspan`/`rowspan`).
- Avoid tables inside numbered procedures when possible.
- Do not rely on color, icons, or symbols alone — provide text equivalents.

## Code and formatting

- Avoid unnecessary font formatting — screen readers describe modifications explicitly.
- Avoid ALL CAPS and camelCase when possible — some screen readers spell letter by letter.
- Use semantic HTML (`em` for emphasis, not for italics alone).
- Do not use `&` instead of `and` in headings or body text.

## Multimedia

- Provide captions and transcripts for video and audio.
- Avoid flickering or flashing content.

## State and visual cues

- Do not use color, size, or position as the only indicator of state — add a text label change too.
- Refer to buttons and controls by their visible label, not icon appearance.

## Testing

- Navigate with keyboard only.
- Test with a screen reader.
- Verify the document works: without images, without color, without sound, with magnification.
