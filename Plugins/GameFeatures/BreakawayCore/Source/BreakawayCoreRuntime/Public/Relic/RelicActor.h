#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Relic/RelicSettings.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystemInterface.h"
#include "RelicActor.generated.h"

class UAbilitySystemComponent;
class UStaticMeshComponent;
class USphereComponent;
class URelicSettings;
class URelicMovementReplicationComponent;
class ABwayCharacterWithAbilities; // Forward declaration
class UNiagaraComponent;
class UMaterialInstanceDynamic;

// Enum defining the possible states of the Relic
UENUM(BlueprintType)
enum class ERelicState : uint8
{
    Neutral			UMETA(DisplayName = "Neutral"), // On the ground, available
    Carried			UMETA(DisplayName = "Carried"), // Attached to a player
    Dropped			UMETA(DisplayName = "Dropped"), // Recently dropped, maybe temporary cooldown
    Thrown			UMETA(DisplayName = "Thrown"), // Moving via physics after a throw
    BeingPassed		UMETA(DisplayName = "BeingPassed"), // Moving via physics after a pass
    PendingRequest	UMETA(DisplayName = "PendingRequest"), // Optional: Waiting for a specific requester
    Scoring			UMETA(DisplayName = "Scoring"), // In a score zone
    Resetting		UMETA(DisplayName = "Resetting") // After scoring, before respawn
};

/**
 * The main actor class for the Relic object
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API ARelicActor : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()
    
public:    
    ARelicActor();
    
    //~ AActor Interface
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;

    UFUNCTION()
    virtual void OnInteractionSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    //~ End AActor Interface

    //~ IAbilitySystemInterface
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    //~ End IAbilitySystemInterface

    UFUNCTION(BlueprintCallable, Category = "Relic")
    void InitializeRelicData(const URelicSettings* InRelicSettings);
    
    // --- Replication ---

    // Replicated state variable with notification function
    UPROPERTY(ReplicatedUsing = OnRep_CurrentState, BlueprintReadOnly, Category = "Relic|State")
    ERelicState CurrentState = ERelicState::Neutral;
    
    UFUNCTION()
    virtual void OnRep_CurrentState();

    // Replicated reference to the character currently carrying the relic
    UPROPERTY(ReplicatedUsing = OnRep_CurrentCarrier, BlueprintReadOnly, Category = "Relic|State")
    TObjectPtr<ABwayCharacterWithAbilities> CurrentCarrier = nullptr;
    
    UFUNCTION()
    virtual void OnRep_CurrentCarrier();
    
    UPROPERTY(BlueprintReadOnly, Replicated)
    int32 LastPossessingTeam = -1;

    UPROPERTY(BlueprintReadOnly, Replicated)
    bool bHasScoredThisRound = false;

    // --- Core Logic ---

    // Called by GA_PickupRelic on the server to attach the relic
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    virtual void OnPickedUp(ABwayCharacterWithAbilities* NewCarrier);

    // Called by GA_DropRelic, GA_ThrowRelic, GA_PassRelic on the server to detach
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    virtual void OnDropped();

    // Called when relic enters a goal volume (server only)
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    void OnEnteredGoal(int32 ScoringTeam);

    // Clear the scoring flag (called on round reset)
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    void ClearScoringFlag();

    // Server RPC called by GA_ThrowRelic
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_ThrowRelic(const FVector& ThrowVelocity);

    // Server RPC called by GA_PassRelic
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_PassRelic(const FVector& PassVelocity);

    // Multicast RPC for cosmetic effects (e.g., throw/pass VFX/SFX)
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayThrowPassFX();

    // Client RPC for immediate throw prediction
    UFUNCTION(Client, Unreliable)
    void ClientPredictThrow(const FVector& ThrowVelocity);

    // Helper to check if pickup is allowed based on state and character request
    bool CanBePickedUpBy(ABwayCharacterWithAbilities* Character) const;

    // Inline function for getting the relic settings
    FORCEINLINE const URelicSettings* GetRelicSettings() const
    {
        return RelicSettings;
    }

    // Accessor for current state (needed by goal volume)
    FORCEINLINE ERelicState GetCurrentState() const
    {
        return CurrentState;
    }
    
protected:
    // --- Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> RelicMesh; // Ensure this is the RootComponent for physics replication [1]

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    /** Component for smooth network replication of physics movement */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<URelicMovementReplicationComponent> MovementReplicationComponent;

    // --- Configuration ---
    UPROPERTY(BlueprintReadOnly, Category = "Relic|Config")
    TObjectPtr<const URelicSettings> RelicSettings; // Assume this contains ThrowForce, PassForce, SocketName etc.

    /** Handle to the ability set granted to the carrier's PlayerState ASC */
    FLyraAbilitySet_GrantedHandles GrantedCarrierSetHandle; // Use the correct Lyra type

    /** Preloaded pointer to the Relic Ability Set (Server-side) */
    UPROPERTY(Transient) // Transient as it's loaded at runtime from RelicSettings
    TObjectPtr<ULyraAbilitySet> LoadedRelicAbilitySet;
    
    // --- Internal State Management ---
    UFUNCTION(BlueprintCallable, Category = "Relic|State")
    void SetRelicState(ERelicState NewState);

    // Internal helper to handle attachment
    void AttachToCarrier(ABwayCharacterWithAbilities* Carrier);

    // Internal helper to handle detachment and physics setup
    void DetachFromCarrier(const FVector* InitialVelocity = nullptr);

    //Helper to apply visual/audio configuration from settings
    void ApplyRelicConfiguration();

    // --- Visual/Audio Feedback ---
    
    // VFX components for different states
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Relic|VFX")
    TObjectPtr<UNiagaraComponent> IdleEffectComponent;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Relic|VFX")
    TObjectPtr<UNiagaraComponent> CarriedEffectComponent;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Relic|VFX")
    TObjectPtr<UNiagaraComponent> DroppedEffectComponent;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Relic|VFX")
    TObjectPtr<UNiagaraComponent> ScoringEffectComponent;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Relic|VFX")
    TObjectPtr<UNiagaraComponent> TrailEffectComponent;
    
    // Dynamic material instance for team color tinting
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> TeamColorMaterialInstance;
    
    // Helper functions for VFX/Audio
    void UpdateStateVFX(ERelicState NewState);
    void CleanupStateVFX(ERelicState OldState);
    void SpawnTrailEffect();
    void DestroyTrailEffect();
    void UpdateTeamColorTinting();
    void PlayStateAudio(ERelicState State);
};