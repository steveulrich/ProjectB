#include "HeroSystems/BwayHeroAbilityUILibrary.h"

#include "Abilities/BwayGameplayAbility.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "UI/BwayAbilityUISettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayHeroAbilityUILibrary)

namespace BwayAbilitySetAccess
{
	/** Read Lyra's protected GrantedGameplayAbilities without modifying ULyraAbilitySet. */
	const TArray<FLyraAbilitySet_GameplayAbility>& GetGrantedAbilities(const ULyraAbilitySet* AbilitySet)
	{
		struct FAccessor : public ULyraAbilitySet
		{
			static const TArray<FLyraAbilitySet_GameplayAbility>& Get(const ULyraAbilitySet* Set)
			{
				return static_cast<const FAccessor*>(Set)->GrantedGameplayAbilities;
			}
		};

		check(AbilitySet);
		return FAccessor::Get(AbilitySet);
	}
}

TArray<FGameplayTag> UBwayHeroAbilityUILibrary::GetDefaultAbilityBarSlotTags()
{
	return UBwayAbilityUISettings::Get().DefaultAbilityBarSlotTags;
}

FAbilityDisplayInfo UBwayHeroAbilityUILibrary::MakeDisplayInfoFromAbility(
	TSubclassOf<ULyraGameplayAbility> AbilityClass,
	FGameplayTag InputTag)
{
	FAbilityDisplayInfo DisplayInfo;
	DisplayInfo.InputActionTag = InputTag;

	if (!AbilityClass)
	{
		return DisplayInfo;
	}

	FBwayAbilityUIData UIData;
	if (UBwayGameplayAbility::GetDisplayDataFromAbilityClass(AbilityClass, UIData))
	{
		DisplayInfo.AbilityName = UIData.AbilityName;
		DisplayInfo.Description = UIData.Description;
		DisplayInfo.Icon = UIData.Icon;
		DisplayInfo.PreviewVideo = UIData.PreviewVideo;
		DisplayInfo.PreviewImage = UIData.PreviewImage;
		DisplayInfo.bIsUltimate = UIData.bIsUltimate;
	}
	else
	{
		DisplayInfo.AbilityName = FText::FromString(AbilityClass->GetName());
	}

	return DisplayInfo;
}

bool UBwayHeroAbilityUILibrary::FindGrantedAbilityForInputTag(
	FGameplayTag InputTag,
	const TArray<ULyraAbilitySet*>& AbilitySets,
	TSubclassOf<ULyraGameplayAbility>& OutAbilityClass)
{
	OutAbilityClass = nullptr;

	if (!InputTag.IsValid())
	{
		return false;
	}

	for (const ULyraAbilitySet* AbilitySet : AbilitySets)
	{
		if (!AbilitySet)
		{
			continue;
		}

		for (const FLyraAbilitySet_GameplayAbility& GrantedAbility : BwayAbilitySetAccess::GetGrantedAbilities(AbilitySet))
		{
			if (GrantedAbility.InputTag == InputTag && GrantedAbility.Ability)
			{
				OutAbilityClass = GrantedAbility.Ability;
				return true;
			}
		}
	}

	return false;
}

TArray<FAbilityDisplayInfo> UBwayHeroAbilityUILibrary::ResolveAbilityBarFromSets(
	const TArray<FGameplayTag>& SlotTags,
	const TArray<ULyraAbilitySet*>& AbilitySets)
{
	TArray<FAbilityDisplayInfo> ResolvedInfos;
	ResolvedInfos.Reserve(SlotTags.Num());

	for (const FGameplayTag& SlotTag : SlotTags)
	{
		if (!SlotTag.IsValid())
		{
			continue;
		}

		TSubclassOf<ULyraGameplayAbility> AbilityClass;
		if (!FindGrantedAbilityForInputTag(SlotTag, AbilitySets, AbilityClass))
		{
			continue;
		}

		ResolvedInfos.Add(MakeDisplayInfoFromAbility(AbilityClass, SlotTag));
	}

	return ResolvedInfos;
}

TArray<FAbilityDisplayInfo> UBwayHeroAbilityUILibrary::ResolveAbilityBarForHero(const UBwayHeroDataAsset* HeroData)
{
	if (!HeroData)
	{
		return {};
	}

	TArray<ULyraAbilitySet*> AbilitySets;
	AbilitySets.Reserve(HeroData->AbilitySets.Num() + UBwayAbilityUISettings::Get().CommonAbilitySets.Num());

	for (const TObjectPtr<const ULyraAbilitySet>& HeroSet : HeroData->AbilitySets)
	{
		if (HeroSet)
		{
			AbilitySets.Add(const_cast<ULyraAbilitySet*>(HeroSet.Get()));
		}
	}

	for (const TSoftObjectPtr<const ULyraAbilitySet>& CommonSetPtr : UBwayAbilityUISettings::Get().CommonAbilitySets)
	{
		if (const ULyraAbilitySet* CommonSet = CommonSetPtr.LoadSynchronous())
		{
			AbilitySets.Add(const_cast<ULyraAbilitySet*>(CommonSet));
		}
	}

	return ResolveAbilityBarFromSets(GetDefaultAbilityBarSlotTags(), AbilitySets);
}

TArray<FAbilityDisplayInfo> UBwayHeroAbilityUILibrary::ResolveAbilityBarFromASC(const UAbilitySystemComponent* AbilitySystemComponent)
{
	TArray<FAbilityDisplayInfo> ResolvedInfos;

	if (!AbilitySystemComponent)
	{
		return ResolvedInfos;
	}

	const TArray<FGameplayTag>& SlotTags = GetDefaultAbilityBarSlotTags();
	ResolvedInfos.Reserve(SlotTags.Num());

	for (const FGameplayTag& SlotTag : SlotTags)
	{
		if (!SlotTag.IsValid())
		{
			continue;
		}

		const FGameplayAbilitySpec* MatchingSpec = nullptr;
		for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
		{
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(SlotTag))
			{
				MatchingSpec = &Spec;
				break;
			}
		}

		if (!MatchingSpec || !MatchingSpec->Ability)
		{
			continue;
		}

		ResolvedInfos.Add(MakeDisplayInfoFromAbility(MatchingSpec->Ability->GetClass(), SlotTag));
	}

	return ResolvedInfos;
}
