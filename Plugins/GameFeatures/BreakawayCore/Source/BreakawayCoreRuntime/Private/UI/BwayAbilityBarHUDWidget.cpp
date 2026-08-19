#include "UI/BwayAbilityBarHUDWidget.h"

#include "BwayPlayerState.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "Character/LyraPawnData.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "HeroSystems/BwayHeroAbilityUILibrary.h"
#include "Input/LyraInputConfig.h"
#include "InputAction.h"
#include "UI/BwayAbilityUISettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayAbilityBarHUDWidget)

#define LOCTEXT_NAMESPACE "BwayAbilityBarHUDWidget"

namespace BwayMatchAbilityBar
{
	constexpr int32 SlotCount = 6;

	const FName HeroSlotTagNames[SlotCount] = {
		TEXT("InputTag.Ability.Buildable"),
		TEXT("InputTag.Ability.Primary"),
		TEXT("InputTag.Ability.Ability4"),
		TEXT("InputTag.Ability.Ability1"),
		TEXT("InputTag.Ability.Ability2"),
		TEXT("InputTag.Ability.Ability3"),
	};
}

UBwayAbilityBarHUDWidget::UBwayAbilityBarHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayAbilityBarHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	RebuildSlotWidgets();
	BindToLocalPlayerState();
	RefreshAbilityBar();
}

void UBwayAbilityBarHUDWidget::NativeDestruct()
{
	UnbindFromLocalPlayerState();

	if (BoundInputSubsystem.IsValid())
	{
		BoundInputSubsystem->ControlMappingsRebuiltDelegate.RemoveDynamic(
			this,
			&ThisClass::HandleControlMappingsRebuilt);
	}
	BoundInputSubsystem.Reset();
	SlotWidgets.Reset();

	Super::NativeDestruct();
}

void UBwayAbilityBarHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	ABwayPlayerState* CurrentPlayerState = Cast<ABwayPlayerState>(GetOwningPlayerState());
	if (CurrentPlayerState != BoundPlayerState.Get())
	{
		BindToLocalPlayerState();
	}

	TimeSinceLastAbilityRefresh += InDeltaTime;
	const float RefreshInterval = bAnySlotOnCooldown ? CooldownRefreshInterval : AbilityRefreshInterval;
	if (bRefreshRequested || TimeSinceLastAbilityRefresh >= RefreshInterval)
	{
		TimeSinceLastAbilityRefresh = 0.0f;
		bRefreshRequested = false;
		RefreshAbilityBar();
	}
}

void UBwayAbilityBarHUDWidget::NotifyRoundStateChanged(FName NewState)
{
	RequestAbilityBarRefresh();
}

void UBwayAbilityBarHUDWidget::RebuildSlotWidgets()
{
	SlotWidgets.Reset();

	if (!AbilitySlots || !AbilitySlotWidgetClass)
	{
		return;
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	if (!OwningPlayer)
	{
		return;
	}

	AbilitySlots->ClearChildren();
	SlotWidgets.Reserve(BwayMatchAbilityBar::SlotCount);

	for (int32 Position = 0; Position < BwayMatchAbilityBar::SlotCount; ++Position)
	{
		UBwayMatchAbilitySlotWidget* SlotWidget =
			CreateWidget<UBwayMatchAbilitySlotWidget>(OwningPlayer, AbilitySlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}

		if (UHorizontalBoxSlot* HorizontalSlot = AbilitySlots->AddChildToHorizontalBox(SlotWidget))
		{
			HorizontalSlot->SetPadding(AbilitySlotPadding);
		}
		SlotWidgets.Add(SlotWidget);
	}
}

void UBwayAbilityBarHUDWidget::BindToLocalPlayerState()
{
	UnbindFromLocalPlayerState();

	ABwayPlayerState* PlayerState = Cast<ABwayPlayerState>(GetOwningPlayerState());
	if (PlayerState)
	{
		BoundPlayerState = PlayerState;
		BoundAbilitySystemComponent = PlayerState->GetAbilitySystemComponent();

		PlayerState->OnRelicPossessionChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleRelicPossessionChanged);
		PlayerState->OnBuildablePlacedThisRoundChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleBuildableStateChanged);
		PlayerState->OnSelectedHeroChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleSelectedHeroChanged);
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = nullptr;
	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	}

	if (BoundInputSubsystem.Get() != InputSubsystem)
	{
		if (BoundInputSubsystem.IsValid())
		{
			BoundInputSubsystem->ControlMappingsRebuiltDelegate.RemoveDynamic(
				this,
				&ThisClass::HandleControlMappingsRebuilt);
		}

		BoundInputSubsystem = InputSubsystem;
		if (InputSubsystem)
		{
			InputSubsystem->ControlMappingsRebuiltDelegate.AddUniqueDynamic(
				this,
				&ThisClass::HandleControlMappingsRebuilt);
		}
	}

	RequestAbilityBarRefresh();
}

