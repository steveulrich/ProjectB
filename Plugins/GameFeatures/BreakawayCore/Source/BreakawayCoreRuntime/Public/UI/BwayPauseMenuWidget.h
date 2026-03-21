// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "BwayPauseMenuWidget.generated.h"

/**
 * UBwayPauseMenuWidget
 * 
 * Pause menu widget with Resume, Settings, and Quit options.
 * In multiplayer, this only pauses locally (no server pause).
 * Designed to be extended in Blueprint for visual implementation.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Pause Menu"))
class BREAKAWAYCORERUNTIME_API UBwayPauseMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayPauseMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~ UCommonActivatableWidget interface
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~ End UCommonActivatableWidget interface

	// ========== ACTIONS ==========

	/** Resume the game (close this menu) */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void ResumeGame();

	/** Open the settings menu */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void OpenSettings();

	/** Quit to the main menu */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void QuitToMainMenu();

	/** Quit to desktop */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void QuitToDesktop();

	// ========== BLUEPRINT EVENTS ==========

	/** Called when settings should be opened - implement in Blueprint to push settings widget */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pause Menu|Events")
	void OnOpenSettingsRequested();

	/** Called before quitting to main menu - use for confirmation dialogs */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pause Menu|Events")
	void OnQuitToMainMenuRequested();

	/** Called before quitting to desktop - use for confirmation dialogs */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pause Menu|Events")
	void OnQuitToDesktopRequested();

protected:
	/** The widget to focus when this menu opens */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Pause Menu")
	TObjectPtr<UWidget> DefaultFocusWidget;

	/** Settings widget class to push */
	UPROPERTY(EditDefaultsOnly, Category = "Pause Menu")
	TSoftClassPtr<UCommonActivatableWidget> SettingsWidgetClass;

	/** Main menu level to load when quitting */
	UPROPERTY(EditDefaultsOnly, Category = "Pause Menu")
	TSoftObjectPtr<UWorld> MainMenuLevel;
};

