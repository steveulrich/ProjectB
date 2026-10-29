#include "HeroSystems/BwayHeroAbilityUILibrary.h"

#include "Abilities/BwayGameplayAbility.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedActionKeyMapping.h"
#include "EnhancedInputSubsystems.h"
#include "Input/LyraInputConfig.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
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

	FText CompactKeyText(const FKey& Key)
	{
		if (!Key.IsValid())
		{
			return FText::GetEmpty();
		}
		if (Key == EKeys::LeftMouseButton)
		{
			return FText::FromString(TEXT("LMB"));
		}
		if (Key == EKeys::RightMouseButton)
		{
			return FText::FromString(TEXT("RMB"));
		}
		if (Key == EKeys::MiddleMouseButton)
		{
			return FText::FromString(TEXT("MMB"));
		}
		if (Key == EKeys::SpaceBar)
		{
			return FText::FromString(TEXT("Space"));
		}
		if (Key == EKeys::LeftShift)
		{
			return FText::FromString(TEXT("L-Shift"));
		}
		if (Key == EKeys::One)
		{
			return FText::FromString(TEXT("1"));
		}
		return Key.GetDisplayName(/*bLongDisplayName*/ false);
	}

	const ULyraInputConfig* LoadHumanoidInputConfig()
	{
		return LoadObject<ULyraInputConfig>(
			nullptr,
			TEXT("/BreakawayCore/Input/DA_BW_InputData_Humanoid.DA_BW_InputData_Humanoid"));
	}

	const UInputAction* FindActionInHumanoidConfig(const FGameplayTag& InputTag)
	{
		if (!InputTag.IsValid())
		{
			return nullptr;
		}
		const ULyraInputConfig* Config = LoadHumanoidInputConfig();
		if (!Config)
		{
			return nullptr;
		}
		for (const FLyraInputAction& Entry : Config->AbilityInputActions)
		{
			if (Entry.InputTag == InputTag && Entry.InputAction)
			{
				return Entry.InputAction;
			}
		}
		for (const FLyraInputAction& Entry : Config->NativeInputActions)
		{
			if (Entry.InputTag == InputTag && Entry.InputAction)
			{
				return Entry.InputAction;
			}
		}
		return nullptr;
	}

	FKey FindMappedKeyForAction(const UInputAction* Action, const ULocalPlayer* OptionalLocalPlayer)
	{
		if (!Action)
		{
			return EKeys::Invalid;
		}

		bool bPreferGamepad = false;
		if (OptionalLocalPlayer)
		{
			if (const UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(OptionalLocalPlayer))
			{
				bPreferGamepad = (CommonInput->GetCurrentInputType() == ECommonInputType::Gamepad);
			}
			if (const UEnhancedInputLocalPlayerSubsystem* InputSys = OptionalLocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				const TArray<FKey> LiveKeys = InputSys->QueryKeysMappedToAction(Action);
				for (const FKey& Key : LiveKeys)
				{
					if (Key.IsValid() && Key.IsGamepadKey() == bPreferGamepad)
					{
						return Key;
					}
				}
				if (LiveKeys.Num() > 0 && LiveKeys[0].IsValid())
				{
					return LiveKeys[0];
				}
			}
		}

		static const TCHAR* ImcPaths[] = {
			TEXT("/BreakawayCore/Input/Mappings/IMC_BW_HeroAbilities.IMC_BW_HeroAbilities"),
			TEXT("/BreakawayCore/Input/Mappings/IMC_BW_Default.IMC_BW_Default"),
		};

		FKey FallbackKey = EKeys::Invalid;
		for (const TCHAR* ImcPath : ImcPaths)
		{
			const UInputMappingContext* Imc = LoadObject<UInputMappingContext>(nullptr, ImcPath);
			if (!Imc)
			{
				continue;
			}
			FKey PreferredMatch = EKeys::Invalid;
			Imc->ForEachKeyMapping([&](const FEnhancedActionKeyMapping& Mapping)
			{
				if (Mapping.Action == Action && Mapping.Key.IsValid())
				{
					if (!FallbackKey.IsValid())
					{
						FallbackKey = Mapping.Key;
					}
					if (!PreferredMatch.IsValid() && Mapping.Key.IsGamepadKey() == bPreferGamepad)
					{
						PreferredMatch = Mapping.Key;
					}
				}
			});
			if (PreferredMatch.IsValid())
			{
				return PreferredMatch;
			}
		}

		return FallbackKey;
	}

	FText FallbackKeyLabelForTag(const FGameplayTag& InputTag)
	{
		const FString TagStr = InputTag.ToString();
		if (TagStr.EndsWith(TEXT(".Primary")))
		{
			return FText::FromString(TEXT("LMB"));
		}
		if (TagStr.EndsWith(TEXT(".Ability1")))
		{
			return FText::FromString(TEXT("Q"));
		}
		if (TagStr.EndsWith(TEXT(".Ability2")))
		{
			return FText::FromString(TEXT("E"));
		}
		if (TagStr.EndsWith(TEXT(".Ability3")))
		{
			return FText::FromString(TEXT("R"));
		}
		if (TagStr.EndsWith(TEXT(".Ability4")))
		{
			return FText::FromString(TEXT("F"));
		}
		if (TagStr.EndsWith(TEXT(".Buildable")))
		{
			return FText::FromString(TEXT("1"));
		}
		if (TagStr.Contains(TEXT("Jump")))
		{
			return FText::FromString(TEXT("Space"));
		}
		if (TagStr.Contains(TEXT("Slide")))
		{
			return FText::FromString(TEXT("L-Shift"));
		}
		return FText::GetEmpty();
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
	DisplayInfo.bIsBuildable = InputTag.ToString().EndsWith(TEXT(".Buildable"));
	DisplayInfo.InputKeyLabel = ResolveKeyLabelForInputTag(InputTag, nullptr);

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

	DisplayInfo.CooldownSeconds = UBwayGameplayAbility::GetDisplayCooldownFromAbilityClass(AbilityClass);
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
		TSubclassOf<ULyraGameplayAbility> AbilityClass;
		if (!FindGrantedAbilityForInputTagOnASC(SlotTag, AbilitySystemComponent, AbilityClass))
		{
			continue;
		}

		ResolvedInfos.Add(MakeDisplayInfoFromAbility(AbilityClass, SlotTag));
	}

	return ResolvedInfos;
}

