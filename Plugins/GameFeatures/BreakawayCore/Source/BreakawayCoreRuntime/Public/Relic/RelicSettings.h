// RelicSettings.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Sound/SoundBase.h"
#include "NiagaraSystem.h"
#include "GameplayEffect.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "RelicSettings.generated.h"

/**
 * Data asset containing configurable settings for the Relic System
 */
UCLASS(BlueprintType)
class BREAKAWAYCORERUNTIME_API URelicSettings : public UDataAsset
{
    GENERATED_BODY()
    
public:
    URelicSettings();
    
    //-----------------------------------------------------------
    // Visual Representation
    //-----------------------------------------------------------
    
    // Main relic mesh
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
    TSoftObjectPtr<UStaticMesh> RelicMesh;
    
    // Material to apply to the relic mesh
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual")
    TSoftObjectPtr<UMaterialInterface> RelicMaterial;
    
    // Visual effect for neutral/idle state
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects")
    TSoftObjectPtr<UNiagaraSystem> IdleEffect;
    
    // Visual effect for carried state
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects")
    TSoftObjectPtr<UNiagaraSystem> CarriedEffect;
    
    // Visual effect for dropped state
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects")
    TSoftObjectPtr<UNiagaraSystem> DroppedEffect;
    
    // Visual effect for scoring state
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visual|Effects")
    TSoftObjectPtr<UNiagaraSystem> ScoringEffect;
    
    //-----------------------------------------------------------
    // Audio Feedback
    //-----------------------------------------------------------
    
    // Sound played when the relic is picked up
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TSoftObjectPtr<USoundBase> PickupSound;
    
    // Sound played when the relic is dropped
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TSoftObjectPtr<USoundBase> DropSound;
    
    // Sound played when the relic is scored
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TSoftObjectPtr<USoundBase> ScoringSound;
    
    // Sound played when the relic is respawned/reset
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    TSoftObjectPtr<USoundBase> ResetSound;
    
    //-----------------------------------------------------------
    // Gameplay Settings
    //-----------------------------------------------------------

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay", meta = (ClampMin = "0.0", ClampMax = "2000.0"))
    float ThrowVelocity = 1000.0f;
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float ThrowAngle = 30.0f;
    
    // How much to modify carrier's movement speed (multiplicative)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CarrierSpeedModifier = 0.8f;
    
    // Radius for pickups
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float PickupRadius = 150.0f;
    
    // If true, certain abilities will be restricted while carrying
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
    bool bRestrictAbilitiesWhileCarrying = true;

    // If true, certain abilities will be restricted while carrying
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
    FString RelicSocket = "relic_r";

    // Tags for abilities that should be restricted while carrying the relic
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
    TSoftObjectPtr<ULyraAbilitySet> RelicAbilitySet;
    
    // Tags for abilities that should be restricted while carrying the relic
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay", meta = (EditCondition = "bRestrictAbilitiesWhileCarrying"))
    FGameplayTagContainer RestrictedAbilityTags;
    
    // Gameplay effect to apply to carriers
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
    TSubclassOf<UGameplayEffect> CarrierEffect;
    
    // Points awarded for scoring
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay", meta = (ClampMin = "1", UIMin = "1"))
    int32 PointsPerScore = 1;
    
    // Auto-pickup when in radius (vs. requiring button press)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
    bool bAutoPickup = true;
    
    //-----------------------------------------------------------
    // Physics Settings
    //-----------------------------------------------------------
    
    // Multiplier for impulse applied when dropping the relic
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Physics")
    float DropImpulseMultiplier = 1.0f;
    
    // Mass of the relic when physics simulation is active
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Physics")
    float RelicMass = 10.0f;
    
    // Linear damping for physics simulation
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Physics")
    float LinearDamping = 1.0f;
    
    // Angular damping for physics simulation
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Physics")
    float AngularDamping = 1.0f;
    
    //-----------------------------------------------------------
    // Timing Settings
    //-----------------------------------------------------------
    
    // Duration for scoring confirmation/celebration
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
    float ScoringConfirmationDuration = 1.0f;
    
    //-----------------------------------------------------------
    // Network Settings
    //-----------------------------------------------------------
    
    // Amount of smoothing to apply for network replication
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Network", meta = (ClampMin = "0.0"))
    float NetworkSmoothing = 0.1f;
    
    //-----------------------------------------------------------
    // Spawn Settings
    //-----------------------------------------------------------
    
    // If true, spawn locations will be chosen randomly, otherwise round-robin
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    bool bUseRandomSpawnLocation = true;
    
    // Array of possible spawn locations
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    TArray<FVector> SpawnLocations;
    
    // Default spawn location if array is empty
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
    FVector DefaultSpawnLocation = FVector::ZeroVector;
    
    //-----------------------------------------------------------
    // Gameplay Tags
    //-----------------------------------------------------------
    
    // Tag that identifies this object as a relic
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
    FGameplayTag RelicTag;
    
    // Tag applied to characters carrying the relic
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
    FGameplayTag CarrierTag;
    
    // Tag applied when the relic is in dropped state
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
    FGameplayTag DroppedTag;
    
    // Tag applied when the relic is being scored
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tags")
    FGameplayTag ScoringTag;

    // Tag to listen for pickup of relic
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta=(GameplayTagFilter="GameplayEventTagsCategory"))
    FGameplayTag PickupEventTag;
    //-----------------------------------------------------------
    // Team Identification
    //-----------------------------------------------------------
    
    // Tag for Team 1
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teams")
    FGameplayTag Team1Tag;
    
    // Tag for Team 2
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Teams")
    FGameplayTag Team2Tag;
    
    //-----------------------------------------------------------
    // Collision Settings
    //-----------------------------------------------------------

    // Collision profile to use when the relic is neutral/idle
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision")
    FName NeutralCollisionProfileName = FName("OverlapAll");

    // Collision profile to use when the relic is being carried
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision")
    FName CarriedCollisionProfileName = FName("NoCollision");

    // Collision profile to use when the relic is dropped
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision")
    FName DroppedCollisionProfileName = FName("PhysicsActor");

    // Collision profile to use when the relic is in scoring state
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision")
    FName ScoringCollisionProfileName = FName("NoCollision");

    // Collision profile to use when the relic is resetting
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision")
    FName ResettingCollisionProfileName = FName("OverlapAll");

    // Collision profile to use when the relic is inactive
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Collision")
    FName InactiveCollisionProfileName = FName("NoCollision");
};