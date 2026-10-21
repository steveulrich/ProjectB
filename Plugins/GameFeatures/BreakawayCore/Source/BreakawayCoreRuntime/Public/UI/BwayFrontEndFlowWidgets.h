// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GameState/BwayFrontendStateSubsystem.h"
#include "BwayFrontEndFlowWidgets.generated.h"

class UButton;
class UTextBlock;
class UBorder;
class UOverlay;
class UVerticalBox;
class UHorizontalBox;
class UCommonSession_SearchResult;
class UCommonSession_SearchSessionRequest;

/**
 * Phase 2.1: Breakaway Main Menu Screen (W_MainMenu)
 * Left-aligned vertical navigation list: PLAY, WARRIORS, HELP, SETTINGS, QUIT.
 * Top-right persistent party/profile header bar.
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayMainMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayMainMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	UCommonActivatableWidget* ClickPlay();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ClickWarriors();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ClickHelp();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ClickSettings();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ClickQuit();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	TSoftClassPtr<UCommonActivatableWidget> ModeSelectionWidgetClass;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PlayButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> WarriorsButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HelpButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QuitButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProfileNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusBannerText;

private:
	UFUNCTION()
	void OnPlayButtonClicked();
};

/**
 * Phase 2.2: Breakaway Play / Mode Selection Screen (W_ModeSelection)
 * 5 horizontal angled mode cards: TUTORIAL, ARENA, VERSUS AI, CUSTOM, LOCAL.
 * Hovering LOCAL displays: "Host a local server. For dev testing only."
 * Bottom CANCEL button pops back to W_MainMenu.
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayModeSelectionWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayModeSelectionWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	UCommonActivatableWidget* SelectMode(FName ModeId);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	UCommonActivatableWidget* ClickLocalMode() { return SelectMode(TEXT("LOCAL")); }

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	UCommonActivatableWidget* ClickCustomMode() { return SelectMode(TEXT("CUSTOM")); }

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ClickCancel();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void SetHoveredMode(FName ModeId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|FrontEnd")
	FText GetCurrentModeDescription() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	TSoftClassPtr<UCommonActivatableWidget> LocalMatchBrowserWidgetClass;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UFUNCTION() void OnTutorialClicked();
	UFUNCTION() void OnArenaClicked();
	UFUNCTION() void OnVersusAIClicked();
	UFUNCTION() void OnCustomClicked();
	UFUNCTION() void OnLocalClicked();

	UFUNCTION() void OnTutorialHovered();
	UFUNCTION() void OnArenaHovered();
	UFUNCTION() void OnVersusAIHovered();
	UFUNCTION() void OnCustomHovered();
	UFUNCTION() void OnLocalHovered();

	UPROPERTY(Transient)
	TObjectPtr<UButton> TutorialCardButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ArenaCardButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> VersusAICardButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CustomCardButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LocalCardButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CancelButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ModeDescriptionText;

	FName ActiveHoveredModeId = FName(TEXT("LOCAL"));
};

/**
 * Phase 2.3: Breakaway Local Server Browser (W_LocalMatchBrowser)
 * Central server list panel showing REFRESHING SERVER LIST... -> NO GAMES IN PROGRESS
 * (or active LAN sessions) with manual Refresh button and bottom HOST MATCH button.
 * Clicking HOST MATCH displays CREATING SERVER... overlay and transitions to W_MatchLobby.
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayLocalMatchBrowserWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayLocalMatchBrowserWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void RefreshServerList();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ClickHostMatch();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	UCommonActivatableWidget* CompleteCreatingServerAndOpenLobby();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ClickBack();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|FrontEnd")
	bool IsCreatingServer() const { return bIsCreatingServer; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	TSoftClassPtr<UCommonActivatableWidget> MatchLobbyWidgetClass;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	void FinishSimulatedRefresh();
	void HandleSessionSearchFinished(bool bSucceeded, const FText& ErrorMessage);
	void JoinDiscoveredSession(int32 SessionIndex);

	UFUNCTION() void OnSessionRow0Clicked();

	UPROPERTY(Transient)
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HostMatchButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BackButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ServerListStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ServerRowsBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CreatingServerOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UCommonSession_SearchSessionRequest> ActiveSearchRequest;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCommonSession_SearchResult>> DiscoveredSessions;

	FTimerHandle RefreshTimerHandle;
	FTimerHandle CreatingServerTimerHandle;
	bool bIsCreatingServer = false;
};

/**
 * Phase 3.2: Breakaway Match Settings Modal (W_MatchSettingsModal)
 * Pushed when clicking MATCH OPTIONS in W_MatchLobby.
 * Horizontal Map Carousel, ENABLE BOTS toggle, BOT DIFFICULTY selector, ACCEPT and CANCEL.
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayMatchSettingsModalWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayMatchSettingsModalWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void CycleMap(int32 Direction = 1);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void ToggleEnableBots();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void SetWorkingEnableBots(bool bEnable);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void CycleBotDifficulty(int32 Direction = 1);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void SetWorkingBotDifficulty(EBwayBotDifficulty Difficulty);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void AcceptSettings();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void CancelSettings();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Lobby")
	int32 GetWorkingMapIndex() const { return WorkingMapIndex; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Lobby")
	bool GetWorkingBotsEnabled() const { return bWorkingBotsEnabled; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|Lobby")
	EBwayBotDifficulty GetWorkingBotDifficulty() const { return WorkingBotDifficulty; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UFUNCTION() void OnPrevMapClicked();
	UFUNCTION() void OnNextMapClicked();
	UFUNCTION() void OnToggleBotsClicked();
	UFUNCTION() void OnCycleDifficultyClicked();

	void RefreshModalDisplay();
	UBwayFrontendStateSubsystem* GetFrontendSubsystem() const;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PrevMapButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> NextMapButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MapNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MapDescText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EnableBotsButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnableBotsValueText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BotDifficultyButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BotDifficultyValueText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AcceptButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CancelButton;

	int32 WorkingMapIndex = 0;
	bool bWorkingBotsEnabled = true;
	EBwayBotDifficulty WorkingBotDifficulty = EBwayBotDifficulty::Normal;
};

/**
 * Phase 3.1: Breakaway Custom Match Lobby (W_MatchLobby)
 * Displays selected Map Header, host controls (MATCH OPTIONS, INVITES, INVITE PLAYER),
 * read-only summary block (Bots Enabled, Bot Difficulty, Invites),
 * 3-Column Team Roster Grid (BLUE TEAM, RED TEAM, SPECTATORS, 4 slots each),
 * and bottom bar (Leave, Settings, START).
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayMatchLobbyWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayMatchLobbyWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	UCommonActivatableWidget* OpenMatchOptions();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void ClickCycleInviteMode();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void ClickInvitePlayer();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	bool SelectTeamSlot(EBwayLobbyTeam Team, int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void ClickLeaveLobby();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void ClickStartMatch();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void RefreshLobbyDisplay();

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breakaway|Lobby")
	TSoftClassPtr<UCommonActivatableWidget> MatchSettingsModalClass;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UBwayFrontendStateSubsystem* GetFrontendSubsystem() const;

	UFUNCTION() void OnMatchOptionsClicked();
	UFUNCTION() void OnBlueSlot0Clicked();
	UFUNCTION() void OnBlueSlot1Clicked();
	UFUNCTION() void OnBlueSlot2Clicked();
	UFUNCTION() void OnBlueSlot3Clicked();
	UFUNCTION() void OnRedSlot0Clicked();
	UFUNCTION() void OnRedSlot1Clicked();
	UFUNCTION() void OnRedSlot2Clicked();
	UFUNCTION() void OnRedSlot3Clicked();
	UFUNCTION() void OnSpecSlot0Clicked();
	UFUNCTION() void OnSpecSlot1Clicked();
	UFUNCTION() void OnSpecSlot2Clicked();
	UFUNCTION() void OnSpecSlot3Clicked();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MapHeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> MatchOptionsButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> InvitesSelectorButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InvitesButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> InvitePlayerButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryBotsEnabledText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryBotDifficultyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryInvitesText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> BlueSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BlueSlotLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> RedSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RedSlotLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> SpectatorSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> SpectatorSlotLabels;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LeaveButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SettingsButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StartButton;
};
