#pragma once

#include "CoreMinimal.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayHeroAbilityUILibrary.generated.h"

class UAbilitySystemComponent;
class ULyraAbilitySet;
class ULyraGameplayAbility;

/**
 * Resolves ability bar display data from ability set grants and ability CDO UI metadata.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayHeroAbilityUILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Project-default slot order from UBwayAbilityUISettings. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Ability UI")
	static TArray<FGameplayTag> GetDefaultAbilityBarSlotTags();

	/** Build display infos for a hero using project slot order + hero/common ability sets. Skips unresolved slots. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Ability UI")
	static TArray<FAbilityDisplayInfo> ResolveAbilityBarForHero(const UBwayHeroDataAsset* HeroData);

	/**
	 * Build display infos from an explicit slot order and ability set list.
	 * @param SlotTags Ordered input tags to resolve.
	 * @param AbilitySets Ability sets to scan (hero + common).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Ability UI")
	static TArray<FAbilityDisplayInfo> ResolveAbilityBarFromSets(
		const TArray<FGameplayTag>& SlotTags,
		const TArray<ULyraAbilitySet*>& AbilitySets);

	/** Resolve display infos from a live ASC using project slot order. Skips specs not on the local pawn. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Ability UI", meta = (DefaultToSelf = "AbilitySystemComponent"))
	static TArray<FAbilityDisplayInfo> ResolveAbilityBarFromASC(const UAbilitySystemComponent* AbilitySystemComponent);

	/** Find one live ASC grant by its exact dynamic input tag. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Ability UI", meta = (DefaultToSelf = "AbilitySystemComponent"))
	static bool FindGrantedAbilityForInputTagOnASC(
		FGameplayTag InputTag,
		const UAbilitySystemComponent* AbilitySystemComponent,
		TSubclassOf<ULyraGameplayAbility>& OutAbilityClass);

	/** Merge ability CDO display data with the grant input tag. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Ability UI")
	static FAbilityDisplayInfo MakeDisplayInfoFromAbility(
		TSubclassOf<ULyraGameplayAbility> AbilityClass,
		FGameplayTag InputTag);

	/** Find the granted ability class for an input tag across ability sets. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Ability UI")
	static bool FindGrantedAbilityForInputTag(
		FGameplayTag InputTag,
		const TArray<ULyraAbilitySet*>& AbilitySets,
		TSubclassOf<ULyraGameplayAbility>& OutAbilityClass);
};
