# Life-leech implementation and verification

Gate: `Saved/Logs/life-leech-20260922/gate-contract.json`.
Candidate `LAN-20260922-04` passes the packaged four-player owner-purchase and fractional-healing
gate at 200 ms latency. Candidate `LAN-20260922-05` passes the alternative four-item loadout
through actual death/respawn, later rounds, production rematch, normal selection, and the next
playable match. The source implementation, catalog save/cold validation, and affected economy
automation also pass. All-kit attack attribution and final-alpha eligibility remain open.

## Implemented behavior

`UBwayHeroAttributeSet::LifeLeech` is a replicated fraction, bounded to finite values from zero
through one. `UBwayUpgradeEffect_LifeLeech` adds a reversible infinite modifier. The existing
purchase component replaces prior ranks and removes the effect on reset; the persistent
PlayerState ASC retains it across avatar replacement.

`UBwayLifeLeechComponent` is a default PlayerState component. Only authority registers its
damage listener. It consumes the float damage message emitted by `ULyraHealthSet` after health
has been clamped. Requested damage, overkill, and rounded UI numbers do not determine healing.
The listener requires an attributed living player, a valid pawn avatar, and a different player
on a known opposing team. Self, friendly, unassigned-team, and non-player targets are excluded.
Dead attackers, death transitions, invalid values, and full health cannot produce healing.

The native instant healing effect uses the normal Healing meta-attribute. This preserves health
caps, actual-healing messages, and existing feedback/stat consumers. It does not write Health
directly. Component teardown unregisters the listener, and a reentrancy guard prevents nested
damage/heal feedback from recursively granting life leech.

The catalog now offers five choices with the existing four-item limit. Life leech uses provisional
rank fractions of 5%, 10%, 15%, and 20%, and the existing 75/125/175/225 price curve. The scoped
Unreal authoring operation preserved all prior entries and the equipment limit. Each higher rank
replaces the previous fraction. A fifth available choice does not add a fifth equipped slot.

These values and the current eligibility policy do not establish final-alpha parity. The dated
hands-on reference establishes the existence of life-leech equipment, not exact prices, rates,
eligible attacks, or buildable interactions. See `AI_Planning/FINAL_ALPHA_PARITY.md` and the gate's
`source-reference-audit.json`. Do not claim those reference gaps are resolved by implementation.

## Verified scope

The focused `Breakaway.Economy.LifeLeech` test applies real damage and healing Gameplay Effects
through the production message listener. It verifies armor-adjusted fractional healing, one
healing event per damage application, overkill and already-dead targets, damage immunity,
zero/negative damage, self/friendly/unassigned targets, dead attackers, death tags, maximum-health
caps, non-player event targets, invalid values, rank replacement/debits, avatar replacement,
alternate four-item loadouts, fifth-item rejection, effect reset, and listener teardown.

The full `Breakaway.Economy` suite passes 6/6 with zero test errors or warnings, including existing
gold, purchase, movement, and cooldown coverage. This is headless automation, not multiplayer,
physical input, or visual acceptance. Catalog preview, apply, and a separate cold process each
validated one asset with zero validation warnings; the cold process made no asset mutation.

Evidence is in the gate directory:

- `compile-editor-fix.json`: LyraEditor passed in 14.07 seconds and the runtime DLL advanced.
- `automation-life-leech-r1.json`: focused test passed, framework marker and process exit zero.
- `automation-economy-r1.json`: 6/6 passed, framework marker and process exit zero.
- `catalog-{preview,apply,cold}-r1.json` and corresponding `*-run.json`: exact before/after data,
  prices, effect class, slot count, validation, mutation count, and process results.

The first compile failed on a protected team-initialization call in the test and three pre-existing
UE_MCP_Bridge JSON-key conversions. The test now initializes only its reflected team fixture,
then uses production team assignment. Two FoliageHandlers conversions and one LevelHandlers
conversion explicitly construct FString from UE 5.8 shared keys, matching BF-031/BF-046.
The raw failed build is preserved as `compile-editor.json` and `compile-editor.log`.

## Packaged four-player evidence

