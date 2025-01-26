// LyraGameplayAbility_InteractiveObjectPickup.cpp
#include "LyraGameplayAbility_InteractiveObjectPickup.h"
#include "InteractivePhysicsObject.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Player/LyraPlayerState.h"

ULyraGameplayAbility_InteractiveObjectPickup::ULyraGameplayAbility_InteractiveObjectPickup()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;

    // This ability is granted temporarily when near interactive objects
    ReplicationPolicy = EGameplayAbilityReplicationPolicy::ReplicateYes;
}

void ULyraGameplayAbility_InteractiveObjectPickup::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle, 
    const FGameplayAbilityActorInfo* ActorInfo, 
    const FGameplayAbilityActivationInfo ActivationInfo, 
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        Super::EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UInteractivePhysicsObject* InteractiveObject = GetInteractiveObjectFromEventData(TriggerEventData);
    if (!InteractiveObject)
    {
        Super::EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Get the player state that's picking up the object
    ALyraPlayerState* LyraPS = Cast<ALyraPlayerState>(ActorInfo->OwnerActor);
    if (!LyraPS)
    {
        Super::EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Initiate pickup on the server
    InteractiveObject->RequestPickup(LyraPS);

    // End the ability - pickup itself is handled by the object
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

UInteractivePhysicsObject* ULyraGameplayAbility_InteractiveObjectPickup::GetInteractiveObjectFromEventData(const FGameplayEventData* TriggerEventData) const
{
    if (TriggerEventData)
    {
        // Handle OptionalObject case
        if (TriggerEventData->OptionalObject)
        {
            UObject* NonConstObject = const_cast<UObject*>(TriggerEventData->OptionalObject.Get());
            return Cast<UInteractivePhysicsObject>(NonConstObject);
        }

        // Handle Target case
        if (TriggerEventData->Target)
        {
            if (AActor* NonConstActor = const_cast<AActor*>(Cast<AActor>(TriggerEventData->Target)))
            {
                return Cast<UInteractivePhysicsObject>(NonConstActor->GetComponentByClass(UInteractivePhysicsObject::StaticClass()));
            }
        }
    }

    return nullptr;
}