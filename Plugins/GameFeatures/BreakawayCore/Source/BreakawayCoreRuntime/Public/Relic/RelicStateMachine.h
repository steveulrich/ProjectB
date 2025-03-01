#pragma once

#include "CoreMinimal.h"
#include "RelicStateMachine.generated.h"

class ABwayCharacterWithAbilities;

/**
 * States for the Relic
 */
UENUM(BlueprintType)
enum class ERelicState : uint8
{
    Inactive,   // Not in play (before match, between rounds)
    Neutral,    // Spawned but not possessed by any player
    Carried,    // Being carried by a player
    Dropped,    // Dropped and temporarily on the ground
    Scoring,    // In the process of being scored
    Resetting   // Being returned to spawn point
};

/**
 * Structure containing state machine logic for Relic
 */
USTRUCT(BlueprintType)
struct FRelicStateMachine
{
    GENERATED_BODY()

public:
    FRelicStateMachine();
    
    // Current state of the Relic
    UPROPERTY(BlueprintReadOnly, Category = "Relic")
    ERelicState CurrentState;
    
    // Previous state (useful for transitions)
    UPROPERTY(BlueprintReadOnly, Category = "Relic")
    ERelicState PreviousState;
    
    // Time spent in current state
    UPROPERTY(BlueprintReadOnly, Category = "Relic")
    float TimeInCurrentState;
    
    // Flag to indicate a transition is in progress
    UPROPERTY(BlueprintReadOnly, Category = "Relic")
    bool bTransitionInProgress;
    
    // Current carrier of the Relic (null if not carried)
    UPROPERTY(BlueprintReadOnly, Category = "Relic")
    TObjectPtr<ABwayCharacterWithAbilities> CurrentCarrier;
    
    // Team that last possessed the Relic
    UPROPERTY(BlueprintReadOnly, Category = "Relic")
    int32 LastPossessingTeam;
    
    // Update the state machine
    void UpdateState(float DeltaTime);
    
    // Request a state change
    bool RequestStateChange(ERelicState NewState, ABwayCharacterWithAbilities* NewCarrier = nullptr);
    
    // Check if a transition is valid
    bool CanTransitionTo(ERelicState NewState, ABwayCharacterWithAbilities* NewCarrier = nullptr);
    
    // Get state as string (for logging/debugging)
    FString GetStateAsString();
    
    // State change delegate
    DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnRelicStateChanged, ERelicState, ERelicState, ABwayCharacterWithAbilities*);
    FOnRelicStateChanged OnStateChanged;
};