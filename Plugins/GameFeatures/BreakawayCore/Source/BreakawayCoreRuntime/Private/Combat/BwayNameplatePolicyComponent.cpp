// Copyright Epic Games, Inc. All Rights Reserved.

#include "Combat/BwayNameplatePolicyComponent.h"

#include "Combat/BwayCombatReadabilityConfig.h"
#include "Combat/BwayCombatReadabilityLibrary.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/IndicatorSystem/IndicatorDescriptor.h"
#include "UI/IndicatorSystem/LyraIndicatorManagerComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayNameplatePolicyComponent)

UBwayNameplatePolicyComponent::UBwayNameplatePolicyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UBwayNameplatePolicyComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetController<APlayerController>();
	if (!PC || !PC->IsLocalController())
	{
		SetComponentTickEnabled(false);
		return;
	}

	RefreshConfig();
	IndicatorManager = ULyraIndicatorManagerComponent::GetComponent(PC);
}

void UBwayNameplatePolicyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CachedLOSByIndicator.Reset();
	Super::EndPlay(EndPlayReason);
}

void UBwayNameplatePolicyComponent::RefreshConfig()
{
	CachedConfig = UBwayCombatReadabilityLibrary::ResolveCombatReadabilityConfig(this);
}

void UBwayNameplatePolicyComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	LOSAccumulatedTime += DeltaTime;
	EvaluateIndicators();
}

bool UBwayNameplatePolicyComponent::HasLineOfSightTo(
	const FVector& ViewLocation,
	const FVector& TargetLocation,
	const AActor* TargetActor) const
{
	UWorld* World = GetWorld();
	if (!World || !CachedConfig)
	{
		return true;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BwayNameplateLOS), false);
	if (const APlayerController* PC = GetController<APlayerController>())
	{
		Params.AddIgnoredActor(PC->GetPawn());
	}
	if (TargetActor)
	{
		Params.AddIgnoredActor(TargetActor);
	}

	FHitResult Hit;
	const bool bHit = World->LineTraceSingleByChannel(
		Hit,
		ViewLocation,
		TargetLocation,
		CachedConfig->NameplateLOSTraceChannel,
		Params);

	return !bHit;
}

void UBwayNameplatePolicyComponent::EvaluateIndicators()
{
	APlayerController* PC = GetController<APlayerController>();
	if (!PC)
	{
		return;
	}

	if (!CachedConfig)
	{
		RefreshConfig();
	}
	if (!CachedConfig)
	{
		return;
	}

	if (!IndicatorManager.IsValid())
	{
		IndicatorManager = ULyraIndicatorManagerComponent::GetComponent(PC);
	}
	if (!IndicatorManager.IsValid())
	{
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const APawn* LocalPawn = PC->GetPawn();
	const float MaxRangeSq = FMath::Square(CachedConfig->MaxNameplateRange);
	const bool bHideSelf = CachedConfig->bHideLocalPlayerNameplate;
	const bool bRequireLOS = CachedConfig->bRequireLineOfSight;
	const float LOSInterval = CachedConfig->NameplateLOSTraceInterval;
	const int32 MaxTraces = CachedConfig->MaxNameplateLOSTracesPerTick;

	const TArray<UIndicatorDescriptor*>& Indicators = IndicatorManager->GetIndicators();
	int32 TracesThisTick = 0;
	const bool bCanIssueNewLOS = (LOSAccumulatedTime >= LOSInterval);

	for (int32 Index = 0; Index < Indicators.Num(); ++Index)
	{
		UIndicatorDescriptor* Indicator = Indicators[Index];
		if (!Indicator)
		{
			continue;
		}

		USceneComponent* SceneComp = Indicator->GetSceneComponent();
		if (!IsValid(SceneComp))
		{
			Indicator->SetDesiredVisibility(false);
			continue;
		}

		AActor* TargetActor = SceneComp->GetOwner();
		if (bHideSelf && LocalPawn && TargetActor == LocalPawn)
		{
			Indicator->SetDesiredVisibility(false);
			continue;
		}

		const FVector TargetLocation = SceneComp->GetComponentLocation() + Indicator->GetWorldPositionOffset();
		const float DistSq = FVector::DistSquared(ViewLocation, TargetLocation);
		if (DistSq > MaxRangeSq)
		{
			Indicator->SetDesiredVisibility(false);
			continue;
		}

		const float Distance = FMath::Sqrt(DistSq);
		Indicator->SetScreenScale(CachedConfig->EvaluateNameplateScale(Distance));

		bool bHasLOS = true;
		if (bRequireLOS)
		{
			const TObjectKey<UIndicatorDescriptor> Key(Indicator);
			bool* CachedLOS = CachedLOSByIndicator.Find(Key);

			const bool bShouldTrace =
				bCanIssueNewLOS
				&& (TracesThisTick < MaxTraces)
				&& ((Index + NextLOSTraceIndex) % FMath::Max(1, Indicators.Num()) < MaxTraces || !CachedLOS);

			if (bShouldTrace && TracesThisTick < MaxTraces)
			{
				// Aim chest/head-high so floor geometry between camera and feet does not fail LOS.
				const FVector LOSTarget =
					TargetLocation + FVector(0.f, 0.f, CachedConfig->NameplateLOSVerticalOffset);
				bHasLOS = HasLineOfSightTo(ViewLocation, LOSTarget, TargetActor);
				CachedLOSByIndicator.Add(Key, bHasLOS);
				++TracesThisTick;
			}
			else if (CachedLOS)
			{
				bHasLOS = *CachedLOS;
			}
			else
			{
				// Not yet traced this indicator — keep visible until the first LOS sample.
				bHasLOS = true;
			}
		}

		Indicator->SetDesiredVisibility(bHasLOS);
	}

	if (bCanIssueNewLOS)
	{
		LOSAccumulatedTime = 0.f;
		NextLOSTraceIndex = (NextLOSTraceIndex + MaxTraces) % FMath::Max(1, Indicators.Num());
	}
}
