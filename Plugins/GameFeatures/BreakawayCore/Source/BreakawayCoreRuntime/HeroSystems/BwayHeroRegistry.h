// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BwayHeroRegistry.generated.h"

class UBwayHeroDataAsset;

/**
 * 
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayHeroRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static TObjectPtr<UBwayHeroRegistry> Get(const UObject* WorldContext);

	// Called by our helper when a GameFeature loads
	void RegisterHero(const UBwayHeroDataAsset* HeroData);
	void UnregisterHero(const UBwayHeroDataAsset* HeroData);

	// Called at subsystem init to scan registry
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const TArray<TObjectPtr<const UBwayHeroDataAsset>>& GetRegisteredHeroes() const { return RegisteredHeroes; }
	
	// Hero lookup for UI
	UFUNCTION(BlueprintCallable, Category="Heroes")
	TArray<UBwayHeroDataAsset*> GetAllHeroes() const;

private:
	UPROPERTY()
	TArray<TObjectPtr<const UBwayHeroDataAsset>> RegisteredHeroes;
};
