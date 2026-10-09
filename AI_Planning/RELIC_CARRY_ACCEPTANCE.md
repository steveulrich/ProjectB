# Relic carry socket and placeholder presentation

Evidence root: `Saved/Logs/buildable-authority-20260927/`.

**Current result:** candidate `LAN-20260927-44` cooks the six socket/HeroData changes and passes four-process hand attachment checks for all four heroes at 200 ms packet lag. Thirty-six carried observations have zero measured socket error. Carry checks pass 35 assertions/eight corruption controls; the existing ownership verifier passes 37 assertions/48 controls. Earlier PIE evidence covers animated movement and viewed placeholder grips. The original editor travel ASC GUID defect remains open; full multiplayer acceptance is incomplete.

## Defect and replacement contract

`DA_BW_RelicSettings_Default` requests `relic_r`. Before this change, neither `SKM_Manny` nor `SKM_Quinn` had that socket, and all four hero definitions had empty `RequiredMeshSockets` arrays. `relic-carry01-assets.json` records all four failed lookups. Attaching to a nonexistent socket could place the relic at the mesh origin instead of the hand.

The two placeholder meshes now have a mesh socket named `relic_r`, parented to `hand_r`, with relative location `(12,0,0)` cm, zero rotation, and unit scale. Each of the four hero definitions requires `relic_r`. Existing `UBwayHeroDataAsset::IsDataValid` rejects a replacement mesh missing a required socket. Keep the settings socket and hero requirements synchronized when changing this contract. Tune a replacement rig's socket transform to its grip; gameplay and effect ownership stay unchanged.

The changed assets are Manny, Quinn, and the Argus, Gunslinger/Rawlins, Hexweaver/Korryn, and Alona hero definitions. No skeleton, animation, map, gameplay tuning, or other asset was saved. `relic-carry02-preflight.json` records the exact mutation and targeted rollback. `relic-carry03-authored.json` records saves; `relic-carry04-validation.json` reports six requested/checked/valid, zero warnings, invalid, skipped, or unable-to-validate assets. `relic-carry05-reloaded-assets.json` confirms all four definitions resolve the saved socket to `hand_r` after package reload.

## Runtime and visual evidence

The production selection RPCs chose four distinct heroes before travel (`relic-carry07-selection.json`). The fixture calls production `OnPickedUp`/`OnDropped` and drives the actual owning pawn's movement input synthetically at configured 100 ms latency. It does not prove physical pickup, keyboard/gamepad input, or a packaged carry path.

`relic-carry13-verification.json` joins complete hero sample sets from runs11 and12 without relabeling failed runs. It passes 33 checks and six corruption controls. Twelve samples contain 48 peer rows; the four final settled samples contain 16 strictly checked rows. Each settled row has the correct carrier, `Carried` state, carrier mesh parent, `relic_r` socket, active carried effect, and nonreplicated VFX attachment parent. Recomputed socket-to-relic position error is zero on all settled rows. The authority's actual displacement exceeds 5 cm for every hero: about 220 cm for Argus and 35 cm for each remote owner.

The viewed `relic-carry11-DA_BW_HeroData_{Argus,Gunslinger,Hexweaver}.png` and `relic-carry12-DA_BW_HeroData_Alona.png` show a visible relic at the hand during the movement sequence. Captures use the host viewport and the selected pawn's gameplay camera through a temporary view-target change. They do not prove each remote player's physical camera/input or HUD presentation. The socket offset provides an initial placeholder grip; combat transitions, throws, catches, all viewing angles, and final animation/art quality remain open.

## Attempts and cleanup

- Run09 used wall-clock stage advancement. Slow editor frames skipped checkpoints; a missing Python `get_character_movement` binding also interrupted cleanup. The callback stopped, and `relic-carry09-cleanup-repair.json` records the corrected component lookup and cleanup. These samples are not a pass.
- Run10 uses game-clock checkpoints and requires complete sample counts. It rejected an early snapshot in which one peer still had the previous dropped state. Cleanup completed. Keep this transient; it is not settled convergence evidence.
- Run11 preserves early samples and requires the 2.8-second settled sample. Argus, Rawlins, and Korryn each complete their three samples and movement checks, then the 120-second wall-clock limit expires. The failed attempt and its successful cleanup remain recorded.
- Run12 selects only the remaining Alona scope. It completes all samples and verifies released carriers on all four peers after cleanup. Both run11 and12 callbacks are stopped. The joined verifier requires all four heroes, distinct peers, actual displacement, correct transforms, local effects, and final cleanup.
- One bridge performance snapshot reported about 0.258 FPS and 3.875 seconds per frame in this hidden four-world editor. The background CPU throttle setting was already false; checking/restoring it did not fix or diagnose the slowdown. This is not a target-device performance profile.
- `relic-carry14-cleanup.json` confirms released carriers, original PIE options, restored throttle, and no dirty content/maps. `relic-carry15-stopped-pie.json` confirms zero PIE worlds. The bridge reported a forced editor stop after its graceful timeout; PID30220 was verified gone, restored closed-editor INI values were checked, and the log contains its exit sequence. Preserve that distinction instead of claiming a measured normal process exit.

This work used one editor lifecycle and one PIE lifecycle, with four bounded observers. It added two mesh sockets and one four-definition configuration batch; no extra C++ build or package was run for these six asset changes.

