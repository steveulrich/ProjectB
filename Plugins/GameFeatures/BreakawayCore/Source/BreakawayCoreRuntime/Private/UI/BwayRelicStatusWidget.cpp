// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayRelicStatusWidget.h"

#include "Blueprint/WidgetTree.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "CommonInputSubsystem.h"
#include "CommonInputTypeEnum.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "GameModes/BwayMatchPhaseTypes.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRelicStatusWidget)

UBwayRelicStatusWidget::UBwayRelicStatusWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UBwayRelicStatusWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureBannerStyling();
	BindInputDelegates();
	RefreshRelicStatus();

	if (RelicPollInterval > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RelicPollHandle,
				this,
				&UBwayRelicStatusWidget::RefreshRelicStatus,
				RelicPollInterval,
				true);
		}
	}
}

void UBwayRelicStatusWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RelicPollHandle);
	}

	UnbindInputDelegates();

	Super::NativeDestruct();
}

void UBwayRelicStatusWidget::NotifyRoundStateChanged(FName NewState)
{
	RefreshRelicStatus();
}

void UBwayRelicStatusWidget::SetInputDeviceOverrideForTest(const bool bEnableOverride, const bool bPreferGamepad)
{
	bTestOverrideInputDevice = bEnableOverride;
	bTestPreferGamepad = bPreferGamepad;
	bHasInitializedState = false;
	RefreshRelicStatus();
}

void UBwayRelicStatusWidget::BindInputDelegates()
{
	UnbindInputDelegates();

	if (!CachedUpgradeShopAction)
	{
		if (const ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(GetOwningPlayer()))
		{
			CachedUpgradeShopAction = BwayPC->GetUpgradeShopInputAction();
		}
		if (!CachedUpgradeShopAction)
		{
			CachedUpgradeShopAction = LoadObject<UInputAction>(
				nullptr,
				TEXT("/BreakawayCore/Input/Actions/IA_BW_UpgradeShop.IA_BW_UpgradeShop"));
		}
	}

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			BoundInputSubsystem = InputSubsystem;
			InputSubsystem->ControlMappingsRebuiltDelegate.AddUniqueDynamic(
				this,
				&ThisClass::HandleControlMappingsRebuilt);
		}

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
}

void UBwayRelicStatusWidget::UnbindInputDelegates()
{
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
}

void UBwayRelicStatusWidget::HandleControlMappingsRebuilt()
{
	bHasInitializedState = false;
	RefreshRelicStatus();
}

void UBwayRelicStatusWidget::EnsureBannerStyling()
{
	if (!WidgetTree)
	{
		return;
	}

	if (!WidgetTree->RootWidget && !Text_Status)
	{
		StatusBannerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusBannerBorder"));
		StatusBannerBorder->SetPadding(FMargin(14.0f, 5.0f));
		StatusBannerBorder->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.06f, 0.84f));

		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusVBox"));
		Text_Status = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Status"));
		Text_Carrier = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Text_Carrier"));

		if (UVerticalBoxSlot* PrimarySlot = VBox->AddChildToVerticalBox(Text_Status))
		{
			PrimarySlot->SetHorizontalAlignment(HAlign_Center);
		}
		if (UVerticalBoxSlot* SecondarySlot = VBox->AddChildToVerticalBox(Text_Carrier))
		{
			SecondarySlot->SetHorizontalAlignment(HAlign_Center);
			SecondarySlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
		}

		StatusBannerBorder->SetContent(VBox);
		WidgetTree->RootWidget = StatusBannerBorder;
	}
	else if (!StatusBannerBorder)
	{
		if (UOverlay* RootOverlay = Cast<UOverlay>(WidgetTree->FindWidget(TEXT("RootOverlay"))))
		{
			if (UVerticalBox* ExistingVBox = Cast<UVerticalBox>(WidgetTree->FindWidget(TEXT("StatusVBox"))))
			{
				if (ExistingVBox->GetParent() == RootOverlay)
				{
					RootOverlay->ClearChildren();
					StatusBannerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusBannerBorder"));
					StatusBannerBorder->SetPadding(FMargin(14.0f, 5.0f));
					StatusBannerBorder->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.06f, 0.82f));
					StatusBannerBorder->SetContent(ExistingVBox);
					if (UOverlaySlot* BorderOverlaySlot = RootOverlay->AddChildToOverlay(StatusBannerBorder))
					{
						BorderOverlaySlot->SetHorizontalAlignment(HAlign_Center);
						BorderOverlaySlot->SetVerticalAlignment(VAlign_Center);
					}
				}
			}
		}
	}

	if (Text_Status)
	{
		Text_Status->SetShadowOffset(FVector2D(1.5f, 1.5f));
		Text_Status->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.92f));
		Text_Status->SetJustification(ETextJustify::Center);
	}
	if (Text_Carrier)
	{
		Text_Carrier->SetShadowOffset(FVector2D(1.25f, 1.25f));
		Text_Carrier->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f));
		Text_Carrier->SetJustification(ETextJustify::Center);
	}
}

