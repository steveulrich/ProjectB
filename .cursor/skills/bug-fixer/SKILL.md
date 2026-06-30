---
name: bug-fixer
description: >-
  Fix compile errors and runtime bugs while documenting root cause and prevention.
  Use when fixing build failures, Blueprint wiring bugs, UMG BindWidget issues,
  linker errors, or any defect — then append a prevention note to this skill.
---

# Bug Fixer

When fixing a compile error or bug, **always** do both:

1. **Fix** the immediate issue (minimal correct diff).
2. **Document prevention** — append a row to [Prevention catalog](#prevention-catalog) below (or extend an existing row).

After fixing, tell the user:
- **Root cause** (one sentence)
- **Prevention** (what to do next time)
- Whether the skill catalog was updated

---

## Fix workflow

```
1. Reproduce / read the error (log, BP compiler, UHT, linker)
2. Identify root cause — not just the symptom
3. Apply minimal fix
4. Verify (Tier 1 compile, or editor BP compile, or PIE if runtime)
5. Append prevention entry to catalog below
6. Scan same pattern in nearby files (same module, same widget base, etc.)
```

---

## Prevention catalog

| ID | Symptom | Root cause | Prevention |
|----|---------|------------|------------|
| **BF-001** | `Text_Timer` (or any `BindWidget` member) missing in BP Event Graph variable picker; MCP `GetVar` fails; C++ `if (Text_Timer)` still works | `UPROPERTY(meta=(BindWidget))` without **`BlueprintReadOnly`** binds at runtime but is **not exposed to Blueprint**. Designer name match alone is insufficient. | **Always** declare widget bindings as:<br>`UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="…\|Widgets")`<br>Match C++ type to UMG widget class (`UCommonTextBlock`, `UCommonNumericTextBlock`, not generic `UTextBlock` when tree uses CommonUI). Follow `BwayPostRoundSummaryWidget` / `BwayPostMatchInterstitialWidget` pattern. After C++ change: recompile module, reopen BP, **Compile** BP. |
| **BF-002** | BindWidget property is `nullptr` at runtime despite correct name | Widget tree class ≠ C++ property type (e.g. `TextBlock` in C++, `CommonTextBlock` in UMG — usually OK if subclass; fails if unrelated type). Or name typo / widget not under this BP's tree. | Use exact widget type in C++ when using CommonUI/Lyra widgets. Verify in Designer: widget **Name** matches property name exactly (case-sensitive). Prefer `BindWidgetOptional` + null checks in C++. |
| **BF-003** | Tier 1 compile fails: "Unable to build while Live Coding is active" | Editor has Live Coding enabled while UBT runs. | Close editor or press Ctrl+Alt+F11 before Tier 1. Do not claim compile pass until UBT exits 0. |
| **BF-004** | HUD round timer shows 3:00 while sudden death / round FSM use 90s; client `GetRoundTimeRemaining()` wrong | `RoundDuration` set authority-only via `SetRoundDuration` but **not replicated**; clients kept C++ default 180 while `RoundStartTime` replicated. | Replicate `RoundDuration` on `UBwayRoundManagementComponent`; `OnRep_RoundDuration` one-shot `OnRoundTimeChanged` on clients; HUD keeps local 1 Hz poll using `RoundStartTime + RoundDuration`. Do **not** replicate remaining seconds every tick. |

---

## UE widget binding checklist

Before marking a slot widget "done":

- [ ] Every `BindWidget` / `BindWidgetOptional` has **`BlueprintReadOnly`**
- [ ] C++ member type matches UMG widget class (CommonUI types included)
- [ ] Designer widget **Name** == C++ property name (e.g. `Text_Timer`)
- [ ] BP subclass **Compile** succeeds; `Get Text_Timer` appears in graph
- [ ] C++ module recompiled after header change (not Live Coding only)

---

## UE compile checklist

- [ ] `#include` full type in `.cpp` when calling methods on pointer
- [ ] `TSubclassOf<T>` / `BindWidget` need full type in header
- [ ] GF primary assets: `AssetBaseClass=/Script/Engine.PrimaryDataAsset` in `DefaultGame.ini`
- [ ] Run Tier 1 after C++ edits (editor closed or Live Coding off)

---

## When to extend this skill

Append a new **BF-00N** row when:

- A bug took >1 attempt to diagnose
- The same mistake could recur in another file/module
- A tooling/MCP script assumed wrong API (e.g. wrong MCP param names)

Keep rows concise. Link to file paths, not long prose.
