#include "LyraGameplayAbility_BWThrow.h"
#include "GameFramework/Character.h"
#include "InteractionComponent.h"
#include "InteractionHandlerComponent.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerController.h"
#include "InteractionLogging.h"

ULyraGameplayAbility_BWThrow::ULyraGameplayAbility_BWThrow()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void ULyraGameplayAbility_BWThrow::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    const FGameplayEventData* TriggerEventData)
{
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    bIsCharging = true;
    ChargeStartTime = GetWorld()->GetTimeSeconds();

    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
    if (!Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UInteractionComponent* InteractionComp = Character->FindComponentByClass<UInteractionComponent>();
    if (!InteractionComp || InteractionComp->GetCurrentState() != EInteractionState::Held)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
}

void ULyraGameplayAbility_BWThrow::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    if (!bWasCancelled && bIsCharging)
    {
        ExecuteThrow();
    }

    bIsCharging = false;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void ULyraGameplayAbility_BWThrow::HandleChargeCompleted()
{
    if (bIsCharging)
    {
        ExecuteThrow();
        bIsCharging = false; // Ensure charging is stopped after execution
    }
}

float ULyraGameplayAbility_BWThrow::CalculateThrowSpeed() const
{
    float ChargeTime = GetWorld()->GetTimeSeconds() - ChargeStartTime;
    float ChargePercent = FMath::Clamp(ChargeTime / MaxChargeTime, 0.0f, 1.0f);
    return FMath::Lerp(MinThrowSpeed, MaxThrowSpeed, ChargePercent);
}

FVector ULyraGameplayAbility_BWThrow::CalculateThrowVelocity(const FVector& StartLocation, const FVector& TargetDirection) const
{
    FVector ThrowDirection = TargetDirection; // Now using provided TargetDirection (player view direction)
    float ThrowSpeed = CalculateThrowSpeed();
    FVector Velocity = ThrowDirection * ThrowSpeed;

    return Velocity;
}

void ULyraGameplayAbility_BWThrow::ExecuteThrow()
{
    if (ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo()))
    {
        UInteractionComponent* InteractionComp = Character->FindComponentByClass<UInteractionComponent>();
        if (!InteractionComp || InteractionComp->GetCurrentState() != EInteractionState::Held)
        {
            return;
        }
        APlayerController* PC = Cast<APlayerController>(Character->GetController());
        FRotator ControlRotation = PC ? PC->GetControlRotation() : Character->GetActorRotation();
 
        FVector ThrowDirection = Character->GetActorForwardVector();
        FVector StartLocation = Character->GetActorLocation() + 
                              (ThrowDirection * 50.0f) + 
                              FVector(0.0f, 0.0f, 50.0f);

        FVector ThrowVelocity = CalculateThrowVelocity(StartLocation, ThrowDirection);
        
        UInteractionHandlerComponent* InteractionHandler = Character->FindComponentByClass<UInteractionHandlerComponent>();
        if (InteractionHandler)
        {
            InteractionHandler->Server_RequestThrow(InteractionComp, ThrowVelocity, CalculateThrowSpeed()); // Pass calculated velocity and strength to server
            
        }
        
        InteractionComp->SetInteractionState(EInteractionState::Thrown); // State change still happens locally for prediction
        InteractionComp->SetOwner(nullptr);
        
        if (UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(InteractionComp->GetOwner()->GetRootComponent()))
        {
            PrimComponent->AddImpulse(ThrowVelocity, NAME_None, true);
        }
    }
}