bool UBwayHeroAbilityUILibrary::FindGrantedAbilityForInputTagOnASC(
	const FGameplayTag InputTag,
	const UAbilitySystemComponent* AbilitySystemComponent,
	TSubclassOf<ULyraGameplayAbility>& OutAbilityClass)
{
	OutAbilityClass = nullptr;

	if (!AbilitySystemComponent || !InputTag.IsValid())
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			OutAbilityClass = Spec.Ability->GetClass();
			return true;
		}
	}

	return false;
}

FText UBwayHeroAbilityUILibrary::ResolveKeyLabelForInputTag(FGameplayTag InputTag, const ULocalPlayer* OptionalLocalPlayer)
{
	if (!InputTag.IsValid())
	{
		return FText::GetEmpty();
	}

	if (const UInputAction* Action = BwayAbilitySetAccess::FindActionInHumanoidConfig(InputTag))
	{
		const FKey MappedKey = BwayAbilitySetAccess::FindMappedKeyForAction(Action, OptionalLocalPlayer);
		if (MappedKey.IsValid())
		{
			return BwayAbilitySetAccess::CompactKeyText(MappedKey);
		}
	}

	return BwayAbilitySetAccess::FallbackKeyLabelForTag(InputTag);
}

FText UBwayHeroAbilityUILibrary::ResolveSharedMovementSummaryText(const ULocalPlayer* OptionalLocalPlayer)
{
	FText JumpKeyLabel;
	const FGameplayTag JumpTag = FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.Ability.Jump")), /*ErrorIfNotFound*/ false);
	if (JumpTag.IsValid())
	{
		JumpKeyLabel = ResolveKeyLabelForInputTag(JumpTag, OptionalLocalPlayer);
	}
	if (JumpKeyLabel.IsEmpty())
	{
		JumpKeyLabel = FText::FromString(TEXT("Space"));
	}

	FText SlideKeyLabel;
	const FGameplayTag SlideTag = FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.Ability.Slide")), /*ErrorIfNotFound*/ false);
	if (SlideTag.IsValid())
	{
		SlideKeyLabel = ResolveKeyLabelForInputTag(SlideTag, OptionalLocalPlayer);
	}
	if (SlideKeyLabel.IsEmpty())
	{
		if (const UInputAction* SlideAction = LoadObject<UInputAction>(nullptr, TEXT("/BreakawayCore/Input/Actions/IA_BW_Slide.IA_BW_Slide")))
		{
			const FKey SlideKey = BwayAbilitySetAccess::FindMappedKeyForAction(SlideAction, OptionalLocalPlayer);
			if (SlideKey.IsValid())
			{
				SlideKeyLabel = BwayAbilitySetAccess::CompactKeyText(SlideKey);
			}
		}
	}
	if (SlideKeyLabel.IsEmpty())
	{
		SlideKeyLabel = FText::FromString(TEXT("L-Shift"));
	}

	return FText::Format(
		NSLOCTEXT("BwayAbilityUI", "SharedMovementFmt", "Shared Movement: [{0}] Jump   |   [{1}] Slide / Slide-Jump"),
		JumpKeyLabel,
		SlideKeyLabel);
}