One rendered listen host and three rendered clients bought rank 1 through their own components.
All four public life-leech attributes settled to 0.05, while unrelated clients retained zero
private ranks and owned-item counts. The fixture disabled and later restored healing rates on
both base zones. It funded and later restored four wallets and health values and removed the
fixture's upgrades. The deliberate production round transition is part of this test's setup.

The remote attacker dealt 121.25 raw damage to an opposing player with 3 armor. Actual health loss
was 117.6125; the attacker's health rose from 200 to 205.8806, matching 5% of actual damage within
the 0.02 health tolerance. Baseline and settled health, teams, and public attributes matched on
all four peers. The 32 peer rows cover two four-player samples in each process. This invokes the
production damage path from the remote player's ASC on authority; it does not prove physical
attack input or every kit's attribution.

The harness observed all four processes alive after their pass markers, then captured three
normal client exits with code 0. The owned host was torn down separately. All four logs contain
zero runtime errors or critical lines. The staged executable matches the compiled game hash,
and all 131 staged artifact hashes passed verification.

Evidence in the gate directory includes:

- `compile-game.json`: initial game compile failed because an unbraced `if/else` surrounded a
  logging macro. `compile-game-braces.json` passed after adding braces (27.3 seconds, exit 0,
  empty failure signals).
- `candidate22-04-preflight.json`, `candidate22-04-changed-files-sha256.json`, and
  `source-snapshot-LAN-20260922-04.*`: frozen source and executable identity.
- `package-result.json`, `artifact-verification.json`, and
  `package-log-encoding-corrected.json`: Cook/Stage/Pak completion and artifact verification.
- `packaged-LAN-20260922-04-life-leech-result.json`, `candidate22-04-life-leech-brief.json`, and
  `candidate22-04-flow-inventory.json`: process results, purchases, independent damage/healing
  calculations, private-state checks, and all-peer health evidence.
- `verifier-checks.json`: 26 copied-evidence controls passed, including missing purchases,
  fractional healing loss, wrong health/teams/catalog data, privacy leaks, missing readiness,
  missing cleanup, missing latency/renderer evidence, and unobserved or failed client exits.

The first independent extraction incorrectly selected PowerShell's integer `Math.Max` overload,
rounding 117.6125 damage to 118. A double zero preserves fractional math. The original rejection
is retained in `candidate22-04-life-leech-brief-before-verifier-fix.json`; the corrected checker
and all controls passed against the same run. No game rebuild or runtime retry was needed.

Incremental log inspection also needed a fix: a reader resumed in the middle of a UTF-16 build
log without retaining its encoding. `Get-LogDeltaBrief.ps1` now detects the BOM before seeking
and checks newline boundaries using that encoding. Eight append/unchanged controls across
UTF-8, UTF-16 LE/BE, and UTF-32 passed (`log-encoding-controls/checks.json`). A complete corrected
package scan confirms Cook/Stage/Build success and zero errors or critical lines.

## Alternate-loadout lifetime

Candidate `LAN-20260922-05` adds development-only lifetime/rematch variants for attack strength,
armor, movement, and life leech. The alternative replaces cooldown reduction; it never equips
five items. Public observations include both life leech and the unequipped cooldown attribute.
The existing default commands and extraction options retain their prior loadout.

The four-process lifetime gate passes 16 exact authority purchases and 80 checkpoint rows at
200 ms. Its selected remote player actually dies, respawns into a different pawn, and enters a
later production round. The same PlayerState/ASC retains all four modifiers; private ranks remain
owner-only, and the unequipped cooldown modifier remains at baseline. All four processes were
alive at completion, all three clients exited normally with code 0, and the logs have zero errors.

`compile-game-lifecycle.json` passed in 34.34 seconds with no failure signatures.
`package22-05-result.json` and `candidate22-05-artifact-verification.json` establish the new
candidate, including 131 matching file hashes and a matching compiled/staged executable.
`candidate22-05-lifetime-brief.json` and `packaged-LAN-20260922-05-life-leech-lifetime-result.json`
preserve the gameplay/process evidence. `lifetime-verifier-checks.json` records nine passing
controls, including missing leech after respawn, baseline residue, privacy, unequipped cooldown,
fifth-item ownership, catalog mismatch, live checkpoint, and failed client exit. The updated
extractors also still accept the saved older default-loadout lifetime and rematch gates.

## Alternate-loadout rematch

