# Lists and procedures

Sources: [Lists](https://developers.google.com/style/lists), [Procedures](https://developers.google.com/style/procedures)

## Choose list type

| Type | When to use |
| --- | --- |
| **Numbered** (`ol`) | Ordered steps, sequences, priorities |
| **Bulleted** (`ul`) | Unordered options, features, examples |
| **Description** (`dl`) | Term + definition pairs (glossaries) |
| **Run-in headings** | Space-saving term: definition lists |

Do not use a list for a single item — use a sentence or paragraph instead.

## Introductory sentences

- Precede lists with a complete sentence, not a fragment completed by list items.
- End the intro with a colon if the list follows immediately; period if material intervenes.
- OK to skip an intro when the heading provides enough context.

| Recommended | Not recommended |
| --- | --- |
| To get the USB driver, follow these steps: | To get the USB driver: |
| Use the **Submit** button for any of the following purposes: | Use the **Submit** button to: |

## Capitalization and punctuation

- Start each item with a capital letter (unless case is meaningful, e.g., glossary terms).
- End items with a period when they are complete sentences or contain verbs.
- No end punctuation for: single words, items without verbs, all-code items, link-only items.
- When items mix styles, rewrite for parallel structure or punctuate all items consistently.

## Parallel structure

All items in a list should share the same grammatical pattern:

| Recommended | Not recommended |
| --- | --- |
| Create an item. / Replace an item. / Delete an item. | Create. / Replacing one item. / Delete an item. |

## Serial commas

Use the Oxford comma in all lists, including inline:

> The service processes event logs, clickstream data, and e-commerce transactions.

Do not end lists with `etc.` or `and so on`.

## Procedures

A procedure is a numbered sequence of steps for accomplishing a task.

### Single-step procedures

Format as one bulleted item, not a numbered list of one:

> - To clear the log, click **Clear logcat**.

### Step structure

Follow this order within each step when applicable:

1. State goal or purpose (if helpful): `To create a VM, click...`
2. Set location/context: `In the console, go to the...`
3. State the action with imperative verb: `Click **Create**.`
4. Show command or code (if needed)
5. Explain placeholders
6. Describe output or result (same paragraph as action)

### Location before action

| Recommended | Not recommended |
| --- | --- |
| In Google Docs, click **File > New > Document**. | Click **File > New > Document** in Google Docs. |

Restate location at the start of each procedure when multiple procedures share a document.

### Goal before action

| Recommended | Not recommended |
| --- | --- |
| To start a new document, click **File > New > Document**. | Click **File > New > Document** to start a new document. |

Use colon format when the goal might imply optional: `Sort the data by date: click...`

### Multi-action steps

Combine small sequential UI actions with `>`:

> Click **Next > Finish**.

Keep steps short — split if too long.

### Sub-steps

Use lowercase letters for sub-steps, lowercase Roman numerals for sub-sub-steps. End the parent step with a colon:

```
1. To add a VM instance, do the following:
   a. Click **Create instance**.
   b. For **Name**, enter a name, and then do the following:
      i. For **Region**, select a region.
      ii. For **Machine type**, select an option.
   c. Click **Create**.
```

### Optional steps

Prefix with `Optional:` at the start of the step:

> 1. Optional: Type an arbitrary string...

Not `(Optional)`.

### Repetitive procedures

Reference earlier steps instead of repeating:

> Create a user as you did in the previous step.

### Conditions before instructions

Put prerequisites and conditions before the steps they govern:

| Recommended | Not recommended |
| --- | --- |
| If you need HTTPS, enable TLS before you deploy. | Deploy the service. If you need HTTPS, enable TLS. |

### Procedure anti-patterns

- Do not use `please`
- Do not use `run the following command` — describe what the command does
- Do not include keyboard shortcuts (`Ctrl+C`) — describe the action (`Copy the command`)
- Do not document multiple methods for the same task unless necessary — pick the best one
- Do not use directional language — provide screenshots for hard-to-find UI elements
