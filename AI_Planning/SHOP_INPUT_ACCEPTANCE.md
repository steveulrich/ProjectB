# Shop input integration

Gate: `Saved/Logs/shop-input-20260920/gate-contract.json`.
This is an active gate, not completed physical input or LAN acceptance.

## Intended behavior

Open the local shop with B or gamepad D-pad Up. Keep View for the scoreboard.
Hold a button without repeated opens; use the keyboard toggle or CommonUI Back to
close. The selector receives initial focus, purchase submission remains on the
owning upgrade component, and disabling Buy must preserve usable focus. Closing,
phase changes, and controller teardown must restore the underlying input state.

`IA_BW_UpgradeShop` is a Boolean action bound once on the player controller's
Enhanced Input component with `Started`. `IMC_BW_PlayerUI` is installed by the
existing shared input feature at priority 2 and reinstalled by its existing
`BindInputsNow` path after pawn input rebuilds. It intentionally overrides only
the inherited B emote mapping. No combat mappings are changed. CommonUI retains
the project's existing Back configuration; the shop resolves its toggle from
the local player's active Enhanced Input keys instead of hard-coding them.

## Evidence and attempts

- `input-inventory.json`: authored Breakaway/default context inventory.
- `input-authored-r2.json`: final B/D-pad Up context and shared-feature registration;
  the original default context retains its nine mappings.
- `compile-editor.json`: failed link because the development input-device fixture
  needed ApplicationCore. The module dependency was added; `compile-editor-r2.json`
  passed with no failure signatures.
- `runtime-input-r1.json`: the first fixture stopped before input when its mapping
  assertion failed. `runtime-mapping-diagnosis.json` and the native mapping dump in
  `editor-runtime.log` show inherited ShooterCore consuming B for emote and View
  for scoreboard. The corrected context uses D-pad Up and explicit priority 2.
- Native `bway.Test.ShopInputRoutes` checks mapped keys, exactly one binding,
  held-button behavior, one local modal, actual selector focus, menu input blocking,
  mapped-key close, CommonUI Back, and prior input-state restoration. It enumerates
  local controllers in the live game worlds and owns cleanup and a 45-second timeout.
  These are synthetic engine/widget events, not physical device evidence.

The corrected mappings were present on all peers. Native diagnostics then proved
B was held, its action value was true, and exactly one shop opened. The host's
unpainted same-process viewport retained the router's fallback `All` mode; only
the visible viewport applied a menu configuration. This does not pass focus or
input blocking. Do not rerun that unchanged viewport topology for these assertions.

The same investigation found the match HUD's `Default` input setting inheriting
`Menu` during Playing. The intermediate `Game` setting was corrected to
`GameAndMenu` before final acceptance: `ULyraHUDLayout` registers its pause action
in Menu mode, and CommonUI's binding filter requires `All` to admit both gameplay
and those HUD actions. `hud-input-final.json` and the successful cold asset
commandlet record the saved final value. The shop requests `Menu` separately.

Both current targets compiled (`compile-editor-r5.json`, `compile-game.json`).
Candidate08 cooked and staged in 95.93 seconds with zero package errors;
`package08-result.json` records its manifest and executable hash. Its native probe
waits for Playing and the local input mapping before dispatching startup keys.
The next check uses four rendered packaged processes and preserves the full
focus/input assertions. `Get-ShopInputBrief.ps1` extracts actual native markers,
expected listen/client net modes, client exits, and all runtime error lines.

Candidate08 confirmed one B-opened shop, actual selector focus, and Menu mode on
all four packaged processes. It then failed the movement/look blocking assertion
on all four: the shop's input configuration left both ignore flags false. The
screen now explicitly requests both flags, allowing CommonUI to balance them on
close. All candidate08 processes were closed after terminal failures; its brief
retains four fixture errors and no acceptance pass.

Both modal-fix targets passed. Candidate09 packaged successfully in 63.78 seconds,
but was superseded before runtime testing by the HUD's GameAndMenu correction.
Candidate10 contains the final asset setting and the same compiled modal fix.
No new C++ build was needed for that asset-only correction. Earlier candidates
remain frozen. Current run/result files in the gate directory determine whether
the candidate10 runtime gate has passed.

Candidate10 packaged successfully (79.63 seconds) but did not pass runtime.
Its first run passed three peers and failed client2 after the selector initially
gained focus. The staggered `shop-input-r2` run passed the host but failed client1;
client2/client3 were never launched. Both runs are terminal and all owned processes
were closed. This rejects the hypothesis that overlapping window launches alone
caused the failure. The second client log records Menu and selector focus at
04:05:52.132 UTC, followed by forced gameplay capture at 04:05:52.313 with no
CommonUI input-config transition, then focus and both input locks missing.

