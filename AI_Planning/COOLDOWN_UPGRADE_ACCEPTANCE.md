# Cooldown upgrade acceptance

Gate: `Saved/Logs/cooldown-upgrade-20260920/gate-contract.json`.
Candidate18 contains the cooldown change and the corrected packaged probe.
Candidate16 retains the earlier movement-only proof; candidate17's failed attempt
is preserved below.

## Implemented behavior

`CooldownReduction` is a replicated hero attribute on the persistent PlayerState
ability system, bounded to a fraction from zero to one. A reversible infinite
upgrade effect modifies it. The shared `UBwayGameplayAbility_Base` cooldown path
reduces newly applied finite durations once, using either the hero kit duration or
the effect-defined fallback. Existing active cooldowns are unchanged. A full
reduction applies no blocking cooldown effect; infinite/instant effects retain
their lifetime policy.

All four current kit bases (Argus, Rawlins, Alona, Korryn) route through this helper.
The purchase component still owns authority, rank replacement, debit rollback,
and match reset. No shared effect defaults are mutated.

The catalog now has a fourth entry, CooldownReduction, with provisional
5/10/15/20 percent ranks costing 75/125/175/225 gold. Dated final-alpha material
has not established these values or every equipment rule. They remain replaceable
data, not a claim of completed parity.

## Verified scope

- `candidate18-cooldown-brief.json`: four rendered Argus processes on one PC at
  200 ms packet lag passed production purchases and actual ForGlory activation.
  The independently exported kit duration is 25 seconds and the catalog rank-one
  reduction is 5%; all observed effect durations were 23.75 seconds. The record
  contains eight authority cycles, four authority expiries and twelve owner HUD
  samples, including cooldown reuse rejection, ticking timer/progress values,
  dimmed icon, expiry cleanup and successful reactivation. Owner elapsed expiry
  includes the recorded latency/tick tolerance; it is not a zero-latency claim.
  All four processes were alive at the completion checkpoint, three clients
  exited zero, host cleanup followed their exits, and runtime error count is zero.
- `verifier-checks.json`: 21 extractor checks passed. Copied evidence with wrong
  durations, countdown/progress, controls, rejection/reactivation, expiry,
  wallets, network settings, process exits, catalog/kit values or authority
  records is rejected. These mutated copies are not gameplay evidence.
- `package18-result.json`: all 131 staged hashes matched; tracked runtime source,
  the frozen probe and 16 untracked runtime file hashes matched before launch.
  `compile-game-r3.json` built the corrected probe. The game implementation is
  frozen with `source-snapshot-LAN-20260920-18.*` and its additional hash manifest.

- `compile-editor.json`: editor compile passed with no failure signals; the
  runtime DLL advanced before automation launched.
- `compile-game.json`: game target also passed with no failure signals. No new
  package was made before the multiplayer probe was ready.
- `automation-economy-r1.json`: all five economy tests passed, no warnings/errors,
  framework exit zero and process exit zero. `CooldownUpgrade` loads the four real
  hero ability sets, grants their abilities, and exercises the virtual cooldown
  application path for all 16 configured cooldown abilities. It checks the eight
  primary/placement abilities without cooldown effects, plus a concrete
  effect-default fallback case.
- The test verifies baseline/rank-one/rank-two/reset durations, cooldown-tag
  blocking and removal, exact gold debits, stale request rejection, rollback,
  preservation of an already active cooldown on reset, and fraction boundaries.
  It does not activate full gameplay abilities or simulate network prediction.
- `catalog-preview-r2.json`, `catalog-apply-r1.json`, and `catalog-cold-r2.json`
  record the reviewed plan, one saved entry and separate-process readback. The
  catalog contains four entries, validation has no warnings/errors, and no dirty
  packages remain. Existing entries are preserved.
- `shared-cooldown-audit.json` records the common source route and outstanding
  runtime scope.

## Tooling attempts retained

Candidate17 passed all four purchase checkpoints but timed out before activation.
The probe incorrectly required `Image_CooldownOverlay`, an optional native binding
that is absent from the authored slot. Read-only template inspection confirmed
that the layout instead contains `Image_Icon`, `Text_CooldownTimer`, and
`ProgressBar_Cooldown`. Candidate18 requires the actual icon tint, timer text,
progress value and visibility, retaining the purchase/activation/timing/expiry
checks. An optional overlay, if present, must also have the correct visibility.
No production UI behavior was changed to accommodate the probe.

