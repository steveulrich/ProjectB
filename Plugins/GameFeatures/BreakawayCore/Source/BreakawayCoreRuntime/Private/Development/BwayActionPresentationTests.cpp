#include "Development/BwayActionPresentationTestTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Buildable/BuildableBase.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "Combat/BwayCuePresentationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayCueManager.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayActionPresentationCommitTest,
	"Breakaway.CombatPolish.Actions.CommitAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayActionPresentationCommitTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Ability presentation world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	World->InitializeActorsForPlay(FURL());
	World->SetGameState(World->SpawnActor<ABwayGameState>());
	ABwayPlayerState* Player = World->SpawnActor<ABwayPlayerState>();
	APawn* Pawn = World->SpawnActor<APawn>();
	if (!TestNotNull(TEXT("Player"), Player) || !TestNotNull(TEXT("Avatar"), Pawn)) return false;
	ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
	ASC->InitAbilityActorInfo(Player, Pawn);
	const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(FGameplayAbilitySpec(UBwayActionPresentationTestAbility::StaticClass(), 1));
	FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle);
	UBwayActionPresentationTestAbility* Ability = Spec ? Cast<UBwayActionPresentationTestAbility>(Spec->GetPrimaryInstance()) : nullptr;
	if (!TestNotNull(TEXT("Instanced test ability"), Ability)) return false;
	int32 Casts = 0, Ends = 0;
	bool bClearPrediction = true;
	bool bStableCueSource = true;
	UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	const FDelegateHandle Listener = Manager->OnGameplayCueRouted().AddLambda(
		[&](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event, const FGameplayCueParameters& Params, EGameplayCueExecutionOptions)
		{
			if (Target != Pawn || Event != EGameplayCueEvent::Executed) return;
			if (Tag == BwayGameplayCueTags::AbilityCastStatus)
			{
				++Casts;
				bClearPrediction &= !ASC->ScopedPredictionKey.IsValidKey();
				bStableCueSource &= Params.SourceObject.Get() == UBwayActionPresentationTestAbility::StaticClass()->GetDefaultObject()
					&& Params.EffectContext.GetSourceObject() == UBwayActionPresentationTestAbility::StaticClass()->GetDefaultObject();
			}
			if (Tag == BwayGameplayCueTags::AbilityEnd) ++Ends;
		});
	ON_SCOPE_EXIT { Manager->OnGameplayCueRouted().Remove(Listener); };
	Ability->bAllowCommit = false;
	ASC->TryActivateAbility(Handle);
	TestFalse(TEXT("Controlled real CommitCheck rejects the commit"), Ability->bCommitResult);
	TestFalse(TEXT("Rejected commit ends the ability"), Ability->IsActive());
	TestEqual(TEXT("Rejected commit emits no cast"), Casts, 0);
	TestEqual(TEXT("Rejected commit emits no end"), Ends, 0);
	TestFalse(TEXT("Rejected commit adds no persistent cue"), ASC->IsGameplayCueActive(BwayGameplayCueTags::AbilityActiveDefense));
	Ability->bAllowCommit = true;
	TestTrue(TEXT("Normal GAS activation succeeds"), ASC->TryActivateAbility(Handle));
	TestTrue(TEXT("Real commit succeeds"), Ability->bCommitResult);
	TestEqual(TEXT("Successful commit emits one cast"), Casts, 1);
	TestTrue(TEXT("Committed duration owns its persistent cue"), ASC->IsGameplayCueActive(BwayGameplayCueTags::AbilityActiveDefense));
	Ability->RepeatCommit();
	TestEqual(TEXT("Repeated commit cannot duplicate the cast"), Casts, 1);
	ASC->CancelAbilityHandle(Handle);
	TestFalse(TEXT("Cancel removes the owned active cue"), ASC->IsGameplayCueActive(BwayGameplayCueTags::AbilityActiveDefense));
	TestEqual(TEXT("Cancel produces exactly one configured end"), Ends, 1);
	ASC->CancelAbilityHandle(Handle);
	TestEqual(TEXT("Repeated cancellation has no presentation history"), Ends, 1);
	ASC->TryActivateAbility(Handle);
	TestEqual(TEXT("A new accepted activation gets one fresh cast"), Casts, 2);
	Ability->Finish();
	TestEqual(TEXT("Normal completion also produces one end"), Ends, 2);
	TestFalse(TEXT("Normal completion clears its loop"), ASC->IsGameplayCueActive(BwayGameplayCueTags::AbilityActiveDefense));
	TestTrue(TEXT("Authority-only casts are not tagged as predicted owner events"), bClearPrediction);
	TestTrue(TEXT("Cue source identity is the stable class default, never the nonreplicated ability instance"), bStableCueSource);
	Ability->ClearCueBindings();
	TestTrue(TEXT("Unset presentation does not block gameplay activation"), ASC->TryActivateAbility(Handle));
	Ability->Finish();
	TestEqual(TEXT("Unset cast fields do not duplicate existing native action cues"), Casts, 2);
	TestEqual(TEXT("Unset end field has no presentation side effect"), Ends, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayCuePresentationLifecycleTest,
	"Breakaway.CombatPolish.Buildables.PresentationLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayCuePresentationLifecycleTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Buildable presentation world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	World->InitializeActorsForPlay(FURL());
	ABwayCuePresentationTestBuildable* Buildable = World->SpawnActor<ABwayCuePresentationTestBuildable>();
	if (!TestNotNull(TEXT("Buildable"), Buildable)) return false;
	Buildable->DispatchBeginPlay();
	UAbilitySystemComponent* ASC = Buildable->GetAbilitySystemComponent();
	UBwayCuePresentationComponent* Component = NewObject<UBwayCuePresentationComponent>(Buildable);
	Buildable->AddInstanceComponent(Component);
	Component->CueAttachComponent = Buildable->PresentationRoot;
	Component->Cue_Start = BwayGameplayCueTags::BuildableStart;
	Component->Cue_Active = BwayGameplayCueTags::BuildableActive;
	Component->Cue_End = BwayGameplayCueTags::BuildableEnd;
	Component->VisualMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
	Component->VisualRelativeTransform = FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, 60.f), FVector(2.f, 1.f, 1.f));
	const ECollisionEnabled::Type OriginalCollision = Buildable->GetMesh()->GetCollisionEnabled();
	const bool bOriginalVisibility = Buildable->GetMesh()->IsVisible();
	Component->RegisterComponent();
	Component->InitializeVisual();
	if (!TestNotNull(TEXT("Optional cosmetic mesh"), Component->VisualComponent.Get())) return false;
	TestEqual(TEXT("Cosmetic geometry has no collision"), Component->VisualComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestFalse(TEXT("Cosmetic geometry cannot replicate local attachment objects"), Component->VisualComponent->GetIsReplicated());
	TestTrue(TEXT("Replicated mesh exports only a stable native cosmetic attachment parent"), Buildable->PresentationRoot->IsNameStableForNetworking());
	TestFalse(TEXT("Cosmetic parent does not replicate its dynamic children"), Buildable->PresentationRoot->GetIsReplicated());
	TestTrue(TEXT("Dynamic geometry attaches below the stable nonreplicated parent"), Component->VisualComponent->GetAttachParent() == Buildable->PresentationRoot);
	TestFalse(TEXT("Replicated root does not directly reference dynamic geometry"), Buildable->GetRootComponent()->GetAttachChildren().Contains(Component->VisualComponent.Get()));
	TestTrue(TEXT("Cosmetic transform is applied only to replacement geometry"), Component->VisualComponent->GetRelativeTransform().Equals(Component->VisualRelativeTransform));
	TestEqual(TEXT("Original gameplay collision is unchanged"), Buildable->GetMesh()->GetCollisionEnabled(), OriginalCollision);
	TestFalse(TEXT("Original visual is hidden while the replacement exists"), Buildable->GetMesh()->IsVisible());
	int32 Starts = 0, Ends = 0;
	UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	const FDelegateHandle Listener = Manager->OnGameplayCueRouted().AddLambda(
		[&](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event, const FGameplayCueParameters&, EGameplayCueExecutionOptions)
		{
			if (Target != Buildable || Event != EGameplayCueEvent::Executed) return;
			if (Tag == BwayGameplayCueTags::BuildableStart) ++Starts;
			if (Tag == BwayGameplayCueTags::BuildableEnd) ++Ends;
		});
	ON_SCOPE_EXIT { Manager->OnGameplayCueRouted().Remove(Listener); };
	Component->StartPresentation();
	Component->StartPresentation();
	TestEqual(TEXT("Repeated activation emits exactly one deployment burst"), Starts, 1);
	TestTrue(TEXT("The current loop is active on the replicated ASC"), ASC->IsGameplayCueActive(BwayGameplayCueTags::BuildableActive));
	FStructProperty* CueProperty = FindFProperty<FStructProperty>(UAbilitySystemComponent::StaticClass(), TEXT("ActiveGameplayCues"));
	if (!TestNotNull(TEXT("Replicated cue state"), CueProperty)) return false;
	const FActiveGameplayCueContainer* Snapshot = CueProperty->ContainerPtrToValuePtr<FActiveGameplayCueContainer>(ASC);
	TestEqual(TEXT("Late-observer state contains only the current loop"), Snapshot->GameplayCues.Num(), 1);
	if (Snapshot->GameplayCues.Num() == 1)
	{
		TestTrue(TEXT("Transient start/end history is absent from replicated cue state"), Snapshot->GameplayCues[0].GameplayCueTag == BwayGameplayCueTags::BuildableActive);
		TestTrue(TEXT("Replicated loop parameters attach below the same stable cosmetic parent"), Snapshot->GameplayCues[0].Parameters.TargetAttachComponent.Get() == Buildable->PresentationRoot);
	}
	Component->StopPresentation();
	Component->StopPresentation();
	TestEqual(TEXT("Actual terminal event emits one end burst"), Ends, 1);
	TestFalse(TEXT("Terminal event removes current loop"), ASC->IsGameplayCueActive(BwayGameplayCueTags::BuildableActive));
	FBwayCuePresentationState FirstOverlap, SecondOverlap;
	FirstOverlap.Start(ASC, Buildable, Buildable, FGameplayTag(), BwayGameplayCueTags::BuildableActive, FGameplayTag());
	SecondOverlap.Start(ASC, Buildable, Buildable, FGameplayTag(), BwayGameplayCueTags::BuildableActive, FGameplayTag());
	FirstOverlap.Stop(false);
	TestTrue(TEXT("Ending one lifetime preserves the same-tag overlapping loop"), ASC->IsGameplayCueActive(BwayGameplayCueTags::BuildableActive));
	SecondOverlap.Stop(false);
	TestFalse(TEXT("Last owner removes the shared loop"), ASC->IsGameplayCueActive(BwayGameplayCueTags::BuildableActive));
	ASC->AddGameplayCue(BwayGameplayCueTags::BuildableActive);
	FBwayCuePresentationState Borrowed;
	Borrowed.Start(ASC, Buildable, Buildable, FGameplayTag(), BwayGameplayCueTags::BuildableActive, FGameplayTag());
	Borrowed.Stop(false);
	TestTrue(TEXT("Preexisting external cues are never removed by this helper"), ASC->IsGameplayCueActive(BwayGameplayCueTags::BuildableActive));
	ASC->RemoveGameplayCue(BwayGameplayCueTags::BuildableActive);
	Component->StartPresentation();
	TestEqual(TEXT("Terminal component cannot replay historical deployment"), Starts, 1);
	Component->EndPlay(EEndPlayReason::RemovedFromWorld);
	TestEqual(TEXT("Teardown never invents a terminal burst"), Ends, 1);
	TestNull(TEXT("Teardown destroys owned cosmetic geometry"), Component->VisualComponent.Get());
	TestEqual(TEXT("Teardown restores prior mesh visibility"), Buildable->GetMesh()->IsVisible(), bOriginalVisibility);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayZonePresentationLifetimeTest,
	"Breakaway.CombatPolish.Zones.PresentationLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayZonePresentationLifetimeTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Zone presentation world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	World->InitializeActorsForPlay(FURL());
	ABwayCuePresentationTestZone* Zone = World->SpawnActor<ABwayCuePresentationTestZone>();
	ABwayCuePresentationTestZone* LateObserver = World->SpawnActor<ABwayCuePresentationTestZone>();
	if (!TestNotNull(TEXT("Zone"), Zone) || !TestNotNull(TEXT("Late-observer zone"), LateObserver)) return false;
	int32 ActiveCalls = 0, RemovedCalls = 0, HistoricalCalls = 0;
	bool bStableAttachment = true;
	UGameplayCueManager* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	const FDelegateHandle Listener = Manager->OnGameplayCueRouted().AddLambda(
		[&](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event, const FGameplayCueParameters& Params, EGameplayCueExecutionOptions)
		{
			if ((Target != Zone && Target != LateObserver) || Tag != BwayGameplayCueTags::ZoneHealing) return;
			if (Event == EGameplayCueEvent::WhileActive) ++ActiveCalls;
			else if (Event == EGameplayCueEvent::Removed) ++RemovedCalls;
			else ++HistoricalCalls;
			bStableAttachment &= Params.TargetAttachComponent.IsValid() && Params.TargetAttachComponent->IsNameStableForNetworking();
		});
	ON_SCOPE_EXIT { Manager->OnGameplayCueRouted().Remove(Listener); };
	TestNull(TEXT("Zone lifetime presentation requires no gameplay ASC"), UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Zone));
	Zone->DispatchBeginPlay();
	Zone->Presentation->StartPresentation();
	TestEqual(TEXT("Initial zone lifetime starts its current loop once"), ActiveCalls, 1);
	LateObserver->DispatchBeginPlay();
	TestEqual(TEXT("A newly observed live zone starts only its current loop"), ActiveCalls, 2);
	TestEqual(TEXT("Neither initial nor late observation replays historical one-shots"), HistoricalCalls, 0);
	Zone->Destroy();
	TestEqual(TEXT("Actual zone destruction removes its persistent cue"), RemovedCalls, 1);
	LateObserver->Destroy();
	TestEqual(TEXT("Each observer cleans its own current loop"), RemovedCalls, 2);
	TestTrue(TEXT("Zone loop parameters use a stable native attachment"), bStableAttachment);
	return true;
}
#endif
