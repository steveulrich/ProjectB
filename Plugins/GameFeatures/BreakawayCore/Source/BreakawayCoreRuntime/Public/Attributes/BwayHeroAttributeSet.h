#pragma once

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraAttributeSet.h"
#include "BwayHeroAttributeSet.generated.h"

/**
 * Breakaway hero sheet stats (Attack Str, Armor, Speed rating).
 * Applied from UBwayHeroDataAsset::HeroStats during InitializeHeroData.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayHeroAttributeSet : public ULyraAttributeSet
{
	GENERATED_BODY()

public:
	UBwayHeroAttributeSet();

	ATTRIBUTE_ACCESSORS(UBwayHeroAttributeSet, AttackStrength);
	ATTRIBUTE_ACCESSORS(UBwayHeroAttributeSet, Armor);
	ATTRIBUTE_ACCESSORS(UBwayHeroAttributeSet, MoveSpeedRating);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	UFUNCTION()
	void OnRep_AttackStrength(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeedRating(const FGameplayAttributeData& OldValue);

private:
	UPROPERTY(BlueprintReadOnly, Category = "Breakaway|Hero", ReplicatedUsing = OnRep_AttackStrength, Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData AttackStrength;

	UPROPERTY(BlueprintReadOnly, Category = "Breakaway|Hero", ReplicatedUsing = OnRep_Armor, Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Armor;

	/** Design-sheet speed rating (e.g. Argus = 10). Converted to cm/s in BwayHeroStatsLibrary. */
	UPROPERTY(BlueprintReadOnly, Category = "Breakaway|Hero", ReplicatedUsing = OnRep_MoveSpeedRating, Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MoveSpeedRating;
};
