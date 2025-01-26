// Private/InteractionHandlerComponent.cpp
#include "InteractionHandlerComponent.h"
#include "DrawDebugHelpers.h"
#include "InteractionComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UInteractionHandlerComponent::UInteractionHandlerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    InteractionRange = 200.0f;
    PassTargetRange = 400.0f; // Default Pass Target Range, adjust as needed
    HoldSocketName = FName("hand_r"); // Default socket name, can be changed in BP
    InteractRadius = 150.0f;
    InteractableTagFilter = FGameplayTag::RequestGameplayTag(FName("Interactable")); // Default Interactable Tag Filter
}

void UInteractionHandlerComponent::OnRegister()
{
    Super::OnRegister();
    SetIsReplicated(true);
}

void UInteractionHandlerComponent::BeginPlay()
{
    Super::BeginPlay();
}

bool UInteractionHandlerComponent::Server_RequestPickup_Validate(UInteractionComponent* Target)
{
    return true;
}

void UInteractionHandlerComponent::Server_RequestPickup_Implementation(UInteractionComponent* Target)
{
    if (!ValidateInteraction(Target) || !Target->CanBePickedUp(GetOwner()))
    {
        return;
    }

    // Get the physical representation (usually a StaticMeshComponent or SkeletalMeshComponent)
    UPrimitiveComponent* TargetPhysics = Cast<UPrimitiveComponent>(Target->GetOwner()->GetRootComponent());
    if (!TargetPhysics)
    {
        return;
    }

    // Get the character owner
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character)
    {
        return;
    }

    // Disable physics and attach to socket
    TargetPhysics->SetSimulatePhysics(false);
    TargetPhysics->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    
    FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, EAttachmentRule::SnapToTarget, 
                                        EAttachmentRule::KeepWorld, true);
    Target->GetOwner()->AttachToComponent(Character->GetMesh(), 
                                        AttachRules, 
                                        HoldSocketName);

    // Update interaction state
    Target->SetInteractionState(EInteractionState::Held);
    Target->SetOwner(GetOwner());
}

bool UInteractionHandlerComponent::Server_RequestDrop_Validate(UInteractionComponent* Target)
{
    return true;
}

void UInteractionHandlerComponent::Server_RequestDrop_Implementation(UInteractionComponent* Target)
{
    if (!ValidateInteraction(Target) || Target->GetCurrentState() != EInteractionState::Held)
    {
        return;
    }

    // Get the physical representation
    UPrimitiveComponent* TargetPhysics = Cast<UPrimitiveComponent>(Target->GetOwner()->GetRootComponent());
    if (!TargetPhysics)
    {
        return;
    }

    // Detach and enable physics
    FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
    Target->GetOwner()->DetachFromActor(DetachRules);

    TargetPhysics->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TargetPhysics->SetSimulatePhysics(true);

    // Update interaction state
    Target->SetInteractionState(EInteractionState::Free);
    Target->SetOwner(nullptr);
}

bool UInteractionHandlerComponent::Server_RequestThrow_Validate(UInteractionComponent* Target, 
                                                              const FVector& Direction, 
                                                              float Strength)
{
    return true;
}

void UInteractionHandlerComponent::Server_RequestThrow_Implementation(UInteractionComponent* Target, 
                                                                    const FVector& Direction, 
                                                                    float Strength)
{
    if (!ValidateInteraction(Target) || !Target->CanBeThrown(GetOwner()))
    {
        return;
    }

    UPrimitiveComponent* TargetPhysics = Cast<UPrimitiveComponent>(Target->GetOwner()->GetRootComponent());
    if (!TargetPhysics)
    {
        return;
    }

    // Detach and enable physics
    FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
    Target->GetOwner()->DetachFromActor(DetachRules);

    TargetPhysics->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TargetPhysics->SetSimulatePhysics(true);

    // Apply throw force
    float FinalStrength = Strength * Target->ThrowStrengthMultiplier;
    TargetPhysics->AddImpulse(Direction * FinalStrength, NAME_None, true);

    // Update interaction state
    Target->SetInteractionState(EInteractionState::Thrown);
    Target->SetOwner(nullptr);
}

bool UInteractionHandlerComponent::Server_RequestPass_Validate(UInteractionComponent* Target, 
                                                             AActor* TargetActor)
{
    return true;
}

void UInteractionHandlerComponent::Server_RequestPass_Implementation(UInteractionComponent* Target, 
                                                                   AActor* TargetActor)
{
    if (!ValidateInteraction(Target) || !Target->CanBePassed(GetOwner(), TargetActor))
    {
        return;
    }

    UPrimitiveComponent* TargetPhysics = Cast<UPrimitiveComponent>(Target->GetOwner()->GetRootComponent());
    if (!TargetPhysics)
    {
        return;
    } // Early out if no physics component

    FVector Direction = GetOwner()->GetActorForwardVector(); // Default forward pass direction
    if (TargetActor) // Targeted pass
    {
        // Calculate pass trajectory to target
        FVector StartLocation = GetOwner()->GetActorLocation();
        Direction = (TargetActor->GetActorLocation() - StartLocation).GetSafeNormal();
    }
    
    // Add some arc to the pass
    Direction.Z += 0.2f; // Adjust this value to change the arc height
    Direction.Normalize();

    // Detach and enable physics
    FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
    Target->GetOwner()->DetachFromActor(DetachRules);

    TargetPhysics->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    TargetPhysics->SetSimulatePhysics(true);

    // Apply pass force
    float PassStrength = Target->PassStrengthMultiplier * (TargetActor ? 1.0f : 0.5f); // Reduce strength for forward pass
    TargetPhysics->AddImpulse(Direction * PassStrength, NAME_None, true);

    // Update interaction state
    Target->SetInteractionState(EInteractionState::Passing);
    Target->SetOwner(nullptr);
}

