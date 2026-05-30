// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameModes/LyraUserFacingExperienceDefinition.h"

#include "BwayUserFacingExperienceDefinition.generated.h"

class UCommonSession_HostSessionRequest;

/**
 * Breakaway playlist tile. Match map/experience on the asset are the post-hero-select destination.
 * CreateHostingRequest rewrites travel to the shared hero-select staging map first.
 */
UCLASS(BlueprintType, Const)
class BREAKAWAYCORERUNTIME_API UBwayUserFacingExperienceDefinition : public ULyraUserFacingExperienceDefinition
{
	GENERATED_BODY()

public:
	/** When false, this tile travels directly to MapID like stock Lyra (PIE/dev tiles only). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Hero Select")
	bool bRouteThroughHeroSelectStaging = true;

	/** Shared hero-select lobby map loaded before the match map. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Hero Select", meta = (AllowedTypes = "Map"))
	FPrimaryAssetId HeroSelectStagingMapID;

	/** Minimal experience used on the staging map. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Breakaway|Hero Select", meta = (AllowedTypes = "LyraExperienceDefinition"))
	FPrimaryAssetId HeroSelectStagingExperienceID;

	virtual UCommonSession_HostSessionRequest* CreateHostingRequest(const UObject* WorldContextObject) const override;
};
