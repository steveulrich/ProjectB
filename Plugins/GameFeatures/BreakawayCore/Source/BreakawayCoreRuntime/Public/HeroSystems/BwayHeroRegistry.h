// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BwayHeroRegistry.generated.h"

class UBwayHeroDataAsset;

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayHeroRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static TObjectPtr<UBwayHeroRegistry> Get(const UObject* WorldContext);

	/** Returns *soft* references for every hero asset found on disk. */
	UFUNCTION(BlueprintCallable, Category="Heroes")
	TArray<TSoftObjectPtr<UBwayHeroDataAsset>> GetAllHeroSoftObjects() const;

	/** Loads (sync) a single hero by soft-path – UI can call when highlighted. */
	UFUNCTION(BlueprintCallable, Category="Heroes")
	UBwayHeroDataAsset* LoadHeroSync(const TSoftObjectPtr<UBwayHeroDataAsset>& SoftPtr) const;
	
	UFUNCTION(BlueprintCallable, Category="Heroes")
	static UBwayHeroDataAsset* GetHeroDataById(const FPrimaryAssetId& HeroId);

private:
	/** Cached Asset Registry pointer – filled in Initialize() */
	IAssetRegistry* Registry = nullptr;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
};
