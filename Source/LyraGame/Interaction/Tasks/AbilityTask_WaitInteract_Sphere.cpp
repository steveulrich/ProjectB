// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilityTask_WaitInteract_Sphere.h"

#include "Interaction/InteractionStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AbilityTask_WaitInteract_Sphere)

UAbilityTask_WaitInteract_Sphere::UAbilityTask_WaitInteract_Sphere(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UAbilityTask_WaitInteract_Sphere* UAbilityTask_WaitInteract_Sphere::WaitInteract_Sphere(UGameplayAbility* OwningAbility, FInteractionQuery InteractionQuery, FCollisionProfileName TraceProfile, FGameplayAbilityTargetingLocationInfo StartLocation, float InteractionScanRange, float InteractionScanRate, bool bShowDebug)
{
	UAbilityTask_WaitInteract_Sphere* MyObj = NewAbilityTask<UAbilityTask_WaitInteract_Sphere>(OwningAbility);
	MyObj->InteractionScanRange = InteractionScanRange;
	MyObj->InteractionScanRate = InteractionScanRate;
	MyObj->StartLocation = StartLocation;
	MyObj->InteractionQuery = InteractionQuery;
	MyObj->TraceProfile = TraceProfile;
	MyObj->bShowDebug = bShowDebug;

	return MyObj;
}

void UAbilityTask_WaitInteract_Sphere::Activate()
{
	SetWaitingOnAvatar();

	UWorld* World = GetWorld();
	World->GetTimerManager().SetTimer(TimerHandle, this, &ThisClass::PerformTrace, InteractionScanRate, true);
}

void UAbilityTask_WaitInteract_Sphere::OnDestroy(bool AbilityEnded)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle);
	}

	Super::OnDestroy(AbilityEnded);
}

void UAbilityTask_WaitInteract_Sphere::PerformTrace()
{
	AActor* AvatarActor = Ability->GetCurrentActorInfo()->AvatarActor.Get();
	if (!AvatarActor)
	{
		return;
	}

	UWorld* World = GetWorld();

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(AvatarActor);
	
	FVector TraceStart = StartLocation.GetTargetingTransform().GetLocation();

	// Prepare the query parameters
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UAbilityTask_WaitInteract_Sphere), true);
	Params.AddIgnoredActors(ActorsToIgnore);

	// Prepare the collision shape
	FCollisionShape CollisionShape = FCollisionShape::MakeSphere(InteractionScanRange);

	// Conduct the trace
	TArray<FOverlapResult> OutOverlaps;
	World->OverlapMultiByChannel(OutOverlaps, TraceStart, FQuat::Identity, ECollisionChannel::ECC_Visibility, CollisionShape, Params);

	TArray<TScriptInterface<IInteractableTarget>> InteractableTargets;
	UInteractionStatics::AppendInteractableTargetsFromOverlapResults(OutOverlaps, InteractableTargets);

	UpdateInteractableOptions(InteractionQuery, InteractableTargets);

#if ENABLE_DRAW_DEBUG
	if (bShowDebug)
	{
		for (const FOverlapResult& OverlapResult : OutOverlaps)
		{
		    FColor DebugColor = OverlapResult.bBlockingHit ? FColor::Red : FColor::Green;
		    DrawDebugSphere(World, TraceStart, 5, 16, DebugColor, false, InteractionScanRate);
		}
	}
#endif // ENABLE_DRAW_DEBUG
}