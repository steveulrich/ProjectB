#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Relic/RelicStateMachine.h"
#include "RelicNetworkManager.generated.h"

class ABwayCharacterWithAbilities;
class ARelicActor;

/**
 * Helper struct for prediction and reconciliation
 */
USTRUCT(BlueprintType)
struct FRelicPredictionData
{
	GENERATED_BODY()
    
	// Unique prediction ID
	UPROPERTY()
	uint32 PredictionKey;
    
	// Predicted state
	UPROPERTY()
	ERelicState PredictedState;
    
	// Predicted carrier
	UPROPERTY()
	TObjectPtr<ABwayCharacterWithAbilities> PredictedCarrier;
    
	// Timestamp of prediction
	UPROPERTY()
	float TimeStamp;
};

/**
 * Manager for client prediction and network optimization
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API URelicNetworkManager : public UObject
{
	GENERATED_BODY()
    
public:
	URelicNetworkManager();
    
	// Add a new prediction
	void AddPrediction(uint32 PredictionKey, ERelicState PredictedState, ABwayCharacterWithAbilities* PredictedCarrier);
    
	// Confirm a prediction
	bool ConfirmPrediction(uint32 PredictionKey);
    
	// Reject a prediction
	bool RejectPrediction(uint32 PredictionKey, ERelicState CorrectState, ABwayCharacterWithAbilities* CorrectCarrier);
    
	// Clear old predictions
	void ClearStaleData(float CurrentTime, float StaleThreshold = 2.0f);
    
	// Get next prediction key
	uint32 GetNextPredictionKey();
    
private:
	// List of active predictions
	UPROPERTY()
	TArray<FRelicPredictionData> ActivePredictions;
    
	// Counter for prediction keys
	uint32 NextPredictionKey;
};