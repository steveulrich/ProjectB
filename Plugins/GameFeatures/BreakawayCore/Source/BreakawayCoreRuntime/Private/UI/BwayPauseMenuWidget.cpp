// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/BwayPauseMenuWidget.h"
#include "CommonUIExtensions.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagsManager.h"
#include "Kismet/GameplayStatics.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayPauseMenuWidget)

UBwayPauseMenuWidget::UBwayPauseMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set up as a menu (blocks input behind it)
	bIsBackHandler = true;
	bIsBackActionDisplayedInActionBar = true;
}

UWidget* UBwayPauseMenuWidget::NativeGetDesiredFocusTarget() const
{
	if (DefaultFocusWidget)
	{
		return DefaultFocusWidget;
	}
	return Super::NativeGetDesiredFocusTarget();
}

void UBwayPauseMenuWidget::ResumeGame()
{
	DeactivateWidget();
}

void UBwayPauseMenuWidget::OpenSettings()
{
	OnOpenSettingsRequested();

	// If settings widget class is set, push it
	if (!SettingsWidgetClass.IsNull())
	{
		// Use Lyra's layer system to push settings
		if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		{
			// Push to Game layer so it stacks on top of pause menu
			FGameplayTag MenuLayer = FGameplayTag::RequestGameplayTag(FName("UI.Layer.Menu"));
			UCommonUIExtensions::PushStreamedContentToLayer_ForPlayer(LocalPlayer, MenuLayer, SettingsWidgetClass);
		}
	}
}

void UBwayPauseMenuWidget::QuitToMainMenu()
{
	// Notify Blueprint for confirmation dialog
	OnQuitToMainMenuRequested();

	// If no Blueprint handling, do it directly
	if (APlayerController* PC = GetOwningPlayer())
	{
		// Disconnect from server/end session
		if (UWorld* World = GetWorld())
		{
			if (World->IsNetMode(NM_Client))
			{
				// Client - disconnect from server
				PC->ClientTravel(TEXT(""), ETravelType::TRAVEL_Absolute);
			}
			else
			{
				// Server/Standalone - load main menu level
				if (!MainMenuLevel.IsNull())
				{
					UGameplayStatics::OpenLevelBySoftObjectPtr(World, MainMenuLevel);
				}
				else
				{
					// Fallback - just disconnect
					PC->ConsoleCommand(TEXT("disconnect"));
				}
			}
		}
	}
}

void UBwayPauseMenuWidget::QuitToDesktop()
{
	// Notify Blueprint for confirmation dialog
	OnQuitToDesktopRequested();

	// Actually quit
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ConsoleCommand(TEXT("quit"));
	}
}

