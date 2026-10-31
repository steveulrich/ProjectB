#include "UI/BwayAbilityBarHUDWidget.h"

#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Buildable/BuildableBase.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Character/LyraPawnData.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "HeroSystems/BwayHeroAbilityUILibrary.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "Input/LyraInputConfig.h"
#include "InputAction.h"
#include "InputMappingContext.h"
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

	const FName RelicCarryingStateTagName(TEXT("State.Relic.Carrying"));
}

UBwayAbilityBarHUDWidget::UBwayAbilityBarHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayAbilityBarHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!CachedSharedSlideAction)
	{
		CachedSharedSlideAction = LoadObject<UInputAction>(
			nullptr,
			TEXT("/BreakawayCore/Input/Actions/IA_BW_Slide.IA_BW_Slide"));
	}
	if (!CachedRelicRequestAction)
	{
		CachedRelicRequestAction = LoadObject<UInputAction>(
			nullptr,
			TEXT("/BreakawayCore/Input/Actions/IA_BW_Relic_Request.IA_BW_Relic_Request"));
	}

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

	if (InputMethodChangedHandle.IsValid())
	{
		if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		{
			if (UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(LocalPlayer))
			{
				CommonInput->OnInputMethodChangedNative.Remove(InputMethodChangedHandle);
			}
		}
		InputMethodChangedHandle.Reset();
	}

	SlotWidgets.Reset();
	AuxiliaryPromptsBorder = nullptr;
	Text_SharedSlidePrompt = nullptr;
	Text_RelicPrompt = nullptr;

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

void UBwayAbilityBarHUDWidget::SetInputDeviceOverrideForTest(const bool bEnableOverride, const bool bPreferGamepad)
{
	bTestOverrideInputDevice = bEnableOverride;
	bTestPreferGamepad = bPreferGamepad;
	RequestAbilityBarRefresh();
	RefreshAbilityBar();
}

bool UBwayAbilityBarHUDWidget::ShouldPreferGamepadLabels() const
{
	if (bTestOverrideInputDevice)
	{
		return bTestPreferGamepad;
	}

	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (const UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(LocalPlayer))
		{
			return CommonInput->GetCurrentInputType() == ECommonInputType::Gamepad;
		}
	}

	return false;
}

