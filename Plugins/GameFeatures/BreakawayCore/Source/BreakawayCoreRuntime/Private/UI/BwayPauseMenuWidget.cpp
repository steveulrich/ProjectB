// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayPauseMenuWidget.h"
#include "Blueprint/WidgetTree.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "CommonInputModeTypes.h"
#include "CommonSessionSubsystem.h"
#include "CommonUIExtensions.h"
#include "ICommonInputModule.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Kismet/GameplayStatics.h"
#include "UI/BwayCommonButton.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayPauseMenuWidget)

UBwayPauseMenuWidget::UBwayPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsBackHandler = true;
	bIsBackActionDisplayedInActionBar = true;
	SettingsWidgetClass = TSoftClassPtr<UCommonActivatableWidget>(
		FSoftClassPath(TEXT("/Game/UI/Settings/W_LyraSettingScreen.W_LyraSettingScreen_C")));
	MainMenuLevel = TSoftObjectPtr<UWorld>(
		FSoftObjectPath(TEXT("/Game/System/FrontEnd/Maps/L_LyraFrontEnd.L_LyraFrontEnd")));
}

TOptional<FUIInputConfig> UBwayPauseMenuWidget::GetDesiredInputConfig() const
{
	return FUIInputConfig(ECommonInputMode::Menu, EMouseCaptureMode::NoCapture);
}

TSharedRef<SWidget> UBwayPauseMenuWidget::RebuildWidget()
{
	UOverlay* Root = nullptr;
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PauseMenuRootOverlay"));
		WidgetTree->RootWidget = Root;
	}
	else if (WidgetTree)
	{
		if (UPanelWidget* ExistingRoot = Cast<UPanelWidget>(WidgetTree->RootWidget);
			ExistingRoot && ExistingRoot->GetChildrenCount() == 0)
		{
			Root = Cast<UOverlay>(ExistingRoot);
			if (!Root)
			{
				Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("PauseMenuRootOverlay"));
				ExistingRoot->AddChild(Root);
			}
		}
	}

	if (Root)
	{
		const FLinearColor GoldBright(0.98f, 0.84f, 0.42f, 1.0f);
		const FLinearColor GoldAccent(0.86f, 0.68f, 0.24f, 1.0f);
		const FLinearColor PanelBg(0.04f, 0.07f, 0.13f, 0.96f);
		const FLinearColor BtnNormal(0.07f, 0.12f, 0.21f, 0.96f);
		const FLinearColor BtnFocused(0.14f, 0.25f, 0.42f, 1.00f);
		const FLinearColor MutedText(0.66f, 0.72f, 0.80f, 1.0f);

		UBorder* Dimmer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PauseDimmer"));
		Dimmer->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.05f, 0.78f));
		UOverlaySlot* DimSlot = Root->AddChildToOverlay(Dimmer);
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PauseCardSize"));
		CardSize->SetWidthOverride(500.f);
		UOverlaySlot* CardSlot = Root->AddChildToOverlay(CardSize);
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetVerticalAlignment(VAlign_Center);

		UBorder* OuterFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PauseOuterFrame"));
		OuterFrame->SetBrushColor(GoldAccent);
		OuterFrame->SetPadding(FMargin(2.f));
		CardSize->AddChild(OuterFrame);

		UBorder* InnerPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PauseInnerPanel"));
		InnerPanel->SetBrushColor(PanelBg);
		InnerPanel->SetPadding(FMargin(32.f, 28.f));
		OuterFrame->SetContent(InnerPanel);

		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseButtonStack"));
		InnerPanel->SetContent(VBox);

		UTextBlock* HeaderTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PauseTitleText"));
		HeaderTitle->SetText(NSLOCTEXT("BwayPause", "PauseHeader", "MATCH PAUSED"));
		HeaderTitle->SetJustification(ETextJustify::Center);
		HeaderTitle->SetColorAndOpacity(FSlateColor(GoldBright));
		{
			FSlateFontInfo Font = HeaderTitle->GetFont();
			Font.Size = 26;
			HeaderTitle->SetFont(Font);
		}
		VBox->AddChildToVerticalBox(HeaderTitle)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

		RoleSubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PauseRoleSubtitle"));
		RoleSubtitleText->SetJustification(ETextJustify::Center);
		RoleSubtitleText->SetAutoWrapText(true);
		RoleSubtitleText->SetColorAndOpacity(FSlateColor(MutedText));
		{
			FSlateFontInfo Font = RoleSubtitleText->GetFont();
			Font.Size = 13;
			RoleSubtitleText->SetFont(Font);
		}
		VBox->AddChildToVerticalBox(RoleSubtitleText)->SetPadding(FMargin(0.f, 0.f, 0.f, 22.f));

		auto CreateMenuButton = [&](FName Name, const FText& Label, bool bPrimaryAccent) -> UBwayCommonButton*
		{
			UBwayCommonButton* Btn = WidgetTree->ConstructWidget<UBwayCommonButton>(UBwayCommonButton::StaticClass(), Name);
			Btn->SetLabelText(Label);
			Btn->SetFontSize(18);
			Btn->SetContentPadding(FMargin(22.f, 13.f));
			if (bPrimaryAccent)
			{
				Btn->SetButtonColors(
					FLinearColor(0.58f, 0.43f, 0.12f, 0.96f),
					FLinearColor(0.78f, 0.58f, 0.16f, 1.00f),
					GoldBright,
					FLinearColor::White);
			}
			else
			{
				Btn->SetButtonColors(BtnNormal, BtnFocused, GoldBright, FLinearColor::White);
			}
			VBox->AddChildToVerticalBox(Btn)->SetPadding(FMargin(0.f, 6.f));
			return Btn;
		};

		ResumeButton = CreateMenuButton(TEXT("Button_Resume"), NSLOCTEXT("BwayPause", "Btn_Resume", "Resume"), true);
		OptionsButton = CreateMenuButton(TEXT("Button_Options"), NSLOCTEXT("BwayPause", "Btn_Options", "Options"), false);
		LeaveMatchButton = CreateMenuButton(TEXT("Button_LeaveMatch"), GetLeaveActionLabelText(), false);
	}

	return Super::RebuildWidget();
}

