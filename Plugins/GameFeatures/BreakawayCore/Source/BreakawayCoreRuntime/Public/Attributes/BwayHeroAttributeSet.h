#pragma once

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraAttributeSet.h"
#include "BwayHeroAttributeSet.generated.h"

/**
 * Breakaway hero sheet stats (Attack Str, Armor, Speed rating) plus combat modifiers.
 * Applied from UBwayHeroDataAsset::HeroStats during InitializeHeroData.
 * MoveSpeedMultiplier / IncomingDamageMultiplier are modified by temporary GEs (slows, Circle of Spite).
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
	ATTRIBUTE_ACCESSORS(UBwayHeroAttributeSet, MoveSpeedMultiplier);
	ATTRIBUTE_ACCESSORS(UBwayHeroAttributeSet, IncomingDamageMultiplier);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;

protected:
	void ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const;

	UFUNCTION()
	void OnRep_AttackStrength(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_Armor(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeedRating(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MoveSpeedMultiplier(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_IncomingDamageMultiplier(const FGameplayAttributeData& OldValue);

private:
	UPROPERTY(BlueprintReadOnly, Category = "Breakaway|Hero", ReplicatedUsing = OnRep_AttackStrength, Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData AttackStrength;

	UPROPERTY(BlueprintReadOnly, Category = "Breakaway|Hero", ReplicatedUsing = OnRep_Armor, Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData Armor;

	/** Design-sheet speed rating (e.g. Argus = 10). Converted to cm/s in BwayHeroStatsLibrary. */
	UPROPERTY(BlueprintReadOnly, Category = "Breakaway|Hero", ReplicatedUsing = OnRep_MoveSpeedRating, Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MoveSpeedRating;

	/**
	 * Multiplier applied to MaxWalkSpeed / slide max speed.
	 * 1.0 = normal; 0.5 = 50% slow. Modified by temporary slow GEs.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breakaway|Hero", ReplicatedUsing = OnRep_MoveSpeedMultiplier, Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData MoveSpeedMultiplier;

	/**
	 * Multiplier applied to incoming damage after armor.
	 * 1.0 = normal; 1.35 = Circle of Spite amp. Modified by temporary GEs.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Breakaway|Hero", ReplicatedUsing = OnRep_IncomingDamageMultiplier, Meta = (AllowPrivateAccess = true))
	FGameplayAttributeData IncomingDamageMultiplier;
};
