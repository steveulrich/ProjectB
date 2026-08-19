# Common word-list entries

Source: [Word list](https://developers.google.com/style/word-list) — check the full list for terms not covered here. Spelling defaults to Merriam-Webster (first listed form wins, e.g., `canceled` not `cancelled`).

## Do not use

| Term | Use instead |
| --- | --- |
| `please note`, `at this time`, `as of this writing` | (omit — implied) |
| `simply`, `easy`, `quickly` | (omit or be specific) |
| `let's` | Direct imperative or `you` |
| `click here` | Descriptive link text |
| `allows you to` | `lets you` |
| `aka` | `also known as`, parentheses, or `or` |
| `authN`, `authZ` | `authentication`, `authorization` |
| `&` (as conjunction) | `and` |
| `etc.`, `and so on` | Rewrite to be complete or use `such as` |
| `above` / `below` (positional) | `preceding` / `following` / `earlier` |
| `on` (in cross-refs) | `about` — `For more information about...` |
| `-agnostic` | Precise term like `platform-independent` |
| `anti-pattern` (standalone heading) | Specific heading like `Avoid these five SQL errors` |
| `blacklist` / `whitelist` | `denylist` / `allowlist` (noun); rewrite verbs for clarity |
| `master` / `slave` | Context-specific inclusive alternatives |
| `crazy`, `insane`, `dummy`, `blind`, `cripple` | Neutral technical terms |

## Preferred forms

| Term | Guidance |
| --- | --- |
| `API` | Refers to a web or language API — not a single method or class |
| `app` | Not `application` for end-user mobile/web software |
| `backend`, `frontend` | One word, no hyphen |
| `checkbox` | One word |
| `click` | For mouse; use `tap` for touch when relevant |
| `data` | Singular — "the data is" not "the data are" |
| `database` | One word |
| `email` | No hyphen; verb and noun |
| `enable` | Prefer over `allows you to` |
| `filename` | One word |
| `follow-up` | Hyphenated as adjective; two words as noun |
| `for example` | Not `e.g.` in running text (OK in tables/parens if space-limited) |
| `iOS`, `macOS` | Respect Apple casing |
| `login` (noun/adj), `log in` (verb) | `login page` but `log in to the console` |
| `OK` | Not `okay` |
| `online` | Not `on-line` |
| `setup` (noun/adj), `set up` (verb) | `setup script` but `set up the cluster` |
| `sync` | Not `synch` |
| `username` | Not `account name` or `user name` |
| `web page` | Two words (but `website` one word) |
| `Wi-Fi` | Hyphenated, capitals as shown |

## Capitalization patterns

| Term | Form |
| --- | --- |
| Product phases | lowercase: `alpha`, `beta`, `generally available` |
| `Boolean` | Capital B when naming the type; `` `true` ``/`` `false` `` in code font |
| `ID`, `IDs`, `URL`, `URLs` | All caps |
| `JavaScript`, `TypeScript` | Respect official casing |
| `open source` | Lowercase unless in a product name |
| `runtime` | One word, lowercase |
| `todo` | Lowercase unless starting a sentence |

## Prefix and compound rules

| Pattern | Example |
| --- | --- |
| `auto*` prefixes | Closed: `autoscaling`, `autoupdate` → prefer `automatically update` |
| `multi*` prefixes | Closed: `multicloud`, `multithreaded` |
| `non*` prefixes | Usually closed: `nonexistent` |
| `re-*` verbs | Hyphenate: `re-create` (create again) vs `recreate` (create anew) |

## When in doubt

1. Search the [full word list](https://developers.google.com/style/word-list).
2. Check [Merriam-Webster](https://www.merriam-webster.com/) for spelling.
3. Prefer the term your audience already uses in the product UI.
4. Define specialized terms on first use; use consistently thereafter.
