// InteractivePhysicsObject.cpp
#include "InteractivePhysicsObject.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Character.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "AbilitySystem/LyraAbilitySet.h"

// Create the gameplay tags we need
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Interaction_Pickup, "Interaction.Action.Pickup");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_State_Holding_InteractiveObject, "State.Holding.InteractiveObject");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Ability_Type_Override_When_Holding, "Ability.Type.OverrideWhenHolding");

UInteractivePhysicsObject::UInteractivePhysicsObject()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    
    // Default socket name - can be overridden in blueprint
    AttachSocketName = FName("hand_r");
    // Default interaction text
    InteractionText = FText::FromString(TEXT("Pick Up"));
}

void UInteractivePhysicsObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UInteractivePhysicsObject, CurrentHolder);
}

void UInteractivePhysicsObject::BeginPlay()
{
    Super::BeginPlay();

    // Ensure physics is enabled initially
    EnablePhysics();
}

void UInteractivePhysicsObject::GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& OptionBuilder)
{
    // Only allow pickup if no one is holding it
    if (!CurrentHolder)
    {
        // Check if we have a valid requesting controller
        AController* RequestingController = InteractQuery.RequestingController.Get();
        if (RequestingController)
        {
            ALyraPlayerState* PlayerState = RequestingController->GetPlayerState<ALyraPlayerState>();
            if (PlayerState)
            {
                FInteractionOption PickupOption;
                PickupOption.Text = InteractionText;
                PickupOption.SubText = InteractionSubText;
                PickupOption.InteractionAbilityToGrant = InteractionAbilityToGrant;
                PickupOption.InteractionWidgetClass = InteractionWidgetClass;

                OptionBuilder.AddInteractionOption(PickupOption);
            }
        }
    }
}

void UInteractivePhysicsObject::RequestDrop()
{
    if (GetOwnerRole() < ROLE_Authority)
    {
        Server_HandleDrop();
    }
    else
    {
        HandleDrop();
    }
}

void UInteractivePhysicsObject::Server_HandlePickup_Implementation(ALyraPlayerState* NewHolder)
{
    // Server received pickup request from client, perform the pickup
    HandlePickup(NewHolder);
}

void UInteractivePhysicsObject::Server_HandleDrop_Implementation()
{
    HandleDrop();
}

void UInteractivePhysicsObject::RequestPickup(ALyraPlayerState* NewHolder)
{
    if (GetOwnerRole() < ROLE_Authority)
    {
        // Client just sends request to server
        Server_HandlePickup(NewHolder);
    }
    else
    {
        // Server performs the pickup
        HandlePickup(NewHolder);
    }
}

void UInteractivePhysicsObject::HandlePickup(ALyraPlayerState* NewHolder)
{
    if (!NewHolder || CurrentHolder)
        return;

    CurrentHolder = NewHolder;
    
    // Disable physics and attach to player
    DisablePhysics();
    
    if (AActor* OwningActor = GetOwner())
    {
        if (ACharacter* HolderPawn = Cast<ACharacter>(NewHolder->GetPawn()))
        {
            OwningActor->AttachToComponent(
                HolderPawn->GetMesh(),
                FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                AttachSocketName);
        }
    }

    // Grant abilities from the ability set
    if (ULyraAbilitySystemComponent* ASC = NewHolder->GetLyraAbilitySystemComponent())
    {
        // Add the holding state tag
        ASC->AddLooseGameplayTag(TAG_State_Holding_InteractiveObject);
        
        // Remove abilities that should be overridden
        FGameplayTagContainer TagsToCancel;
        TagsToCancel.AddTag(TAG_Ability_Type_Override_When_Holding);
        ASC->CancelAbilities(&TagsToCancel);
        
        // Grant new abilities
        PickupAbilitySet->GiveToAbilitySystem(ASC, &GrantedHandles);
    }
}

void UInteractivePhysicsObject::HandleDrop()
{
    if (!CurrentHolder)
        return;

    // Remove granted abilities
    if (ULyraAbilitySystemComponent* ASC = CurrentHolder->GetLyraAbilitySystemComponent())
    {
        ASC->RemoveLooseGameplayTag(TAG_State_Holding_InteractiveObject);
        GrantedHandles.TakeFromAbilitySystem(ASC);
    }

    // Detach from player and enable physics
    if (AActor* OwningActor = GetOwner())
    {
        OwningActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    }
    
    EnablePhysics();
    
    CurrentHolder = nullptr;
}

void UInteractivePhysicsObject::OnRep_CurrentHolder(ALyraPlayerState* PreviousHolder)
{
    // Handle visual updates for clients
    if (CurrentHolder)
    {
        DisablePhysics();
        
        if (AActor* OwningActor = GetOwner())
        {
            if (ACharacter* HolderPawn = Cast<ACharacter>(CurrentHolder->GetPawn()))
            {
                OwningActor->AttachToComponent(
                    HolderPawn->GetMesh(),
                    FAttachmentTransformRules::SnapToTargetNotIncludingScale,
                    AttachSocketName);
            }
        }
    }
    else
    {
        if (AActor* OwningActor = GetOwner())
        {
            OwningActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
        }
        EnablePhysics();
    }
}

void UInteractivePhysicsObject::EnablePhysics()
{
    if (UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
    {
        PrimComponent->SetSimulatePhysics(true);
        if (bWasSimulatingPhysics)
        {
            PrimComponent->SetPhysicsLinearVelocity(LastLinearVelocity);
            PrimComponent->SetPhysicsAngularVelocityInDegrees(LastAngularVelocity);
        }
    }
}

void UInteractivePhysicsObject::DisablePhysics()
{
    if (UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
    {
        // Cache the current physics state
        bWasSimulatingPhysics = PrimComponent->IsSimulatingPhysics();
        LastLinearVelocity = PrimComponent->GetPhysicsLinearVelocity();
        LastAngularVelocity = PrimComponent->GetPhysicsAngularVelocityInDegrees();
        
        // Disable physics
        PrimComponent->SetSimulatePhysics(false);
    }
}