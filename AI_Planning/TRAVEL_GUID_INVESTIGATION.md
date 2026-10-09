# PlayerState GUID travel investigation

The two ASC replication errors in `relic-carry-editor01.log` remain unresolved. Three new four-world PIE transitions did not reproduce them. This does not establish a fix or clean final multiplayer acceptance.

Evidence is under `Saved/Logs/buildable-authority-20260927/`.

## Original failure and comparison

The original staging-to-Dorado transition completed on four worlds, but clients 1 and 2 reported `StartReplicating: Invalid Net GUID` on `BwayPlayerState_1.AbilitySystemComponent`. Ten adjacent read-only reassignment warnings cover HealthSet, CombatSet, ASC, and UpgradeComponent. They preceded the carry observer.

`travel-guid01-historical-comparison.json` compares that lifecycle against all five candidate43 relic logs. Candidate43 has no invalid-GUID errors. Its first client has two read-only warnings for the GameState package and class default object. Those are different objects and must not be treated as confirmation or dismissal of the ASC defect.

Engine `DataReplication.cpp` identifies premature replication during seamless travel as one cause of this signature. `FNetGUIDCache::CleanReferences` marks invalid entries read-only, while `AGameModeBase::HandleSeamlessTravelPlayer` replaces controllers and copies their state. These are causal leads, not a proven project defect. Epic's [UE-212693 issue](https://issues.unrealengine.com/issue/UE-212693) concerns class/object package-map warnings after seamless travel; its reported scenario does not establish the cause of these ASC errors.

## Bounded reproduction

Editor44 compiled the opt-in `bway.Test.TravelGuidWatch` observer. It records resolved PlayerState/subobject GUIDs, object IDs, reverse mappings, read-only timestamps, and world paths when values change. It holds strings and weak driver references, never strong world or player references. Its ticker and world-cleanup/GC delegates stop explicitly or after 120 seconds. It does not change GUIDs or replication settings.

One editor lifecycle, PID39812, ran three separate PIE lifecycles with four distinct selected heroes:

| Run | Condition | Result |
|---|---|---|
| 01 | Default packet timing | Four destination worlds, four PlayerStates each, Playing reached; no GUID errors or read-only warnings |
| 02 | 200 ms packet lag before travel | Same result; all four locked hero choices agree on all peers |
| 03 | 200 ms packet lag and temporary 1 FPS cap | Same result; all four locked hero choices agree on all peers |

The raw log is `travel-guid01-editor.log`. Selection, destination, cleanup, and settings records use `travel-guid01`, `02`, and `03` prefixes. `travel-guid01-summary.json` records counts and scope. The initial protected-property Python read failed before the first travel. Its exception references were cleared before selecting heroes. Pixel Streaming startup errors remain outside this structured probe's scope; they remain actionable for that UI channel.

All three observers stopped explicitly. Packet lag returned to zero, the FPS cap returned to its original zero, original PIE settings were restored, and the final bridge inspection found no PIE worlds or dirty assets/maps. The editor then closed successfully.

The first observer version could lose a selected entry when an unresolved parent no longer contained the PlayerState name. Its `removed` marker therefore proves removal from the selected view, not necessarily cache eviction. The source now retains previously selected GUID keys until observer cleanup. Preserve this limitation when reading the first three traces.

## Candidate45 packaged observer (September 30, 01:54–01:55 UTC)

Candidate `LAN-20260929-45` was launched in the four-process Connected fixture with the
corrected read-only `bway.Test.TravelGuidWatch` observer on the host and all three clients,
under the fixture's 200 ms packet lag. This was a diagnostic run, not a replacement for the
Connected acceptance gate: the owned launcher was stopped during the second-lap checkpoint
after travel, so its result is `failed-transport-scope` and no transport pass is claimed.

The three clients reached Dorado through one seamless travel each. The bounded travel brief
records one seamless-travel marker, three experience-load completions, zero travel-failure
markers, zero error lines, and zero unknown-error lines per client. The host also emitted the
Dorado seamless-travel and experience-completion markers before teardown. The watcher resolved
PlayerState, HealthSet, CombatSet, AbilitySystemComponent, UpgradeComponent, and gold-set GUIDs
in both staging and Dorado. It recorded expected world-cleanup and empty reverse mappings after
the destination world was torn down. No `StartReplicating: Invalid Net GUID` line occurred in
any of the four logs.

Evidence: `Saved/Logs/lan-travel-watch-20260929-45/packaged-LAN-20260929-45-connected-input01-result.json`,
`travel-brief.json`, and the four packaged logs. This changed-condition observation narrows the
failure to an unreproduced timing/object-lifetime path; it does not establish that the original
ASC error is fixed. Do not rerun this exact observer condition without a new timing or lifecycle
hypothesis.

## Candidate47 packaged low-frame-rate condition (September 30, 03:25–03:29 UTC)

Candidate `LAN-20260929-47` repeated the Connected browser flow with the corrected observer under
the existing 200 ms packet lag, then applied a 15 FPS cap after startup. This is a distinct timing
condition from the earlier 30 FPS packaged observer run. All four processes completed two seamless
travels and the Connected transport scope; every log recorded zero `Invalid Net GUID` errors, zero
read-only GUID reassignment warnings, zero travel-failure markers, and zero unknown errors. The
observer started and stopped once on each process.

The compact result is
`Saved/Logs/lan-travel-watch-20260930-48/travel15fps-result.json`; the transport result is
`packaged-LAN-20260929-47-connected-travel15fps01-result.json`, and the bounded transition inventory
is `travel-brief.json`. This expands timing coverage and leaves the original editor-only failure
unreproduced; it does not prove that the original failure is fixed. Do not repeat the 15 FPS
condition unchanged.

## Retry condition and independent work

Do not repeat these three conditions unchanged or enable broad GUID-remapping settings to silence the error. Use the corrected observer if a recurrence or new timing/object-lifetime evidence supplies a distinct hypothesis. The original error remains a completion gap.

Continue the independent packaged carry gate: cook the six newer socket/HeroData assets and exercise each human hero through the existing four-peer ownership fixture, with peer-side attachment assertions. Physical input, second-PC LAN, perceived rendering/audio, performance, and final integrated acceptance remain required.
