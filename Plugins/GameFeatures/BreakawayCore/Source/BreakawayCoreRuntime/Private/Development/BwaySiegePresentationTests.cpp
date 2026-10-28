#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Buildable/BwaySiegeEngineBuildable.h"
#include "Buildable/BwaySiegePresentationData.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwaySiegePresentationLifecycleTest,
	"Breakaway.CombatPolish.Siege.PresentationLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwaySiegePresentationLifecycleTest::RunTest(const FString&)
{
	// A real game instance supplies the message subsystem and owned world context. Actor
	// initialization enables ProcessEvent, so this test observes the native multicast receiver.
	TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
	Instance->InitializeStandalone();
	UWorld* World = Instance->GetWorld();
	if (!TestNotNull(TEXT("Siege presentation fixture world"), World)) return false;
	ON_SCOPE_EXIT { World->DestroyWorld(false); Instance->Shutdown(); GEngine->DestroyWorldContext(World); };
	World->InitializeActorsForPlay(FURL());
	ABwaySiegeEngineBuildable* Siege = World->SpawnActor<ABwaySiegeEngineBuildable>();
	ABwaySiegeEngineBuildable* LateObserver = World->SpawnActor<ABwaySiegeEngineBuildable>();
	AActor* ContactA = World->SpawnActor<AActor>();
	AActor* ContactB = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Siege"), Siege) || !TestNotNull(TEXT("Late-observer state fixture"), LateObserver)
		|| !TestNotNull(TEXT("First contact"), ContactA) || !TestNotNull(TEXT("Second contact"), ContactB)) return false;
	const FVector GameplayBounds = Siege->GetAttackCollision()->GetUnscaledBoxExtent();
	const float Speed = Siege->RollSpeed;
	const float Damage = Siege->BuildableDamagePerSecond;
	const float Lifetime = Siege->MaxRollDuration;
	Siege->PresentationData = NewObject<UBwaySiegePresentationData>(Siege);
	Siege->PresentationData->MeshRelativeTransform.SetScale3D(FVector(7.f));
	Siege->OnRep_PresentationData();
	TestEqual(TEXT("Cosmetics do not change roll speed"), Siege->RollSpeed, Speed);
	TestEqual(TEXT("Cosmetics do not change damage"), Siege->BuildableDamagePerSecond, Damage);
	TestEqual(TEXT("Cosmetics do not change lifetime"), Siege->MaxRollDuration, Lifetime);
	TestTrue(TEXT("Cosmetics do not change gameplay collision bounds"), Siege->GetAttackCollision()->GetUnscaledBoxExtent().Equals(GameplayBounds));
	TestEqual(TEXT("Optional mesh cannot collide"), Siege->PresentationMesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	Siege->StartRoll();
	TestEqual(TEXT("Roll start dispatches exactly one launch"), Siege->DispatchedPresentationSequence, uint16(1));
	Siege->StartRoll();
	Siege->OnRep_IsRolling();
	TestEqual(TEXT("Repeated start/RepNotify do not duplicate launch"), Siege->DispatchedPresentationSequence, uint16(1));
	Siege->DispatchPresentationEvent(EBwaySiegePresentationEvent::BuildableImpact, FVector::ZeroVector, FVector::UpVector, ContactA);
	Siege->DispatchPresentationEvent(EBwaySiegePresentationEvent::BuildableImpact, FVector::ZeroVector, FVector::UpVector, ContactA);
	TestEqual(TEXT("Repeated damage contact produces one impact for its target"), Siege->DispatchedPresentationSequence, uint16(2));
	Siege->DispatchPresentationEvent(EBwaySiegePresentationEvent::BuildableImpact, FVector::ZeroVector, FVector::UpVector, ContactB);
	TestEqual(TEXT("A distinct buildable contact gets its own impact"), Siege->DispatchedPresentationSequence, uint16(3));
	ABwaySiegeEngineBuildable* LethalTarget = World->SpawnActor<ABwaySiegeEngineBuildable>();
	if (!TestNotNull(TEXT("Lethal native buildable target"), LethalTarget)) return false;
	LethalTarget->DispatchBeginPlay();
	UAbilitySystemComponent* TargetASC = LethalTarget->GetAbilitySystemComponent();
	if (!TestNotNull(TEXT("Lethal target ASC"), TargetASC)) return false;
	TargetASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 1.f);
	Siege->ApplyDamageToBuildable(LethalTarget, 10.f);
	TestTrue(TEXT("Actual lethal damage synchronously destroys the native target"), LethalTarget->IsActorBeingDestroyed());
	TestTrue(TEXT("Lethal fixture exercises removal of the target ASC attributes"),
		!IsValid(TargetASC) || TargetASC->GetSet<ULyraHealthSet>() == nullptr);
	TestEqual(TEXT("Lethal contact still dispatches its impact after target destruction"), Siege->DispatchedPresentationSequence, uint16(4));
	Siege->DispatchPresentationEvent(EBwaySiegePresentationEvent::TerminalImpact, FVector::ZeroVector, FVector::UpVector);
	Siege->DispatchPresentationEvent(EBwaySiegePresentationEvent::Expiry, FVector::ZeroVector, FVector::UpVector);
	TestEqual(TEXT("Terminal impact cannot also emit expiry"), Siege->DispatchedPresentationSequence, uint16(5));
	TestTrue(TEXT("Terminal receipt is latched"), Siege->bTerminalPresentationReceived);
	Siege->OnRep_IsRolling();
	TestFalse(TEXT("A late rolling RepNotify cannot restart a terminal loop"), Siege->bRollingPresentationActive);
	const uint16 Received = Siege->ReceivedPresentationSequence;
	Siege->Multicast_PlayPresentationEvent_Implementation(1, EBwaySiegePresentationEvent::Launch, nullptr, FVector::ZeroVector, FVector::UpVector);
	TestEqual(TEXT("An old/duplicate event is ignored"), Siege->ReceivedPresentationSequence, Received);
	LateObserver->bIsRolling = true;
	LateObserver->OnRep_IsRolling();
	TestEqual(TEXT("Late rolling state does not replay historical one-shots"), LateObserver->ReceivedPresentationSequence, uint16(0));
	TestEqual(TEXT("RepNotify never dispatches a new launch"), LateObserver->DispatchedPresentationSequence, uint16(0));
	LateObserver->StopPresentation();
	TestFalse(TEXT("Local teardown clears the current-loop latch"), LateObserver->bRollingPresentationActive);
	return true;
}
#endif
