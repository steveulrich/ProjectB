#include "Relic/RelicNetworkManager.h"
#include "BwayCharacterWithAbilities.h"

URelicNetworkManager::URelicNetworkManager()
{
	NextPredictionKey = 1;
}

void URelicNetworkManager::AddPrediction(uint32 PredictionKey, ERelicState PredictedState, ABwayCharacterWithAbilities* PredictedCarrier)
{
	FRelicPredictionData NewPrediction;
	NewPrediction.PredictionKey = PredictionKey;
	NewPrediction.PredictedState = PredictedState;
	NewPrediction.PredictedCarrier = PredictedCarrier;
	NewPrediction.TimeStamp = GetWorld()->GetTimeSeconds();
    
	ActivePredictions.Add(NewPrediction);
}

bool URelicNetworkManager::ConfirmPrediction(uint32 PredictionKey)
{
	for (int32 i = 0; i < ActivePredictions.Num(); ++i)
	{
		if (ActivePredictions[i].PredictionKey == PredictionKey)
		{
			// Remove confirmed prediction
			ActivePredictions.RemoveAt(i);
			return true;
		}
	}
    
	return false;
}

bool URelicNetworkManager::RejectPrediction(uint32 PredictionKey, ERelicState CorrectState, ABwayCharacterWithAbilities* CorrectCarrier)
{
	for (int32 i = 0; i < ActivePredictions.Num(); ++i)
	{
		if (ActivePredictions[i].PredictionKey == PredictionKey)
		{
			// Remove rejected prediction
			ActivePredictions.RemoveAt(i);
			return true;
		}
	}
    
	return false;
}

void URelicNetworkManager::ClearStaleData(float CurrentTime, float StaleThreshold)
{
	for (int32 i = ActivePredictions.Num() - 1; i >= 0; --i)
	{
		if (CurrentTime - ActivePredictions[i].TimeStamp > StaleThreshold)
		{
			ActivePredictions.RemoveAt(i);
		}
	}
}

uint32 URelicNetworkManager::GetNextPredictionKey()
{
	uint32 Key = NextPredictionKey;
	NextPredictionKey++;
    
	// Handle overflow by avoiding 0 (which is treated as invalid)
	if (NextPredictionKey == 0)
	{
		NextPredictionKey = 1;
	}
    
	return Key;
}