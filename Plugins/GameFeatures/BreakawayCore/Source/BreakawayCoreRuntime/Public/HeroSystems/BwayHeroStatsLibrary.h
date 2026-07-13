#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayHeroStatsLibrary.generated.h"

class UAbilitySystemComponent;
class ABwayCharacterWithAbilities;
class UBwayHeroDataAsset;

UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayHeroStatsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Sheet speed rating → max walk speed (uu/s). Argus Speed 10 → 600. */
	UFUNCTION(BlueprintPure, Category = "Breakaway|Hero")
	static float ConvertSpeedRatingToMaxWalkSpeed(float SpeedRating);

	/** Apply HeroStats from data asset to ASC + movement (server authoritative). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|Hero")
	static void ApplyHeroStatsFromDataAsset(ABwayCharacterWithAbilities* Character, const UBwayHeroDataAsset* HeroData);

	/** Ensure UBwayHeroAttributeSet exists on ASC. */
	static void EnsureHeroAttributeSet(UAbilitySystemComponent* ASC);
};
