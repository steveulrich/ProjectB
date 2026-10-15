# Buildable Authority & Persistence Acceptance

## 1. Current Gate Verdict & Frozen Candidate Identity

- **Verdict:** **PASSED** (4-world PIE + 4-process packaged LAN placement, obstruction rejection/recovery, active-cap enforcement, 3-round persistence, and per-hero combat/healing/debuff/trap-cage mechanics at 200ms lag).
- **Frozen Candidates:**
  - `LAN-20260927-30`: Exe SHA-256 `8358D97EC14BAF8D3BC302A0970A0AB9985E2DB6EF3862DB5EE7500923E54D1E` (`Saved/Logs/buildable-authority-20260927/candidate30-shrine-ward-summary.json`, 131 artifacts).
  - `LAN-20260927-33`: Exe SHA-256 `DAD6EDB392512CD54DA67C52E57BEB5911F67626AE55CE442EFA9CDBA8A1394F` (`Saved/Logs/buildable-authority-20260927/candidate33-jail-immunity-summary.json`, 131 artifacts).

## 2. Production Architecture & Invariants

- **Shared Placement Contract (`FBwayPlacementQuery` & `UBwayGameplayAbility_PlaceBuildable`):**
  - Evaluates ground trace, surface slope (`<= MaxPlacementSlopeDegrees`), multi-corner support footprint, bottom-pivot lift (`CalculateActorBottomOffset`), and oriented-box obstruction (`PlacementQueryBox` vs pawns/structures/walls) identically on preview client and server authority.
  - Consumes `ABwayPlayerState::bHasPlacedBuildableThisRound` (replicated `COND_OwnerOnly`, reset each round) only after authority spawn succeeds; rejected placements keep the round slot available for immediate recovery.
- **Base Actor & Registry (`ABuildableActor` & `UBwayBuildableRegistryComponent`):**
  - `ABuildableActor` replicates `ReplicatedVisualMesh` (non-colliding cosmetic mesh), `PlacementBoundsCenter/Extent` (collision-enabled query box), `AttackCollision` (hit-target capsule), `TeamIndex`, and `BuildState`.
  - `UBwayBuildableRegistryComponent` persists surviving structures across rounds (`ResetForNewRound` preserves live actors) and enforces per-player active caps by evicting the oldest instance (`SiegeEngine = 1`, `SunShrine = 3`, `CursedWard = 3`, `Jail = 3`).
- **Hero Buildable Invariants:**
  - **Argus (`ABwaySiegeEngineBuildable`):** Fires catapult projectiles (`80` enemy damage, `150` structure damage).
  - **Alona (`ABwayAlonaSunShrineBuildable`):** Pulses `35 HP` ally heal (`600` radius) and grants `15` armor (`GE_BW_Alona_ShrineArmor`).
  - **Korryn (`ABwayKorrynCursedWardBuildable`):** Applies `20%` slow (`State.Status.Slowed`) + `10%` damage amplification (`State.Status.DamageAmplified`, `IncomingDamageMultiplier = 1.1`).
  - **Rawlins (`ABwayRawlinsJailBuildable` & `ABwayRawlinsJailCage`):** Armed floor trap is damage-immune on server and clients (`OnRep_BuildState` keeps `AttackCollision` `NoCollision`); triggering spawns an ownerless, cap-exempt `ABwayRawlinsJailCage` (`225 HP`, `3.5s` duration, `State.Status.Jailed`).

## 3. Verified Scope Summary Table

| Phase | Harness / Command | Result & Evidence |
|---|---|---|
| Automation Suite | `Breakaway.Buildable` (Placement, Caps, Persistence, Combat) | **PASS** — `Saved/Logs/buildable-authority-20260927/automation-buildable.json` |
| 4-Process Packaged Obstructed Recovery (200ms) | `Candidate29` (`LAN-20260927-29`) | **PASS** — 420 checks, 44 controls; rejected obstructed spawn preserves slot & recovers (`candidate29-summary.json`) |
| 4-Process Packaged 3-Round Shrine & Ward Caps (200ms) | `Candidate30` (`LAN-20260927-30`) | **PASS** — 541 checks, 20 controls each; 1->2->3 accumulation & cap=3 eviction (`candidate30-shrine-ward-summary.json`) |
| 4-Process Packaged Jail Immunity & 3-Round Accumulation (200ms) | `Candidate33` (`LAN-20260927-33`) | **PASS** — 747 checks, 29 controls; client trap immunity, 3-trap cap, cage exemption (`candidate33-jail-immunity-summary.json`) |

## 4. Remaining Open Proof Gaps

- Original September 2017 reference confirmation for per-structure cap numbers and tuning.
- Physical mouse/gamepad targeting confirmation, final 3D structure meshes, and second-PC LAN verification.

## 5. Archive Pointer

- Full chronological gate diary: [BUILDABLE_AUTHORITY_ACCEPTANCE.md](./Archive/Full_History_20261013/BUILDABLE_AUTHORITY_ACCEPTANCE.md).
- **Rule:** Do NOT read the archived file during routine gates; open it only if diagnosing a historical regression in this exact subsystem.
