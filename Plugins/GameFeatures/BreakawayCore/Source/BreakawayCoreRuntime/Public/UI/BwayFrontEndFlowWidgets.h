// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "GameState/BwayFrontendStateSubsystem.h"
#include "BwayFrontEndFlowWidgets.generated.h"

class UBorder;
class UButton;
class UCircularThrobber;
class UCommonSession_SearchResult;
class UCommonSession_SearchSessionRequest;
class UHorizontalBox;
class UOverlay;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UBwayCommonButton;
struct FOnlineResultInformation;

/** Distinct UI states for the Breakaway LAN Server Browser. */
UENUM(BlueprintType)
enum class EBwayServerBrowserViewState : uint8
{
	Searching,
	Empty,
	Populated,
	Error
};

/** Presentation view-model for a single LAN session row in UBwayLocalMatchBrowserWidget. */
USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayServerBrowserRowViewModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	FString SessionId;

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	FString SessionName;

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	FString PlaylistName;

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	FString MapName = TEXT("Dorado");

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	int32 CurrentPlayers = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	int32 MaxPlayers = 8;

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	int32 OpenPublicConnections = 7;

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	int32 PingMs = 0;

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	bool bIsFull = false;

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	bool bIsUnreachable = false;

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	FText FormattedPlayerCount;

	UPROPERTY(BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	FText FormattedPing;
};

/**
 * Breakaway Front End Main Menu Screen (W_MainMenu)
 * Clean Breakaway-only navigation list:
 *   - Host LAN Match
 *   - Find LAN Match
 *   - Dev Practice / Bot Match
 *   - Settings
 *   - Quit
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayMainMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayMainMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;

	/** Opens the staging Match Lobby configured for a real LAN session (OnlineMode = LAN). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	UCommonActivatableWidget* ClickHostLanMatch();

	/** Opens the LAN Server Browser (W_LocalMatchBrowser). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	UCommonActivatableWidget* ClickFindLanMatch();

	/** Opens the staging Match Lobby configured for offline bot practice (OnlineMode = Offline, Bots Enabled). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	UCommonActivatableWidget* ClickDevPractice();

	/** Pushes the Lyra/Breakaway Settings screen onto UI.Layer.Menu. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	UCommonActivatableWidget* OpenSettingsScreen();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ClickSettings();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ClickQuit();

	/** Compatibility alias routing legacy Play callers into Host LAN Match. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	UCommonActivatableWidget* ClickPlay() { return ClickHostLanMatch(); }

	UBwayCommonButton* GetHostLanMatchButton() const { return HostLanMatchButton; }
	UBwayCommonButton* GetFindLanMatchButton() const { return FindLanMatchButton; }
	UBwayCommonButton* GetDevPracticeButton() const { return DevPracticeButton; }
	UBwayCommonButton* GetSettingsButton() const { return SettingsButton; }
	UBwayCommonButton* GetQuitButton() const { return QuitButton; }

	/** Returns the exact visible menu flow button labels in top-to-bottom order. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|FrontEnd")
	TArray<FString> GetVisibleMenuButtonLabels() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	TSoftClassPtr<UCommonActivatableWidget> ModeSelectionWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	TSoftClassPtr<UCommonActivatableWidget> LocalMatchBrowserWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	TSoftClassPtr<UCommonActivatableWidget> MatchLobbyWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	TSoftClassPtr<UCommonActivatableWidget> SettingsScreenClass;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UCommonActivatableWidget* PushLobbyWithOnlineMode(bool bIsLanMatch, bool bForceBotsOn);

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> HostLanMatchButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> FindLanMatchButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> DevPracticeButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> SettingsButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> QuitButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProfileNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusBannerText;
};

/**
 * Legacy Mode Selection Screen class retained so existing W_ModeSelection Blueprint asset loads cleanly.
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
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> LocalCardButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> CancelButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ModeDescriptionText;

	FName ActiveHoveredModeId = FName(TEXT("LOCAL"));
};

/**
 * Breakaway LAN Server Browser (W_LocalMatchBrowser)
 * Displays discovered LAN sessions with Session Name, Map (Dorado), Player Count (X/8),
 * Ping, focusable Join buttons, Searching spinner / Empty states, and Full / Unreachable
 * error feedback with Retry focus recovery.
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
	void ClickRetrySearch();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ClickHostMatch();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	UCommonActivatableWidget* CompleteCreatingServerAndOpenLobby();

	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ClickBack();

	/** Validates row capacity/reachability and initiates JoinSession if joinable. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	bool RequestJoinRow(int32 RowIndex);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|FrontEnd")
	bool IsCreatingServer() const { return bIsCreatingServer; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|FrontEnd")
	EBwayServerBrowserViewState GetBrowserViewState() const { return ViewState; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|FrontEnd")
	const TArray<FBwayServerBrowserRowViewModel>& GetRowViewModels() const { return RowViewModels; }

	const TArray<TObjectPtr<UBwayCommonButton>>& GetRowJoinButtons() const { return RowJoinButtons; }
	UCommonSession_SearchSessionRequest* GetActiveSearchRequest() const { return ActiveSearchRequest; }
	const TArray<TObjectPtr<UCommonSession_SearchResult>>& GetDiscoveredSessions() const { return DiscoveredSessions; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|FrontEnd")
	FText GetLastErrorMessage() const { return LastErrorMessage; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|FrontEnd")
	bool IsSpinnerVisible() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|FrontEnd")
	bool IsErrorBannerVisible() const;

	UBwayCommonButton* GetRefreshButton() const { return RefreshButton; }
	UBwayCommonButton* GetRetryButton() const { return RetryButton; }
	UBwayCommonButton* GetHostMatchButton() const { return HostMatchButton; }
	UBwayCommonButton* GetBackButton() const { return BackButton; }

	/** Deterministic view-state injection used by UI automation and visual state audits. */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|FrontEnd")
	void ApplySyntheticBrowserStateForAudit(
		EBwayServerBrowserViewState NewState,
		const TArray<FBwayServerBrowserRowViewModel>& SyntheticRows,
		const FText& ErrorText = FText::GetEmpty());

	static FBwayServerBrowserRowViewModel BuildRowViewModelFromSearchResult(const UCommonSession_SearchResult* SearchResult, int32 Index);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breakaway|FrontEnd")
	TSoftClassPtr<UCommonActivatableWidget> MatchLobbyWidgetClass;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	void HandleSessionSearchFinished(bool bSucceeded, const FText& ErrorMessage);
	void HandleJoinSessionComplete(const FOnlineResultInformation& Result);
	void RebuildRowWidgets();
	void RefreshBrowserStatePresentation();
	void ShowActionableJoinError(const FText& ErrorText);

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> RefreshButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> RetryButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> HostMatchButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> BackButton;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> SearchingStateBox;

	UPROPERTY(Transient)
	TObjectPtr<UCircularThrobber> SearchingSpinner;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ServerListStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ServerListSubStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ErrorBannerBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ErrorBannerText;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> ServerRowsScrollBox;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ServerRowsBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CreatingServerOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UCommonSession_SearchSessionRequest> ActiveSearchRequest;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCommonSession_SearchResult>> DiscoveredSessions;

	UPROPERTY(Transient)
	TArray<FBwayServerBrowserRowViewModel> RowViewModels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBwayCommonButton>> RowJoinButtons;

	EBwayServerBrowserViewState ViewState = EBwayServerBrowserViewState::Searching;
	FText LastErrorMessage;
	FDelegateHandle JoinSessionCompleteHandle;
	FTimerHandle CreatingServerTimerHandle;
	bool bIsCreatingServer = false;
	bool bPendingJoinAttempt = false;
};

