#include "Abilities/BwayGameplayAbility_RelicRequest.h"

#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameplayEffect.h"
#include "LyraGameplayTags.h"
#include "Relic/BwayRelicRequestIndicatorComponent.h"
#include "Relic/RelicSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility_RelicRequest)

DEFINE_LOG_CATEGORY_STATIC(LogBwayRelicRequest, Log, All);

namespace
{
FGameplayTag RelicCarrierStateTag()
{
	return FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.RelicCarrier"), false);
}
}

UBwayGameplayAbility_RelicRequest::UBwayGameplayAbility_RelicRequest(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// BP may override these — keep LocalPredicted + InstancedPerActor. ServerInitiated breaks Lyra RMB input (BF-014).
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	ActivationPolicy = ELyraAbilityActivationPolicy::OnInputTriggered;

	// Ability identity tags (e.g. Ability.Relic.RequestPickup) are authored on the BP.
	// Do NOT put InputTag.* on AbilityTags — grants carry InputTag via DynamicSpecSourceTags (BF-009).

	ActivationBlockedTags.AddTag(BwayGameplayTags::State_BuildablePlacement);
	ActivationBlockedTags.AddTag(LyraGameplayTags::Status_Death);
	ActivationBlockedTags.AddTag(LyraGameplayTags::Status_Death_Dying);
	CancelAbilitiesWithTag.Reset();
}

bool UBwayGameplayAbility_RelicRequest::CheckCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// Prepared catch (RMB hold) must remain responsive even immediately after a prior request tap.
	return true;
}

const URelicSettings* UBwayGameplayAbility_RelicRequest::ResolveRelicSettings() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	const UBwayRelicManagerComponent* RelicMgr = GameState ? GameState->RelicManagerComponent : nullptr;
	return RelicMgr ? RelicMgr->GetRelicSettings() : nullptr;
}

void UBwayGameplayAbility_RelicRequest::SetPreparingCatchTag(const FGameplayAbilityActorInfo* ActorInfo, bool bPreparingCatch) const
{
	ULyraAbilitySystemComponent* LyraASC = ActorInfo ? Cast<ULyraAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	if (!LyraASC || !BwayGameplayTags::State_Relic_PreparingCatch.GetTag().IsValid())
	{
		return;
	}

	const int32 DesiredCount = bPreparingCatch ? 1 : 0;
	if (ActorInfo->IsNetAuthority())
	{
		LyraASC->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, DesiredCount, EGameplayTagReplicationState::TagAndCountToAll);
	}
	else
	{
		LyraASC->SetLooseGameplayTagCount(BwayGameplayTags::State_Relic_PreparingCatch, DesiredCount, EGameplayTagReplicationState::None);
	}
}

void UBwayGameplayAbility_RelicRequest::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bClearRequestEffectOnEnd = false;
	if (!ActorInfo || !ActorInfo->AvatarActor.IsValid() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
		return;
	}

	const URelicSettings* Settings = ResolveRelicSettings();
	const TSubclassOf<UGameplayEffect> RequestEffectClass = Settings ? Settings->RequestingGameplayEffectClass : nullptr;
	if (!RequestEffectClass)
	{
		UE_LOG(LogBwayRelicRequest, Warning,
			TEXT("RelicRequest: no RequestingGameplayEffectClass (RelicSettings=%s). Wire it on the match RelicSettings asset."),
			*GetNameSafe(Settings));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	ActiveRequestTag = UBwayRelicRequestIndicatorComponent::ResolveRequestTag(ActorInfo->AvatarActor.Get());
	if (!ActiveRequestTag.IsValid() && Settings && Settings->RequestingTag.IsValid())
	{
		ActiveRequestTag = Settings->RequestingTag;
	}

	if (ASC && ActiveRequestTag.IsValid() && ActorInfo->IsNetAuthority())
	{
		// Refreshing the call-for-relic indicator replaces any prior request GE so effect count stays 1.
		ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ActiveRequestTag));
	}

	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(RequestEffectClass, GetAbilityLevel(Handle, ActorInfo));
	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogBwayRelicRequest, Warning, TEXT("RelicRequest: failed to build GE spec for %s"), *GetNameSafe(RequestEffectClass));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const FActiveGameplayEffectHandle ActiveHandle = ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
	RequestEffectDuration = SpecHandle.Data.IsValid() ? SpecHandle.Data->GetDuration() : 3.0f;
	if (RequestEffectDuration <= 0.0f)
	{
		RequestEffectDuration = 3.0f;
	}
	ActivationWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	SetPreparingCatchTag(ActorInfo, true);

	if (ActorInfo->IsNetAuthority())
	{
		if (ABwayCharacterWithAbilities* BwayChar = Cast<ABwayCharacterWithAbilities>(ActorInfo->AvatarActor.Get()))
		{
			BwayChar->TryPickupOverlappingRelic();
			if (!IsActive())
			{
				return;
			}
		}
	}

	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	const bool bHasPressedInput = Spec && Spec->InputPressed;
	const bool bIsRemoteClientOnServer = HasAuthority(&ActivationInfo) && !ActorInfo->IsLocallyControlled() && ActorInfo->PlayerController.IsValid();
	bActivatedWithPressedInput = bHasPressedInput || bIsRemoteClientOnServer;

	PreparedAvatar = Cast<APawn>(ActorInfo->AvatarActor.Get());
	PreparedController = PreparedAvatar.IsValid() ? PreparedAvatar->GetController() : nullptr;
	if (PreparedAvatar.IsValid())
	{
		PreparedAvatar->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &ThisClass::OnPawnControllerChanged);
	}

	if (ASC && ActiveRequestTag.IsValid())
	{
		TrackedASC = ASC;
		RequestTagEventHandle = ASC->RegisterGameplayTagEvent(ActiveRequestTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleRequestTagChanged);
	}

	const FGameplayTag CarrierTag = RelicCarrierStateTag();
	if (CarrierTag.IsValid())
	{
		UAbilityTask_WaitGameplayTagAdded* CarrierTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, CarrierTag, nullptr, true);
		CarrierTask->Added.AddDynamic(this, &ThisClass::OnAbortTagAdded);
		CarrierTask->ReadyForActivation();
		if (!IsActive())
		{
			return;
		}
	}

	UAbilityTask_WaitGameplayTagAdded* DyingTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, LyraGameplayTags::Status_Death_Dying, nullptr, true);
	DyingTask->Added.AddDynamic(this, &ThisClass::OnAbortTagAdded);
	DyingTask->ReadyForActivation();
	if (!IsActive())
	{
		return;
	}

	UAbilityTask_WaitGameplayTagAdded* DeathTask = UAbilityTask_WaitGameplayTagAdded::WaitGameplayTagAdd(this, LyraGameplayTags::Status_Death, nullptr, true);
	DeathTask->Added.AddDynamic(this, &ThisClass::OnAbortTagAdded);
	DeathTask->ReadyForActivation();
	if (!IsActive())
	{
		return;
	}

	if (bActivatedWithPressedInput)
	{
		UAbilityTask_WaitInputRelease* ReleaseTask = UAbilityTask_WaitInputRelease::WaitInputRelease(this, /*bTestAlreadyReleased=*/ false);
		ReleaseTask->OnRelease.AddDynamic(this, &ThisClass::OnInputReleased);
		ReleaseTask->ReadyForActivation();
	}

	UE_LOG(LogBwayRelicRequest, Log, TEXT("RelicRequest activated %s Active=%d InputHeld=%d"),
		*GetNameSafe(RequestEffectClass),
		ActiveHandle.IsValid() ? 1 : 0,
		bActivatedWithPressedInput ? 1 : 0);
}

