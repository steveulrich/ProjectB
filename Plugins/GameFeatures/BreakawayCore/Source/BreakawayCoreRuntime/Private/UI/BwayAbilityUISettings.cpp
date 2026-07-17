#include "UI/BwayAbilityUISettings.h"

#include "InputAction.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAbilityUISettings)

#define LOCTEXT_NAMESPACE "BwayAbilityUISettings"

namespace BwayAbilityUIDefaults
{
	static const FName PrimaryTagName(TEXT("InputTag.Ability.Primary"));
	static const FName Ability1TagName(TEXT("InputTag.Ability.Ability1"));
	static const FName Ability2TagName(TEXT("InputTag.Ability.Ability2"));
	static const FName Ability3TagName(TEXT("InputTag.Ability.Ability3"));
	static const FName Ability4TagName(TEXT("InputTag.Ability.Ability4"));
	static const FName BuildableTagName(TEXT("InputTag.Ability.Buildable"));
	static const FName RelicThrowTagName(TEXT("InputTag.Relic.Throw"));
	static const FName RelicDashTagName(TEXT("InputTag.Relic.Dash"));
	static const FName RelicPassTagName(TEXT("InputTag.Relic.Pass"));
	static const FName RelicDropTagName(TEXT("InputTag.Relic.Drop"));
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

	MatchAbilityBarSlotTags = {
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::BuildableTagName, /*ErrorIfNotFound*/ false),
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::PrimaryTagName, /*ErrorIfNotFound*/ false),
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::Ability4TagName, /*ErrorIfNotFound*/ false),
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::Ability1TagName, /*ErrorIfNotFound*/ false),
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::Ability2TagName, /*ErrorIfNotFound*/ false),
		FGameplayTag::RequestGameplayTag(BwayAbilityUIDefaults::Ability3TagName, /*ErrorIfNotFound*/ false),
	};

	auto AddRelicSlot = [this](const FName TagName, const TCHAR* InputActionPath, const FText& PlaceholderLabel)
	{
		FBwayRelicAbilityBarSlot& Slot = RelicCombatSlots.AddDefaulted_GetRef();
		Slot.InputTag = FGameplayTag::RequestGameplayTag(TagName, /*ErrorIfNotFound*/ false);
		Slot.InputAction = TSoftObjectPtr<const UInputAction>(FSoftObjectPath(InputActionPath));
		Slot.PlaceholderLabel = PlaceholderLabel;
	};

	// Audited content mapping: the carrier set grants Throw, Dash, and Drop.
	// Pass has a real mapped InputAction but no carrier grant, so it renders disabled;
	// the fifth carrier position is also an explicit disabled placeholder.
	AddRelicSlot(
		BwayAbilityUIDefaults::RelicThrowTagName,
		TEXT("/BreakawayCore/Input/Actions/IA_BW_Relic_Throw.IA_BW_Relic_Throw"),
		LOCTEXT("RelicThrow", "Throw"));
	AddRelicSlot(
		BwayAbilityUIDefaults::RelicDashTagName,
		TEXT("/BreakawayCore/Input/Actions/IA_BW_Relic_Dash.IA_BW_Relic_Dash"),
		LOCTEXT("RelicDash", "Relic Dash"));
	AddRelicSlot(
		BwayAbilityUIDefaults::RelicPassTagName,
		TEXT("/BreakawayCore/Input/Actions/IA_BW_Relic_Pass.IA_BW_Relic_Pass"),
		LOCTEXT("RelicPass", "Pass"));
	AddRelicSlot(
		BwayAbilityUIDefaults::RelicDropTagName,
		TEXT("/BreakawayCore/Input/Actions/IA_BW_Relic_Drop.IA_BW_Relic_Drop"),
		LOCTEXT("RelicDrop", "Drop"));

	FBwayRelicAbilityBarSlot& MissingSlot = RelicCombatSlots.AddDefaulted_GetRef();
	MissingSlot.PlaceholderLabel = LOCTEXT("RelicMissingAbility", "Unavailable");
}

const UBwayAbilityUISettings& UBwayAbilityUISettings::Get()
{
	return *GetDefault<UBwayAbilityUISettings>();
}

#undef LOCTEXT_NAMESPACE
