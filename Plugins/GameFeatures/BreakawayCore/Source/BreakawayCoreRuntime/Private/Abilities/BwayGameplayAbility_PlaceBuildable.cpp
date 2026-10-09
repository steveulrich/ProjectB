#include "Abilities/BwayGameplayAbility_PlaceBuildable.h"

#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "ActorPlacementTargeting/BwayTargetActor_ActorPlacementFace.h"
#include "ActorPlacementTargeting/BwayWorldReticle_ActorVisualization.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "Buildable/BwaySiegeEngineBuildable.h"
#include "Buildable/BwayPlacementValidation.h"
#include "Buildable/BuildableBase.h"
#include "BwayGameplayTags.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_PlaceBuildable)

DEFINE_LOG_CATEGORY_STATIC(LogBwayPlaceBuildable, Log, All);

UBwayGameplayAbility_PlaceBuildable::UBwayGameplayAbility_PlaceBuildable(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;
	bRetriggerInstancedAbility = false;

	ReticleClass = ABwayWorldReticle_ActorVisualization::StaticClass();
	TargetActorClass = ABwayTargetActor_ActorPlacementFace::StaticClass();

	// Do NOT put InputTag.* on AbilityTags — grants carry InputTag via DynamicSpecSourceTags.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags.AddTag(BwayGameplayTags::Ability_Buildable_PlacementSession);
	AbilityTags.AddTag(BwayGameplayTags::Ability_Buildable_PlacementExempt);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	ActivationOwnedTags.AddTag(BwayGameplayTags::State_BuildablePlacement);
	CancelAbilitiesWithTag.Reset();
}

bool UBwayGameplayAbility_PlaceBuildable::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	if (const UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		if (const FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecFromHandle(Handle))
		{
			if (Spec->IsActive())
			{
				return false;
			}
		}

		if (ASC->HasMatchingGameplayTag(BwayGameplayTags::State_BuildablePlacement))
		{
			return false;
		}
	}

	const APlayerController* PC = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;
	if (!PC)
	{
		return false;
	}

	const UBwayBuildableDataAsset* BuildableData = UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(ActorInfo->AvatarActor.Get(), PC);
	if (!BuildableData || !BuildableData->BuildableActorClass)
	{
		return false;
	}

	FText FailureReason;
	return UBwayBuildablePlacementLibrary::CanPlayerPlaceBuildable(ActorInfo->AvatarActor.Get(), PC, BuildableData, FailureReason);
}

