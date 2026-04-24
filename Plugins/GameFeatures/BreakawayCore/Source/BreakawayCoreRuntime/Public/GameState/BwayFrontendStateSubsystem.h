// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "BwayFrontendStateSubsystem.generated.h"

/**
 * Subsystem to hold state in the FrontEnd (like selected hero, custom options)
 * that persists right until the player calls OpenLevel to enter a match.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayFrontendStateSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	UBwayFrontendStateSubsystem();

	/** Gets the hero the local player currently has selected in the frontend */
	UFUNCTION(BlueprintCallable, Category = "Frontend State")
	FPrimaryAssetId GetSelectedHeroId() const { return FrontEndSelectedHeroId; }

	/** Sets the hero the local player currently has selected in the frontend */
	UFUNCTION(BlueprintCallable, Category = "Frontend State")
	void SetSelectedHeroId(FPrimaryAssetId HeroId) { FrontEndSelectedHeroId = HeroId; }

private:
	UPROPERTY(Transient)
	FPrimaryAssetId FrontEndSelectedHeroId;
};
