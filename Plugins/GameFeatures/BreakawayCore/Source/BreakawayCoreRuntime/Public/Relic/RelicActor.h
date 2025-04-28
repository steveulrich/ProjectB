#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Relic/RelicSettings.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "RelicActor.generated.h"

class UAbilitySystemComponent;
class UStaticMeshComponent;
class USphereComponent;
class URelicSettings;
class ABwayCharacterWithAbilities; // Forward declaration

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
    
    UPROPERTY(BlueprintReadOnly)
    int32 LastPossessingTeam;

    // --- Core Logic ---

    // Called by GA_PickupRelic on the server to attach the relic
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    virtual void OnPickedUp(ABwayCharacterWithAbilities* NewCarrier);

    // Called by GA_DropRelic, GA_ThrowRelic, GA_PassRelic on the server to detach
    UFUNCTION(BlueprintCallable, Category = "Relic|Interaction")
    virtual void OnDropped();

    // Server RPC called by GA_ThrowRelic
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_ThrowRelic(const FVector& ThrowVelocity);

    // Server RPC called by GA_PassRelic
    UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
    void Server_PassRelic(const FVector& PassVelocity);

    // Multicast RPC for cosmetic effects (e.g., throw/pass VFX/SFX)
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayThrowPassFX();
    
protected:
    // --- Components ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<USphereComponent> InteractionSphere;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> RelicMesh; // Ensure this is the RootComponent for physics replication [1]

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

    // --- Configuration ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Relic|Config")
    TObjectPtr<URelicSettings> RelicSettings; // Assume this contains ThrowForce, PassForce, SocketName etc.

    // --- Internal State Management ---
    UFUNCTION(BlueprintCallable, Category = "Relic|State")
    void SetRelicState(ERelicState NewState);

    // Internal helper to handle attachment
    void AttachToCarrier(ABwayCharacterWithAbilities* Carrier);

    // Internal helper to handle detachment and physics setup
    void DetachFromCarrier(const FVector* InitialVelocity = nullptr);
private:
    // Helper to check if pickup is allowed based on state and character request
    bool CanBePickedUpBy(ABwayCharacterWithAbilities* Character) const;
};