The first template inspection established that Python editor properties do not
expose `WidgetTree` here. The supported `unreal.find_object(bp, "WidgetTree")`
lookup plus a scoped `ObjectIterator` inventory succeeded; both attempts remain
in the evidence directory. Do not repeat the unsupported property lookup.

The first preview runner referenced a nonexistent script after a copied path was
renamed. No script ran and no asset changed. Correcting it to the shared scoped
authoring helper made preview pass.

The first cold readback validated the asset, but Windows PowerShell did not retain
the process exit code. That record remains insufficient for a commandlet pass.
The runner now retains the native process handle; cold attempt two passed with
exit zero. A separate parent-shell exit check stopped after the successful save;
the save was not repeated.

## All-kit activation and cooldown acceptance

The September 22 all-kit source audit found that Korryn's Flock overwrote `MaxFlySpeed`
without restoring it. The ability now saves and restores that value alongside walk speed
and gravity. `Breakaway.Abilities.Korryn.FlockLifecycle` passed four actual activations
of the production ability-set entry: two cancellations and two timer-driven completions.
Each case starts with different movement values. The test checks restored fly/walk speed,
gravity, orientation, falling/custom movement mode, pawn collision, and released immunity
and ethereal tags. Reusing the same granted instance checks that later activations capture
their own baseline. Natural completions took 1.509 and 1.517 seconds for the loaded 1.5-second duration.

Evidence is in `Saved/Logs/all-kit-cooldown-20260922/flock-lifecycle-r2.json` and its
report/log; the passing editor compile is `compile-editor-flock-r2.json` in the same directory.
Both affected runtime DLLs were relinked before the run. The test had zero errors or warnings
and exited normally with code 0. Startup asset warnings remain in the raw log; this result
does not classify them as resolved. The first test fixture omitted the required GameState
and asserted before activation. Its failed record is retained as `flock-lifecycle.json`.
Cost so far: two successful `LyraEditor` builds, two headless runs, zero packages.

This proves isolated authority lifecycle cleanup. The fixture advances its private world's
timers and clears cooldowns only between cases. It does not prove natural cooldown expiry,
movement simulation, prediction, HUD behavior, or multiplayer convergence. Candidate22-14 now
contains this repair and adds the packaged owner/authority restoration evidence below.
Do not repeat the passing headless test
without a relevant Flock, GAS lifecycle, kit-config, or fixture change.

The all-four-upgrade lifetime and rematch extensions now have scoped candidate22-01/03
evidence; the alternative leech loadout has candidate22-05 evidence. See the current ledger
and linked lifetime/rematch reports before repeating those gates. Candidate22-08 also verifies
the four current primaries and candidate22-11 verifies replicated leech limits.

The all-16 native fixture is implemented in `BwayAllKitCooldownProbe`. It uses production
mixed-roster selection, purchased cooldown reduction, actual granted owner activations,
two natural cooldown cycles per ability, owner HUD samples, and independent authority checks.
It validates a clear movement lane and targets, and restores fixture-owned health, maximum
health, wallets, upgrades, transforms, view, and base healing. Controlled health prevents
this cooldown gate from becoming an accidental death test; exact combat effects remain open.

Candidate `LAN-20260922-12` compiled and packaged, then stopped before activation because the
spawn platform did not provide a clear dash route. Candidate `LAN-20260922-13` added a bounded
arena-lane search with floor, target, obstacle, and relic-clearance guards. Both packages
passed all 131 artifact hashes. Candidate13 reached 20 actual owner activations and 40 HUD
samples in four rendered D3D12 processes at 200 ms. Nineteen authority cooldown cycles
completed: all eight Argus cycles, all eight Rawlins cycles, both Radiance cycles, and the
first Blessing cycle. These are partial observations, not an accepted all-16 run.

