#include "LyraGameplayAbility_BWInteractiveObjectHolding.h"
#include "InteractionComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Player/LyraPlayerState.h"

ULyraGameplayAbility_BWInteractiveObjectHolding::ULyraGameplayAbility_BWInteractiveObjectHolding()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

UInteractionComponent* ULyraGameplayAbility_BWInteractiveObjectHolding::GetHeldObject() const
{
    if (ALyraPlayerState* LyraPS = Cast<ALyraPlayerState>(GetOwningActorFromActorInfo()))
    {
        // Look for any objects being held by this player
        if (APawn* Pawn = LyraPS->GetPawn())
        {
            TArray<UInteractionComponent*> AttachedObjects;
            Pawn->GetComponents<UInteractionComponent>(AttachedObjects);
            
            for (UInteractionComponent* Object : AttachedObjects)
            {
                if (Object->GetCurrentState() == EInteractionState::Held && 
                    Object->GetCurrentOwner() == LyraPS)
                {
                    return Object;
                }
            }
        }
    }
    return nullptr;
}

void ULyraGameplayAbility_BWInteractiveObjectHolding::EndAbility(
    const FGameplayAbilitySpecHandle Handle, 
    const FGameplayAbilityActorInfo* ActorInfo, 
    const FGameplayAbilityActivationInfo ActivationInfo, 
    bool bReplicateCancelAbility, 
    bool bWasCancelled)
{
    if (bWasCancelled)
    {
        if (UInteractionComponent* HeldObject = GetHeldObject())
        {
            // Drop the object if ability is cancelled
            if (APlayerController* PC = Cast<APlayerController>(ActorInfo->PlayerController))
            {
                HeldObject->SetInteractionState(EInteractionState::Free);
                HeldObject->SetOwner(nullptr);
            }
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility, bWasCancelled);
}