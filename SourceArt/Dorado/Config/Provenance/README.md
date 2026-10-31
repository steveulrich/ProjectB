# Starting snapshots for a fresh checkout

These ten JSON files preserve source observations, the protected-map baseline, and the import/assembly registries associated with the delivered native assets. `snapshot.json` records their original local report paths and SHA-256 hashes. They are a known starting point, not new validation results.

Run `python SourceArt/Dorado/Scripts/bootstrap_checkout.py` from ProjectB before using the documented incremental assembly, source-audit, or validation commands on a fresh checkout. The helper verifies the snapshots and copies only missing allowlisted files into the normal ignored `Saved/Logs/dorado-reconstruction-20261030` working directory. Existing reports are never overwritten. Use `--dry-run` to inspect the action.

Source mapping and effects snapshots support source regeneration. Map baseline and inherited-dependency evidence support the scoped validator. Material, mesh, and scene registries support incremental assembly and stale-owned-actor reconciliation. The shader-filter and Nanite records support their documented maintenance/readback helpers.

The package retains workstation-specific authoring paths. Bootstrapping does not relocate them. See the integration guide before rebuilding in another workspace. Do not edit these archived snapshots and expect their original hashes to remain valid; rebase the working copies when required. No personal PIE-settings baseline is included. Capture the current session's own settings before running a traversal fixture.