void UBwayGameplayAbility_PlaceBuildable::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	APlayerController* PC = ActorInfo ? ActorInfo->PlayerController.Get() : nullptr;
	UBwayBuildableDataAsset* BuildableData = PC
		? UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(ActorInfo->AvatarActor.Get(), PC)
		: nullptr;

	if (!BuildableData || !BuildableData->BuildableActorClass || !TargetActorClass)
	{
		UE_LOG(LogBwayPlaceBuildable, Warning, TEXT("PlaceBuildable ActivateAbility aborted: BuildableData=%s TargetActorClass=%s PC=%s"),
			BuildableData ? *BuildableData->GetName() : TEXT("null"),
			*GetNameSafe(TargetActorClass),
			*GetNameSafe(PC));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;

	UAbilityTask_WaitTargetData* WaitTask = UAbilityTask_WaitTargetData::WaitTargetData(
		this,
		NAME_None,
		EGameplayTargetingConfirmation::UserConfirmed,
		TargetActorClass);

	WaitTask->ValidData.AddDynamic(this, &UBwayGameplayAbility_PlaceBuildable::OnTargetDataValid);
	WaitTask->Cancelled.AddDynamic(this, &UBwayGameplayAbility_PlaceBuildable::OnTargetDataCancelled);
	ActiveWaitTargetDataTask = WaitTask;

	BindPlacementInputDebugCallbacks(ASC);

	AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
	if (WaitTask->BeginSpawningActor(this, TargetActorClass, SpawnedTargetActor))
	{
		if (ABwayTargetActor_ActorPlacementFace* PlacementTargetActor = Cast<ABwayTargetActor_ActorPlacementFace>(SpawnedTargetActor))
		{
			ConfigurePlacementTargetActor(PlacementTargetActor, BuildableData);
		}

		WaitTask->FinishSpawningActor(this, SpawnedTargetActor);
	}
	else
	{
		const AGameplayAbilityTargetActor* TargetDefaults = TargetActorClass->GetDefaultObject<AGameplayAbilityTargetActor>();
		const bool bExpectedTargetActor = ActorInfo->IsLocallyControlled()
			|| TargetDefaults->GetIsReplicated() || TargetDefaults->ShouldProduceTargetDataOnServer;
		if (bExpectedTargetActor)
		{
			UE_LOG(LogBwayPlaceBuildable, Error, TEXT("PlaceBuildable BeginSpawningActor failed for %s"), *GetNameSafe(TargetActorClass));
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
			return;
		}
		// The server for a remote player waits for GAS target data without a local preview actor.
	}

	// RegisterTargetDataCallbacks in BeginSpawningActor may consume an already-arrived
	// remote submission synchronously and end this ability.
	if (!IsActive())
	{
		return;
	}
	WaitTask->ReadyForActivation();

	if (ABwayTargetActor_ActorPlacementFace* PlacementTargetActor = Cast<ABwayTargetActor_ActorPlacementFace>(SpawnedTargetActor))
	{
		UE_LOG(LogBwayPlaceBuildable, Log,
			TEXT("PlaceBuildable WaitTargetData ready. TargetActor=%s ShouldProduceTargetData=%d LocallyControlled=%d ConfirmListeners=%d CancelListeners=%d"),
			*GetNameSafe(PlacementTargetActor),
			PlacementTargetActor->ShouldProduceTargetData() ? 1 : 0,
			ActorInfo && ActorInfo->IsLocallyControlled() ? 1 : 0,
			ASC ? ASC->GenericLocalConfirmCallbacks.IsBound() : 0,
			ASC ? ASC->GenericLocalCancelCallbacks.IsBound() : 0);
	}
}

void UBwayGameplayAbility_PlaceBuildable::BindPlacementInputDebugCallbacks(UAbilitySystemComponent* ASC)
{
	if (!ASC || bPlacementInputDebugCallbacksBound)
	{
		return;
	}

	ASC->GenericLocalConfirmCallbacks.AddDynamic(this, &UBwayGameplayAbility_PlaceBuildable::OnDebugLocalConfirm);
	ASC->GenericLocalCancelCallbacks.AddDynamic(this, &UBwayGameplayAbility_PlaceBuildable::OnDebugLocalCancel);
	bPlacementInputDebugCallbacksBound = true;
}

void UBwayGameplayAbility_PlaceBuildable::UnbindPlacementInputDebugCallbacks(UAbilitySystemComponent* ASC)
{
	if (!ASC || !bPlacementInputDebugCallbacksBound)
	{
		return;
	}

	ASC->GenericLocalConfirmCallbacks.RemoveDynamic(this, &UBwayGameplayAbility_PlaceBuildable::OnDebugLocalConfirm);
	ASC->GenericLocalCancelCallbacks.RemoveDynamic(this, &UBwayGameplayAbility_PlaceBuildable::OnDebugLocalCancel);
	bPlacementInputDebugCallbacksBound = false;
}

void UBwayGameplayAbility_PlaceBuildable::OnDebugLocalConfirm()
{
	UE_LOG(LogBwayPlaceBuildable, Log, TEXT("PlaceBuildable ASC GenericLocalConfirmCallbacks fired (LocalInputConfirm reached ASC)"));
}

