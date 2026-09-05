#include "Abilities/BwayGameplayAbility_PlaceBuildable.h"

#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystemComponent.h"
#include "ActorPlacementTargeting/BwayTargetActor_ActorPlacementFace.h"
#include "ActorPlacementTargeting/BwayWorldReticle_ActorVisualization.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "Buildable/BuildableBase.h"
#include "BwayGameplayTags.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Engine/CollisionProfile.h"
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

void UBwayGameplayAbility_PlaceBuildable::OnTargetDataValid(const FGameplayAbilityTargetDataHandle& Data)
{
	UE_LOG(LogBwayPlaceBuildable, Log, TEXT("PlaceBuildable OnTargetDataValid Valid=%d"), Data.IsValid(0) ? 1 : 0);

	const FGameplayAbilityActivationInfo ActivationInfo = GetCurrentActivationInfo();
	bool bShouldEndAbility = false;

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

					if (BuildableData && CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
					{
						FTransform SpawnTransform;
						if (UBwayBuildablePlacementLibrary::MakePlacementTransformFromHit(HitData->HitResult, PC, SpawnTransform))
						{
							if (UBwayBuildablePlacementLibrary::SpawnBuildableForPlayer(GetAvatarActorFromActorInfo(), PC, BuildableData, SpawnTransform))
							{
								bShouldEndAbility = true;
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
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bShouldEndAbility);
}

void UBwayGameplayAbility_PlaceBuildable::OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& Data)
{
	UE_LOG(LogBwayPlaceBuildable, Log, TEXT("PlaceBuildable OnTargetDataCancelled"));
	ActiveWaitTargetDataTask = nullptr;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
