// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CommonInputTypeEnum.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "BwayHeroSelectWidget.generated.h"

class UBwayHeroRegistry;
class UBwayHeroSelectionManager;
class UBwayHeroSelectionPhaseComponent;
class UBwayHeroSlotWidget;
class UBwayAbilityPreviewWidget;
class UBwayCommonButton;
class UCommonButtonBase;
class ABwayPlayerState;
class UBwayFrontendStateSubsystem;
class UCommonTextBlock;
class ABwayHeroPreviewActor;
class UHorizontalBox;
class UScrollBox;
class UImage;
class UBorder;

// Delegate for when hero selection is confirmed (for dev tools)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnHeroConfirmedDelegate);

/**
 * Struct containing all display info for a hero in the UI
 */
USTRUCT(BlueprintType)
struct FHeroDisplayInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FPrimaryAssetId HeroId;

	UPROPERTY(BlueprintReadOnly)
	FText DisplayName;

	/** The hero's reconciled class display text (e.g., "Fighter", "Support", "Debuff Support", "Shooter") */
	UPROPERTY(BlueprintReadOnly)
	FText ClassName;

	/** The hero's class enum value */
	UPROPERTY(BlueprintReadOnly)
	EHeroClass HeroClass = EHeroClass::Fighter;

	/** The hero class index (for display order within their respective class) */
	UPROPERTY(BlueprintReadOnly)
	int HeroClassIndex = 0;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UTexture2D> Portrait;

	UPROPERTY(BlueprintReadOnly)
	FHeroStats Stats;

	/** Ability + signature buildable display information for the character select UI */
	UPROPERTY(BlueprintReadOnly)
	TArray<FAbilityDisplayInfo> Abilities;

	UPROPERTY(BlueprintReadOnly)
	bool bIsAvailable = true;

	UPROPERTY(BlueprintReadOnly)
	bool bIsSelected = false;

	UPROPERTY(BlueprintReadOnly)
	bool bIsLocked = false;

	/** Player index on the local team who has selected this hero (-1 if none) */
	UPROPERTY(BlueprintReadOnly)
	int32 SelectedByPlayerIndex = -1;
};

/**
 * View-model representing one slot in the 4v4 Blue/Red staging team roster sidebars
 */
USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayTeamRosterSlotViewModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	int32 TeamIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	FPrimaryAssetId HeroId;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	FText HeroDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	TObjectPtr<UTexture2D> Portrait;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	FText StatusText;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	bool bIsOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	bool bIsBot = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	bool bIsLocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	bool bIsLocalPlayer = false;
};

/**
 * UBwayHeroSelectWidget
 *
 * Unified Breakaway Staging & Late-Join Hero Selection screen.
 * Presents:
 *   - 4-hero roster grid with reconciled class badges, portraits, highlight, and lock state
 *   - Live 4v4 Blue Team (4 slots) & Red Team (4 slots) sidebars (humans + pre-assigned staging bots)
 *   - Ready (X/Y) counter and shared phase countdown
 *   - Hero Kit & Buildable Inspection Panel (FHeroStats, Primary + 4 kit actions, non-duplicated
 *     Shared Movement summary, and Signature Buildable)
 *   - Lock-In & Travel Failure Recovery UX ("Retry travel" / "Waiting for host")
 */