void UBwayGameplayAbility_PlaceBuildable::OnDebugLocalCancel()
{
	UE_LOG(LogBwayPlaceBuildable, Log, TEXT("PlaceBuildable ASC GenericLocalCancelCallbacks fired (LocalInputCancel reached ASC)"));
}

void UBwayGameplayAbility_PlaceBuildable::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	UE_LOG(LogBwayPlaceBuildable, Log, TEXT("PlaceBuildable EndAbility Cancelled=%d"), bWasCancelled ? 1 : 0);
	if (UAbilitySystemComponent* ASC = ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr)
	{
		UnbindPlacementInputDebugCallbacks(ASC);
	}
	ActiveWaitTargetDataTask = nullptr;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UBwayGameplayAbility_PlaceBuildable::ConfigurePlacementTargetActor(ABwayTargetActor_ActorPlacementFace* TargetActor, UBwayBuildableDataAsset* BuildableData)
{
	if (!TargetActor || !BuildableData)
	{
		return;
	}

	TargetActor->PlacedActorClass = BuildableData->BuildableActorClass;
	TargetActor->BuildableDataAsset = BuildableData;
	TargetActor->ValidPlacementMaterial = ValidPlacementMaterial;
	TargetActor->InvalidPlacementMaterial = InvalidPlacementMaterial;
	TargetActor->MaxPlacementAngleDegrees = MaxPlacementAngleDegrees;
	TargetActor->MaxFootprintZDifference = MaxFootprintZDifference;
	TargetActor->FootprintTraceDownLength = FootprintTraceDownLength;
	TargetActor->FootprintTraceUpOffset = FootprintTraceUpOffset;
	TargetActor->MaxRange = MaxPlacementDistance;
	TargetActor->ReticleClass = ReticleClass;
	TargetActor->StartLocation = MakeTargetLocationInfoFromOwnerActor();
	TargetActor->bTraceAffectsAimPitch = true;
	TargetActor->TraceProfile = UCollisionProfile::BlockAll_ProfileName;
	TargetActor->CollisionRadius = 0.0f;
	TargetActor->CollisionHeight = 0.0f;
	TargetActor->bDebug = bDebugTargeting;
}

FBwayPlacementQuery UBwayGameplayAbility_PlaceBuildable::MakePlacementQuery(const UBwayBuildableDataAsset* BuildableData, bool bAuthorityTolerance) const
{
    APlayerController* PC = GetCurrentActorInfo() ? GetCurrentActorInfo()->PlayerController.Get() : nullptr;
    if (!BuildableData || !TargetActorClass || !PC) return FBwayPlacementQuery();
    FBwayPlacementQuery Query = TargetActorClass->GetDefaultObject<ABwayTargetActor_ActorPlacementFace>()
        ->MakePlacementQuery(GetAvatarActorFromActorInfo(), PC);
    Query.PlacedClass = BuildableData->BuildableActorClass;
    Query.AuthoredBounds = BuildableData->GetPlacementLocalBounds();
    Query.MaxRange = MaxPlacementDistance;
    Query.RangeTolerance = bAuthorityTolerance ? 100.f : 0.f;
    Query.MaxSlopeDegrees = MaxPlacementAngleDegrees;
    Query.MaxFootprintZDifference = MaxFootprintZDifference;
    Query.TraceUp = FootprintTraceUpOffset;
    Query.TraceDown = FootprintTraceDownLength;
    return Query;
}