UInteractionComponent* UInteractionHandlerComponent::FindBestInteractable() const
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character)
    {
        return nullptr;
    }

    // Get character's view point
    FVector Location;
    FRotator Rotation;
    Character->Controller->GetPlayerViewPoint(Location, Rotation);

    // Setup trace parameters
    FVector Start = Location;
    FVector End = Start + (Rotation.Vector() * InteractionRange);
    
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(Character);
    
    FHitResult HitResult;
    bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);
    UInteractionComponent* LineTraceInteractable = nullptr;

    if (bHit)
    {
        // Check if the hit actor has an interaction component
        AActor* HitActor = HitResult.GetActor();
        if (HitActor)
        {
            LineTraceInteractable = HitActor->FindComponentByClass<UInteractionComponent>();
            if (LineTraceInteractable && !LineTraceInteractable->GetOwner()->ActorHasTag(InteractableTagFilter.GetTagName()))
            {
                LineTraceInteractable = nullptr; // Ignore if tag doesn't match
            }
        }
    }
    
    TArray<UInteractionComponent*> RadiusInteractables = FindInteractablesInRadius();
    UInteractionComponent* BestInteractable = LineTraceInteractable; // Prioritize line trace target
    
    if (!BestInteractable && RadiusInteractables.Num() > 0)
    {
        // If no line trace hit, but radius found interactables, pick the closest
        BestInteractable = RadiusInteractables[0]; // Default to first in array (TODO:could refine to closest later)
    }
    
    if (bDebugDrawInteraction)
    {
        DrawDebugLine(
            GetWorld(),
            Start,
            End,
            LineTraceInteractable ? FColor::Green : FColor::Red,
            false, DebugDrawDuration);
            
        if (BestInteractable)
        {
            DrawDebugSphere(
                GetWorld(),
                BestInteractable->GetOwner()->GetActorLocation(),
                50.0f,
                8,
                FColor::Yellow,
                false,
                DebugDrawDuration
            );
        }
    }
    
    return BestInteractable;
}

TArray<UInteractionComponent*> UInteractionHandlerComponent::FindInteractablesInRadius() const
{
    TArray<UInteractionComponent*> FoundInteractables;
    
    FVector CenterLocation = GetOwner()->GetActorLocation();
    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetOwner());
    
    
    bool bOverlap = GetWorld()->OverlapMultiByChannel(
        OverlapResults,
        CenterLocation,
        FQuat::Identity,
        ECC_Visibility, // Or ECC_WorldDynamic if your interactables are dynamic objects
        FCollisionShape::MakeSphere(InteractRadius),
        QueryParams
    );
    
    if( bDebugDrawInteraction )
    {
        // Set the sphere's color (red in this case)
        FColor SphereColor = (bOverlap ? FColor::Green : FColor::Red);

        // Set the thickness of the sphere's lines
        float Thickness = 2.0f;

        // Draw the debug sphere
        DrawDebugSphere(GetWorld(), CenterLocation, InteractRadius, 16, SphereColor, false, DebugDrawDuration, 0, Thickness);
    }
    
    if (bOverlap)
    {
        for (const FOverlapResult& OverlapResult : OverlapResults)
        {
            AActor* OverlapActor = OverlapResult.GetActor();
            if (OverlapActor)
            {
                UInteractionComponent* InteractionComp = OverlapActor->FindComponentByClass<UInteractionComponent>();
                if (InteractionComp && InteractionComp->InteractableTag == InteractableTagFilter) // Check for tag filter
                {
                    FoundInteractables.Add(InteractionComp);
                }
            }
        }
    }
    
    return FoundInteractables;
}

TArray<AActor*> UInteractionHandlerComponent::FindValidPassTargets() const
{
    TArray<AActor*> ValidTargets;
    
    // Find all characters in range
    TArray<AActor*> Characters;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), Characters);

    ACharacter* Owner = Cast<ACharacter>(GetOwner());
    if (!Owner)
    {
        return ValidTargets;
    }

    // Filter by distance and line of sight
    for (AActor* Character : Characters)
    {
        if (Character == Owner)
        {
            continue;
        }

        float Distance = FVector::Distance(Owner->GetActorLocation(), Character->GetActorLocation());
        if (Distance <= PassTargetRange) // Pass range is typically larger than pickup range
        {
            // Check line of sight
            FHitResult HitResult;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(Owner);
            
            bool bHasLineOfSight = !GetWorld()->LineTraceSingleByChannel(
                HitResult,
                Owner->GetActorLocation(),
                Character->GetActorLocation(),
                ECC_Visibility,
                QueryParams
            );

            if (bHasLineOfSight)
            {
                ValidTargets.Add(Character);
            }
        }
    }

    return ValidTargets;
}

bool UInteractionHandlerComponent::ValidateInteraction(const UInteractionComponent* Target) const
{
    if (!Target || !GetOwner() || !Target->GetOwner())
    {
        return false;
    }

    // Check if we have authority
    if (!GetOwner()->HasAuthority())
    {
        return false;
    }

    return true;
}