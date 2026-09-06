// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BwayHeroRegistry.generated.h"

class UBwayHeroDataAsset;

/**
 * UBwayHeroRegistry
 * 
 * Game Instance Subsystem that provides access to all available hero data assets.
 * Uses Asset Manager to discover heroes registered in the /BreakawayCore/Characters/Heroes/ folder.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayHeroRegistry : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Get the hero registry from any world context object */
	static TObjectPtr<UBwayHeroRegistry> Get(const UObject* WorldContext);

	/** Returns soft references for every hero asset registered with Asset Manager */
	UFUNCTION(BlueprintCallable, Category="Heroes")
	TArray<TSoftObjectPtr<UBwayHeroDataAsset>> GetAllHeroSoftObjects() const;

	/** Loads (sync) a single hero by soft-path – UI can call when highlighted */
	UFUNCTION(BlueprintCallable, Category="Heroes")
	UBwayHeroDataAsset* LoadHeroSync(const TSoftObjectPtr<UBwayHeroDataAsset>& SoftPtr) const;
	
	/** Get hero data by primary asset ID */
	UFUNCTION(BlueprintCallable, Category="Heroes")
	static UBwayHeroDataAsset* GetHeroDataById(const FPrimaryAssetId& HeroId);

	/** Resolve HeroDataAsset primary asset id from URL/codename (e.g. Argus -> HeroDataAsset:Argus). */
	UFUNCTION(BlueprintCallable, Category="Heroes")
	static FPrimaryAssetId ResolveHeroIdByName(const FString& HeroName);
};
