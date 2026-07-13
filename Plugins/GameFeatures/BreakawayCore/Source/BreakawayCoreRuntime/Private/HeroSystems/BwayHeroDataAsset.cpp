#include "HeroSystems/BwayHeroDataAsset.h"

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "HeroSystems/BwayHeroAbilityUILibrary.h"
#include "UI/BwayAbilityUISettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroDataAsset)

#if WITH_EDITOR
EDataValidationResult UBwayHeroDataAsset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (AbilitySets.IsEmpty())
	{
		Context.AddWarning(FText::FromString(TEXT("AbilitySets is empty — hero will spawn with no hero-specific abilities.")));
	}

	TArray<ULyraAbilitySet*> AllSets;
	for (const TObjectPtr<const ULyraAbilitySet>& HeroSet : AbilitySets)
	{
		if (HeroSet)
		{
			AllSets.Add(const_cast<ULyraAbilitySet*>(HeroSet.Get()));
		}
	}

	for (const TSoftObjectPtr<const ULyraAbilitySet>& CommonSetPtr : UBwayAbilityUISettings::Get().CommonAbilitySets)
	{
		if (const ULyraAbilitySet* CommonSet = CommonSetPtr.LoadSynchronous())
		{
			AllSets.Add(const_cast<ULyraAbilitySet*>(CommonSet));
		}
	}

	TSet<FGameplayTag> SeenInputTags;
	for (const ULyraAbilitySet* AbilitySet : AllSets)
	{
		if (!AbilitySet)
		{
			continue;
		}

		TArray<ULyraAbilitySet*> SingleSetArray;
		SingleSetArray.Add(const_cast<ULyraAbilitySet*>(AbilitySet));

		for (const FGameplayTag& SlotTag : UBwayAbilityUISettings::Get().DefaultAbilityBarSlotTags)
		{
			TSubclassOf<ULyraGameplayAbility> AbilityClass;
			if (UBwayHeroAbilityUILibrary::FindGrantedAbilityForInputTag(SlotTag, SingleSetArray, AbilityClass))
			{
				if (SeenInputTags.Contains(SlotTag))
				{
					Context.AddError(FText::Format(
						NSLOCTEXT("BwayHeroDataAsset", "DuplicateInputTag", "Duplicate InputTag '{0}' found in ability set '{1}'."),
						FText::FromString(SlotTag.ToString()),
						FText::FromString(AbilitySet->GetName())));
					Result = EDataValidationResult::Invalid;
				}
				else
				{
					SeenInputTags.Add(SlotTag);
				}
			}
		}
	}

	for (const FGameplayTag& SlotTag : UBwayAbilityUISettings::Get().DefaultAbilityBarSlotTags)
	{
		if (!SlotTag.IsValid())
		{
			continue;
		}

		TSubclassOf<ULyraGameplayAbility> AbilityClass;
		if (!UBwayHeroAbilityUILibrary::FindGrantedAbilityForInputTag(SlotTag, AllSets, AbilityClass))
		{
			static const FGameplayTag BuildableTag = FGameplayTag::RequestGameplayTag(FName("InputTag.Ability.Buildable"), /*ErrorIfNotFound*/ false);
			if (SlotTag == BuildableTag && !BuildableDataAsset)
			{
				continue;
			}

			Context.AddWarning(FText::Format(
				NSLOCTEXT("BwayHeroDataAsset", "MissingAbilitySlot", "No granted ability found for UI slot tag '{0}'."),
				FText::FromString(SlotTag.ToString())));
			continue;
		}

		const FAbilityDisplayInfo DisplayInfo = UBwayHeroAbilityUILibrary::MakeDisplayInfoFromAbility(AbilityClass, SlotTag);
		if (DisplayInfo.AbilityName.IsEmpty() && DisplayInfo.Icon.IsNull())
		{
			Context.AddWarning(FText::Format(
				NSLOCTEXT("BwayHeroDataAsset", "MissingDisplayData", "Ability '{0}' (slot '{1}') has no DisplayData on its CDO."),
				FText::FromString(GetNameSafe(AbilityClass)),
				FText::FromString(SlotTag.ToString())));
		}
	}

	return Result;
}
#endif