void UBwayGameplayAbility_RelicRequest::InputReleased(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);
	if (!IsActive() || !bActivatedWithPressedInput)
	{
		return;
	}

	bClearRequestEffectOnEnd = false;
	EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}

void UBwayGameplayAbility_RelicRequest::OnInputReleased(float TimeHeld)
{
	if (!IsActive() || !bActivatedWithPressedInput)
	{
		return;
	}

	bClearRequestEffectOnEnd = false;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
}

void UBwayGameplayAbility_RelicRequest::OnAbortTagAdded()
{
	if (!IsActive())
	{
		return;
	}

	bClearRequestEffectOnEnd = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
}

void UBwayGameplayAbility_RelicRequest::OnPawnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	if (!IsActive() || Pawn != PreparedAvatar.Get() || NewController == PreparedController.Get())
	{
		return;
	}

	bClearRequestEffectOnEnd = true;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ true);
}

void UBwayGameplayAbility_RelicRequest::HandleRequestTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (!IsActive() || NewCount > 0)
	{
		return;
	}

	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : ActivationWorldTime;
	const double Elapsed = Now - ActivationWorldTime;
	const bool bRemovedEarly = Elapsed + 0.1 < static_cast<double>(RequestEffectDuration);
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	const bool bInputCurrentlyHeld = bActivatedWithPressedInput && (!CurrentActorInfo || !CurrentActorInfo->IsLocallyControlled() || (Spec && Spec->InputPressed));

	// End prepared catch when the request effect was explicitly cleared early or when it expired without held input.
	if (bRemovedEarly || !bInputCurrentlyHeld)
	{
		bClearRequestEffectOnEnd = false;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, /*bReplicateEndAbility*/ true, /*bWasCancelled*/ false);
	}
}

void UBwayGameplayAbility_RelicRequest::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, ActorInfo))
	{
		return;
	}
	if (ScopeLockCount > 0)
	{
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &ThisClass::EndAbility,
			Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
		return;
	}

	if (TrackedASC.IsValid() && RequestTagEventHandle.IsValid() && ActiveRequestTag.IsValid())
	{
		TrackedASC->RegisterGameplayTagEvent(ActiveRequestTag, EGameplayTagEventType::NewOrRemoved).Remove(RequestTagEventHandle);
	}
	RequestTagEventHandle.Reset();
	TrackedASC.Reset();

	if (PreparedAvatar.IsValid())
	{
		PreparedAvatar->ReceiveControllerChangedDelegate.RemoveDynamic(this, &ThisClass::OnPawnControllerChanged);
	}
	PreparedAvatar.Reset();
	PreparedController.Reset();

	SetPreparingCatchTag(ActorInfo, false);

	if ((bClearRequestEffectOnEnd || bWasCancelled) && ActorInfo && ActorInfo->IsNetAuthority())
	{
		if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
		{
			if (ActiveRequestTag.IsValid())
			{
				ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(ActiveRequestTag));
			}
		}
	}

	bActivatedWithPressedInput = false;
	bClearRequestEffectOnEnd = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
