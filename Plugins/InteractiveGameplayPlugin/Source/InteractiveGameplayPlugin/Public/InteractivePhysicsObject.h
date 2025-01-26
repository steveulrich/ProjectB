// InteractivePhysicsObject.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/IInteractableTarget.h"
#include "Interaction/InteractionQuery.h"
#include "Interaction/InteractionOption.h"
#include "AbilitySystem/LyraAbilitySet.h"  // Contains both ULyraAbilitySet and FLyraAbilitySet_GrantedHandles
#include "GameFramework/Character.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Player/LyraPlayerState.h"
#include "GameplayTagContainer.h"
#include "InteractivePhysicsObject.generated.h"

class ALyraPlayerState;

UCLASS(BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class INTERACTIVEGAMEPLAYPLUGIN_API UInteractivePhysicsObject : public UActorComponent, public IInteractableTarget
{
    GENERATED_BODY()

public:
    UInteractivePhysicsObject();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // IInteractableTarget interface
    virtual void GatherInteractionOptions(const FInteractionQuery& InteractQuery, FInteractionOptionBuilder& OptionBuilder) override;

    // Drop the object
    UFUNCTION(BlueprintCallable, Category = "Interactive Object")
    void RequestDrop();
    
    // Public function to request pickup
    UFUNCTION(BlueprintCallable, Category = "Interactive Object")
    void RequestPickup(ALyraPlayerState* NewHolder);
    
    // Get the current holder of the object
    UFUNCTION(BlueprintPure, Category = "Interactive Object")
    ALyraPlayerState* GetCurrentHolder() const { return CurrentHolder; }

protected:
    // The ability set to grant when picked up
    UPROPERTY(EditDefaultsOnly, Category = "Interactive Object")
    TObjectPtr<ULyraAbilitySet> PickupAbilitySet;
    
    UPROPERTY(EditDefaultsOnly, Category = "Interactive Object")
    TSubclassOf<UGameplayAbility> InteractionAbilityToGrant;

    // Socket name to attach to when picked up
    UPROPERTY(EditDefaultsOnly, Category = "Interactive Object")
    FName AttachSocketName;

    // Text display options
    UPROPERTY(EditDefaultsOnly, Category = "Interactive Object|Display")
    FText InteractionText;

    UPROPERTY(EditDefaultsOnly, Category = "Interactive Object|Display")
    FText InteractionSubText;

    // Optional widget override
    UPROPERTY(EditDefaultsOnly, Category = "Interactive Object|Display")
    TSoftClassPtr<UUserWidget> InteractionWidgetClass;

    // Current holder of the object
    UPROPERTY(ReplicatedUsing = OnRep_CurrentHolder)
    TObjectPtr<ALyraPlayerState> CurrentHolder;

    // Handles to granted abilities for cleanup
    FLyraAbilitySet_GrantedHandles GrantedHandles;

    // Original physics settings cached when picked up
    bool bWasSimulatingPhysics;
    FVector LastLinearVelocity;
    FVector LastAngularVelocity;

    UFUNCTION()
    void OnRep_CurrentHolder(ALyraPlayerState* PreviousHolder);

    // RPC to handle pickup/drop
    UFUNCTION(Server, Reliable)
    void Server_HandlePickup(ALyraPlayerState* NewHolder);

    UFUNCTION(Server, Reliable)
    void Server_HandleDrop();

    void HandlePickup(ALyraPlayerState* NewHolder);
    void HandleDrop();

    // Physics state management
    void EnablePhysics();
    void DisablePhysics();
};