void UBwayAbilityBarHUDWidget::RebuildSlotWidgets()
{
	SlotWidgets.Reset();
	AuxiliaryPromptsBorder = nullptr;
	Text_SharedSlidePrompt = nullptr;
	Text_RelicPrompt = nullptr;

	if (WidgetTree)
	{
		// Clear authored white M_UI_FocusBorder on AbilityBarFrame; the trapezoidal pedestal is painted on UBwayMatchBroadcastOverlayWidget.
		WidgetTree->ForEachWidget([](UWidget* Candidate)
		{
			if (UBorder* Border = Cast<UBorder>(Candidate))
			{
				FSlateBrush NoBrush;
				NoBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
				Border->SetBrush(NoBrush);
				Border->SetBrushColor(FLinearColor::Transparent);
				Border->SetPadding(FMargin(0.0f));
			}
			else if (USizeBox* SizeBox = Cast<USizeBox>(Candidate))
			{
				SizeBox->ClearWidthOverride();
				SizeBox->ClearHeightOverride();
			}
		});
	}

	if (!AbilitySlots && WidgetTree && !WidgetTree->RootWidget)
	{
		AbilitySlots = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("AbilitySlots"));
		WidgetTree->RootWidget = AbilitySlots;
	}

	if (!AbilitySlotWidgetClass)
	{
		const FSoftClassPath DefaultSlotClassPath(
			TEXT("/BreakawayCore/UI/Match/W_BW_MatchAbilitySlot.W_BW_MatchAbilitySlot_C"));
		if (UClass* LoadedSlotClass = DefaultSlotClassPath.TryLoadClass<UBwayMatchAbilitySlotWidget>())
		{
			AbilitySlotWidgetClass = LoadedSlotClass;
		}
	}

	if (!AbilitySlots || !AbilitySlotWidgetClass)
	{
		return;
	}

	APlayerController* OwningPlayer = GetOwningPlayer();
	UWorld* World = GetWorld();
	if (!OwningPlayer && !World)
	{
		return;
	}

	AbilitySlots->ClearChildren();
	SlotWidgets.Reserve(BwayMatchAbilityBar::SlotCount);

	// Keep SlotWidgets[0..5] in logical slot index order while placing them into AbilitySlots in
	// Breakaway broadcast visual order: 1 (Buildable) | LMB (Primary) | F (Mobility) | R (Ultimate center) | Q | E.
	for (int32 Position = 0; Position < BwayMatchAbilityBar::SlotCount; ++Position)
	{
		UBwayMatchAbilitySlotWidget* SlotWidget = OwningPlayer
			? CreateWidget<UBwayMatchAbilitySlotWidget>(OwningPlayer, AbilitySlotWidgetClass)
			: CreateWidget<UBwayMatchAbilitySlotWidget>(World, AbilitySlotWidgetClass);
		if (!SlotWidget)
		{
			continue;
		}
		SlotWidgets.Add(SlotWidget);
	}

	static constexpr int32 VisualOrder[BwayMatchAbilityBar::SlotCount] = { 0, 1, 2, 5, 3, 4 };
	for (const int32 LogicalIdx : VisualOrder)
	{
		if (!SlotWidgets.IsValidIndex(LogicalIdx) || !SlotWidgets[LogicalIdx])
		{
			continue;
		}

		if (UHorizontalBoxSlot* HorizontalSlot = AbilitySlots->AddChildToHorizontalBox(SlotWidgets[LogicalIdx]))
		{
			const FMargin HexPadding = (LogicalIdx == 0)
				? FMargin(2.0f, 0.0f, 10.0f, 0.0f)
				: ((LogicalIdx == 5) ? FMargin(3.0f, 0.0f, 3.0f, 0.0f) : FMargin(2.0f, 0.0f, 2.0f, 0.0f));
			HorizontalSlot->SetPadding(HexPadding);
			HorizontalSlot->SetVerticalAlignment(VAlign_Bottom);
		}
	}

	// Retain separate non-slot prompt widgets for Shared Slide (IA_BW_Slide) and Relic Request / Controls
	// so hero mobility (Position 2 / InputTag.Ability.Ability4) is never conflated with shared slide,
	// while keeping the visual pedestal clean per broadcast HUD parity.
	if (WidgetTree)
	{
		AuxiliaryPromptsBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("AuxiliaryPromptsBorder"));
		AuxiliaryPromptsBorder->SetVisibility(ESlateVisibility::Collapsed);

		UVerticalBox* PromptsVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("AuxiliaryPromptsVBox"));
		Text_SharedSlidePrompt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_SharedSlidePrompt"));
		Text_RelicPrompt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_RelicPrompt"));

		FSlateFontInfo PromptFont = Text_SharedSlidePrompt->GetFont();
		PromptFont.Size = 11;
		Text_SharedSlidePrompt->SetFont(PromptFont);
		Text_SharedSlidePrompt->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.92f, 1.0f, 0.96f)));
		Text_SharedSlidePrompt->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text_SharedSlidePrompt->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f));

		Text_RelicPrompt->SetFont(PromptFont);
		Text_RelicPrompt->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.84f, 0.32f, 0.96f)));
		Text_RelicPrompt->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text_RelicPrompt->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f));

		if (UVerticalBoxSlot* SlideSlot = PromptsVBox->AddChildToVerticalBox(Text_SharedSlidePrompt))
		{
			SlideSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		}
		PromptsVBox->AddChildToVerticalBox(Text_RelicPrompt);
		AuxiliaryPromptsBorder->SetContent(PromptsVBox);

		if (UHorizontalBoxSlot* AuxSlot = AbilitySlots->AddChildToHorizontalBox(AuxiliaryPromptsBorder))
		{
			AuxSlot->SetPadding(FMargin(8.0f, 0.0f, 2.0f, 0.0f));
			AuxSlot->SetVerticalAlignment(VAlign_Center);
		}
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
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (LocalPlayer)
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

	if (!InputMethodChangedHandle.IsValid() && LocalPlayer)
	{
		if (UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(LocalPlayer))
		{
			InputMethodChangedHandle = CommonInput->OnInputMethodChangedNative.AddWeakLambda(
				this,
				[this](ECommonInputType /*NewInputType*/)
				{
					HandleControlMappingsRebuilt();
				});
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
		return LOCTEXT("Ability4Fallback", "Hero Mobility");
	case 3:
		return LOCTEXT("Ability1Fallback", "Ability 1");
	case 4:
		return LOCTEXT("Ability2Fallback", "Ability 2");
	case 5:
		return LOCTEXT("Ability3Fallback", "Ultimate");
	default:
		return LOCTEXT("UnknownAbilityFallback", "Unavailable");
	}
}