void UBwayPauseMenuWidget::NativeConstruct()
{
	ICommonInputModule::GetSettings().LoadData();
	Super::NativeConstruct();

	if (ResumeButton)
	{
		ResumeButton->OnClicked().RemoveAll(this);
		ResumeButton->OnClicked().AddUObject(this, &ThisClass::ResumeGame);
	}
	if (OptionsButton)
	{
		OptionsButton->OnClicked().RemoveAll(this);
		OptionsButton->OnClicked().AddWeakLambda(this, [this]() { OpenSettings(); });
	}
	if (LeaveMatchButton)
	{
		LeaveMatchButton->OnClicked().RemoveAll(this);
		LeaveMatchButton->OnClicked().AddUObject(this, &ThisClass::QuitToMainMenu);
	}

	RefreshRolePresentation();
}

void UBwayPauseMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	RefreshRolePresentation();
	RequestRefreshFocus();
}

UWidget* UBwayPauseMenuWidget::NativeGetDesiredFocusTarget() const
{
	if (DefaultFocusWidget)
	{
		return DefaultFocusWidget;
	}
	if (ResumeButton)
	{
		return ResumeButton;
	}
	return Super::NativeGetDesiredFocusTarget();
}

bool UBwayPauseMenuWidget::IsClientPauseRole() const
{
	if (bUseRoleOverrideForAudit)
	{
		return bForcedClientRoleForAudit;
	}
	const UWorld* World = GetWorld();
	return World && World->IsNetMode(NM_Client);
}

void UBwayPauseMenuWidget::SetClientRoleOverrideForAudit(bool bEnableOverride, bool bInForceClient)
{
	bUseRoleOverrideForAudit = bEnableOverride;
	bForcedClientRoleForAudit = bInForceClient;
	RefreshRolePresentation();
}