void UBwayGameplayAbility_PlaceBuildable::OnTargetDataValid(const FGameplayAbilityTargetDataHandle& Data)
{
	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();
	const bool bAuthority = HasAuthority(&ActivationInfo);
	UE_LOG(LogBwayPlaceBuildable, Log,
		TEXT("PlaceBuildable OnTargetDataValid Valid=%d authority=%d entries=%d"),
		Data.IsValid(0) ? 1 : 0,
		bAuthority ? 1 : 0,
		Data.Num());

	bool bShouldEndAbility = false;
	bool bAccepted = false;

	if (Data.Num() == 1 && Data.IsValid(0)
		&& Data.Get(0)->GetScriptStruct() == FGameplayAbilityTargetData_SingleTargetHit::StaticStruct())
	{
		if (const FGameplayAbilityTargetData_SingleTargetHit* HitData = static_cast<const FGameplayAbilityTargetData_SingleTargetHit*>(Data.Get(0)))
		{
			if (HitData->HitResult.bBlockingHit)
			{
				if (HasAuthority(&ActivationInfo))
				{
					APlayerController* PC = GetCurrentActorInfo() ? GetCurrentActorInfo()->PlayerController.Get() : nullptr;
					UBwayBuildableDataAsset* BuildableData = PC
						? UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(GetAvatarActorFromActorInfo(), PC)
						: nullptr;

					if (BuildableData && TargetActorClass)
					{
						const FBwayPlacementQuery Query = MakePlacementQuery(BuildableData, true);
						const FPlacementValidationResult Placement = Query.ValidateSubmittedHit(HitData->HitResult);
						FText FailureReason;
						if (Placement.bIsValid
							&& UBwayBuildablePlacementLibrary::CanPlayerPlaceBuildable(GetAvatarActorFromActorInfo(), PC, BuildableData, FailureReason)
							&& CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
						{
							const FTransform SpawnTransform(Placement.PlacementRotation, Placement.AdjustedLocation);
							if (UBwayBuildablePlacementLibrary::SpawnBuildableForPlayer(GetAvatarActorFromActorInfo(), PC, BuildableData, SpawnTransform))
							{
								bShouldEndAbility = true;
								bAccepted = true;
							}
						}
					}
				}
				else
				{
					bShouldEndAbility = true;
				}
			}
		}
	}

	// UserConfirmed targeting ends its task after this callback, including on rejection.
	// End the ability as well so a failed commit/spawn cannot retain placement tags or input bindings.
	ActiveWaitTargetDataTask = nullptr;
	UE_LOG(LogBwayPlaceBuildable, Log,
		TEXT("PlaceBuildable TargetDataResult authority=%d accepted=%d"),
		bAuthority ? 1 : 0,
		bAccepted ? 1 : 0);
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bShouldEndAbility);
}

void UBwayGameplayAbility_PlaceBuildable::OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	UE_LOG(LogBwayPlaceBuildable, Log, TEXT("PlaceBuildable OnTargetDataCancelled"));
	ActiveWaitTargetDataTask = nullptr;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

