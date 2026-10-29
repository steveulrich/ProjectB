#include "Abilities/BwayGameplayAbility.h"

#include "BwayGameplayTags.h"
#include "GameplayEffect.h"

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

float UBwayGameplayAbility::GetDisplayCooldownSeconds() const
{
	const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (CooldownGE && CooldownGE->DurationPolicy == EGameplayEffectDurationType::HasDuration)
	{
		float StaticDuration = 0.f;
		if (CooldownGE->DurationMagnitude.GetStaticMagnitudeIfPossible(1.0f, StaticDuration) && StaticDuration > 0.f)
		{
			return StaticDuration;
		}
	}
	return 0.f;
}

float UBwayGameplayAbility::GetDisplayCooldownFromAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilityClass)
	{
		return 0.f;
	}

	const UGameplayAbility* AbilityCDO = AbilityClass->GetDefaultObject<UGameplayAbility>();
	if (const UBwayGameplayAbility* BwayAbilityCDO = Cast<UBwayGameplayAbility>(AbilityCDO))
	{
		return BwayAbilityCDO->GetDisplayCooldownSeconds();
	}

	if (AbilityCDO)
	{
		if (const UGameplayEffect* CooldownGE = AbilityCDO->GetCooldownGameplayEffect())
		{
			if (CooldownGE->DurationPolicy == EGameplayEffectDurationType::HasDuration)
			{
				float StaticDuration = 0.f;
				if (CooldownGE->DurationMagnitude.GetStaticMagnitudeIfPossible(1.0f, StaticDuration) && StaticDuration > 0.f)
				{
					return StaticDuration;
				}
			}
		}
	}

	return 0.f;
}
