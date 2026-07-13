#include "Abilities/BwayGameplayAbility.h"

#include "BwayGameplayTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayGameplayAbility)

UBwayGameplayAbility::UBwayGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CancelAbilitiesWithTag.AddTag(BwayGameplayTags::Ability_Buildable_PlacementSession);
}

bool UBwayGameplayAbility::GetDisplayDataFromAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass, FBwayAbilityUIData& OutDisplayData)
{
	OutDisplayData = FBwayAbilityUIData{};

	if (!AbilityClass)
	{
		return false;
	}

	const UGameplayAbility* AbilityCDO = AbilityClass->GetDefaultObject<UGameplayAbility>();
	const UBwayGameplayAbility* BwayAbilityCDO = Cast<UBwayGameplayAbility>(AbilityCDO);
	if (!BwayAbilityCDO)
	{
		return false;
	}

	OutDisplayData = BwayAbilityCDO->GetDisplayData();
	return true;
}
