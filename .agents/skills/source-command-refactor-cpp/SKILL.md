---
name: "source-command-refactor-cpp"
description: "Move Unreal C++ logic between classes or files while preserving lifecycle bindings and dependencies; also supports the explicit refactor_cpp command."
---

# source-command-refactor-cpp

Use for an Unreal C++ responsibility move or the explicit `refactor_cpp` command.

## Command Template

# C++ Refactoring Protocol

When tasked with moving variables, functions, delegates, or components from one Unreal Engine C++ class to another, follow this strict, methodical process to avoid orphaned logic, broken delegate bindings, and missing includes. Do not perform quick, disjointed text replacements.

## Step 1: Component & Logic Isolation
1. Identify all variables and functions associated with the feature being moved.
2. Search the originating `.h` file for any `DECLARE_DYNAMIC_MULTICAST_DELEGATE` macros that support the `UPROPERTY`s being moved.
3. Search the originating `.cpp` file for `#include`s specific to the logic being moved (e.g., `Blueprint/UserWidget.h` or UI Subsystems).

## Step 2: The Migration (Destination First)
1. Add the necessary `#include`s to the **destination** `.cpp` and `.h` files.
2. Copy the property definitions and delegate declarations (`DECLARE_DYNAMIC_...`) to the **destination** `.h` file.
3. Stub out or implement the functions in the **destination** `.cpp` file.

## Step 3: The Cleanup (Origin Purge)
1. Delete the `UPROPERTY`s and function definitions from the **origin** `.h` file.
2. Delete the function implementations from the **origin** `.cpp` file.
3. **CRITICAL: Check Lifecycle Hooks**. Search the origin's `BeginPlay`, `EndPlay`, `PostInitializeComponents`, and `Tick` functions for any leftover references to the moved variables, especially `AddDynamic` or `RemoveDynamic` delegate bindings. Remove them.
4. Remove any `#include`s from the origin `.cpp` that are no longer being used.

## Step 4: Full Validation
Review the actual diff and verify that moved handlers and their old lifecycle bindings are removed together. Complete the authorized project compile gate, fix relevant errors, and run affected Blueprint/runtime checks before reporting completion. Restart the editor when needed to load changed binaries or reflected definitions; preserve unsaved work and existing process ownership.