const UInputAction* UBwayAbilityBarHUDWidget::ResolveInputAction(
	const FGameplayTag InputTag,
	const TSoftObjectPtr<const UInputAction>& FallbackInputAction) const
{
	if (InputTag.IsValid())
	{
		const ULyraInputConfig* InputConfig = nullptr;
		if (BoundPlayerState.IsValid())
		{
			if (const ULyraPawnData* PawnData = BoundPlayerState->GetPawnData<ULyraPawnData>())
			{
				InputConfig = PawnData->InputConfig;
			}
		}
		if (!InputConfig)
		{
			InputConfig = LoadObject<ULyraInputConfig>(
				nullptr,
				TEXT("/BreakawayCore/Input/DA_BW_InputData_Humanoid.DA_BW_InputData_Humanoid"));
		}

		if (InputConfig)
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

	return FallbackInputAction.LoadSynchronous();
}

FText UBwayAbilityBarHUDWidget::ResolveKeyLabel(const UInputAction* InputAction) const
{
	if (!InputAction)
	{
		return LOCTEXT("UnboundKey", "—");
	}

	TArray<FKey> BoundKeys;
	if (BoundInputSubsystem.IsValid())
	{
		BoundKeys = BoundInputSubsystem->QueryKeysMappedToAction(InputAction);
	}
	if (BoundKeys.IsEmpty())
	{
		static const TCHAR* ImcPaths[] = {
			TEXT("/BreakawayCore/Input/Mappings/IMC_BW_HeroAbilities.IMC_BW_HeroAbilities"),
			TEXT("/BreakawayCore/Input/Mappings/IMC_BW_Default.IMC_BW_Default"),
			TEXT("/BreakawayCore/Input/Mappings/IMC_BW_PlayerUI.IMC_BW_PlayerUI"),
		};
		for (const TCHAR* ImcPath : ImcPaths)
		{
			if (const UInputMappingContext* Imc = LoadObject<UInputMappingContext>(nullptr, ImcPath))
			{
				for (const FEnhancedActionKeyMapping& Mapping : Imc->GetMappings())
				{
					if (Mapping.Action == InputAction && Mapping.Key.IsValid())
					{
						BoundKeys.AddUnique(Mapping.Key);
					}
				}
			}
		}
	}

	if (BoundKeys.IsEmpty())
	{
		return LOCTEXT("UnboundKey", "—");
	}

	const bool bPreferGamepad = ShouldPreferGamepadLabels();
	const FKey* PreferredKey = BoundKeys.FindByPredicate(
		[bPreferGamepad](const FKey& Key)
		{
			return Key.IsValid() && (Key.IsGamepadKey() == bPreferGamepad);
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
	if (Key == EKeys::LeftShift || Key == EKeys::RightShift)
	{
		return LOCTEXT("ShiftShort", "SHIFT");
	}
	if (Key == EKeys::LeftControl || Key == EKeys::RightControl)
	{
		return LOCTEXT("CtrlShort", "CTRL");
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

void UBwayAbilityBarHUDWidget::RefreshAuxiliaryPrompts(const bool bIsRelicCarrier)
{
	if (!CachedSharedSlideAction)
	{
		CachedSharedSlideAction = LoadObject<UInputAction>(
			nullptr,
			TEXT("/BreakawayCore/Input/Actions/IA_BW_Slide.IA_BW_Slide"));
	}
	if (!CachedRelicRequestAction)
	{
		CachedRelicRequestAction = LoadObject<UInputAction>(
			nullptr,
			TEXT("/BreakawayCore/Input/Actions/IA_BW_Relic_Request.IA_BW_Relic_Request"));
	}

	CachedSharedSlideKeyLabel = ResolveKeyLabel(CachedSharedSlideAction);
	CachedRelicRequestKeyLabel = ResolveKeyLabel(CachedRelicRequestAction);

	if (Text_SharedSlidePrompt)
	{
		Text_SharedSlidePrompt->SetText(
			FText::FromString(FString::Printf(TEXT("[%s] SLIDE"), *CachedSharedSlideKeyLabel.ToString())));
	}

	if (Text_RelicPrompt)
	{
		if (bIsRelicCarrier)
		{
			const UBwayAbilityUISettings& UISettings = UBwayAbilityUISettings::Get();
			const FText ThrowKey = UISettings.RelicCombatSlots.IsValidIndex(0)
				? ResolveKeyLabel(ResolveInputAction(UISettings.RelicCombatSlots[0].InputTag, UISettings.RelicCombatSlots[0].InputAction))
				: LOCTEXT("UnboundKey", "—");
			const FText PassKey = UISettings.RelicCombatSlots.IsValidIndex(2)
				? ResolveKeyLabel(ResolveInputAction(UISettings.RelicCombatSlots[2].InputTag, UISettings.RelicCombatSlots[2].InputAction))
				: LOCTEXT("UnboundKey", "—");
			const FText DropKey = UISettings.RelicCombatSlots.IsValidIndex(3)
				? ResolveKeyLabel(ResolveInputAction(UISettings.RelicCombatSlots[3].InputTag, UISettings.RelicCombatSlots[3].InputAction))
				: LOCTEXT("UnboundKey", "—");

			Text_RelicPrompt->SetText(
				FText::FromString(FString::Printf(
					TEXT("RELIC [%s] THROW • [%s] PASS • [%s] DROP"),
					*ThrowKey.ToString(),
					*PassKey.ToString(),
					*DropKey.ToString())));
			Text_RelicPrompt->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.84f, 0.22f, 1.0f)));
		}
		else
		{
			Text_RelicPrompt->SetText(
				FText::FromString(FString::Printf(TEXT("[%s] CALL RELIC"), *CachedRelicRequestKeyLabel.ToString())));
			Text_RelicPrompt->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.86f, 0.62f, 0.94f)));
		}
	}
}

