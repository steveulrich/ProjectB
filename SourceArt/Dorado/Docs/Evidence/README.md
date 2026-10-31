# Committed review evidence

This directory contains selected compact results from the October 30 Dorado delivery. Original JSON summaries are byte-for-byte copies. `index.json` records their source paths, sizes, and hashes. Absolute workstation paths inside these archived records describe the original run; they are not links that a fresh checkout must resolve.

- [Structural summary](structural-summary.json): derived from the fresh reopened native validator; retains errors, warnings, calibration, asset counts, protected-actor checks, path results, and scope limits.
- [Runtime summary](runtime-validation-summary.json): derived from 18 character, cover, edge, and Relic runs; retains original outcomes, failed attempts, cleanup, settings, observations, and limitations without per-frame payloads.
- [Final native readback](delivery-final-native.json), [inventory](final-delivery-inventory.json), and [native hashes](final-delivery-native-hashes.json).
- [Final performance](performance-final-summary.json) and [capture settings](performance-final-metadata.json).
- [Effects review](effects-final-review.json): the original two-capture analysis; its full comparison images remain local run evidence. The final reference image is distributed in `../Renders/`.

The seven final gallery PNGs are committed under `../Renders/`. Full editor logs, runtime traces, CSV captures, temporary UI screenshots, reference crops, backups, and intermediate/reopened effect captures remain in the original ignored `Saved/Logs/dorado-reconstruction-20261030` directory. The validation report names those original files for provenance; corresponding compact results are retained here where listed. They are not represented as newly executed tests.

Starting inputs and native import/assembly registries are separate in `../../Config/Provenance/`. The checkout bootstrap never copies these review results into a current validation report and never restores an archived user's PIE preferences.