Source diagnosis: `ULyraHeroComponent::HandleChangeInitState` unconditionally
applied GameOnly after binding pawn input, overriding an already open shop.
`APlayerController::ClientRestart_Implementation` also clears counted input locks.
CommonUI's forced config refresh changes capture but only adjusts those lock
counts when the desired ignore flags change, so refresh alone cannot repair a
restart. Candidate11 makes hero/controller restoration defer to the active
CommonUI leaf and reapplies that leaf's explicit ignore flags once after each
controller reset. It retains the gameplay fallback when no UI leaf owns input.
No arbitrary startup delay is added. The native fixture now repeats the B-open
assertions after the real same-pawn client restart plus a second flag reset, then
requires fully restored prior input on close. This does not prove replacement-pawn
respawn, physical input, or purchase navigation.

Candidate11 passed the strengthened native input scope on all four rendered
processes at 200 ms latency. The first run completed every assertion with no
runtime errors, but its hidden-window `CloseMainWindow` cleanup failed. The second
run reused the exact artifact with engine `-seconds=60` client exits: all three
clients exited 0, all four native fixtures passed, and all four logs had zero
runtime errors. The host stayed alive until client completion and was terminated
by owned cleanup; its exit -1 is not a graceful host-exit pass. No ProjectB process
remained at the final inventory. `candidate11-r2-shop-input-brief.json` and
`candidate11-final-processes.json` record the result. `verifier-checks.json`
accepted the real record and rejected five mutated copies (missing restart,
lost focus, added runtime error, missing client, and nonzero client exit).

The remote log confirms the natural possession retry occurred while the shop was
open, followed by restored UI locks and passing focus checks. This supplies runtime
evidence for the restart diagnosis, beyond the deliberate fixture restart.
This iteration used two builds (33.17/46.19 seconds), one package (95.59 seconds),
and two runs; the second run changed cleanup only. Gate totals: ten builds, four
packages, ten runtime attempts, four editor launches and one asset commandlet.

## Next navigation gate

Source review found that the shop preview handler consumed D-pad Up as a closing
toggle, preventing that same key from navigating upward in the selector. Candidate12
reserves gamepad directions for UI navigation while the shop is open; gamepad Back
closes the shop. The native fixture now routes Accept, Down, Accept, Accept, Up,
Accept through Slate and requires the resulting selection and focus each time.
Use `Get-ShopInputBrief.ps1 -RequireNavigation` for this stricter gate. Its contract
is `Saved/Logs/shop-navigation-20260920/gate-contract.json`. Candidate11's pass
remains frozen to opening/closing/restart behavior and does not claim navigation.

Candidate12 compiled both targets and packaged (95.00 seconds), then stopped on
host step 5: gamepad Accept did not open the selector. No clients were launched.
The log shows CommonInput switching from mouse/keyboard to gamepad only on Accept.
The fixture's controller-level D-pad injection had bypassed the Slate input
preprocessor, which normally updates input type before dispatching a physical
gamepad press. Candidate13 mirrors that input-method update before injected keys,
restores the original method on cleanup, and logs handled input, focused widget,
selector-open state and selected index. Production navigation behavior is unchanged
from candidate12. One linker failure exposed the new fixture's missing CommonInput
module dependency; the direct dependency was added and the editor rebuild passed.
No default CommonUI cursor policy or assertion is disabled for this retry.

Candidate13 passed the navigation gate on the listen host and all three clients
at 200 ms latency. Each process opened the selector with gamepad Accept, committed
the second option with Down/Accept, returned to the first with Up/Accept, and
regained selector focus. The shop stayed open throughout; keyboard close/CommonUI
Back and the prior restart/input-lock assertions also passed. The corrected input
method setup resolves candidate12's test failure without changing cursor policy.

`Saved/Logs/shop-navigation-20260920/candidate13-shop-navigation-brief.json`
records four native passes, four navigation passes and zero runtime error lines.
All three clients exited 0 through the engine time limit; owned host teardown is
separate (exit -1). `final-processes.json` confirms no remaining ProjectB process.
`verifier-checks.json` accepted the real result and rejected six mutated copies,
including missing navigation evidence. The stricter verifier also rejects
candidate11's otherwise valid opening/restart record as navigation proof.

Navigation-gate overhead: five build attempts (one missing CommonInput dependency,
then corrected), two successful packages (95.00/97.93 seconds), two runtime
attempts, no editor launch. Package/source snapshots and both failed and accepted
runtime records remain in the gate directory. Reuse candidate13 only for its frozen
code, assets and configuration. Runtime/UI/input changes require affected retesting;
documentation and extractor changes alone do not require a new package.

The audit editor and first runtime editor closed normally. Runtime startup errors
and failed reflection probes remain in the raw log; Pixel Streaming errors remain
actionable for rendered/UI acceptance. Native reflected types are not uniformly
available as Python module attributes: use verified class paths and function
names. The bridge's `get_runtime_value` requests an unavailable `actorPath` field;
do not repeat its unchanged call. Native `getall` can inspect protected reflected
properties without retaining Python actor wrappers; array entries follow the
header on separate log lines.

## Purchase navigation and pending focus: candidate14