void UBwayAbilityBarHUDWidget::RefreshAbilityBar()
{
	if (SlotWidgets.Num() != BwayMatchAbilityBar::SlotCount)
	{
		RebuildSlotWidgets();
	}

	if (!BoundPlayerState.IsValid())
	{
		BindToLocalPlayerState();
	}

	if (BoundPlayerState.IsValid() && !BoundAbilitySystemComponent.IsValid())
	{
		BoundAbilitySystemComponent = BoundPlayerState->GetAbilitySystemComponent();
	}

	const FGameplayTag CarryingStateTag =
		FGameplayTag::RequestGameplayTag(BwayMatchAbilityBar::RelicCarryingStateTagName, /*ErrorIfNotFound*/ false);
	const bool bHasCarryingTag =
		BoundAbilitySystemComponent.IsValid()
		&& CarryingStateTag.IsValid()
		&& BoundAbilitySystemComponent->HasMatchingGameplayTag(CarryingStateTag);
	const bool bIsRelicCarrier =
		(BoundPlayerState.IsValid() && BoundPlayerState->bHasRelic) || bHasCarryingTag;

	RefreshAuxiliaryPrompts(bIsRelicCarrier);

	if (SlotWidgets.Num() != BwayMatchAbilityBar::SlotCount)
	{
		return;
	}

	const TArray<FGameplayTag> HeroSlotTags = GetFixedHeroSlotTags();
	const UBwayAbilityUISettings& UISettings = UBwayAbilityUISettings::Get();
	const bool bBuildableConsumed =
		BoundPlayerState.IsValid() && BoundPlayerState->HasPlacedBuildableThisRound();

	int32 ActiveBuildableCount = 0;
	int32 MaxActiveBuildables = 0;
	if (BoundPlayerState.IsValid())
	{
		if (const ABwayGameState* GameState = GetBwayGameState())
		{
			if (GameState->BuildableRegistryComponent)
			{
				ActiveBuildableCount = GameState->BuildableRegistryComponent->GetBuildableCountForPlayer(BoundPlayerState.Get());
			}
		}

		const FPrimaryAssetId HeroId = BoundPlayerState->GetSelectedHeroId();
		if (HeroId.IsValid())
		{
			if (const UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId))
			{
				if (const UBwayBuildableDataAsset* BuildableData = HeroData->GetBuildableDataAsset())
				{
					MaxActiveBuildables = BuildableData->MaxActiveBuildablesPerPlayer;
				}
			}
		}
		if (MaxActiveBuildables <= 0)
		{
			MaxActiveBuildables = 2;
		}
	}

	bool bFoundCooldown = false;

	for (int32 Position = 0; Position < BwayMatchAbilityBar::SlotCount; ++Position)
	{
		FBwayMatchAbilitySlotViewModel ViewModel;
		ViewModel.Position = Position;
		ViewModel.bIsBuildable = Position == 0;
		ViewModel.bIsConsumed = ViewModel.bIsBuildable && bBuildableConsumed;
		if (ViewModel.bIsBuildable)
		{
			ViewModel.ActiveBuildableCount = ActiveBuildableCount;
			ViewModel.MaxActiveBuildables = MaxActiveBuildables;
		}

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
				FallbackLabel = LOCTEXT("MissingRelicSlot", "Relic Lock");
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

FText UBwayAbilityBarHUDWidget::GetSharedSlidePromptText() const
{
	return Text_SharedSlidePrompt ? Text_SharedSlidePrompt->GetText() : FText::GetEmpty();
}

FText UBwayAbilityBarHUDWidget::GetRelicPromptText() const
{
	return Text_RelicPrompt ? Text_RelicPrompt->GetText() : FText::GetEmpty();
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