void UBwayAbilityBarHUDWidget::UnbindFromLocalPlayerState()
{
	if (BoundPlayerState.IsValid())
	{
		BoundPlayerState->OnRelicPossessionChanged.RemoveDynamic(
			this,
			&ThisClass::HandleRelicPossessionChanged);
		BoundPlayerState->OnBuildablePlacedThisRoundChanged.RemoveDynamic(
			this,
			&ThisClass::HandleBuildableStateChanged);
		BoundPlayerState->OnSelectedHeroChanged.RemoveDynamic(
			this,
			&ThisClass::HandleSelectedHeroChanged);
	}

	BoundPlayerState.Reset();
	BoundAbilitySystemComponent.Reset();
}

void UBwayAbilityBarHUDWidget::RequestAbilityBarRefresh()
{
	bRefreshRequested = true;
}

TArray<FGameplayTag> UBwayAbilityBarHUDWidget::GetFixedHeroSlotTags() const
{
	const TArray<FGameplayTag>& ConfiguredTags = UBwayAbilityUISettings::Get().MatchAbilityBarSlotTags;
	if (ConfiguredTags.Num() == BwayMatchAbilityBar::SlotCount
		&& !ConfiguredTags.ContainsByPredicate([](const FGameplayTag& Tag) { return !Tag.IsValid(); }))
	{
		return ConfiguredTags;
	}

	TArray<FGameplayTag> SlotTags;
	SlotTags.Reserve(BwayMatchAbilityBar::SlotCount);
	for (const FName TagName : BwayMatchAbilityBar::HeroSlotTagNames)
	{
		SlotTags.Add(FGameplayTag::RequestGameplayTag(TagName, /*ErrorIfNotFound*/ false));
	}
	return SlotTags;
}

FText UBwayAbilityBarHUDWidget::GetHeroFallbackLabel(const int32 Position) const
{
	switch (Position)
	{
	case 0:
		return LOCTEXT("BuildableFallback", "Buildable");
	case 1:
		return LOCTEXT("PrimaryFallback", "Primary");
	case 2:
		return LOCTEXT("Ability4Fallback", "F Ability");
	case 3:
		return LOCTEXT("Ability1Fallback", "Q Ability");
	case 4:
		return LOCTEXT("Ability2Fallback", "E Ability");
	case 5:
		return LOCTEXT("Ability3Fallback", "R Ability");
	default:
		return LOCTEXT("UnknownAbilityFallback", "Unavailable");
	}
}

const UInputAction* UBwayAbilityBarHUDWidget::ResolveInputAction(
	const FGameplayTag InputTag,
	const TSoftObjectPtr<const UInputAction>& FallbackInputAction) const
{
	if (InputTag.IsValid() && BoundPlayerState.IsValid())
	{
		if (const ULyraPawnData* PawnData = BoundPlayerState->GetPawnData<ULyraPawnData>())
		{
			if (const ULyraInputConfig* InputConfig = PawnData->InputConfig)
			{
				for (const FLyraInputAction& InputAction : InputConfig->AbilityInputActions)
				{
					if (InputAction.InputTag == InputTag && InputAction.InputAction)
					{
						return InputAction.InputAction;
					}
				}

				for (const FLyraInputAction& InputAction : InputConfig->NativeInputActions)
				{
					if (InputAction.InputTag == InputTag && InputAction.InputAction)
					{
						return InputAction.InputAction;
					}
				}
			}
		}
	}

	return FallbackInputAction.LoadSynchronous();
}