/**
 * Breakaway Match Settings Modal (W_MatchSettingsModal)
 * Pushed when clicking MATCH OPTIONS in W_MatchLobby.
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

	UBwayCommonButton* GetEnableBotsButton() const { return EnableBotsButton; }
	UBwayCommonButton* GetBotDifficultyButton() const { return BotDifficultyButton; }
	UBwayCommonButton* GetAcceptButton() const { return AcceptButton; }
	UBwayCommonButton* GetCancelButton() const { return CancelButton; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	void RefreshModalDisplay();
	UBwayFrontendStateSubsystem* GetFrontendSubsystem() const;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> PrevMapButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> NextMapButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MapNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MapDescText;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> EnableBotsButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> BotDifficultyButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> AcceptButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> CancelButton;

	int32 WorkingMapIndex = 0;
	bool bWorkingBotsEnabled = true;
	EBwayBotDifficulty WorkingBotDifficulty = EBwayBotDifficulty::Normal;
};

/**
 * Breakaway Match Lobby (W_MatchLobby)
 * Configures match options (Bots On/Off, Bot Difficulty, Team Slot) before hosting
 * either a real LAN session (Host LAN Match) or an offline practice match (Dev Practice / Bot Match)
 * routed through L_BW_HeroSelect_Staging -> L_BW_Dorado.
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

	/** Hosts the configured session through UCommonSessionSubsystem (used by ClickStartMatch and LAN fixtures). */
	bool HostConfiguredMatch(const TMap<FString, FString>& AdditionalExtraArgs, FString& OutAdvertisedPlaylist);

	UFUNCTION(BlueprintCallable, Category = "Breakaway|Lobby")
	void RefreshLobbyDisplay();

	UBwayCommonButton* GetMatchOptionsButton() const { return MatchOptionsButton; }
	UBwayCommonButton* GetLeaveButton() const { return LeaveButton; }
	UBwayCommonButton* GetStartButton() const { return StartButton; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Breakaway|Lobby")
	TSoftClassPtr<UCommonActivatableWidget> MatchSettingsModalClass;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UBwayFrontendStateSubsystem* GetFrontendSubsystem() const;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LobbyModeBadgeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MapHeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> MatchOptionsButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> InvitesSelectorButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> InvitePlayerButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryOnlineModeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryBotsEnabledText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryBotDifficultyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SummaryInvitesText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBwayCommonButton>> BlueSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBwayCommonButton>> RedSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBwayCommonButton>> SpectatorSlotButtons;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> LeaveButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> SettingsButton;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> StartButton;
};
