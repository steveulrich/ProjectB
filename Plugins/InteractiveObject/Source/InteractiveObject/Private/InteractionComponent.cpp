// InteractionComponent.cpp
#include "InteractionComponent.h"
#include "Net/UnrealNetwork.h"
#include "InteractionLogging.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"


UInteractionComponent::UInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    
    bCanBePickedUp = true;
    bCanBeThrown = true;
    bCanBePassed = true;
    ThrowStrengthMultiplier = 1000.0f;
    PassStrengthMultiplier = 800.0f;
    InteractableTag = FGameplayTag::RequestGameplayTag(FName("Interactable")); // Default Interactable Tag
    
    CurrentState = EInteractionState::Free;
    CurrentOwner = nullptr;
    HoldingActor = nullptr;
}

void UInteractionComponent::OnRegister()
{
    Super::OnRegister();
    UE_LOG(LogInteraction, Log, TEXT("InteractionComponent BeginPlay - Owner: %s"), *GetOwner()->GetName());
    SetIsReplicated(true);
}

void UInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UInteractionComponent, CurrentState);
    DOREPLIFETIME(UInteractionComponent, CurrentOwner);
}

void UInteractionComponent::OnRep_CurrentState()
{
    UE_LOG(LogInteraction, Verbose, TEXT("%s - State Replicated: %s"), 
        *GetOwner()->GetName(),
        *UEnum::GetValueAsString(CurrentState));
    OnInteractionStateChanged.Broadcast(CurrentState);
}

void UInteractionComponent::HandleStateChange(EInteractionState NewState)
{
    UE_LOG(LogInteraction, Log, TEXT("%s - State Change: %s -> %s"), 
       *GetOwner()->GetName(),
       *UEnum::GetValueAsString(CurrentState),
       *UEnum::GetValueAsString(NewState));
       
    CurrentState = NewState;
    OnInteractionStateChanged.Broadcast(NewState);
}

void UInteractionComponent::HandleOwnerChange(AActor* NewOwner)
{
    UE_LOG(LogInteraction, Log, TEXT("%s - Owner Change: %s -> %s"), 
        *GetOwner()->GetName(),
        CurrentOwner ? *CurrentOwner->GetName() : TEXT("None"),
        NewOwner ? *NewOwner->GetName() : TEXT("None"));
        
    AActor* PreviousOwner = CurrentOwner;
    CurrentOwner = NewOwner;
    OnOwnerChanged.Broadcast(NewOwner, PreviousOwner);
}

bool UInteractionComponent::CanBePickedUp(AActor* InteractingActor) const
{
    return bCanBePickedUp && 
           CurrentState == EInteractionState::Free && 
           InteractingActor != nullptr;
}

bool UInteractionComponent::CanBeThrown(AActor* InteractingActor) const
{
    return bCanBeThrown && 
           CurrentState == EInteractionState::Held && 
           CurrentOwner == InteractingActor;
}

bool UInteractionComponent::CanBePassed(AActor* InteractingActor, AActor* TargetActor) const
{
    return bCanBePassed && 
           CurrentState == EInteractionState::Held && 
           CurrentOwner == InteractingActor && 
           TargetActor != nullptr && 
           TargetActor != InteractingActor;
}

void UInteractionComponent::SetInteractionState(EInteractionState NewState)
{
    if (GetOwnerRole() < ROLE_Authority)
    {
        return;
    }
    HandleStateChange(NewState);
}

void UInteractionComponent::SetOwner(AActor* NewOwner)
{
    if (GetOwnerRole() < ROLE_Authority)
    {
        return;
    }
    HandleOwnerChange(NewOwner);

    // Update held object reference
    if (NewOwner)
    {
        HoldingActor = NewOwner;
    }
    else
    {
        HoldingActor.Reset();
    }
}

AActor* UInteractionComponent::GetCurrentlyHeldObject() const
{
    return HoldingActor.Get();
}

void UInteractionComponent::ThrowHeldObject(const FVector& Velocity)
{
    if (GetOwnerRole() < ROLE_Authority)
    {
        UE_LOG(LogInteraction, Warning, TEXT("ThrowHeldObject called on client - Ignoring"));
        return;
    }

    if (AActor* ObjectToThrow = HoldingActor.Get())
    {
        UE_LOG(LogInteraction, Log, TEXT("Throwing object %s with velocity %s"), 
            *ObjectToThrow->GetName(), 
            *Velocity.ToString());
        
        // Get the physics component
        UPrimitiveComponent* PhysicsComponent = Cast<UPrimitiveComponent>(ObjectToThrow->GetRootComponent());
        if (!PhysicsComponent)
        {
            UE_LOG(LogInteraction, Error, TEXT("Failed to throw %s - No physics component found"), 
                *ObjectToThrow->GetName());
            return;
        }

        // Detach from holder
        FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
        ObjectToThrow->DetachFromActor(DetachRules);

        // Enable physics and apply velocity
        PhysicsComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        PhysicsComponent->SetSimulatePhysics(true);
        PhysicsComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
        PhysicsComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
        PhysicsComponent->AddImpulse(Velocity, NAME_None, true);

        UE_LOG(LogInteraction, Verbose, TEXT("Physics applied to %s"), *ObjectToThrow->GetName());

        // Update state
        SetInteractionState(EInteractionState::Thrown);
        SetOwner(nullptr);
    }
    else
    {
        UE_LOG(LogInteraction, Warning, TEXT("%s attempted to throw with no held object"), 
            *GetOwner()->GetName());
    }
}