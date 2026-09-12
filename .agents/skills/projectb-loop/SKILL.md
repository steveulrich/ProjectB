---
name: projectb-loop
description: Schedule a requested recurring check or /loop prompt through the current host's automation capability; use legacy shell watchers only where supported and explicitly requested.
---

# Recurring work

Use the available Codex automation tool for recurring tasks, reminders, monitors, and later wakeups. Inspect existing automations before creating one, prefer an attached heartbeat unless the user requests standalone runs, and preserve the requested interval and stopping condition. Notify only on a meaningful change, completion, failure, or required user action unless periodic updates were requested.

An immediate run is separate from scheduling: run now when requested or clearly part of the task, then follow the saved schedule. Do not create duplicate schedules or promise wakeups that the host cannot deliver. Respect existing authorization for each replayed action.

Use [legacy monitored-shell instructions](references/legacy-monitored-shell.md) only when the user explicitly requests that runtime and the available tool schema actually supports background output notifications. If it lacks `notify_on_output`, use the host automation tool; a shell printing a sentinel is not a supported substitute.
