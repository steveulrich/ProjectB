# Formatting, headings, and dates

Sources: [Text-formatting summary](https://developers.google.com/style/text-formatting), [Headings and titles](https://developers.google.com/style/headings), [Dates and times](https://developers.google.com/style/dates-times), [Code in text](https://developers.google.com/style/code-in-text)

## Text formatting

| Style | Use for | Markdown |
| --- | --- | --- |
| **Bold** | UI elements, run-in headings at start of notices | `**text**` |
| *Italic* | Terms being defined, words as words, emphasis, book/movie titles | `_text_` |
| `` `Code font` `` | Code, filenames, classes, methods, flags, placeholders, command output | `` `text` `` |
| Underline | Link text only — never for emphasis |

### Rules

- Do not use `&` as a conjunction — write `and` (except in UI labels that contain `&`).
- Do not override font type, size, or color inline.
- Use American English spelling and punctuation.
- Put quotation marks and end punctuation **outside** link text.

## Headings and titles

- **Sentence case** for all headings, titles, and navigation.
- One `h1` per page; do not skip heading levels.
- Do not use headings purely for visual formatting.
- Do not put links inside headings.
- Avoid numbers in headings to indicate sequence — use hierarchy instead.
- Avoid `-ing` as the first word of a heading.

| Heading type | Pattern | Example |
| --- | --- | --- |
| Task-based | Bare infinitive | `Create an instance` |
| Conceptual | Noun phrase | `Migration overview` |
| Optional section | `Optional:` prefix | `Optional: Customize your alias` |

When introducing a group of subsections, write: "The following sections describe..."

Do not write "this section" or "these sections" — ambiguous for screen readers.

## Code font

Use `` `code font` `` for anything the reader enters, selects, or that names a code entity:

- Filenames, paths, extensions: `` `config.yaml` ``, `` `/etc/postgresql/13/main` ``
- Class, method, function, attribute names: `` `SnapshotDiskOperator` ``, `` `get_job_status` ``
- Commands and CLI tools: `` `kubectl` ``, `` `gcloud compute instances create` ``
- Flags, parameters, env vars: `` `--hostname` ``, `` `CHROME_REMOTE_DESKTOP_DEFAULT_DESKTOP_SIZES` ``
- Data types, keywords, enum values: `` `STRUCT` ``, `` `FROM` ``, `` `true` ``
- HTTP status codes: `` `400 Bad Request` ``, `` `2xx` ``
- Placeholders: `` `PROJECT_ID` ``, `` `SUBNETWORK_NAME` ``
- User-entered text in UI fields: `` `config-management` ``

### Code font + bold

When a UI element displays user-entered or dynamic text, use both:

> In the Network list, select **`my-net-2`**.

### Do not use code font for

- Product, service, or organization names (unless quoting from code)
- Domain names and URLs the reader follows in a browser (use links with descriptive text)
- Inflected code terms — add a noun: "the `ADDRESS` constant's value" not "`ADDRESS`'s value"

## Dates and times

### Dates in prose

| Context | Format | Example |
| --- | --- | --- |
| Full date | `Month DD, YYYY` | January 19, 2017 |
| With weekday | `Day, Month DD, YYYY` | Tuesday, April 27, 2021 |
| Month and year only | `Month YYYY` (no comma) | January 2017 |
| Mid-sentence with year | comma after year | The January 19, 2017, release... |

Avoid numeric-only dates like `04/05/09` — ambiguous globally.

### Numeric-only dates

Use ISO 8601: `YYYY-MM-DD` (e.g., `2017-04-15`). Choose day > 12 when possible in examples.

### Times

- 12-hour clock: `3 PM`, `3:45 PM` — capitalize AM/PM, one space before
- Drop minutes for round hours: `3 PM` not `3:00 PM`
- Time ranges with hyphens, no spaces: `5-10 minutes ago`
- Avoid time zones unless necessary; spell out region + UTC offset: `US and Canadian Pacific Standard Time (UTC-8)`

### Avoid seasons

Use months or quarters instead of "spring" or "fall" — seasons differ by hemisphere.