The second Blessing cycle failed with `authority-expiry-or-cleanup` on the host and
`owner-expiry-timing` on Alona's client. The combined authority condition did not identify
whether timing, an active spec, or a lingering zone caused that branch. No zone defect is
established yet. The owner timing failure and slowing log frame progression justify measuring
frame gaps, but do not prove a resource bottleneck. All owned processes have exited.
Evidence remains in `Saved/Logs/all-kit-cooldown-20260922/runtime13-brief.json`,
`packaged-LAN-20260922-13-all-kit-cooldown-result.json`, and the four raw logs referenced there.
The frozen candidate12/13 contracts preserve their exact failed scopes.

The diagnostic game build, `compile-game-expiry.json`, adds GAS removal-cause callbacks, exact
expiry/cleanup diagnostics, and maximum observed tick gaps. It checks the owner kit config
once at activation instead of repeatedly loading a soft reference during HUD observation.
Authority and owner timing tolerances remain 0.75 and 1.5 seconds. The runner records
owned process CPU/memory samples and attempts a GPU-memory snapshot on failure.

Candidate `LAN-20260922-14` passed the complete gate on September 22, 2026 (local time).
The four rendered D3D12 processes each observed 200 ms packet lag. Production selection
assigned Argus, Rawlins, Alona, and Korryn. The fixture purchased the loaded rank-one
cooldown upgrade and activated each of the 16 actual granted abilities twice.

- All 32 authority cooldown durations matched the loaded base reduced once by 5%.
- All 32 removal callbacks reported natural expiry without prediction rejection.
- All 64 owner HUD samples matched countdown/progress state; reuse during cooldown was rejected.
- All 512 peer rows agreed on team, controlled health, and replicated upgrade state.
- Flock restored its saved movement properties on the remote owner and authority.
- Specs ended, and no checked zone/projectile remained at expiry. Cleanup restored all four
  players and both base-healing rates; all peers observed fixture teardown.
- All three clients exited normally with code 0. The host stayed alive until those exits,
  then the harness terminated it as recorded cleanup, not as a normal game exit.

The runtime brief reports zero errors in all four logs. Warning counts are 163/43/44/43;
these remain available for separate presentation/performance diagnosis. All 131 staged
file hashes match; compiled inputs are unchanged. Executable SHA-256:
`B6FB2327B036CC2F8D9ACFAEF00E0888276A1DA494D06AEC65DA01E80BD6C868`.
The snapshot differs after documentation/evidence-helper edits; artifact verification
independently confirms the frozen build-input scope.

Evidence under `Saved/Logs/all-kit-cooldown-20260922/`:
`runtime14-brief.json`, `packaged-LAN-20260922-14-all-kit-cooldown-result.json`,
`candidate22-14-artifact-verification.json`, and
`verifier-controls-LAN-20260922-14-r2.json`. All 28 copied-evidence controls reject
invalid evidence. The first control run removed only one of two valid renderer markers
and therefore left valid renderer evidence. The corrected control removes both; the
original control record is retained. No runtime rebuild or rerun was needed.

Candidate13's late Blessing failure did not reproduce. This does not establish its root
cause or a production cooldown timing fix. No timing tolerance was relaxed.
The result does not prove exact damage/healing/buff outcomes, interruption behavior,
movement trajectories, physical input, visual quality, final-alpha tuning, or complete
frontend/second-PC LAN. Those gates remain open.

Current all-kit gate cost: five successful target builds (two editor and three game), two
headless attempts for the Flock regression, three packages, and three four-process attempts.
Each retry changed a measured setup or observation hypothesis; no successful package was
reused for changed C++ inputs.

Other-kit network activation, avatar/round/match boundaries, physical controls,
visual inspection and final-alpha tuning remain unproved by candidate18.

Keep physical shop controls, second-PC LAN, full frontend flow, final tuning and
the candidate14 D3D12 startup crash as open gates. Candidate16's frozen movement
result remains valid only for its recorded artifact. Changes to this attribute,
helper, effects, catalog, kit cooldowns, or lifecycle invalidate the affected
cooldown evidence.

Overhead through candidate18 completion: six successful builds, one successful economy
run, eight asset/reference commandlets (one bad script path, one missing exit-code
record and one unsupported property lookup), two packages, two multiplayer
cooldown attempts and zero GUI editor launches. The final candidate18 probe change
was compiled for the game target; the preceding editor build has the original
probe predicate, while the production cooldown implementation is unchanged.
