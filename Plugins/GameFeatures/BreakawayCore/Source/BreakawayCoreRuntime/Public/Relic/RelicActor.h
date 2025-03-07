#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Relic/RelicSettings.h"
#include "Relic/RelicStateMachine.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "RelicActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UAudioComponent;
class UAbilitySystemComponent;
class ABwayCharacterWithAbilities;
class URelicSettings;
class URelicCarrierEffectData;
class URelicAbilityRestrictionData;

/**
 * The main actor class for the Relic object
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API ARelicActor : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()
    
public:    
    ARelicActor();
    
protected:
    virtual void BeginPlay() override;
    
public:    
    virtual void Tick(float DeltaTime) override;
    
    // Called when the game starts or when spawned
    virtual void PostInitializeComponents() override;
    
    // Network setup
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    // Network replication handlers
    UFUNCTION()
    void OnRep_StateMachine();
    
    UPROPERTY(ReplicatedUsing = OnRep_StateMachine)
    FRelicStateMachine StateMachine;
    
    UPROPERTY(Replicated)
    TObjectPtr<ABwayCharacterWithAbilities> CurrentCarrier;
    
    UPROPERTY(Replicated)
    int32 LastPossessingTeam;
    // --------------------------------------------------------------
    // Components
    // --------------------------------------------------------------
    
    // Collision sphere for pickup detection
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;
    
    // Static mesh representation
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> RelicMesh;
    
    // Effects components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UNiagaraComponent> ActiveEffectComponent;
    
    // Audio component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAudioComponent> AudioComponent;
    
    // Ability system component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
    
    // IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override
    {
        return AbilitySystemComponent;
    }
    
    // --------------------------------------------------------------
    // Settings and State
    // --------------------------------------------------------------
    
    // Settings data asset
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Settings")
    TObjectPtr<URelicSettings> RelicSettings;
    
    // Current spawn location index
    UPROPERTY(BlueprintReadOnly, Category = "State")
    int32 CurrentSpawnLocationIndex;
    
    // --------------------------------------------------------------
    // Interaction Methods
    // --------------------------------------------------------------
    
    // Try to pick up the Relic
    UFUNCTION(BlueprintCallable, Category = "Relic")
    bool TryPickup(ABwayCharacterWithAbilities* Character);
    
    // Drop the Relic
    UFUNCTION(BlueprintCallable, Category = "Relic")
    bool DropRelic(bool bIsIntentional = false);
    
    // Try to score with the Relic
    UFUNCTION(BlueprintCallable, Category = "Relic")
    bool TryScore(int32 ScoringTeam);
    
    // Reset the Relic to a spawn location
    UFUNCTION(BlueprintCallable, Category = "Relic")
    void ResetRelic();
    
    // Attach the Relic to a character
    UFUNCTION(BlueprintCallable, Category = "Relic")
    void AttachToCharacter(ABwayCharacterWithAbilities* Character);
    
    // Detach the Relic from a character
    UFUNCTION(BlueprintCallable, Category = "Relic")
    void DetachFromCharacter();
    
    // Apply carrier effects to a character
    UFUNCTION(BlueprintCallable, Category = "Relic")
    void ApplyCarrierEffects(ABwayCharacterWithAbilities* Character);
    
    // Remove carrier effects from a character
    UFUNCTION(BlueprintCallable, Category = "Relic")
    void RemoveCarrierEffects(ABwayCharacterWithAbilities* Character);
    
    // --------------------------------------------------------------
    // Networking
    // --------------------------------------------------------------
    
    // Server RPC for pickup
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerTryPickup(ABwayCharacterWithAbilities* Character);
    bool ServerTryPickup_Validate(ABwayCharacterWithAbilities* Character);
    void ServerTryPickup_Implementation(ABwayCharacterWithAbilities* Character);
    
    // Server RPC for drop
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerDropRelic(bool bIsIntentional);
    bool ServerDropRelic_Validate(bool bIsIntentional);
    void ServerDropRelic_Implementation(bool bIsIntentional);
    
    // Server RPC for scoring
    UFUNCTION(Server, Reliable, WithValidation)
    void ServerTryScore(int32 ScoringTeam);
    bool ServerTryScore_Validate(int32 ScoringTeam);
    void ServerTryScore_Implementation(int32 ScoringTeam);
    
    // Multicast RPCs for visual feedback
    UFUNCTION(NetMulticast, Reliable)
    void MulticastOnStateChanged(ERelicState NewState, ERelicState PreviousState);
    void MulticastOnStateChanged_Implementation(ERelicState NewState, ERelicState PreviousState);
    
    // --------------------------------------------------------------
    // Events
    // --------------------------------------------------------------
    
    // Overlap detection
    UFUNCTION()
    void OnInteractionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    
    // State machine events
    UFUNCTION()
    void HandleStateChanged(ERelicState NewState, ERelicState PreviousState, ABwayCharacterWithAbilities* Carrier);
    
    UFUNCTION()
    void HandleScoringComplete();
    
    // --------------------------------------------------------------
    // Accessors
    // --------------------------------------------------------------
    
    // Get current state
    UFUNCTION(BlueprintPure, Category = "Relic")
    ERelicState GetState() const { return StateMachine.CurrentState; }
    
    // Get current carrier
    UFUNCTION(BlueprintPure, Category = "Relic")
    ABwayCharacterWithAbilities* GetCarrier() const { return StateMachine.CurrentCarrier; }
    
    // Get last possessing team
    UFUNCTION(BlueprintPure, Category = "Relic")
    int32 GetLastPossessingTeam() const { return StateMachine.LastPossessingTeam; }
    
    // Is the Relic being carried?
    UFUNCTION(BlueprintPure, Category = "Relic")
    bool IsCarried() const { return StateMachine.CurrentState == ERelicState::Carried; }
    
    // Is the Relic available for pickup?
    UFUNCTION(BlueprintPure, Category = "Relic")
    bool IsAvailableForPickup() const 
    { 
        return StateMachine.CurrentState == ERelicState::Neutral || 
               StateMachine.CurrentState == ERelicState::Dropped; 
    }
    
    // Get next spawn location
    UFUNCTION(BlueprintCallable, Category = "Relic")
    FVector GetNextSpawnLocation();
    
protected:
    // Handle physics simulation during different states
    void ConfigurePhysics(bool bEnablePhysics);
    
    // Update visual effects based on state
    void UpdateVisualEffects(ERelicState NewState);
    
    // Play appropriate sounds based on state changes
    void PlayStateChangeSound(ERelicState NewState, ERelicState PreviousState);
    
    // Apply an impulse when dropped
    void ApplyDropImpulse(ABwayCharacterWithAbilities* PreviousCarrier);
    
    // Cached active gameplay effect handles for carrier effects
    UPROPERTY()
    TArray<FActiveGameplayEffectHandle> ActiveEffectHandles;
    
    // Timer handles
    FTimerHandle ScoringCompleteHandle;
    
    // Client prediction
    UPROPERTY()
    bool bIsPredictingPickup;
    
    UPROPERTY()
    TObjectPtr<ABwayCharacterWithAbilities> PredictedCarrier;
    
    void ReconcileAfterPrediction(bool bSuccess);

    // Handle to track granted abilities
    UPROPERTY()
    FLyraAbilitySet_GrantedHandles RelicAbilityHandles;
};