The same candidate22-05 artifact passes a separate four-process rematch gate at 200 ms. It
purchases the same four items for each player, seeds nonzero match stats, completes a match,
and calls production rematch. A fresh destination clears owned ranks, wallets, and match stats;
normal hero-selection timeout leads to a playable next match. All five public upgrade attributes
match their unmodified bases after reset. The client snapshots and authority agree across 32
attribute rows. Four completed fixtures were simultaneously live before cleanup; all three
clients then exited normally with code 0. No runtime errors or critical lines were found.

`candidate22-05-rematch-brief.json` and
`packaged-LAN-20260922-05-life-leech-rematch-result.json` retain the complete evidence.
`rematch-verifier-checks.json` records eight passing controls for missing purchased leech,
fractional residue after rematch, wrong ownership/catalog, an unequipped cooldown modifier,
missing live checkpoint, and failed client exit. `candidate22-05-flow-inventory.json` inventories
the eight completed lifecycle logs. The final artifact recheck still matches all 131 hashes.

The source-only attack attribution audit is saved as `all-kit-source-attribution-audit.json`.
It identifies the shared native melee/ability path and three ranged primary projectile families.
Runtime activation and final-alpha eligibility remain separate requirements. A further reference
search did not recover exact final-alpha tuning (`final-alpha-reference-followup.json`); use
known dated shop footage or archived developer item/patch records for the next reference check.

Overhead so far: two LyraEditor builds, three LyraGame builds, two headless automation processes,
three asset commandlets, zero interactive editor launches, two packages, and three multiplayer
attempts (purchase/healing, lifetime, and rematch all completed). Both compile retries
addressed recorded compiler failures; candidate22-05 is a deliberate lifecycle fixture extension,
not a retry of candidate22-04.

## Mixed-roster primary gate and Rawlins burst repair

The gate in `Saved/Logs/all-kit-leech-20260922/` selects Argus, Rawlins, Alona, and Korryn
through production hero selection, then activates each player's actual granted primary. It
uses one rendered listen host and three rendered owning clients at 200 ms. Damage expectations
come from the loaded kit, attack strength, armor, and incoming-damage multiplier. The gate
requires exact hit/heal counts and attribution, fractional health convergence, private upgrade
ownership, and owned fixture cleanup. This covers primaries only, not every ability.

Candidate22-06 was packaged but deliberately not launched: source review caught the fixture
holding pawn snapshots across production round replacement. Candidate22-07 snapshots the live
post-round avatars and passed production mixed selection plus Argus's real melee hit. It then
failed on remote Rawlins: one 18.43 damage event and one 0.9215 heal instead of two of each.
The authority cleaned up all four players, two bases, and both listeners before reporting failure.
See `runtime07-brief.json` and `gate-contract-candidate22-07.json` in that directory.

Rawlins immediately replicated `EndAbility` from its predicted client. GAS permits that request
to end the authority instance, whose cleanup clears the timer for the second shot. The source
repair keeps the predicted instance active until the server ends the completed burst; normal
GAS cancellation remains available. Candidate22-08 also requires inactive primary specs on
authority and owner before accepting each result. Its LyraGame compile passed in 37.43 seconds
with no failure signals. Candidate22-08 now passes the four-process rendered gate at 200 ms:

| Primary | Damage events | Actual damage | Actual healing |
|---|---:|---:|---:|
| Argus | 1 | 30.00 | 1.500 |
| Rawlins | 2 | 36.86 | 1.843 |
| Alona | 1 | 38.00 | 1.900 |
| Korryn | 1 | 32.98 | 1.649 |

All four owning activations, seven authority/owner termination checks, and 128 peer rows passed.
Cleanup restored four players and two bases and removed both listeners. The harness recorded
all four processes alive after their fixture passes, three normal client exits with code 0, and
separate owned host teardown. The four logs have zero runtime errors. All 131 staged files match
the frozen manifest; the game executable hash is
`378582768F31400B52AA67FAFB392EE362B4962ACE70C18F979E9EEB969D4C35`.