Candidate `LAN-20260920-14` passed the purchase-focus gate in four rendered packaged
D3D12 processes at 200 ms latency. `bway.Test.ShopPurchaseFocus` navigates from the
selector to Buy and activates the actual Slate button with gamepad Accept. It
requires disabled Buy and usable selector focus immediately after submission,
observes remote rank 0 while the request is pending, then requires replicated rank
1 and a catalog-priced wallet delta. Gamepad Back closes the shop and restores
input. The authority independently observed exactly one 75-gold AttackStrength
debit for each of the four players. Passive income is bounded separately from the
exact debit assertion.

The development-only fixture funds the loaded catalog's first rank on the authority
in a disposable match. It adds no purchase RPC or shipping privilege. It removes
gold delegates, releases injected keys, restores input method, and deactivates its
widget on cleanup; process teardown discards the funded match.

The first attempt is retained as a failure: client1 exited 3 after seven seconds
with an access violation reading address 0x9c. Symbolization places the game frames
in `CreatePipelineStateFromStream` and D3D12 graphics pipeline-state creation,
below NVIDIA driver frames. The host and two other clients completed purchases;
the host then timed out waiting for the fourth debit. This does not establish a
driver root cause. The crash remains an unresolved rendered-startup stability risk.

Attempt r2 changed startup timing only: each client reached a non-null pawn restart
before the next client launched. This readiness marker is independent of purchase
funding, which requires all four players. All four then passed together, all three
clients exited 0, and all four logs had zero runtime errors. Owned host teardown
is separate (exit -1). No process remained after cleanup. This passing retry does
not prove that concurrent cold startup is reliable or that the earlier crash is fixed.

Evidence is under `Saved/Logs/shop-purchase-focus-20260920/`:

- `candidate14-r2-shop-purchase-brief.json`: accepted four-peer scope.
- `packaged-LAN-20260920-14-shop-purchase-focus-result.json`: failed first attempt.
- `crash-symbolized.txt`: game-frame symbols from the frozen executable/PDB.
- `r2-preflight.json`: matching fixture/widget sources, all 131 staged hashes,
  empty process inventory, and the changed startup hypothesis.
- `verifier-checks.json`: real evidence accepted; ten mutated copies rejected,
  including missing pending/Back/debit markers and incorrect wallet/debit arithmetic.
- `final-processes.json`: cleanup inventory.

Use `Get-ShopInputBrief.ps1 -RequirePurchase` for this scope. It also requires the
earlier navigation and restart assertions. It rejects candidate13's navigation-only
evidence. Overhead: two successful builds, one 94.20-second package, two runtime
attempts, and no editor launch. The retry reused the exact frozen package.

## Replacement-pawn shop recovery: candidate15

Candidate `LAN-20260920-15` passed `bway.Test.ShopRespawnFocus` in four rendered
packaged D3D12 processes at 200 ms latency. It repeats the purchase/navigation
assertions, then applies lethal production damage to one player at a time. This
avoids a team wipe or round transition replacing the intended respawn lifecycle.
Every local player observed the old pawn dying with the shop open, then possessed
a different living pawn with an exact death-count increment. The same modal,
selector focus, move/look locks, purchased rank, and single shop input binding
survived. Gamepad Accept/Down/Accept still changed selection afterward; Back closed
the shop and the original gameplay input state returned.

All three clients exited 0 through the engine deadline. The four logs contained
zero runtime error/critical lines, and cleanup left no ProjectB process. Owned
host teardown is recorded separately (exit -1). The fixture uses weak actor/widget
references, removes its attribute delegates and injected input on cleanup, and
discards its funded disposable match on process exit. This added development-only
coverage; no further production input repair was necessary for the observed run.

Evidence is under `Saved/Logs/shop-respawn-focus-20260920/`:

- `candidate15-shop-respawn-brief.json`: strict four-peer purchase/respawn result.
- `packaged-LAN-20260920-15-shop-respawn-focus-result.json`: launch arguments,
  independent startup readiness, four-live checkpoint, exits and owned teardown.
- `verifier-checks.json`: accepted the real result and rejected sixteen mutations,
  including absent death, unchanged pawn, wrong death count, duplicate binding,
  missing post-respawn navigation and missing authority respawns.
- `runtime-preflight.json`, `package15-result.json`, source snapshot/copy and
  `final-processes.json`: provenance, build identity and cleanup.

`Get-ShopInputBrief.ps1 -RequireRespawn` requires this scope and the prior purchase
gate. It rejects candidate14's otherwise valid purchase result. Overhead: two
successful builds (15.86/35.19 seconds), one 101.29-second package, one runtime
attempt, and no editor launch. Reuse only for the recorded code/assets/configuration;
changes to pawn, input, widget, economy or lifecycle behavior invalidate affected proof.

## Remaining proof

Physical keyboard/gamepad flow, rendered prompts, full frontend LAN/second-PC
acceptance, and the unresolved candidate14 PSO startup crash remain open. The next
economy work is the incomplete final-alpha upgrade catalog and missing gameplay
effects, not another unchanged synthetic shop run. See `FINAL_ALPHA_PARITY.md` and
`Saved/Logs/shop-respawn-focus-20260920/next-upgrade-parity-audit.json`.
These scoped shop gates do not prove the complete slice.
