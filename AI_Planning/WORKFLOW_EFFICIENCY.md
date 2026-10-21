# Workflow efficiency index

Updated: 2026-10-15. Root [AGENTS.md](../AGENTS.md) owns universal scope and approval rules. This index routes on-demand procedures; it is not another acceptance gate. Read only the owner needed for the selected operation.

| Operation | Canonical procedure owner |
| --- | --- |
| Resume, provenance, contracts, ledger, evidence, retries | [Gate resume and evidence](Runbooks/gate-resume-evidence.md) |
| Compile, automation, package preflight, build/package caps | [Build and package](Runbooks/build-package.md) |
| Editor ownership, bridge safety, UI, readiness | [Editor and PIE](Runbooks/editor-pie.md) |
| Combat fixtures and settled peer comparison | [Replication and combat](Runbooks/replication-combat.md) |
| Packaged topology, transport teardown, shop probes | [Packaged transport and shop](Runbooks/packaged-transport-shop.md) |
| Placement, quota, round reset, rematch | [Buildables and rematch](Runbooks/buildables-rematch.md) |
| Current candidate, gate, next action | [Slice status](SLICE_STATUS.md) |
| Helper arguments and gate-record schema | [Test helper reference](../Scripts/Test/README.md) |

## Measure workflow cost

Record source/candidate identity, selected proof, process launches, build/package counts, elapsed time, retries, first causal error, and evidence paths in the gate contract and linked acceptance record. Use run-family and gate briefs before reading individual logs. Compare only equivalent scopes and inputs.

Hard per-gate limits remain **5 Editor builds, 2 Game builds, and 1 package**. Stop an unchanged failure after two attempts without new causal evidence. Use the cheapest applicable verification while preserving every selected proof and final end-to-end requirement.

Keep `AGENTS.md` within **6,000 characters** and `SLICE_STATUS.md` within **4,000 characters**. Detailed acceptance documents may exceed 100 lines. Move detail behind links; never delete evidence to meet a budget.

Raw logs, full records, and [historical workflow notes](Archive/Full_History_20261013/WORKFLOW_EFFICIENCY.md) are not routine context. A bounded brief may justify a narrow necessary read for a changed diagnosis. Startup, headless, and same-process results retain their scope limits; workflow checks do not replace gameplay, UI, or four-player LAN acceptance.