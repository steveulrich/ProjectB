#include "LyraGameplayAbility_BWPass.h"
#include "InteractionComponent.h"
#include "InteractionHandlerComponent.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "InteractionLogging.h"
#include "EngineUtils.h"
#include "Player/LyraPlayerState.h"

ULyraGameplayAbility_BWPass::ULyraGameplayAbility_BWPass()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

void ULyraGameplayAbility_BWPass::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    Super::OnGiveAbility(ActorInfo, Spec);
}

void ULyraGameplayAbility_BWPass::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle, 
    const FGameplayAbilityActorInfo* ActorInfo, 
    const FGameplayAbilityActivationInfo ActivationInfo, 
    const FGameplayEventData* TriggerEventData)
{
    if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UInteractionComponent* HeldObject = GetHeldObject();
    ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor);
    
    if (!HeldObject || !Character)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    UInteractionHandlerComponent* InteractionHandler = Character->FindComponentByClass<UInteractionHandlerComponent>();
    APawn* TargetPawn = FindPassTarget(Character->GetActorLocation(), Character->GetControlRotation());
    if (!TargetPawn)
    {
        UE_LOG(LogInteraction, Log, TEXT("%s - No Pass Target Found - Performing Forward Pass"), *Character->GetName());
        InteractionHandler->Server_RequestPass(HeldObject, nullptr); // Pass to no specific target for forward pass
    }
    else
    {
        UE_LOG(LogInteraction, Log, TEXT("%s - Passing to Target: %s"), *Character->GetName(), *TargetPawn->GetName());
        InteractionHandler->Server_RequestPass(HeldObject, TargetPawn); // Pass to no specific target for forward pass
    }

    EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}

APawn* ULyraGameplayAbility_BWPass::FindPassTarget(const FVector& StartLocation, const FRotator& LookRotation) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    APawn* BestTarget = nullptr;
    float BestScore = -1.0f;
    const FVector ForwardVector = LookRotation.Vector();

    ALyraPlayerState* CurrentPlayerState = Cast<ALyraPlayerState>(CurrentActorInfo->OwnerActor);
    int32 MyTeamId = CurrentPlayerState ? CurrentPlayerState->GetTeamId() : INDEX_NONE;

    for (TActorIterator<APawn> It(World); It; ++It)
    {
        APawn* OtherPawn = *It;
        if (!OtherPawn || OtherPawn == CurrentActorInfo->AvatarActor.Get())
        {
            continue;
        }

        if (ALyraPlayerState* OtherPlayerState = Cast<ALyraPlayerState>(OtherPawn->GetPlayerState()))
        {
            if (MyTeamId != OtherPlayerState->GetTeamId())
            {
                continue;
            }
        }

        FVector DirectionToTarget = OtherPawn->GetActorLocation() - StartLocation;
        float DistanceSquared = DirectionToTarget.SizeSquared();

        if (DistanceSquared > FMath::Square(MaxPassDistance))
        {
            continue;
        }

        DirectionToTarget.Normalize();

        float DirectionScore = FVector::DotProduct(ForwardVector, DirectionToTarget);
        
        if (DirectionScore <= 0.0f)
        {
            continue;
        }

        FHitResult HitResult;
        float DistanceScore = 1.0f - (FMath::Sqrt(DistanceSquared) / MaxPassDistance);
        float FinalScore = (DirectionScore * 0.7f) + (DistanceScore * 0.3f);

        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(CurrentActorInfo->AvatarActor.Get());

        bool bHasLineOfSight = !World->LineTraceSingleByChannel(
            HitResult,
            StartLocation, 
            OtherPawn->GetActorLocation(),
            ECC_Visibility, 
            QueryParams);

        if (bHasLineOfSight && FinalScore > BestScore)
        {
            BestScore = FinalScore;
            BestTarget = OtherPawn;
        }
    }

    if (bDebugDrawTargeting)
    {
        const float ConeLength = MaxPassDistance;
        DrawDebugCone(
            World, 
            StartLocation, 
            ForwardVector, 
            ConeLength,
            FMath::DegreesToRadians(45.0f),
            FMath::DegreesToRadians(45.0f),
            12,
            FColor::Yellow,
            false,
            DebugDrawDuration
        );

        if (BestTarget)
        {
            DrawDebugLine(
                World,
                StartLocation,
                BestTarget->GetActorLocation(),
                FColor::Green,
                false,
                DebugDrawDuration,
                0,
                2.0f
            );
            
            DrawDebugSphere(
                World,
                BestTarget->GetActorLocation(),
                50.0f,
                12,
                FColor::Green,
                false,
                DebugDrawDuration
            );
        }
    }

    return BestTarget;
}