UCLASS(BlueprintType, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayHeroSelectWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayHeroSelectWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~UUserWidget interface
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
	virtual void NativeDestruct() override;
	//~End of UUserWidget interface

	//~UCommonActivatableWidget interface
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;
	//~End of UCommonActivatableWidget interface

public:
	// ========== BLUEPRINT CALLABLE FUNCTIONS ==========

	/** Get all available heroes in canonical Goal 5 order (Argus, Alona, Korryn, Rawlins) */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	TArray<FHeroDisplayInfo> GetAvailableHeroes();

	/** Get the currently selected hero for the local player */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	FPrimaryAssetId GetSelectedHeroId() const;

	/** Get the hero currently inspected in the Kit & Buildable panel (hovered/focused or selected) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	FPrimaryAssetId GetInspectedHeroId() const;

	/** Locally inspect a hero's stats, kit, buildable, and 3D preview without sending a server selection RPC */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	void SetInspectedHero(FPrimaryAssetId HeroId);

	/** Check if the local player's selection is locked */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	bool IsSelectionLocked() const;

	/** Select a hero (sends request to server). Returns false if unavailable or locked. */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	bool SelectHero(FPrimaryAssetId HeroId);

	/** Lock the current hero selection (or invoke host travel retry when travel failed). */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	bool LockSelection();

	/** Confirm the current selection and close when applicable. */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	void ConfirmSelection();

	/** Reconciles lock/unlock UI transitions (including rematch or pooled widget reuse). */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	void RefreshSelectionLockPresentation();

	/** Delegate fired when hero selection is confirmed. */
	UPROPERTY(BlueprintAssignable, Category = "Hero Selection Events")
	FOnHeroConfirmedDelegate OnHeroConfirmed;

	/** Get the local player's team index (0 = Blue, 1 = Red, -1 = unassigned) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	int32 GetLocalPlayerTeam() const;

	/** Check if a specific hero is available for selection on the local player's team */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	bool IsHeroAvailable(FPrimaryAssetId HeroId) const;

	/** Get remaining selection time (in seconds), or -1 if no time limit */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	float GetRemainingSelectionTime() const;

	/** Get number of human players ready vs total registered */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection")
	void GetReadyPlayerCount(int32& OutReady, int32& OutTotal) const;

	/** Get the full hero data asset for a hero ID */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	UBwayHeroDataAsset* GetHeroDataAsset(FPrimaryAssetId HeroId) const;

	/** Get the display info for a specific hero */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	bool GetHeroDisplayInfo(FPrimaryAssetId HeroId, FHeroDisplayInfo& OutDisplayInfo) const;

	/** Get the currently selected hero's full display info */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	bool GetSelectedHeroDisplayInfo(FHeroDisplayInfo& OutDisplayInfo) const;

	/** Get the currently inspected hero's full display info */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	bool GetInspectedHeroDisplayInfo(FHeroDisplayInfo& OutDisplayInfo) const;

	/** Build the 4-slot sidebar view-models for TeamIndex (0 = Blue Team, 1 = Red Team) */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	TArray<FBwayTeamRosterSlotViewModel> GetTeamRosterSlotViewModels(int32 TeamIndex) const;

	/** Toggle the inspection panel visibility */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	void ToggleAbilitiesPanel();

	/** Returns the spawned 3D character preview actor (if active) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Hero Selection|Preview")
	ABwayHeroPreviewActor* GetPreviewActor() const { return PreviewActor; }

	/** Select a hero by its index in GetAvailableHeroes() */
	UFUNCTION(BlueprintCallable, Category = "Hero Selection")
	bool SelectHeroByIndex(int32 Index);

	// ========== C++ / AUDIT ACCESSORS ==========
	const TArray<TObjectPtr<UBwayHeroSlotWidget>>& GetHeroSlotWidgets() const { return HeroSlotWidgets; }
	const TArray<TObjectPtr<UBwayAbilityPreviewWidget>>& GetAbilityPreviewWidgets() const { return AbilityPreviewWidgets; }
	UCommonButtonBase* GetLockInButton() const { return TravelConfirmButton; }
	UBwayCommonButton* GetNativeLockInButton() const { return LockInButton; }
	UCommonTextBlock* GetHeroDetailsTextWidget() const { return Text_HeroDetails; }
	FText GetReadyHeaderText() const;
	FText GetCountdownHeaderText() const;
	FText GetSharedMovementDisplayText() const;
	FText GetInspectedStatsDisplayText() const;

	// ========== BLUEPRINT IMPLEMENTABLE EVENTS ==========

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnHeroListChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnLocalSelectionChanged(FPrimaryAssetId NewHeroId);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnLocalSelectionLocked(FPrimaryAssetId LockedHeroId);

	/** Restore editable controls when a pooled screen is activated for a new selection. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnLocalSelectionUnlocked();

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnOtherPlayerSelectionChanged(APlayerState* PlayerState, FPrimaryAssetId HeroId, int32 TeamIndex);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnSelectionTimerUpdated(float RemainingTime);

	UFUNCTION(BlueprintImplementableEvent, Category = "Hero Selection Events")
	void OnAllPlayersReady();

protected:
	// ========== INTERNAL STATE ==========
	/** Data-driven summary & travel-failure recovery banner (exact widget name: Text_HeroDetails). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Hero Selection")
	TObjectPtr<UCommonTextBlock> Text_HeroDetails;

	/** Shared countdown container (exact widget name: Widget_Timer). */
	UPROPERTY(Transient)
	TObjectPtr<UWidget> SelectionTimerWidget;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	TObjectPtr<ABwayPlayerState> LocalPlayerState;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	TObjectPtr<UBwayHeroSelectionManager> SelectionManager;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	TObjectPtr<UBwayFrontendStateSubsystem> FrontendSubsystem;

	UPROPERTY(BlueprintReadOnly, Category = "Hero Selection")
	TObjectPtr<UBwayHeroRegistry> HeroRegistry;

	UPROPERTY(Transient)
	TObjectPtr<UBwayHeroSelectionPhaseComponent> SelectionPhase;

	/** Lock-In / Retry Travel button (exact widget name: Button_Lock). */
	UPROPERTY(Transient)
	TObjectPtr<UCommonButtonBase> TravelConfirmButton;

private:
	struct FTeamSidebarSlotRow
	{
		TObjectPtr<UBorder> OuterBorder;
		TObjectPtr<UBorder> PortraitBorder;
		TObjectPtr<UImage> PortraitImage;
		TObjectPtr<UCommonTextBlock> PlayerNameText;
		TObjectPtr<UCommonTextBlock> HeroStatusText;
	};

	FTimerHandle SelectionTimerHandle;
	FDelegateHandle InputMethodChangedHandle;

	void BindToPlayerState();
	void RefreshLocalPlayerState();
	void RefreshSelectionManager();
	void UnbindFromPlayerState();
	void BindToSelectionManager();
	void UnbindFromSelectionManager();
	void RefreshSelectionPhase();
	void UnbindFromSelectionPhase();
	void BindInputMethodListener();
	void UnbindInputMethodListener();
	void HandleInputMethodChanged(ECommonInputType NewInputType);

	UFUNCTION()
	void RefreshTravelFailurePresentation();

	UFUNCTION()
	void HandlePlayerSelectionChanged(FPrimaryAssetId NewHeroId);

	UFUNCTION()
	void HandlePlayerHeroLocked(FPrimaryAssetId LockedHeroId);

	UFUNCTION()
	void HandleOtherPlayerSelectionChanged(APlayerState* PlayerState, FPrimaryAssetId HeroId, int32 TeamIndex);

	UFUNCTION()
	void HandleOtherPlayerHeroLocked(APlayerState* PlayerState, FPrimaryAssetId LockedHeroId);

	UFUNCTION()
	void HandleStagingBotSelectionsChanged();

	UFUNCTION()
	void HandleAllPlayersReady();

	UFUNCTION()
	void HandleHeroSlotClicked(FPrimaryAssetId HeroId);

	UFUNCTION()
	void HandleHeroSlotFocusedOrHovered(FPrimaryAssetId HeroId);

	void HandleLockInButtonClicked();

	void UpdateSelectionTimer();
	void RefreshHeroDetails();
	void RefreshGoal5HeroSelectLayout();
	void RefreshTeamRosterSidebars();
	void RefreshHeroRosterGrid();
	void RefreshLockButtonPresentation();
	void ConfigureExplicitNavigationRules();
	void EnsureAndUpdatePreviewActor(FPrimaryAssetId HeroId);
	void CleanupPreviewActor();

	UPROPERTY()
	TArray<TObjectPtr<UBwayHeroDataAsset>> CachedHeroData;

	UPROPERTY(Transient)
	TObjectPtr<ABwayHeroPreviewActor> PreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> PhaseTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> LocalTeamBadgeBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> LocalTeamBadgeText;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> ReadyCountText;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> CountdownTimerText;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> BlueTeamHeaderText;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> RedTeamHeaderText;

	FTeamSidebarSlotRow BlueTeamSlots[4];
	FTeamSidebarSlotRow RedTeamSlots[4];

	UPROPERTY(Transient)
	TObjectPtr<UBorder> InspectionPanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> InspectedHeroNameText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> InspectedHeroClassBorder;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> InspectedHeroClassText;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> InspectedStatsText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> HeroDetailsBannerBorder;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> KitPreviewScrollBox;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBwayAbilityPreviewWidget>> AbilityPreviewWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> SharedMovementText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> HeroSlotsRow;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBwayHeroSlotWidget>> HeroSlotWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UBwayCommonButton> LockInButton;

	UPROPERTY(Transient)
	TObjectPtr<UCommonTextBlock> LockHintText;

	FPrimaryAssetId InspectedHeroId;

	bool bUsingGoal5Layout = false;
	bool bAbilitiesPanelVisible = true;
	bool bIsInitialized = false;
	bool bShowingTravelFailure = false;
	bool bTravelRetryAvailable = false;
	bool bCachedLockState = false;
};