FText UBwayPauseMenuWidget::GetLeaveActionLabelText() const
{
	return IsClientPauseRole()
		? NSLOCTEXT("BwayPause", "Btn_DisconnectClient", "Disconnect to Main Menu")
		: NSLOCTEXT("BwayPause", "Btn_ReturnHost", "Return to Lobby");
}

void UBwayPauseMenuWidget::RefreshRolePresentation()
{
	if (LeaveMatchButton)
	{
		LeaveMatchButton->SetLabelText(GetLeaveActionLabelText());
	}
	if (RoleSubtitleText)
	{
		RoleSubtitleText->SetText(IsClientPauseRole()
			? NSLOCTEXT("BwayPause", "RoleClientSub", "CLIENT SESSION — GAME CONTINUES LIVE")
			: NSLOCTEXT("BwayPause", "RoleHostSub", "HOST SESSION — RETURN BRINGS CONNECTED PLAYERS TO LOBBY"));
	}
}

void UBwayPauseMenuWidget::ResumeGame()
{
	DeactivateWidget();
}

UCommonActivatableWidget* UBwayPauseMenuWidget::OpenSettings()
{
	OnOpenSettingsRequested();

	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	TSubclassOf<UCommonActivatableWidget> LoadedSettingsClass = nullptr;
	if (!SettingsWidgetClass.IsNull())
	{
		LoadedSettingsClass = SettingsWidgetClass.LoadSynchronous();
	}
	if (!LoadedSettingsClass)
	{
		const FSoftClassPath DefaultSettingsPath(TEXT("/Game/UI/Settings/W_LyraSettingScreen.W_LyraSettingScreen_C"));
		LoadedSettingsClass = DefaultSettingsPath.TryLoadClass<UCommonActivatableWidget>();
	}
	if (!LoadedSettingsClass)
	{
		return nullptr;
	}

	const FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
	return UCommonUIExtensions::PushContentToLayer_ForPlayer(LocalPlayer, MenuLayer, LoadedSettingsClass);
}

void UBwayPauseMenuWidget::QuitToMainMenu()
{
	OnQuitToMainMenuRequested();

	UWorld* World = GetWorld();
	APlayerController* PC = GetOwningPlayer();
	if (!World)
	{
		return;
	}

	DeactivateWidget();

	if (IsClientPauseRole())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UCommonSessionSubsystem* SessionSubsystem = GI->GetSubsystem<UCommonSessionSubsystem>())
			{
				SessionSubsystem->CleanUpSessions();
			}
			GI->ReturnToMainMenu();
			return;
		}
		if (PC)
		{
			PC->ClientTravel(TEXT("/Game/System/FrontEnd/Maps/L_LyraFrontEnd?Experience=B_LyraFrontEnd_Experience"), ETravelType::TRAVEL_Absolute);
		}
		return;
	}

	// Host or Standalone: use authoritative connected ReturnToFrontEnd so connected clients return together.
	if (ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(PC))
	{
		BwayPC->Server_RequestReturnToFrontEnd();
		return;
	}
	if (ABwayGameState* BwayGS = World->GetGameState<ABwayGameState>())
	{
		if (BwayGS->HasAuthority())
		{
			BwayGS->ReturnToFrontEnd();
			return;
		}
	}

	if (!MainMenuLevel.IsNull())
	{
		UGameplayStatics::OpenLevelBySoftObjectPtr(World, MainMenuLevel, true, TEXT("Experience=B_LyraFrontEnd_Experience"));
	}
	else
	{
		UGameplayStatics::OpenLevel(World, FName(TEXT("/Game/System/FrontEnd/Maps/L_LyraFrontEnd")), true, TEXT("Experience=B_LyraFrontEnd_Experience"));
	}
}

void UBwayPauseMenuWidget::QuitToDesktop()
{
	OnQuitToDesktopRequested();

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ConsoleCommand(TEXT("quit"));
	}
}