FText UBwayAbilityBarHUDWidget::ResolveKeyLabel(const UInputAction* InputAction) const
{
	if (!InputAction || !BoundInputSubsystem.IsValid())
	{
		return LOCTEXT("UnboundKey", "—");
	}

	const TArray<FKey> BoundKeys = BoundInputSubsystem->QueryKeysMappedToAction(InputAction);
	if (BoundKeys.IsEmpty())
	{
		return LOCTEXT("UnboundKey", "—");
	}

	const FKey* PreferredKey = BoundKeys.FindByPredicate(
		[](const FKey& Key)
		{
			return Key.IsValid() && !Key.IsGamepadKey();
		});

	return CompactKeyDisplayName(PreferredKey ? *PreferredKey : BoundKeys[0]);
}

FText UBwayAbilityBarHUDWidget::CompactKeyDisplayName(const FKey& Key) const
{
	if (Key == EKeys::LeftMouseButton)
	{
		return LOCTEXT("LeftMouseButtonShort", "LMB");
	}
	if (Key == EKeys::RightMouseButton)
	{
		return LOCTEXT("RightMouseButtonShort", "RMB");
	}
	if (Key == EKeys::MiddleMouseButton)
	{
		return LOCTEXT("MiddleMouseButtonShort", "MMB");
	}

	return Key.IsValid() ? Key.GetDisplayName(/*bLongDisplayName*/ false) : LOCTEXT("UnboundKey", "—");
}

void UBwayAbilityBarHUDWidget::FillCooldownState(
	UAbilitySystemComponent* ASC,
	TSubclassOf<ULyraGameplayAbility> AbilityClass,
	FBwayMatchAbilitySlotViewModel& InOutViewModel) const
{
	InOutViewModel.bIsOnCooldown = false;
	InOutViewModel.CooldownPercent = 0.f;
	InOutViewModel.CountdownTime = 0.f;

	if (!ASC || !*AbilityClass)
	{
		return;
	}

	const ULyraGameplayAbility* AbilityCDO = AbilityClass.GetDefaultObject();
	if (!AbilityCDO)
	{
		return;
	}

	const FGameplayTagContainer* CooldownTags = AbilityCDO->GetCooldownTags();
	if (!CooldownTags || CooldownTags->IsEmpty())
	{
		return;
	}

	if (!ASC->HasAnyMatchingGameplayTags(*CooldownTags))
	{
		return;
	}

	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags);
	const TArray<FActiveGameplayEffectHandle> ActiveHandles = ASC->GetActiveEffects(Query);
	if (ActiveHandles.Num() <= 0)
	{
		return;
	}

	float BestRemaining = 0.f;
	float BestDuration = 0.f;
	const float WorldTime = ASC->GetWorld() ? ASC->GetWorld()->GetTimeSeconds() : 0.f;

	for (const FActiveGameplayEffectHandle& Handle : ActiveHandles)
	{
		if (const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(Handle))
		{
			const float Remaining = ActiveGE->GetTimeRemaining(WorldTime);
			const float Duration = ActiveGE->GetDuration();
			if (Remaining > BestRemaining)
			{
				BestRemaining = Remaining;
				BestDuration = Duration;
			}
		}
	}

	if (BestRemaining <= KINDA_SMALL_NUMBER || BestDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	InOutViewModel.bIsOnCooldown = true;
	InOutViewModel.CountdownTime = FMath::Clamp(BestRemaining, 0.f, BestDuration);
	InOutViewModel.CooldownPercent = FMath::Clamp((BestRemaining / BestDuration), 0.f, 1.f);
}

