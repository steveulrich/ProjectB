// LyraGameplayAbility_ThrowObject.cpp
#include "LyraGameplayAbility_ThrowObject.h"
#include "InteractivePhysicsObject.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

ULyraGameplayAbility_ThrowObject::ULyraGameplayAbility_ThrowObject()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

void ULyraGameplayAbility_ThrowObject::ActivateAbility(
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

    // Get the object we're holding
    UInteractivePhysicsObject* HeldObject = GetHeldObject();
    if (!HeldObject)
    {
        Super::EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Get the character throwing the object
    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor);
    if (!Character)
    {
        Super::EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Calculate throw direction and velocity
    FVector ThrowDirection = Character->GetControlRotation().Vector();
    FVector UpVector = FVector::UpVector;
    
    // Apply upward angle to throw
    float ThrowAngleRadians = FMath::DegreesToRadians(ThrowUpwardAngle);
    ThrowDirection = ThrowDirection.RotateAngleAxis(ThrowAngleRadians, FVector::CrossProduct(ThrowDirection, UpVector));
    
    // Calculate final velocity
    FVector ThrowVelocity = ThrowDirection * ThrowSpeed;

    // Request the object to be dropped with the throw velocity
    HeldObject->RequestDrop();

    // Apply the throw impulse to the object's root component
    if (UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(HeldObject->GetOwner()->GetRootComponent()))
    {
        PrimComponent->AddImpulse(ThrowVelocity, NAME_None, true);
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}