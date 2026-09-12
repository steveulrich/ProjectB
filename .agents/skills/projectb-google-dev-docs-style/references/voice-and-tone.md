# Voice and tone

Source: [Voice and tone](https://developers.google.com/style/tone), [Active voice](https://developers.google.com/style/voice), [Highlights](https://developers.google.com/style/highlights)

## Voice

Write like a knowledgeable friend who understands what the developer wants to do:

- Conversational, friendly, respectful
- Casual and natural, not pedantic or pushy
- Clear and direct — information first, personality second
- Human and memorable, but not entertaining for its own sake

Aim for conversational tone, not colloquial speech. You probably speak more verbosely than you should write.

## Second person and active voice

- Address the reader as **you**, not **we** (except when describing product behavior: "The service returns...").
- Use **active voice** so the actor is clear.

| Recommended | Not recommended |
| --- | --- |
| Send a query to the service. | The service is queried. |
| Click **Save**. | **Save** should be clicked. |
| The server sends an acknowledgment. | An acknowledgment is sent. |

### Passive voice exceptions

Passive is OK when:

- Emphasizing the object: `The file is saved.`
- De-emphasizing the actor: `Over 50 conflicts were found in the file.`
- The actor is irrelevant: `The database was purged in January.`

## Avoid

| Category | Examples to avoid |
| --- | --- |
| Tone | Slang, wackiness, pop-culture references, exclamation marks |
| Fillers | `please note`, `at this time`, `as of this writing`, `currently` |
| False simplicity | `simply`, `it's easy`, `it's that simple`, `quickly` |
| Figurative language | Metaphors, ableist language, `-agnostic` (use `platform-independent`) |
| Engagement bait | `Let's create...`, `Dude!`, superlatives without substance |
| Starting patterns | Beginning every sentence with `You can` or `To do` |
| Abbreviations | `tl;dr`, `ymmv`, `aka` (write out `also known as`) |

## Politeness

Skip `please` in instructions.

| Recommended | Not recommended |
| --- | --- |
| To view the document, click **View**. | To view the document, please click **View**. |
| For more information, see [Load balancing](...). | For more information, please see [Load balancing](...). |

## Tone examples

| Too informal | Just right | Too formal |
| --- | --- | --- |
| Dude! This API is totally awesome! | This API lets you collect data about what your users like. | The API documented by this page may enable the acquisition of information pertaining to user preferences. |
| Then—BOOM—just garbage-collect, and you're golden. | To clean up, call the `collectGarbage` method. | Please note that completion of the task requires the following prerequisite: executing an automated memory management function. |

## Global audience

- Avoid culturally specific references (e.g., "Black Friday" → "peak scale event").
- Use simple, consistent phrasing — aids translation.
- Define acronyms on first use unless universally known (`API`, `AI`).
- Prefer common words over idioms.

## Writing for accessibility

See [`accessibility.md`](accessibility.md) for full guidance. Key voice-related rules:

- No directional language (`above`, `below`, `left-hand side`) — use `preceding`, `following`, `earlier`.
- Refer to UI elements by label, not appearance (`Click **Save**`, not `Click the green button`).
- Keep sentences short (under ~26 words).
- Avoid double negatives.