void UBwayAbilityBarHUDWidget::RefreshAbilityBar()
{
	if (SlotWidgets.Num() != BwayMatchAbilityBar::SlotCount)
	{
		RebuildSlotWidgets();
	}

	if (SlotWidgets.Num() != BwayMatchAbilityBar::SlotCount)
	{
		return;
	}

	if (!BoundPlayerState.IsValid())
	{
		BindToLocalPlayerState();
	}

	const TArray<FGameplayTag> HeroSlotTags = GetFixedHeroSlotTags();
	const UBwayAbilityUISettings& UISettings = UBwayAbilityUISettings::Get();
	const bool bIsRelicCarrier = BoundPlayerState.IsValid() && BoundPlayerState->bHasRelic;
	const bool bBuildableConsumed =
		BoundPlayerState.IsValid() && BoundPlayerState->HasPlacedBuildableThisRound();

	bool bFoundCooldown = false;

	for (int32 Position = 0; Position < BwayMatchAbilityBar::SlotCount; ++Position)
	{
		FBwayMatchAbilitySlotViewModel ViewModel;
		ViewModel.Position = Position;
		ViewModel.bIsBuildable = Position == 0;
		ViewModel.bIsConsumed = ViewModel.bIsBuildable && bBuildableConsumed;

		FGameplayTag InputTag = HeroSlotTags.IsValidIndex(Position)
			? HeroSlotTags[Position]
			: FGameplayTag();
		FText FallbackLabel = GetHeroFallbackLabel(Position);
		TSoftObjectPtr<const UInputAction> FallbackInputAction;

		if (bIsRelicCarrier && Position > 0)
		{
			ViewModel.bIsRelicAbility = true;
			const int32 RelicIndex = Position - 1;
			if (UISettings.RelicCombatSlots.IsValidIndex(RelicIndex))
			{
				const FBwayRelicAbilityBarSlot& RelicSlot = UISettings.RelicCombatSlots[RelicIndex];
				InputTag = RelicSlot.InputTag;
				FallbackInputAction = RelicSlot.InputAction;
				if (!RelicSlot.PlaceholderLabel.IsEmpty())
				{
					FallbackLabel = RelicSlot.PlaceholderLabel;
				}
			}
			else
			{
				InputTag = FGameplayTag();
				FallbackLabel = LOCTEXT("MissingRelicSlot", "Unavailable");
			}
		}

		ViewModel.DisplayInfo.InputActionTag = InputTag;
		ViewModel.DisplayInfo.AbilityName = FallbackLabel;

		TSubclassOf<ULyraGameplayAbility> AbilityClass;
		const bool bFoundAbility = UBwayHeroAbilityUILibrary::FindGrantedAbilityForInputTagOnASC(
			InputTag,
			BoundAbilitySystemComponent.Get(),
			AbilityClass);
		if (bFoundAbility)
		{
			ViewModel.DisplayInfo =
				UBwayHeroAbilityUILibrary::MakeDisplayInfoFromAbility(AbilityClass, InputTag);

			const FString ResolvedName = ViewModel.DisplayInfo.AbilityName.ToString();
			if (ViewModel.DisplayInfo.AbilityName.IsEmpty()
				|| ResolvedName.StartsWith(TEXT("GA_"))
				|| ResolvedName.EndsWith(TEXT("_C")))
			{
				ViewModel.DisplayInfo.AbilityName = FallbackLabel;
			}

			FillCooldownState(BoundAbilitySystemComponent.Get(), AbilityClass, ViewModel);
			bFoundCooldown = bFoundCooldown || ViewModel.bIsOnCooldown;
		}

		const UInputAction* InputAction = ResolveInputAction(InputTag, FallbackInputAction);
		ViewModel.KeyLabel = ResolveKeyLabel(InputAction);
		ViewModel.bIsAvailable = bFoundAbility && InputAction && !ViewModel.bIsConsumed;

		SlotWidgets[Position]->SetSlotViewModel(ViewModel);
	}

	bAnySlotOnCooldown = bFoundCooldown;
}

void UBwayAbilityBarHUDWidget::HandleRelicPossessionChanged(bool bHasRelic)
{
	RequestAbilityBarRefresh();
	RefreshAbilityBar();
}

void UBwayAbilityBarHUDWidget::HandleBuildableStateChanged(bool bHasPlacedBuildable)
{
	RequestAbilityBarRefresh();
	RefreshAbilityBar();
}

void UBwayAbilityBarHUDWidget::HandleSelectedHeroChanged(FPrimaryAssetId NewHeroId)
{
	RequestAbilityBarRefresh();
}

void UBwayAbilityBarHUDWidget::HandleControlMappingsRebuilt()
{
	RequestAbilityBarRefresh();
	RefreshAbilityBar();
}

#undef LOCTEXT_NAMESPACE
