# Links and cross-references

Source: [Cross-references and linking](https://developers.google.com/style/link-text)

## Principles

- Be selective — each link is a decision that adds cognitive load.
- Provide context on the page when a few sentences suffice; link only when the reader needs depth.
- Avoid duplicate links to the same destination on one page (except long pages or multiple entry points).
- Link to the most relevant heading, not just the site homepage.

## Descriptive link text

Link text must make sense out of context — screen reader users often jump link to link.

| Recommended | Not recommended |
| --- | --- |
| For more information, see [Load balancing and scaling](...). | Click [here](...). |
| See [Make headings into link targets](...). | See [this document](...). |
| [HTTP/1.1 RFC](http://www.w3.org/...) | http://www.w3.org/Protocols/rfc2616/rfc2616.html |

### Two valid patterns

1. **Match the destination page title** (respect its capitalization rules).
2. **Use a descriptive phrase** — important words first, keep it short.

Do not reuse the same link text for different destinations in one document.

### Abbreviations in link text

Include both long form and abbreviation:

> [Google Kubernetes Engine (GKE)](...)

Not `Google Kubernetes Engine(GKE)`.

### Commands in link text

Include the description with the linked code element:

> To create an instance, run the [`gcloud instances create`](...) command with the `--hostname` flag.

## Cross-reference introductions

Use consistent phrasing:

- `For more information, see [Link text](...).`
- `For more information about TOPIC, see [Link text](...).`

Use **about**, not **on**: `about indexes` not `on indexes`.

Use **see** (not "refer to" or "check out") for links.

## Clarify link purpose

When the reason for linking is not obvious from context, add an "about..." clause or explain in the surrounding sentence.

## Unexpected link behavior

Explain when a link:

- Downloads a file: `download the security features PDF`
- Opens email: `send email to Technical Support`
- Jumps to a section on the same page: `see the Write descriptive link text section of this document`
- Opens in a new tab: `Accessible content (opens in a new tab)`

## Link mechanics

- Open links in the current tab by default — do not force new tabs.
- Do not use external-link icons — mention leaving the domain in text if important.
- Put punctuation outside link tags: `see [Test your code](#test).` not `[Test your code.](#test)`
- Use site-root-relative URLs for internal links.
- Use HTTPS for external links when supported.

## Quotation marks and italics

- **Linked** references: no quotation marks around link text.
- **Unlinked** short works (episodes, sections): use quotation marks.
- **Unlinked** full-length works (books): use italics.

## Navigation

Avoid external links in documentation navigation (TOC/sidebar). If unavoidable, make it clear the reader is leaving the doc set.
