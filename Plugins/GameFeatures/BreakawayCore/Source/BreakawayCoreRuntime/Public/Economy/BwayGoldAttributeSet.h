// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AbilitySystem/Attributes/LyraAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "BwayGoldAttributeSet.generated.h"

/**
 * GAS Attribute Set for the Breakaway economy system.
 * Tracks gold per-player, used to purchase buildables.
 * Lives on the PlayerState's AbilitySystemComponent.
 *
 * Gold values are modified exclusively via GameplayEffects:
 * - GE_AwardGold_Kill, GE_AwardGold_Assist, GE_AwardGold_Objective
 * - GE_SpendGold_Buildable (negative modifier, applied by GA_PlaceBuildable)
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGoldAttributeSet : public ULyraAttributeSet
{
	GENERATED_BODY()

public:
	UBwayGoldAttributeSet();

	// ========================================
	// Attributes
	// ========================================

	ATTRIBUTE_ACCESSORS(UBwayGoldAttributeSet, CurrentGold);
	ATTRIBUTE_ACCESSORS(UBwayGoldAttributeSet, MaxGold);
	ATTRIBUTE_ACCESSORS(UBwayGoldAttributeSet, GoldPerSecond);

private:
	/** Current gold the player has available to spend */
	UPROPERTY(BlueprintReadOnly, Category = "Economy", ReplicatedUsing = OnRep_CurrentGold, Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData CurrentGold;

	/** Maximum gold a player can hold */
	UPROPERTY(BlueprintReadOnly, Category = "Economy", ReplicatedUsing = OnRep_MaxGold, Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData MaxGold;

	/** Passive gold income per second (awarded by GE with periodic application) */
	UPROPERTY(BlueprintReadOnly, Category = "Economy", ReplicatedUsing = OnRep_GoldPerSecond, Meta=(AllowPrivateAccess=true))
	FGameplayAttributeData GoldPerSecond;

	// ========================================
	// UAttributeSet overrides
	// ========================================

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

protected:
	UFUNCTION()
	void OnRep_CurrentGold(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_MaxGold(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	void OnRep_GoldPerSecond(const FGameplayAttributeData& OldValue);
};
