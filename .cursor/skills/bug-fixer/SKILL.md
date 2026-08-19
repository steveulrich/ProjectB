---
name: bug-fixer
description: >-
  Fix compile errors and runtime bugs while documenting root cause and prevention.
  Use when fixing build failures, Blueprint wiring bugs, UMG BindWidget issues,
  linker errors, or any defect — then append a prevention note to this skill.
---

# Bug Fixer

When fixing a compile error or bug, **always** do both:

1. **Fix** the immediate issue (minimal correct diff).
2. **Document prevention** — append a row to [Prevention catalog](#prevention-catalog) below (or extend an existing row).

After fixing, tell the user:
- **Root cause** (one sentence)
- **Prevention** (what to do next time)
- Whether the skill catalog was updated

---

## Fix workflow

```
1. Reproduce / read the error (log, BP compiler, UHT, linker)
2. Identify root cause — not just the symptom
3. Apply minimal fix
4. Verify (ask user to recompile for C++; editor BP compile or PIE if runtime)
5. Append prevention entry to catalog below
6. Scan same pattern in nearby files (same module, same widget base, etc.)
```

---

## Prevention catalog

| ID | Symptom | Root cause | Prevention |
|----|---------|------------|------------|
| **BF-001** | `Text_Timer` (or any `BindWidget` member) missing in BP Event Graph variable picker; MCP `GetVar` fails; C++ `if (Text_Timer)` still works | `UPROPERTY(meta=(BindWidget))` without **`BlueprintReadOnly`** binds at runtime but is **not exposed to Blueprint**. Designer name match alone is insufficient. | **Always** declare widget bindings as:<br>`UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="…\|Widgets")`<br>Match C++ type to UMG widget class (`UCommonTextBlock`, `UCommonNumericTextBlock`, not generic `UTextBlock` when tree uses CommonUI). Follow `BwayPostRoundSummaryWidget` / `BwayPostMatchInterstitialWidget` pattern. After C++ change: recompile module, reopen BP, **Compile** BP. |
| **BF-002** | BindWidget property is `nullptr` at runtime despite correct name | Widget tree class ≠ C++ property type (e.g. `TextBlock` in C++, `CommonTextBlock` in UMG — usually OK if subclass; fails if unrelated type). Or name typo / widget not under this BP's tree. | Use exact widget type in C++ when using CommonUI/Lyra widgets. Verify in Designer: widget **Name** matches property name exactly (case-sensitive). Prefer `BindWidgetOptional` + null checks in C++. |
| **BF-003** | UBT fails: "Unable to build while Live Coding is active" | Editor has Live Coding enabled while UBT runs. | Close editor or press Ctrl+Alt+F11 before rebuilding. Ask user to recompile; do not claim compile pass until build succeeds. |
| **BF-004** | HUD round timer shows 3:00 while sudden death / round FSM use 90s; client `GetRoundTimeRemaining()` wrong | `RoundDuration` set authority-only via `SetRoundDuration` but **not replicated**; clients kept C++ default 180 while `RoundStartTime` replicated. | Replicate `RoundDuration` on `UBwayRoundManagementComponent`; `OnRep_RoundDuration` one-shot `OnRoundTimeChanged` on clients; HUD keeps local 1 Hz poll using `RoundStartTime + RoundDuration`. Do **not** replicate remaining seconds every tick. |
| **BF-006** | LMB on invalid buildable ghost ends placement preview; confirm stops working | `UAbilityTask_WaitTargetData` always `EndTask()` (and destroys target actor) after `ValidData` broadcast, even when target data is empty/invalid | In `ConfirmTargetingAndContinue`, only broadcast `TargetDataReadyDelegate` when `CreateTargetData` returns a valid handle (`Handle.IsValid(0)`). Invalid confirm must not fire the delegate — keeps WaitTargetData and preview ghost alive for retry. |
| **BF-008** | Buildable placement cancel/suppress logic hardcoded in `UBwayGameplayAbility` (class checks, manual spec iteration) | Custom `CanActivateAbility` / `ActivateAbility` bypasses GAS tag-driven cancel/block | Use GAS CDO tags: `Ability.Buildable.PlacementSession` on PlaceBuildable asset tags; default `CancelAbilitiesWithTag` on `UBwayGameplayAbility`; exempt abilities `CancelAbilitiesWithTag.Reset()` + `Ability.Buildable.PlacementExempt`; Primary/RelicRequest `ActivationBlockedTags` → `State.BuildablePlacement`. Centralize tags in `BwayGameplayTags.h`. |
| **BF-009** | LMB/RMB during buildable preview: Confirm/Cancel GAs never activate; LocalInputConfirm appears dead | Common causes: (1) `InputTag.*` on Place AbilityTags; (2) `WaitTargetData` only forwards confirm/cancel when `TargetActor->ShouldProduceTargetData()` is true — base impl requires `PrimaryPC->IsLocalController()`, and `PrimaryPC` can be null; (3) Confirm GA fails `ActivationRequiredTags` / tag blocks before `ActivateAbility` | Keep confirm/cancel on Lyra IMC + InputConfig + Confirm/Cancel GAs (or native ConfirmTargeting). Never bind Enhanced Input inside PlaceBuildable. Ensure `PrimaryPC` is set in `StartTargeting`; override `ShouldProduceTargetData` defensively. Log CanActivate failures on Confirm/Cancel. Do not put `InputTag.*` on AbilityTags. |
| **BF-010** | Buildable placement ghost always invalid on flat ground | Obstruction overlap uses actor pivot at ground hit; mesh/sphere collision extends into floor → always overlaps. QueryOnly DamageSphere was included. Floor may also be a different actor than the ground-hit actor. | Lift placement by pivot-to-bottom bounds (`ComputePivotLiftAlongLocalUp`) + skin. Skip `QueryOnly`. **Ignore `GroundHit.GetActor()`** in obstruction checks, and skip WorldStatic pieces whose bounds top is at/below the placement plane. |
| **BF-011** | LNK2019 from GF module calling `ULyraInputConfig::FindNativeInputActionForTag` / `FindAbilityInputActionForTag` | Methods are `UFUNCTION` but **not** `LYRAGAME_API`-exported; GF DLL cannot link LyraGame internals | Do not call unexported Lyra helpers from Game Features. Iterate public `NativeInputActions` / `AbilityInputActions` locally, **or** add `LYRAGAME_API` / `UE_API` on the Lyra method declarations and rebuild LyraGame. A `UFUNCTION` is reflected but not automatically DLL-exported—check the declaration for an API macro before cross-module calls. |
| **BF-012** | `GenericLocalConfirmCallbacks.AddLambda` / `Remove(FDelegateHandle)` fails to compile | `FAbilityConfirmOrCancel` is `DECLARE_DYNAMIC_MULTICAST_DELEGATE` — no `AddLambda`, only `AddDynamic`/`RemoveDynamic` + `UFUNCTION` | Bind ASC confirm/cancel with `AddDynamic(this, &UMyClass::OnConfirm)` and matching `UFUNCTION()` handlers. Never use native-delegate APIs on dynamic multicast delegates. |
| **BF-013** | PlaceBuildable preview works (key 1) but LMB/RMB Confirm/Cancel never fire (`LogBwayPlaceConfirm` silent; no ASC GenericLocalConfirm) | Enhanced Input **User Settings** registered `IMC_BW_HeroAbilities` into a stale player profile that kept LMB→Primary only, dropping later Confirm/Cancel dual-binds. Confirm GA + native ConfirmTargeting never receive the IA. | Do not register ability IMCs with user settings while iterating keybinds (`bRegisterWithSettings=false` on HeroAbilities). Push a dedicated non-settings placement IMC at high priority during PlaceBuildable. Clear `Saved` Enhanced Input user settings after IMC changes. |
| **BF-014** | RMB RelicRequest shares key with Cancel but does nothing outside placement | `GA_BW_Relic_Request` BP CDO overrode C++ to `NetExecutionPolicy=ServerInitiated` + `InstancedPerExecution`. Lyra ability input is local (`OnInputTriggered` / `TryActivateAbility` on owning client); ServerInitiated prevents normal local activation. Reparent/setup scripts can leave BP policy overrides. | Input-driven Breakaway abilities must stay `LocalPredicted` + `InstancedPerActor` (match C++ parent). After reparenting a GA BP, verify NetExecution/Instancing were not left ServerInitiated. Do not put `InputTag.*` on RelicRequest AbilityTags. |
| **BF-015** | RelicRequest activates (RMB) but never grants requesting tag / cannot pickup | Reparented `GA_BW_Relic_Request` to C++ while BP Event Graph GE apply was cleared; C++ parent was constructor-only (no `ActivateAbility`). | When moving GA logic from BP → C++, implement `ActivateAbility` in C++ before stripping the graph. Apply `RelicSettings::RequestingGameplayEffectClass` via `MakeOutgoingGameplayEffectSpec` + `ApplyGameplayEffectSpecToOwner` (resolve settings from `UBwayRelicManagerComponent::GetRelicSettings()` — no hardcoded GE paths). |
| **BF-016** | Editor fatal on load: `NewObject with empty name can't be used to create default subobjects (inside of UObject derived class constructor)` from `UGameplayEffect::FindOrAddComponent` | `FindOrAddComponent` / `AddComponent` uses `NewObject(..., NAME_None)` which is illegal during `UObject` construction. Calling it from a native `UGameplayEffect` constructor (e.g. to attach `UTargetTagsGameplayEffectComponent` for cooldown tags) crashes CDO creation. | In GE constructors: set only POD/properties (`DurationPolicy`, `DurationMagnitude`, store pending tags). Call `FindOrAddComponent` + `SetAndApplyTargetTagChanges` in **`PostInitProperties`** (or later). Never `FindOrAddComponent` / `AddComponent` inside a `UGameplayEffect` constructor. |
| **BF-017** | LNK2019 in hero GF module: `Z_Construct_UClass_UGenericTeamAgentInterface_NoRegister` / `FGenericTeamId::NoTeam` | `Cast<IGenericTeamAgentInterface>` and `FGenericTeamId::NoTeam` require **AIModule** link; hero GF modules (e.g. `HeroAlonaRuntime`) often omit it while `BreakawayCoreRuntime` includes it. | Prefer Lyra team compare (`ABwayGameState::AreOnSameTeam` / `ULyraTeamSubsystem::CompareTeams`) via owner pawn — same as BlessingZone. If you must use GenericTeam APIs, add `"AIModule"` to that module's `Build.cs`. Do not assume transitive AIModule from BreakawayCore. |
| **BF-018** | C1083: `Engine/PrimaryDataAsset.h` not found | `UPrimaryDataAsset` lives in **`Engine/DataAsset.h`**, not a separate `PrimaryDataAsset.h`. | Include `#include "Engine/DataAsset.h"` for `UPrimaryDataAsset` (match `BwayMatchFlowConfig.h`). |
| **BF-019** | C2039: `Multiply` is not a member of `EGameplayModOp` | This engine's `EGameplayModOp::Type` has `MultiplyAdditive` / `MultiplyCompound` (not `Multiply`). | Use `EGameplayModOp::MultiplyCompound` for stacking product slows (0.5 × 0.85); use `MultiplyAdditive` only when summing into the multiply channel per GAS formula. |
| **BF-020** | LNK2019: `UGameplayEffect::SetStackingType` unresolved from hero GF module | Setter exists but is **not** `GAMEPLAYABILITIES_API`-exported; GF DLLs cannot link it. Direct `StackingType` assign still works (deprecated). | From Game Feature modules, assign `StackingType` / `StackLimitCount` directly with `PRAGMA_DISABLE_DEPRECATION_WARNINGS`. Do not call `SetStackingType` across module boundaries unless Epic exports it. |
| **BF-021** | C3646 / C2059 cascade: `UE_API` treated as unknown override specifier on ctors/methods | `UE_API` used without `#define UE_API LYRAGAME_API` … `#undef UE_API` (Lyra MinimalAPI pattern). Bare `UE_API` is not a compiler keyword. | Match `LyraHealthSet.h`: `#define UE_API LYRAGAME_API` before the class, `#undef UE_API` after. Prefer exporting only the base Lyra type GF modules subclass; leave leaf implementations unexported unless needed. |
| **BF-022** | C1083: `GameFramework/GameplayMessageSubsystem.h` not found in BreakawayCore | Header exists in **GameplayMessageRuntime** plugin; GF `Build.cs` lacked that module dependency (LyraGame has it; not transitive to GF DLLs). | Add `"GameplayMessageRuntime"` to the GF module's `PrivateDependencyModuleNames` (or Public if the include stays in a public header). Do not assume LyraGame dependencies are available to Game Feature modules. |
| **BF-023** | C3668: `GetLifetimeForReplication` does not override; DOREPLIFETIME arg type mismatch | Wrong replication override name/signature. UE5.5 uses `GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&)`, not `GetLifetimeForReplication(TArray<FLifetimeProperty*>&)`. | Verify with unreal-api / `AActor` before adding replication overrides. Use `DOREPLIFETIME` only inside `GetLifetimeReplicatedProps`. |
| **BF-024** | C2065: `TAG_Lyra_*_Message` undeclared in GF module | Tags are declared in `LyraHealthSet.h` via `UE_DECLARE_GAMEPLAY_TAG_EXTERN`; cpp used them without including that header (or a local `UE_DECLARE_GAMEPLAY_TAG_EXTERN`). | Include `AbilitySystem/Attributes/LyraHealthSet.h` (or re-declare the extern tag) wherever `TAG_Lyra_Damage_Message` / `TAG_Lyra_Heal_Message` are referenced. |
| **BF-025** | C2662: `EnsureConfig` cannot convert `this` from const | Lazy cache helper called from a `const` method but itself was non-const. | Mark lazy resolvers `const` and cache members `mutable` (standard const-correct lazy init). |
| **BF-026** | Nameplates init but never appear; `bRequireLineOfSight` on | Policy traced camera→indicator attach (often feet/root). Floor hits Visibility channel → permanent hide. Also defaulted uncached LOS to hidden. | Trace to attach + `NameplateLOSVerticalOffset` (default 90cm). Default uncached LOS to visible until first sample. Isolate by setting `bRequireLineOfSight=false` on DA. |
| **BF-027** | Everyone looks blue; nameplates same color | Dual team systems: Breakaway `FTeamInfo` + Lyra `TeamCreation` both assign. Lyra reassigns on experience load and desyncs from Breakaway. Mesh tint used hardcoded Blue/Red vs Lyra Green/Blue display assets. | After experience load, `UBwayTeamBridgeComponent` re-syncs Breakaway→Lyra. `UpdateAppearanceForTeam` applies `ULyraTeamDisplayAsset`. Re-apply on team-changed. Verify PIE: players have Lyra TeamId 1 and 2. |
| **BF-028** | LNK2019: `ULyraTeamDisplayAsset::ApplyToMeshComponent` from BreakawayCore | Method exists but was not DLL-exported; GF module cannot link LyraGame internals (`UFUNCTION` ≠ export). | Export with Lyra `MinimalAPI` + `#define UE_API LYRAGAME_API` on `ApplyTo*` methods (same pattern as `LyraHealthSet` / BF-011). Rebuild LyraGame then the GF module. |

---

## UE widget binding checklist

Before marking a slot widget "done":

- [ ] Every `BindWidget` / `BindWidgetOptional` has **`BlueprintReadOnly`**
- [ ] C++ member type matches UMG widget class (CommonUI types included)
- [ ] Designer widget **Name** == C++ property name (e.g. `Text_Timer`)
- [ ] BP subclass **Compile** succeeds; `Get Text_Timer` appears in graph
- [ ] C++ module recompiled after header change (not Live Coding only)

---

## UE compile checklist

- [ ] `#include` full type in `.cpp` when calling methods on pointer
- [ ] `TSubclassOf<T>` / `BindWidget` need full type in header
- [ ] GF primary assets: `AssetBaseClass=/Script/Engine.PrimaryDataAsset` in `DefaultGame.ini`
- [ ] Ask user to recompile after C++ edits (editor closed or Live Coding off)

---

## When to extend this skill

Append a new **BF-00N** row when:

- A bug took >1 attempt to diagnose
- The same mistake could recur in another file/module
- A tooling/MCP script assumed wrong API (e.g. wrong MCP param names)

Keep rows concise. Link to file paths, not long prose.
