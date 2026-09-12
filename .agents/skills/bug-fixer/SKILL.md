---
name: bug-fixer
description: Diagnose and fix Unreal compile, linker, Blueprint, and runtime defects in ProjectB; consult matching prevention history when it helps explain the failure.
---

# Bug Fixer

Reproduce the failure or inspect its error, identify the root cause, and apply the smallest correct fix. Check nearby code for the same demonstrated cause when relevant.

## Verification and completion

- Compile a coherent set of C++ or build-input changes using the project testing ladder. Fix relevant compiler failures and rerun affected checks within existing authorization.
- For Blueprint-only changes, compile/validate the affected assets and run the relevant behavior checks. Restart the editor when needed to load changed binaries or reflected definitions, or to recover a demonstrated stale state.
- Run affected runtime or cook/package checks and every proof required by the selected acceptance gate. A compile, startup screen, or same-process PIE pass proves only that scope.
- Report the root cause, change, verification result, evidence path, and any remaining blocker. Record prevention in the task's normal evidence. Do not block a verified repair on a separate skill update.

## Search prevention history when relevant

Search [references/prevention-catalog.md](references/prevention-catalog.md) with `rg` for the error, symbol, subsystem, or BF identifier, then read only matching entries. Do not load the whole catalog for every repair. Project paths in historical entries are relative to the repository root; historical results are not current acceptance evidence.

Update this reference only for a reusable new lesson, or a correction to an existing lesson, when instruction editing is authorized. Preserve filesystem permission boundaries; otherwise leave the lesson in the ordinary task evidence.
