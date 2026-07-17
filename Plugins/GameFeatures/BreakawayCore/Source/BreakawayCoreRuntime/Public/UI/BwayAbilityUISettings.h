#pragma once

#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "BwayAbilityUISettings.generated.h"

class ULyraAbilitySet;
class UInputAction;

/**
 * One combat-position replacement while the local player carries the relic.
 * Array index 0 maps to the LMB position, then F, Q, E, and R.
 */
USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayRelicAbilityBarSlot
{
	GENERATED_BODY()

	/** Input tag on the real ability grant. Invalid means this is an intentional disabled placeholder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Ability", meta = (Categories = "InputTag.Relic"))
	FGameplayTag InputTag;

	/** Input action fallback when the active Lyra input config has not yet replicated/loaded. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Ability")
	TSoftObjectPtr<const UInputAction> InputAction;

	/** Stable label used while a grant is unresolved or has no UBwayGameplayAbility DisplayData. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Relic Ability")
	FText PlaceholderLabel;
};

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

	/** Fixed in-match order: buildable, primary, F, Q, E, R. Kept separate from hero-select presentation. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ability Bar", meta = (Categories = "InputTag"))
	TArray<FGameplayTag> MatchAbilityBarSlotTags;

	/** Shared ability sets granted to all heroes (e.g. movement slide, relic request). Excludes per-hero buildable placement. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ability Bar")
	TArray<TSoftObjectPtr<const ULyraAbilitySet>> CommonAbilitySets;

	/**
	 * Explicit relic replacements for the five combat positions (LMB/F/Q/E/R).
	 * A missing real action stays as an invalid entry and renders disabled.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Ability Bar")
	TArray<FBwayRelicAbilityBarSlot> RelicCombatSlots;

	virtual FName GetCategoryName() const override { return FName(TEXT("Breakaway")); }
};