#if !UE_BUILD_SHIPPING
bool UBwayGameplayAbility_PlaceBuildable::FindDevelopmentGroundTarget(FHitResult& OutHit) const
{
    APlayerController* PC = GetCurrentActorInfo() ? GetCurrentActorInfo()->PlayerController.Get() : nullptr;
    const auto* Data = PC ? UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(GetAvatarActorFromActorInfo(), PC) : nullptr;
    const FBwayPlacementQuery Query = MakePlacementQuery(Data, false);
    if (!Query.World || !Query.Source) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(BwayBuildableFixtureGround), true);
    Params.AddIgnoredActor(Query.Source);
    const FVector Origin = Query.Source->GetActorLocation();
    // Rolling barrels do not consume themselves on pawn proximity. The spawn exclusion
    // isolates stationary persistence probes (particularly Jail), not legal placement.
    const bool bRollingFixture = Data && Data->BuildableActorClass && Data->BuildableActorClass->IsChildOf<ABwaySiegeEngineBuildable>();
    const double FixtureClearanceSquared = bRollingFixture ? 0.0 : 1000.0 * 1000.0;
    // A held captive now occupies the former trap's test area. Use the live
    // trap trigger envelope around that pawn instead of the broad spawn margin;
    // the replacement must still stay outside actual overlap and placement bounds.
    float LiveTriggerRadius = 0.f;
    for (TActorIterator<ABuildableActor> It(Query.World); It; ++It)
        if (Data && It->GetClass() == Data->BuildableActorClass)
            if (const auto* Sphere = It->FindComponentByClass<USphereComponent>())
                LiveTriggerRadius = FMath::Max(LiveTriggerRadius, Sphere->GetScaledSphereRadius());
    int32 Hits = 0, ClearanceRejected = 0, GeometryRejected = 0;
    auto TryGround = [&](float Distance, float Yaw, bool bFallback)
    {
        const FVector Point = Origin + FRotator(0, Yaw + Query.Source->GetActorRotation().Yaw, 0).Vector() * Distance;
        FHitResult Hit;
        if (!Query.World->LineTraceSingleByChannel(Hit, Point + FVector(0,0,200), Point - FVector(0,0,3000), Query.CollisionChannel, Params)) return false;
        ++Hits;
        // A persistence probe must not accidentally consume a Jail trap when round-reset
        // pawns appear. This is fixture isolation, not a production placement exclusion.
        bool bNearPawnOrSpawn = false;
        for (TActorIterator<APawn> It(Query.World); It; ++It)
        {
            double PawnClearanceSquared = FixtureClearanceSquared;
            const auto* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(*It);
            if (LiveTriggerRadius > 0.f && ASC && ASC->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Jailed))
                PawnClearanceSquared = FMath::Square(double(LiveTriggerRadius + It->GetSimpleCollisionRadius() + 50.f));
            if (FVector::DistSquared2D(It->GetActorLocation(), Hit.ImpactPoint) < PawnClearanceSquared) { bNearPawnOrSpawn = true; break; }
        }
        if (!bNearPawnOrSpawn) for (TActorIterator<APlayerStart> It(Query.World); It; ++It)
            if (FVector::DistSquared2D(It->GetActorLocation(), Hit.ImpactPoint) < FixtureClearanceSquared) { bNearPawnOrSpawn = true; break; }
        if (bNearPawnOrSpawn) { ++ClearanceRejected; return false; }
        if (Query.ValidateSubmittedHit(Hit).bIsValid)
        {
            OutHit = Hit;
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: GroundSearch valid=1 rolling=%d maxRange=%.1f hits=%d clearanceRejected=%d geometryRejected=%d distance=%.1f fallback=%d"), bRollingFixture, Query.MaxRange, Hits, ClearanceRejected, GeometryRejected, Distance, bFallback);
            return true;
        }
        ++GeometryRejected;
        return false;
    };
    // Keep previously valid fixture routes unchanged. If a new round's spawn
    // has no legal point on the sparse grid, search a bounded denser grid using
    // the exact same production geometry checks and fixture spawn clearance.
    for (float Distance : {1600.f, 1200.f, 900.f, 600.f, 350.f})
    for (float Yaw : {0.f, 45.f, -45.f, 90.f, -90.f, 135.f, -135.f, 180.f})
        if (TryGround(Distance, Yaw, false)) return true;
    for (float Distance : {1950.f, 1800.f, 1650.f, 1500.f, 1350.f, 1200.f, 1050.f, 900.f, 750.f, 600.f, 450.f, 300.f})
    for (int32 Direction = 0; Direction < 16; ++Direction)
        if (TryGround(Distance, Direction * 22.5f, true)) return true;
    UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: GroundSearch valid=0 rolling=%d maxRange=%.1f hits=%d clearanceRejected=%d geometryRejected=%d x=%.1f y=%.1f z=%.1f"), bRollingFixture, Query.MaxRange, Hits, ClearanceRejected, GeometryRejected, Origin.X, Origin.Y, Origin.Z);
    return false;
}

void UBwayGameplayAbility_PlaceBuildable::RunDevelopmentTargetDataProbe(const FGameplayAbilityTargetDataHandle& Data)
{
	OnTargetDataValid(Data);
}
#endif