FText UBwayRelicStatusWidget::GetResolvedShopKeyLabel() const
{
	const UInputAction* ShopAction = CachedUpgradeShopAction;
	if (!ShopAction)
	{
		ShopAction = LoadObject<UInputAction>(
			nullptr,
			TEXT("/BreakawayCore/Input/Actions/IA_BW_UpgradeShop.IA_BW_UpgradeShop"));
	}
	if (!ShopAction)
	{
		return NSLOCTEXT("BwayRelicStatus", "ShopUnboundKey", "SHOP");
	}

	bool bPreferGamepad = bTestPreferGamepad;
	if (!bTestOverrideInputDevice)
	{
		if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		{
			if (const UCommonInputSubsystem* CommonInput = UCommonInputSubsystem::Get(LocalPlayer))
			{
				bPreferGamepad = (CommonInput->GetCurrentInputType() == ECommonInputType::Gamepad);
			}
		}
	}

	TArray<FKey> BoundKeys;
	if (BoundInputSubsystem.IsValid())
	{
		BoundKeys = BoundInputSubsystem->QueryKeysMappedToAction(ShopAction);
	}
	if (BoundKeys.IsEmpty())
	{
		static const TCHAR* ImcPaths[] = {
			TEXT("/BreakawayCore/Input/Mappings/IMC_BW_PlayerUI.IMC_BW_PlayerUI"),
			TEXT("/BreakawayCore/Input/Mappings/IMC_BW_Default.IMC_BW_Default"),
		};
		for (const TCHAR* ImcPath : ImcPaths)
		{
			if (const UInputMappingContext* Imc = LoadObject<UInputMappingContext>(nullptr, ImcPath))
			{
				for (const FEnhancedActionKeyMapping& Mapping : Imc->GetMappings())
				{
					if (Mapping.Action == ShopAction && Mapping.Key.IsValid())
					{
						BoundKeys.AddUnique(Mapping.Key);
					}
				}
			}
		}
	}

	if (BoundKeys.IsEmpty())
	{
		return NSLOCTEXT("BwayRelicStatus", "ShopUnboundKey", "SHOP");
	}

	const FKey* PreferredKey = BoundKeys.FindByPredicate(
		[bPreferGamepad](const FKey& Key)
		{
			return Key.IsValid() && (Key.IsGamepadKey() == bPreferGamepad);
		});

	const FKey ChosenKey = PreferredKey ? *PreferredKey : BoundKeys[0];
	return ChosenKey.IsValid()
		? ChosenKey.GetDisplayName(/*bLongDisplayName*/ false)
		: NSLOCTEXT("BwayRelicStatus", "ShopUnboundKey", "SHOP");
}

