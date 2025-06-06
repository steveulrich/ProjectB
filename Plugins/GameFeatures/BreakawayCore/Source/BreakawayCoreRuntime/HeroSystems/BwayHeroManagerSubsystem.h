// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h" // Or your chosen subsystem base
#include "GameFeatureStateChangeObserver.h" // <--- Include the observer interface
#include "BwayHeroManagerSubsystem.generated.h"

class UBwayHeroDataAsset;
class UGameFeatureData;

/**
 * 
 */

class UGameFeatureData;
class UBwayHeroDataAsset;

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayHeroManagerSubsystem : public UGameInstanceSubsystem, public IGameFeatureStateChangeObserver
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	//~ Begin IGameFeatureStateChangeObserver interface
	// We are primarily interested in the generic state change, but you can override others if needed.
    virtual void OnGameFeatureActivated(const UGameFeatureData* GameFeatureData, const FString& PluginName) override;
	virtual void OnGameFeatureDeactivating(const UGameFeatureData* GameFeatureData, FGameFeatureDeactivatingContext& Context, const FString& PluginURL) override;

	// You can also override specific state changes if you want more granular control, e.g.:
	// virtual void OnGameFeatureActivating(const UGameFeatureData* GameFeatureData, const FString& PluginURL) override;
	// virtual void OnGameFeatureDeactivating(const UGameFeatureData* GameFeatureData, const FString& PluginURL, FGameFeatureDeactivatingContext& DeactivatingContext) override;
	//~ End IGameFeatureStateChangeObserver interface

private:
	void ProcessGameFeatureDataForRegistration(const UGameFeatureData* GFD, const FString& PluginURL);
	void ProcessGameFeatureDataForUnregistration(const UGameFeatureData* GFD, const FString& PluginURL);

	// Keep track of which Game Features provided which heroes for easy unregistration
	TMap<FString, TArray<TObjectPtr<const UBwayHeroDataAsset>>> RegisteredHeroesByPlugin;

};

