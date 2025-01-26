// LyraGameplayAbility_BWInteract.cpp
#include "LyraGameplayAbility_BWInteract.h"
#include "InteractionLogging.h"
#include "InteractionHandlerComponent.h"
#include "InteractionComponent.h"

ULyraGameplayAbility_BWInteract::ULyraGameplayAbility_BWInteract()
{
	// Constructor implementation
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void ULyraGameplayAbility_BWInteract::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
 	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
 	// Implementation
	UInteractionHandlerComponent* InteractionHandler = ActorInfo->AvatarActor->FindComponentByClass<UInteractionHandlerComponent>();
	if (!InteractionHandler)
	{
		UE_LOG(LogInteraction, Warning, TEXT("%s - No InteractionHandlerComponent found"), *ActorInfo->AvatarActor->GetName());
	        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	        return;
	}

	UInteractionComponent* Interactable = InteractionHandler->FindBestInteractable();
	if (Interactable && Interactable->CanBePickedUp(ActorInfo->AvatarActor.Get()))
 	{
 		InteractionHandler->Server_RequestPickup(Interactable);
 	}
 	
	// For a simple pickup, we can end the ability immediately after request
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
 
void ULyraGameplayAbility_BWInteract::OnInteractionCompleted()
{
	UE_LOG(LogInteraction, Log, TEXT("Interaction Completed"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void ULyraGameplayAbility_BWInteract::OnInteractionCancelled()
{
	UE_LOG(LogInteraction, Log, TEXT("Interaction Cancelled"));
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void ULyraGameplayAbility_BWInteract::OnMontageCompleted(FGameplayTag EventTag, FGameplayEventData EventData)
{
	UE_LOG(LogInteraction, Verbose, TEXT("Interaction Montage Completed"));
	OnInteractionCompleted();
}