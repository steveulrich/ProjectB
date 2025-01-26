// LyraGameplayAbility_PassObject.cpp
#include "LyraGameplayAbility_PassObject.h"
#include "InteractivePhysicsObject.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/LyraPlayerState.h"

ULyraGameplayAbility_PassObject::ULyraGameplayAbility_PassObject()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

void ULyraGameplayAbility_PassObject::ActivateAbility(
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

    UInteractivePhysicsObject* HeldObject = GetHeldObject();
    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor);
    
    if (!HeldObject || !Character)
    {
        Super::EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Find a target to pass to
    FVector StartLocation = Character->GetActorLocation();
    FRotator LookRotation = Character->GetControlRotation();
    
    APawn* TargetPawn = FindPassTarget(StartLocation, LookRotation);
    if (!TargetPawn)
    {
        // If no valid target, just do a normal throw in the look direction
        FVector ThrowDirection = LookRotation.Vector();
        FVector ThrowVelocity = ThrowDirection * PassSpeed;

        HeldObject->RequestDrop();
        if (UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(HeldObject->GetOwner()->GetRootComponent()))
        {
            PrimComponent->AddImpulse(ThrowVelocity, NAME_None, true);
        }
    }
    else
    {
        // Calculate direction and velocity for passing to target
        FVector TargetLocation = TargetPawn->GetActorLocation();
        // Aim slightly above their feet
        TargetLocation.Z += TargetPawn->GetDefaultHalfHeight();

        FVector DirectionToTarget = (TargetLocation - StartLocation).GetSafeNormal();
        FVector PassVelocity = DirectionToTarget * PassSpeed;

        HeldObject->RequestDrop();
        if (UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(HeldObject->GetOwner()->GetRootComponent()))
        {
            PrimComponent->AddImpulse(PassVelocity, NAME_None, true);
        }
    }

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

APawn* ULyraGameplayAbility_PassObject::FindPassTarget(const FVector& StartLocation, const FRotator& LookRotation) const
{
    // Get all pawns in the game
    TArray<AActor*> FoundPawns;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APawn::StaticClass(), FoundPawns);

    APawn* BestTarget = nullptr;
    float BestScore = -1.0f;
    FVector ForwardVector = LookRotation.Vector();

    ALyraPlayerState* CurrentHolder = nullptr;
    if (const AActor* Avatar = GetAvatarActorFromActorInfo())
    {
        if (const AController* Controller = Cast<AController>(Avatar->GetInstigatorController()))
        {
            CurrentHolder = Cast<ALyraPlayerState>(Controller->PlayerState);
        }
    }

    // Find the best target based on direction and distance
    for (AActor* Actor : FoundPawns)
    {
        APawn* OtherPawn = Cast<APawn>(Actor);
        if (!OtherPawn || !OtherPawn->GetController())
            continue;

        // Don't pass to self
        if (OtherPawn == GetAvatarActorFromActorInfo())
            continue;

        // Don't pass to teammates if this is a competitive game mode
        ALyraPlayerState* OtherPS = Cast<ALyraPlayerState>(OtherPawn->GetPlayerState());
        if (CurrentHolder && OtherPS && CurrentHolder->GetTeamId() != OtherPS->GetTeamId())
            continue;

        FVector DirectionToTarget = (OtherPawn->GetActorLocation() - StartLocation).GetSafeNormal();
        float Distance = FVector::Distance(StartLocation, OtherPawn->GetActorLocation());

        // Skip if too far
        if (Distance > MaxPassDistance)
            continue;

        // Calculate score based on how closely they align with where we're looking
        float DirectionScore = FVector::DotProduct(ForwardVector, DirectionToTarget);
        
        // Must be somewhat in front of us
        if (DirectionScore < 0.0f)
            continue;

        // Factor in distance (closer is better, but not as important as direction)
        float DistanceScore = 1.0f - (Distance / MaxPassDistance);
        float FinalScore = (DirectionScore * 0.7f) + (DistanceScore * 0.3f);

        if (FinalScore > BestScore)
        {
            BestScore = FinalScore;
            BestTarget = OtherPawn;
        }
    }

    return BestTarget;
}