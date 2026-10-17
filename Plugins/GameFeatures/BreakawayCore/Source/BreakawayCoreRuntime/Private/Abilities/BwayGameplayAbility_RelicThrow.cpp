#include "Abilities/BwayGameplayAbility_RelicThrow.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "LyraGameplayTags.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_RelicThrow)

namespace
{
FGameplayTag RelicCarrierTag()
{
	return FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.RelicCarrier"));
}

ARelicActor* FindOwnedRelic(const FGameplayAbilityActorInfo* ActorInfo)
{
	ABwayCharacterWithAbilities* Character = ActorInfo ? Cast<ABwayCharacterWithAbilities>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UWorld* World = Character ? Character->GetWorld() : nullptr;
	const ABwayGameState* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	ARelicActor* Relic = GameState && GameState->RelicManagerComponent ? GameState->RelicManagerComponent->GetRelicActor() : nullptr;
	if (Relic && Relic->CurrentCarrier == Character && Relic->GetCurrentState() == ERelicState::Carried)
	{
		return Relic;
	}
	if (Character)
	{
		TArray<AActor*> AttachedActors;
		Character->GetAttachedActors(AttachedActors);
		for (AActor* Attached : AttachedActors)
		{
			ARelicActor* AttachedRelic = Cast<ARelicActor>(Attached);
			if (AttachedRelic && AttachedRelic->CurrentCarrier == Character && AttachedRelic->GetCurrentState() == ERelicState::Carried)
			{
				return AttachedRelic;
			}
		}
	}
	return nullptr;
}

int32 ThrowPlayerId(const FGameplayAbilityActorInfo* ActorInfo)
{
	const APawn* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const APlayerState* Player = Pawn ? Pawn->GetPlayerState() : nullptr;
	return Player ? Player->GetPlayerId() : INDEX_NONE;
}
}

UBwayGameplayAbility_RelicThrow::UBwayGameplayAbility_RelicThrow(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;
	ActivationRequiredTags.AddTag(RelicCarrierTag());
	ActivationBlockedTags.AddTag(LyraGameplayTags::Status_Death);
	ActivationBlockedTags.AddTag(LyraGameplayTags::Status_Death_Dying);
	FGameplayTagContainer Tags = GetAssetTags();
	Tags.AddTag(FGameplayTag::RequestGameplayTag(TEXT("Ability.Relic.Throw")));
	SetAssetTags(Tags);
	DisplayData.AbilityName = NSLOCTEXT("Relic", "ChargedThrowName", "Throw");
	DisplayData.Description = NSLOCTEXT("Relic", "ChargedThrowDescription", "Hold primary fire to charge; release to throw the relic in your aim direction.");
	DisplayData.Icon = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/BreakawayCore/GameModes/BallMode/Abilities/UI/Icons/icon_treasure_throw.icon_treasure_throw")));
}

bool UBwayGameplayAbility_RelicThrow::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	return ActorInfo && Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)
		&& (!ActorInfo->IsNetAuthority() || FindOwnedRelic(ActorInfo));
}

void UBwayGameplayAbility_RelicThrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	ChargingRelic = FindOwnedRelic(ActorInfo);
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || (HasAuthority(&ActivationInfo) && !ChargingRelic.IsValid()))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	UAbilityTask_WaitGameplayTagRemoved* CarrierTask = UAbilityTask_WaitGameplayTagRemoved::WaitGameplayTagRemove(this, RelicCarrierTag(), nullptr, true);
	CarrierTask->Removed.AddDynamic(this, &ThisClass::OnCarrierTagRemoved);
	CarrierTask->ReadyForActivation();
	if (!IsActive()) return;
	UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Display, TEXT("BwayRelicThrow: START role=%d player=%d authoritative=%d"),
		int32(GetWorld()->GetNetMode()), ThrowPlayerId(ActorInfo), HasAuthority(&ActivationInfo));
#endif
	ReleaseTask->ReadyForActivation();
}

void UBwayGameplayAbility_RelicThrow::OnInputReleased(float TimeHeld)
{
	if (!IsActive()) return;
	if (!HasAuthority(&CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
		return;
	}
	ABwayCharacterWithAbilities* Character = Cast<ABwayCharacterWithAbilities>(GetAvatarActorFromActorInfo());
	ARelicActor* Relic = ChargingRelic.Get();
	if (!Character || !Relic || FindOwnedRelic(CurrentActorInfo) != Relic || !FMath::IsFinite(TimeHeld))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	const URelicSettings* Settings = Relic->GetRelicSettings();
	const float MaxChargeTime = Settings ? Settings->MaxChargeTime : 1.5f;
	const float ChargeSeconds = FMath::Clamp(TimeHeld, 0.f, FMath::Max(0.f, MaxChargeTime));
	const AController* Controller = Character->GetController();
	const FVector AimDirection = (Controller ? Controller->GetControlRotation().Vector() : Character->GetActorForwardVector()).GetSafeNormal();
	if (!FMath::IsFinite(MaxChargeTime) || AimDirection.ContainsNaN() || AimDirection.IsNearlyZero()
		|| !CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	const TWeakObjectPtr<ARelicActor> ReleaseRelic = Relic;
	const TWeakObjectPtr<ABwayCharacterWithAbilities> ReleaseCharacter = Character;
#if !UE_BUILD_SHIPPING
	const FVector LaunchVelocity = Relic->ComputeChargedThrowVelocity(AimDirection, ChargeSeconds);
	const int32 PlayerId = ThrowPlayerId(CurrentActorInfo);
#endif
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	if (!ReleaseRelic.IsValid() || !ReleaseCharacter.IsValid()
		|| ReleaseRelic->CurrentCarrier != ReleaseCharacter.Get() || ReleaseRelic->GetCurrentState() != ERelicState::Carried) return;
	Relic->ThrowRelicCharged(ChargeSeconds, AimDirection);
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Display, TEXT("BwayRelicThrow: RELEASE role=%d player=%d chargeSeconds=%.6f speed=%.6f aim=%s authoritative=1 accepted=%d"),
		int32(Relic->GetNetMode()), PlayerId, ChargeSeconds, LaunchVelocity.Size(), *AimDirection.ToString(),
		Relic->GetCurrentState() == ERelicState::Thrown && !Relic->CurrentCarrier);
#endif
}

void UBwayGameplayAbility_RelicThrow::OnCarrierTagRemoved()
{
	if (IsActive()) CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
}

void UBwayGameplayAbility_RelicThrow::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo)) return;
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}
	ChargingRelic.Reset();
#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Display, TEXT("BwayRelicThrow: END role=%d player=%d cancelled=%d"),
		GetWorld() ? int32(GetWorld()->GetNetMode()) : INDEX_NONE, ThrowPlayerId(ActorInfo), bWasCancelled);
#endif
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
