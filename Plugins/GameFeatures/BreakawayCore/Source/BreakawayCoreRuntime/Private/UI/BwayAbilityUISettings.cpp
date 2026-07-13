#include "UI/BwayAbilityUISettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAbilityUISettings)

namespace BwayAbilityUIDefaults
{
	static const FName PrimaryTagName(TEXT("InputTag.Ability.Primary"));
	static const FName Ability1TagName(TEXT("InputTag.Ability.Ability1"));
	static const FName Ability2TagName(TEXT("InputTag.Ability.Ability2"));
	static const FName Ability3TagName(TEXT("InputTag.Ability.Ability3"));
	static const FName Ability4TagName(TEXT("InputTag.Ability.Ability4"));
	static const FName BuildableTagName(TEXT("InputTag.Ability.Buildable"));
}

UBwayAbilityUISettings::UBwayAbilityUISettings()
{
	DefaultAbilityBarSlotTags = {
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::PrimaryTagName, /*ErrorIfNotFound*/ false),
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::Ability1TagName, /*ErrorIfNotFound*/ false),
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::Ability2TagName, /*ErrorIfNotFound*/ false),
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::Ability3TagName, /*ErrorIfNotFound*/ false),
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::Ability4TagName, /*ErrorIfNotFound*/ false),
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::BuildableTagName, /*ErrorIfNotFound*/ false),
	};
}

const UBwayAbilityUISettings& UBwayAbilityUISettings::Get()
{
	return *GetDefault<UBwayAbilityUISettings>();
}
