# Gameplay Polish — Verification Checklist

Run after rebuilding C++ (editor closed / Live Coding off) and restarting the editor.

## Automation (Session Frontend)

Session Frontend → Automation → filter `Breakaway.CombatPolish`:

- [ ] `Breakaway.CombatPolish.SlideJump.LaunchVelocity`
- [ ] `Breakaway.CombatPolish.Nameplates.ScaleEvaluation`
- [ ] `Breakaway.CombatPolish.Cooldown.PercentMath`

## Editor asset wiring

1. Rebuild + restart editor (picks up `DefaultGame.ini` primary asset scans).
2. Run (optional MCP): `node Scripts/setup-gameplay-polish-combat-readability.mjs`
3. Confirm assets:
  - `/BreakawayCore/CombatFeedback/DA_BW_CombatReadability`
  - `/BreakawayCore/Movement/DA_BW_MovementFeel_Default`
4. On `B_BW_Experience_Dev` and `B_BW_Experience_CaptureTheRelic`:
  - Add action `Breakaway Combat Readability Config` → DA above
  - Dev experience: also enable nameplate manager/source + number pop (or add `LAS_ShooterGame_StandardComponents`) so nameplates exist to policy
5. On `W_BW_MatchAbilitySlot`:
  - Add optional BindWidgets: `Image_CooldownOverlay`, `ProgressBar_Cooldown` (fill from bottom), `Text_CooldownTimer` (hidden when ready)
6. Create GCN Blueprint parented to `GCN_BwayCombatFeedback` tagged `GameplayCue.Breakaway.Combat.HitConfirm`
7. Assign projectile presentation DAs on hero kit configs when art stubs are ready

## PIE matrices

### Combat numbers

- [ ] Take damage → red numbers on self
- [ ] Deal damage to enemy → white numbers on enemy
- [ ] Receive heal → green numbers on self
- [ ] Enemy heals → **no** green numbers for local viewer
- [ ] Projectile damage attributes to owner (stats + white numbers)
- [ ] Listen-server + dedicated: only owning clients see their numbers (no host duplicates)

### Nameplates

- [ ] Both teams visible within `MaxNameplateRange`
- [ ] Self hidden
- [ ] Occluded (LOS blocked) → hidden
- [ ] If nothing shows with LOS on: try `bRequireLineOfSight=false` — if plates appear, raise `NameplateLOSVerticalOffset` (feet/floor false negatives)
- [ ] Scale shrinks with distance
- [ ] No per-frame hitch at 8–10 pawns (trace budget)

### Cooldowns

- [x] Ability use → grey overlay + vertical fill + countdown text
- [x] Fill progresses `1 - Remaining/Duration`
- [x] Ready → overlay + bar + timer text hidden
- [x] Hero swap / relic mode / respawn rebinds correctly

### Slide-jump

- [ ] Standing jump: normal air control
- [ ] Slide → jump: farther horizontal travel, less steering
- [ ] `bway.Movement.DebugSlideJump 1` shows state
- [ ] 100ms / 200ms lag: no correction snaps
- [ ] Landing applies velocity factor only for slide-jumps

### Presentation / art

- [ ] Projectile shows stub mesh/VFX when presentation DA assigned
- [ ] Hero mesh transform from DA applies
- [ ] Follow [Hero_Art_Replacement_Guide.md](Hero_Art_Replacement_Guide.md) for a stub swap

## Performance spot checks

- [ ] Number-pop Niagara pooling does not grow unbounded
- [ ] Nameplate LOS ≤ `MaxNameplateLOSTracesPerTick`
- [ ] No duplicate NumberPop / NameplatePolicy components on CaptureTheRelic (PC already creates them in C++)