void UBwayRelicStatusWidget::RefreshRelicStatus()
{
	EnsureBannerStyling();
	if (!BoundInputSubsystem.IsValid())
	{
		BindInputDelegates();
	}

	UWorld* World = GetWorld();
	ABwayGameState* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	UBwayRoundManagementComponent* RoundMgmt = GameState ? GameState->GetRoundManagement() : nullptr;

	// Collapse cleanly when occluded by a CommonUI menu/shop or during PostRound / PostMatch overlays.
	// During active Playing, keep internal status/carrier texts updated while collapsing the floating pill visually
	// so the area underneath the top-center broadcast trapezoid stays clean like the reference HUD.
	const EBwayMatchPhase Phase = RoundMgmt ? RoundMgmt->GetCurrentMatchPhase() : EBwayMatchPhase::Playing;
	const bool bHardHiddenPhase =
		IsLocalGameplayHUDOccludedByMenu(GetOwningPlayer())
		|| Phase == EBwayMatchPhase::PostRound
		|| Phase == EBwayMatchPhase::PostMatch
		|| Phase == EBwayMatchPhase::None;
	const bool bShowVisualBanner = !bHardHiddenPhase && (Phase == EBwayMatchPhase::Prematch || Phase == EBwayMatchPhase::Warmup);

	if (StatusBannerBorder)
	{
		StatusBannerBorder->SetVisibility(bShowVisualBanner ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	SetVisibility(bShowVisualBanner ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	if (bHardHiddenPhase)
	{
		bHasInitializedState = false;
		return;
	}

	// Clean CommonUI Prematch / Warmup banner: Round number, authoritative server countdown, and Upgrade Shop availability + active key/gamepad label.
	if (Phase == EBwayMatchPhase::Prematch || Phase == EBwayMatchPhase::Warmup)
	{
		const int32 RoundNum = FMath::Max(1, GetCurrentRoundNumberFromWorld(World));
		const int32 CountdownSec = (Phase == EBwayMatchPhase::Prematch && RoundMgmt)
			? FMath::Max(0, RoundMgmt->GetPrematchTimeRemaining())
			: FMath::Max(0, GetRoundTimeRemainingFromWorld(World));

		const FText PrimaryText = (Phase == EBwayMatchPhase::Prematch)
			? FText::FromString(FString::Printf(TEXT("PREMATCH — ROUND %d STARTS IN %ds"), RoundNum, CountdownSec))
			: FText::FromString(FString::Printf(TEXT("ROUND %d — BATTLE BEGINS IN %ds"), RoundNum, CountdownSec));

		const FText ShopKey = GetResolvedShopKeyLabel();
		const FText SecondaryText = FText::FromString(
			FString::Printf(TEXT("UPGRADE SHOP AVAILABLE [%s]"), *ShopKey.ToString()));

		if (!bHasInitializedState || !PrimaryText.EqualTo(LastPrimaryText) || !SecondaryText.EqualTo(LastSecondaryText))
		{
			bHasInitializedState = true;
			LastPossessingTeamIndex = INDEX_NONE;
			bLastIsCarried = false;
			LastPrimaryText = PrimaryText;
			LastSecondaryText = SecondaryText;
			UpdateBoundStatusTexts(INDEX_NONE, true, PrimaryText, SecondaryText, FLinearColor(0.95f, 0.80f, 0.22f, 1.0f));
		}
		return;
	}

	const int32 LocalTeam = GetLocalPlayerTeamForWidget(this);
	const int32 DisplayPossessingTeam = GetDisplayRelicPossessingTeam(World, LocalTeam);
	const int32 DisplayTerritoryHalf = GetDisplayRelicTerritoryHalf(World, LocalTeam);
	const bool bIsCarried = IsRelicCarriedInWorld(World);
	const FText PrimaryText = BuildStatusText(DisplayPossessingTeam, DisplayTerritoryHalf);
	const FText CarrierName = bIsCarried ? GetRelicCarrierNameFromWorld(World) : FText::GetEmpty();
	const FText TerritoryText = BuildTerritoryText(DisplayTerritoryHalf);
	const FText SecondaryText = (bIsCarried && !CarrierName.IsEmpty())
		? FText::FromString(FString::Printf(TEXT("CARRIER: %s • %s"), *CarrierName.ToString(), *TerritoryText.ToString()))
		: TerritoryText;

	const bool bStateChanged =
		!bHasInitializedState
		|| DisplayPossessingTeam != LastPossessingTeamIndex
		|| bIsCarried != bLastIsCarried
		|| !PrimaryText.EqualTo(LastPrimaryText)
		|| !SecondaryText.EqualTo(LastSecondaryText);

	if (!bStateChanged)
	{
		return;
	}

	bHasInitializedState = true;
	LastPossessingTeamIndex = DisplayPossessingTeam;
	bLastIsCarried = bIsCarried;
	LastPrimaryText = PrimaryText;
	LastSecondaryText = SecondaryText;

	FLinearColor AccentColor(0.86f, 0.90f, 0.96f, 1.0f);
	if (DisplayPossessingTeam == 0)
	{
		AccentColor = FLinearColor(0.32f, 0.72f, 1.0f, 1.0f);
	}
	else if (DisplayPossessingTeam == 1)
	{
		AccentColor = FLinearColor(1.0f, 0.36f, 0.34f, 1.0f);
	}

	UpdateBoundStatusTexts(DisplayPossessingTeam, !SecondaryText.IsEmpty(), PrimaryText, SecondaryText, AccentColor);
	OnRelicStatusUpdated(DisplayPossessingTeam, bIsCarried, PrimaryText, CarrierName);
}

FText UBwayRelicStatusWidget::BuildTerritoryText(const int32 DisplayTerritoryHalf) const
{
	switch (DisplayTerritoryHalf)
	{
	case 0:
		return NSLOCTEXT("BwayRelicStatus", "FriendlyTerritory", "FRIENDLY TERRITORY");
	case 1:
		return NSLOCTEXT("BwayRelicStatus", "EnemyTerritory", "ENEMY TERRITORY");
	default:
		return NSLOCTEXT("BwayRelicStatus", "MidfieldTerritory", "MIDFIELD");
	}
}

FText UBwayRelicStatusWidget::BuildStatusText(const int32 DisplayPossessingTeam, const int32 /*DisplayTerritoryHalf*/) const
{
	switch (DisplayPossessingTeam)
	{
	case 0:
		return Team1StatusText;
	case 1:
		return Team2StatusText;
	default:
		return NeutralStatusText;
	}
}

void UBwayRelicStatusWidget::UpdateBoundStatusTexts(
	int32 /*PossessingTeamIndex*/,
	const bool bHasSecondaryLine,
	const FText& PrimaryText,
	const FText& SecondaryText,
	const FLinearColor& AccentColor)
{
	if (Text_Status)
	{
		Text_Status->SetText(PrimaryText);
		Text_Status->SetColorAndOpacity(FSlateColor(AccentColor));
		Text_Status->SetVisibility(ESlateVisibility::HitTestInvisible);
	}

	if (Text_Carrier)
	{
		if (bHasSecondaryLine && !SecondaryText.IsEmpty())
		{
			Text_Carrier->SetText(SecondaryText);
			Text_Carrier->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.93f, 0.98f, 0.95f)));
			Text_Carrier->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_Carrier->SetText(FText::GetEmpty());
			Text_Carrier->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

FText UBwayRelicStatusWidget::GetDisplayedPrimaryStatusText() const
{
	return Text_Status ? Text_Status->GetText() : LastPrimaryText;
}

FText UBwayRelicStatusWidget::GetDisplayedSecondaryStatusText() const
{
	return Text_Carrier ? Text_Carrier->GetText() : LastSecondaryText;
}

bool UBwayRelicStatusWidget::IsBannerVisible() const
{
	return GetVisibility() != ESlateVisibility::Collapsed && GetVisibility() != ESlateVisibility::Hidden;
}
