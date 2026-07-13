#pragma once

#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "BwayAbilityUISettings.generated.h"

class ULyraAbilitySet;

/**
 * Project-wide ability bar slot order and shared ability sets used when resolving hero UI.
 * Hero-specific abilities (including per-hero buildable placement) live on each hero's ability sets.
 */
UCLASS(Config = BreakawayCore, DefaultConfig, meta = (DisplayName = "Breakaway Ability UI"))
class BREAKAWAYCORERUNTIME_API UBwayAbilityUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UBwayAbilityUISettings();

	static const UBwayAbilityUISettings& Get();

	/** Fixed UI slot order — widgets iterate this list and match abilities by InputTag. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ability Bar", meta = (Categories = "InputTag"))
	TArray<FGameplayTag> DefaultAbilityBarSlotTags;

	/** Shared ability sets granted to all heroes (e.g. movement slide, relic request). Excludes per-hero buildable placement. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ability Bar")
	TArray<TSoftObjectPtr<const ULyraAbilitySet>> CommonAbilitySets;

	virtual FName GetCategoryName() const override { return FName(TEXT("Breakaway")); }
};