`runtime08-brief.json`, `packaged-LAN-20260922-08-all-kit-leech-result.json`, and
`candidate22-08-artifact-verification.json` retain the full results. The 19 cases in
`verifier-checks.json` include the positive baseline and rejected missing/wrong damage,
healing, ownership, termination, cleanup, rendering, catalog, live checkpoint, and exit evidence.
This gate used three LyraGame builds, three packages, and two multiplayer attempts. Candidate06
was withheld after the avatar-lifetime review; candidate07 exposed the gameplay defect;
candidate08 verifies its repair. No editor or whole-solution build was needed for this gate.

The nearby timer-based Slide Shot and Radiance implementations do not use that immediate
client-end path. This is a source check, not runtime acceptance of those abilities. The diagnosis,
source trail, and prevention lesson are in `rawlins-burst-diagnosis.json`.

## Replicated limits and exclusions — candidate22-11 passed

`Saved/Logs/life-leech-limits-20260922/gate-contract.json` owns the next gate. Its development
actor publishes a complete serialized case frame while observers read actual replicated health.
Seven cases cover fractional health caps, full health, immunity, friendly/self damage, overkill
and a second hit after death, and delayed native damage attributed to a dead attacker. Natural
replacement pawns and restored fixture state are required before acceptance. The test extends
respawn delay to 20 seconds for observation and restores the original value; it does not prove
the lifetime of an actual in-flight projectile.

Candidate22-09 packaged successfully but failed before the first case: the match phase enum
became Playing before the deferred GAS transition cleared damage immunity. The fixture now
waits for that production protection to end. Candidate22-10 was packaged but withheld after
source review identified that health-component teardown clears death tags before respawn.
Candidate22-11 permits zero health with either a death tag or absent pawn and still requires
actual lethal damage and replacement-pawn evidence. The five LyraGame builds include one
corrected `DOREPLIFETIME` output-parameter
error, a complete-frame serializer addition, the readiness repair, and the death-observer repair.

Candidate22-11 passes all seven cases in four rendered processes at 200 ms. A remote player's
0.05 leech modifier heals exactly 0.125 missing health from 117.6125 damage. Full-health,
immunity, friendly, self, and dead-attacker cases emit no forbidden healing. Overkill removes
exactly three health and heals 0.15; the second postmortem hit adds neither damage nor healing.
Delayed native damage from the dead source removes 117.6125 target health while leaving the
source at zero. This uses persistent PlayerState attribution through the production damage
path, not a spawned in-flight projectile.

All four peers completed 16 observation frames (256 rows), agreed on health and private rank
visibility, and observed two different living replacement pawns after the real respawn timers.
Cleanup restored four health/wallet snapshots, upgrades, two base-healing rates, the original
three-second respawn delay, and both listeners. The probe was destroyed on all four peers.
The harness recorded four completed fixtures simultaneously alive, three normal client exits
with code 0, separate owned host teardown, and zero runtime errors. All 131 staged hashes match;
the executable hash is `E42C74B38667EF3C6744CAA9DA843411B194E8F89417CE9F053D047AC5E45C4D`.

`runtime11-brief.json`, `packaged-LAN-20260922-11-leech-limits-result.json`,
`candidate22-11-artifact-verification.json`, and `verifier-checks.json` preserve the results.
The checker passed 20 controls, including rejected missing frames, wrong health/healing,
attribution errors, missing second-hit evidence, forbidden healing, incomplete restoration,
missing/false death or respawn proof, teardown, renderer, live-checkpoint, exit, and catalog
errors. Gate overhead: five LyraGame builds, three packages (candidate10 deliberately not run),
and two multiplayer attempts. Candidate09's pre-case failure remains recorded.

## Remaining acceptance

Candidate22-08 establishes the four current primaries' activation, collision/damage attribution,
and leech convergence; candidate22-11 adds the current replicated limits/exclusions. Other
abilities, actual projectile lifetime after owner death, and the complete final-alpha roster
remain unverified. Next, extend real network activation and cooldown/HUD/expiry/reactivation
coverage beyond Argus's For Glory to the remaining configured hero abilities. Preserve all
accepted artifacts for their frozen scopes. Physical shop controls, visual feedback,
final-alpha rates/eligibility, and complete frontend/second-PC LAN remain open.

Invalidate affected evidence after changes to life-leech eligibility, health/message semantics,
attribute/effect behavior, purchase/reset code, catalog data, fixtures, or artifact identity.