## Candidate44 packaged carry gate

The next iteration adds development-only socket assertions to `BwayRelicOwnershipProbe`. Each peer compares the real carried actor's mesh parent, configured socket, socket existence, and world position. The log records the hero, mesh, socket bone, and measured distance. The carry verifier additionally requires `hand_r`, both placeholder meshes, four distinct heroes per viewer, unique stages, four distinct viewers, peer agreement, normal client exits, and no errors.

`bway.Test.SelectDistinctCarryRoster` supplies the fixture roster through production selection methods. It temporarily permits reassignment, restores the original duplicate rule before locking, and leaves four distinct human selections. The normal staging map has no bots; the destination spawns the four bots. This is controlled roster setup, not physical or remote selection-input evidence.

Candidate44 uses game44 and the editor46 cooker. The later game44 change only corrects the unreflected test command's staging prerequisite; it changes no cooked defaults or reflected layout. Editor46 does not contain that final prerequisite correction, so rebuild the editor target before using that command in PIE. No production gameplay code changed in this diagnostic iteration.

Build history preserves editor45's two test-code type errors, corrected by editor46. Game43 passed but was superseded after the staging-bot prerequisite was checked against previous logs; game44 contains the correction. The iteration used five target builds (one failed), one cook/package, one packaged run, and the three PIE diagnostic runs documented separately. Avoid another build or cook for evidence extraction alone.

`candidate27-44-preflight.json`, the frozen source snapshot, and `candidate27-44-artifact-verification.json` identify 131 verified staged files. The staged executable SHA-256 is `C7EE5676435FE341C2E3D9D5BFC529A162A435EA58735101FC207289F32247D6`. `candidate27-44-source-equivalence.json` records an encoding-only difference between snapshot manifests: their decoded text and binary patch match. Cook/Stage/Pak passed in 103.23 seconds.

`run-packaged-carry44.ps1 -Attempt packaged-carry01` ran one listen host and three clients with D3D12 offscreen rendering and 200 ms packet lag. Its 172.94-second result is `packaged-carry01-result.json`. Each of Argus, Rawlins, Korryn, and Alona performed controlled pickup and release through the existing ownership fixture. Duplicate pickup retention, request/pass/throw abilities, ownership cleanup, and one goal also passed. The fixture disables movement while arranging actors; this run does not prove packaged locomotion or perceived presentation.

- `packaged-carry01-carry-verification.json`: 35 checks, eight rejected corruption controls, nine carried samples per viewer, 36 total, all at `relic_r`/`hand_r` with zero measured error.
- `packaged-carry01-verification.json`: 37 ownership checks and 48 rejected negative controls.
- All three clients reached their fixture pass/exit trigger and exited with code 0. The host remained alive until they completed, then the runner terminated its owned host. Host exit -1 is teardown, not a gameplay failure or normal-exit proof.
- All four logs contain zero errors, critical failures, or invalid-GUID errors. Client2 has two read-only GUID warnings for the match-upgrade catalog asset. Preserve those warnings as a separate travel/cache investigation; do not call the run warning-free.

Physical input, perceived packaged carry/VFX/camera quality, moving carry transitions, second-PC LAN, target performance, the original travel ASC error, and final integrated match acceptance remain open.

## Remaining replication issue and next action

`relic-carry17-editor-errors.json` preserves all 36 editor error lines: 32 Python setup/binding lines, two Pixel Streaming target-window lines, and two `LogRep` errors. There are no fatal/assert/ensure lines and no unsupported Niagara references.

The two unresolved errors occur during staging-to-Dorado travel, before the carry fixture, on the PlayerState ability-system components in PIE worlds1 and2. Adjacent warnings report attempts to reassign read-only NetGUIDs for `HealthSet`, `CombatSet`, `AbilitySystemComponent`, and `UpgradeComponent`. Engine `DataReplication.cpp` explains that this signature can arise when a seamless-travel client receives objects before finishing travel and later removes those package-map references. That is a diagnosis lead, not proof of the cause here. Do not suppress these errors or promote this editor lifecycle as clean replication evidence.

Three subsequent four-world travel runs did not reproduce the ASC errors, including 200 ms packet lag and a temporary 1 FPS cap. The original defect remains open; see the [travel GUID investigation](./TRAVEL_GUID_INVESTIGATION.md) for exact scope, cleanup, and retry conditions. Continue by cooking the six carry assets and verifying their actual packaged attachment behavior. Physical actions, second-PC LAN, performance, and the complete final match flow remain required.

## Cached editor capabilities

Hero mount points use `/Hero_Spartacus`, `/Hero_Rawlins`, `/Hero_Morgan`, and `/Hero_Alona`. Use a transient `SkeletalMeshComponent` to query socket/bone existence, then `destroy_component(component)`; the Python binding requires its caller argument. Read component scale through `get_editor_property('RelativeScale3D')`. `load_package` takes one package path. Validation settings use the actual `bSkipExcludedDirectories` field. Read the play-settings CDO at `/Script/UnrealEd.Default__LevelEditorPlaySettings`; the class wrapper is unavailable and its `PlayNetMode` byte enum cannot be pythonized. Preserve that mode and verify it from the closed-editor INI. Use `get_component_by_class(CharacterMovementComponent)` for movement cleanup. Prefer one hero per bounded observer when editor frame pacing is this slow.
