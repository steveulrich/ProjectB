#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "Relic/RelicSettings.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystemInterface.h"
#include "RelicActor.generated.h"

class UAbilitySystemComponent;
class UStaticMeshComponent;
class USceneComponent;
class USphereComponent;
class URelicSettings;
class URelicMovementReplicationComponent;
class ABwayCharacterWithAbilities; // Forward declaration
class ABwayPlayerState;
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
    Resetting		UMETA(DisplayName = "Resetting"), // After scoring, before respawn
    Passed			UMETA(DisplayName = "Passed") // Alias for pass-in-flight state
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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
    
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_LastPossessingTeam)
    int32 LastPossessingTeam = -1;

    UFUNCTION()
    void OnRep_LastPossessingTeam();

    /** True while the Relic is in flight from a teammate pass (BeingPassed / Passed). */
    UPROPERTY(BlueprintReadOnly, Replicated, Category = "Relic|State")
    bool bIsPassInFlight = false;

    /** Team index (0 or 1) that initiated the currently in-flight pass, or INDEX_NONE. */
    UPROPERTY(BlueprintReadOnly, Replicated, Category = "Relic|State")
    int32 PassingTeamIndex = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Replicated)
    bool bHasScoredThisRound = false;

    /** Authority-only scorer identity retained after throws, passes, and drops. */
    UFUNCTION(BlueprintPure, Category = "Relic|State")
    ABwayPlayerState* GetLastPossessingPlayerState() const;

    // --- Core Logic ---

    // Called by GA_PickupRelic on the server to attach the relic
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    virtual void OnPickedUp(ABwayCharacterWithAbilities* NewCarrier);

    // Called by GA_DropRelic, GA_ThrowRelic, GA_PassRelic on the server to detach
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    virtual void OnDropped();

    /**
     * Server-only: drop the relic with an explicit or default fumble impulse and optional attacker attribution.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Relic|Interaction")
    void DropRelic(const FVector& ImpulseVelocity = FVector::ZeroVector, AActor* InstigatorActor = nullptr, bool bIsForcedFumble = true);

    /**
     * Server-only: accumulate carrier damage and force a fumble once FumbleDamageThreshold (50.0) is reached.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Relic|Interaction")
    bool HandleCarrierDamage(float DamageAmount, AActor* InstigatorActor = nullptr);

    /**
     * Server-only: drop the relic because the carrier took damage or hard CC.
     * Increments ABwayPlayerState::ForcedFumbles and knocks the relic off the carrier.
     * No-op if not currently Carried.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Relic|Interaction")
    void ForceFumbleFromDamage();

    void ForceFumbleFromDamageWithInstigator(AActor* InstigatorActor);

    /** Compute charged ballistic throw velocity for a given aim direction and charge duration. */
    UFUNCTION(BlueprintPure, Category = "Relic|Ballistics")
    FVector ComputeChargedThrowVelocity(const FVector& AimDirection, float ChargeTimeSeconds) const;

    /** Compute directed pass velocity toward TargetLocation using PassSpeed (1500). */
    UFUNCTION(BlueprintPure, Category = "Relic|Ballistics")
    FVector ComputeDirectedPassVelocity(const FVector& TargetLocation) const;

    /** Execute a charged throw using ComputeChargedThrowVelocity. */
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    void ThrowRelicCharged(float ChargeTimeSeconds, const FVector& AimDirection = FVector::ZeroVector);

    /** True while PickupCooldownAfterDrop (0.5s) blocks the dropping/fumbling carrier from re-acquiring the Relic. */
    UFUNCTION(BlueprintPure, Category = "Relic|Interaction")
    bool IsDropPickupCooldownActiveFor(const ABwayCharacterWithAbilities* Character) const;

    /** Resolve canonical Breakaway team index (0 or 1, or INDEX_NONE) for a character. */
    int32 ResolveCharacterTeamIndex(const ABwayCharacterWithAbilities* Character) const;

    /** Cumulative carrier damage since current possession began. */
    UFUNCTION(BlueprintPure, Category = "Relic|State")
    float GetAccumulatedCarrierDamage() const { return AccumulatedCarrierDamage; }

    // Called when relic enters a goal volume (server only)
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    void OnEnteredGoal(int32 ScoringTeam);

    // Clear the scoring flag (called on round reset)
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    void ClearScoringFlag();

    // Mark the relic as resetting before a round reset moves it back to spawn.
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    void BeginResettingState();

    // Server RPC called by GA_ThrowRelic
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_ThrowRelic(const FVector& ThrowVelocity);

    // Server RPC called by GA_PassRelic
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_PassRelic(const FVector& PassVelocity);

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

    UFUNCTION()
    void HandleRelicMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
    
protected:
    /** Non-owning identity; reset at the round boundary and never sent to clients. */
    UPROPERTY(Transient)
    TWeakObjectPtr<ABwayPlayerState> LastPossessingPlayerState;

    /** Carrier who most recently dropped/fumbled/released the Relic, used for PickupCooldownAfterDrop. */
    UPROPERTY(Transient)
    TWeakObjectPtr<ABwayCharacterWithAbilities> LastDroppedCarrier;

    double LastDropWorldTime = -1000.0;

    float AccumulatedCarrierDamage = 0.0f;

    UPROPERTY(Transient)
    TWeakObjectPtr<ABwayPlayerState> LastCarrierDamageInstigatorPS;

    UPROPERTY(Transient)
    TWeakObjectPtr<UAbilitySystemComponent> BoundCarrierASC;

    FDelegateHandle CarrierTagEventHandle;

    void BindCarrierCCTagListener(ABwayCharacterWithAbilities* Carrier);
    void UnbindCarrierCCTagListener();
    void HandleCarrierGameplayTagChanged(const FGameplayTag Tag, int32 NewCount);

    // --- Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> RelicMesh; // Ensure this is the RootComponent for physics replication [1]

    /** Stable local parent keeps cosmetic children out of the replicated mesh's attachment list. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USceneComponent> VisualEffectsRoot;

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

    /** Native pass ability granted alongside the authored carrier set. */
    FGameplayAbilitySpecHandle GrantedPassAbilityHandle;

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

    /**
     * Shared drop path used by OnDropped (voluntary / reset) and ForceFumbleFromDamage.
     * @param InitialVelocity Optional velocity change applied after physics is enabled.
     * @param bRecordForcedFumble When true, increments ForcedFumbles on the resolved attacker.
     * @param InstigatorActor Optional attacker actor that caused the forced fumble.
     */
    void DropFromCarrierInternal(const FVector* InitialVelocity, bool bRecordForcedFumble, AActor* InstigatorActor = nullptr);

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
    void PlayInteractionFeedback(ERelicFeedbackEvent Event);
    virtual void PlayLocalStateAudio(ERelicState State, USoundBase* Sound, const FVector& Location, uint32 EventId);
    virtual void PlayLocalInteractionVFX(ERelicFeedbackEvent Event, UNiagaraSystem* Effect, const FVector& Location, uint32 EventId);

    // Transient accepted events are not replayed by a state RepNotify or late join.
    // Carry the resolved asset and event location so delivery does not depend on
    // settings, carrier, or movement replication arriving in the same frame.
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayStateAudio(ERelicState State, USoundBase* Sound, FVector_NetQuantize Location, uint32 EventId);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_PlayInteractionFeedback(ERelicFeedbackEvent Event, USoundBase* Sound, UNiagaraSystem* Effect,
        FVector_NetQuantize Location, uint32 EventId);

    uint32 NextAudioEventId = 0;
    uint32 LastReceivedInteractionEventId = 0;
};
