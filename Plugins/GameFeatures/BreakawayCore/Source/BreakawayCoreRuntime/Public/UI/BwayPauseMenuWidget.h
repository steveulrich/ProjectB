// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "BwayPauseMenuWidget.generated.h"

class UBwayCommonButton;
class UTextBlock;

/**
 * UBwayPauseMenuWidget
 *
 * Concrete in-match Breakaway Pause Menu pushed onto CommonUI UI.Layer.Menu.
 * Presents Resume, Options, and role-aware Return to Lobby (Host / Standalone)
 * or Disconnect to Main Menu (Client), restoring Game layer input/focus on close.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "Breakaway Pause Menu"))
class BREAKAWAYCORERUNTIME_API UBwayPauseMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayPauseMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	/** Resume the game (close this menu and restore CommonUI Game layer focus) */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void ResumeGame();

	/** Open the Options / Settings screen on UI.Layer.Menu */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	UCommonActivatableWidget* OpenSettings();

	/** Host: Return connected lobby to L_LyraFrontEnd. Client: Disconnect to Main Menu. */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void QuitToMainMenu();

	/** Quit to desktop */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void QuitToDesktop();

	/** True when the owning world is a remote client (NM_Client), or overridden for UI audit. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Pause Menu")
	bool IsClientPauseRole() const;

	/** Refresh button labels for current Host vs Client role. */
	UFUNCTION(BlueprintCallable, Category = "Pause Menu")
	void RefreshRolePresentation();

	void SetClientRoleOverrideForAudit(bool bEnableOverride, bool bInForceClient);

	UBwayCommonButton* GetResumeButton() const { return ResumeButton; }
	UBwayCommonButton* GetOptionsButton() const { return OptionsButton; }
	UBwayCommonButton* GetLeaveMatchButton() const { return LeaveMatchButton; }
	FText GetLeaveActionLabelText() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	// ========== BLUEPRINT EVENTS ==========

	/** Called when settings should be opened */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pause Menu|Events")
	void OnOpenSettingsRequested();

	/** Called before returning/disconnecting to main menu */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pause Menu|Events")
	void OnQuitToMainMenuRequested();

	/** Called before quitting to desktop */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pause Menu|Events")
	void OnQuitToDesktopRequested();

protected:
	/** Optional authored widget to focus when this menu opens */
	UPROPERTY(BlueprintReadWrite, meta = (BindWidget, OptionalWidget = true), Category = "Pause Menu")
	TObjectPtr<UWidget> DefaultFocusWidget;

	/** Settings widget class to push onto UI.Layer.Menu */
	UPROPERTY(EditDefaultsOnly, Category = "Pause Menu")
	TSoftClassPtr<UCommonActivatableWidget> SettingsWidgetClass;

	/** Main menu level to load when returning offline */
	UPROPERTY(EditDefaultsOnly, Category = "Pause Menu")
	TSoftObjectPtr<UWorld> MainMenuLevel;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> ResumeButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> OptionsButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> LeaveMatchButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RoleSubtitleText;

	bool bUseRoleOverrideForAudit = false;
	bool bForcedClientRoleForAudit = false;
};
