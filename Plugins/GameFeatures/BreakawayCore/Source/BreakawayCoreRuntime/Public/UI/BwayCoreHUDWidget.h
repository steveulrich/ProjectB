// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "BwayCoreHUDWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
class ULyraHealthComponent;
class ABwayGameState;

/**
 * UBwayCoreHUDWidget
 *
 * DEPRECATED (Section 2 pivot): Prefer Lyra slot composition — `UBwayCaptureTheRelicScoreWidget`,
 * `UBwayRelicStatusWidget`, `UBwayHealthHUDWidget`, `UBwayTeamPortraitsHUDWidget` injected via
 * `UGameFeatureAction_AddWidgets` on `EAS_BW_CaptureTheRelic`. Kept for reference / optional overlay use.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Core HUD (Deprecated)"))
class BREAKAWAYCORERUNTIME_API UBwayCoreHUDWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayCoreHUDWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~ UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget interface

	// ========== DATA ACCESS ==========

	/** Get the current health percentage (0.0-1.0) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Health")
	float GetHealthPercent() const;

	/** Get the current health value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Health")
	float GetCurrentHealth() const;

	/** Get the max health value */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Health")
	float GetMaxHealth() const;

	/** Display-column score for the left side (local/friendly team when assigned). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Score")
	int32 GetTeam1Score() const;

	/** Display-column score for the right side (enemy team when local player is assigned). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Score")
	int32 GetTeam2Score() const;

	/** Authoritative game team for the local player (0 or 1, -1 if spectator/unassigned). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Score")
	int32 GetLocalPlayerTeam() const;

	/** Get remaining round time in seconds */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Timer")
	int32 GetRoundTimeRemaining() const;

	/** Get formatted round time string (MM:SS) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Timer")
	FText GetRoundTimeFormatted() const;

	/** Get current round number */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Timer")
	int32 GetCurrentRoundNumber() const;

	/** Relic possessing team in display slot space (0 = left/friendly, 1 = right/enemy, -1 = neutral). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Relic")
	int32 GetRelicPossessingTeam() const;

	/** Check if relic is currently carried */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Relic")
	bool IsRelicCarried() const;

	// ========== BLUEPRINT EVENTS ==========

	/** Called when health changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnHealthChanged(float NewHealth, float MaxHealth, float HealthPercent);

	/** Called when display-column scores change (Team1 = left/friendly). */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnScoreChanged(int32 Team1Score, int32 Team2Score);

	/** Called every second with updated round time */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnRoundTimeUpdated(int32 SecondsRemaining, const FText& FormattedTime);

	/** Called when round state changes */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnRoundStateChanged(FName NewState);

	/** Called when relic possession changes. TeamIndex is display slot (0 = left/friendly). */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnRelicPossessionChanged(int32 TeamIndex, bool bIsCarried);

	/** Called when the local player dies */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnLocalPlayerDied();

	/** Called when the local player respawns */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnLocalPlayerRespawned();

	/** Optional named bindings — auto-updated when present in the widget tree. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Team1Score;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Team2Score;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Timer;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RoundLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RelicStatus;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CarrierName;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> Progress_Health;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_HealthValues;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Team1_Portrait_1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Team1_Portrait_2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Team1_Portrait_3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Team1_Portrait_4;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Team2_Portrait_1;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Team2_Portrait_2;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Team2_Portrait_3;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Team2_Portrait_4;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Timer")
	float TimerPollInterval = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Portraits")
	float PortraitRefreshInterval = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Portraits")
	TObjectPtr<UTexture2D> EmptyPortraitTexture = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	FText NeutralStatusText = NSLOCTEXT("BwayCoreHUD", "Neutral", "NEUTRAL");

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	FText Team1StatusText = NSLOCTEXT("BwayCoreHUD", "Team1", "TEAM 1 POSSESSION");

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	FText Team2StatusText = NSLOCTEXT("BwayCoreHUD", "Team2", "TEAM 2 POSSESSION");

private:
	/** Bind to game state delegates */
	void BindToGameState();
	
	/** Unbind from game state delegates */
	void UnbindFromGameState();

	/** Bind to local player's health component */
	void BindToHealthComponent();

	/** Handle health changed callback */
	UFUNCTION()
	void HandleHealthChanged(ULyraHealthComponent* HealthComp, float OldValue, float NewValue, AActor* Instigator);

	/** Handle score changed callback (from ScoringComponent) */
	UFUNCTION()
	void HandleScoreChanged(int32 TeamIndex, int32 NewScore);

	/** Handle round time changed callback */
	UFUNCTION()
	void HandleRoundTimeChanged(int32 SecondsRemaining);

	/** Handle round state changed callback */
	UFUNCTION()
	void HandleRoundStateChanged(FName NewState);

	/** Get the game state */
	ABwayGameState* GetBwayGameState() const;

	/** Get the local player's pawn health component */
	ULyraHealthComponent* GetLocalPlayerHealthComponent() const;

	/** Cached references */
	UPROPERTY()
	TWeakObjectPtr<ABwayGameState> CachedGameState;

	UPROPERTY()
	TWeakObjectPtr<ULyraHealthComponent> CachedHealthComponent;

	/** Track if we're bound to the health component */
	bool bBoundToHealth = false;

	/** Last known health for change detection */
	float LastKnownHealth = -1.0f;

	/** Relic poll state for OnRelicPossessionChanged */
	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	float RelicPollInterval = 0.25f;

	float TimeSinceLastRelicPoll = 0.0f;
	float TimeSinceLastTimerPoll = 0.0f;
	float TimeSinceLastPortraitRefresh = 0.0f;
	int32 LastRelicPossessingTeam = INDEX_NONE;
	bool bLastRelicCarried = false;

	void RefreshAllDisplay();
	void RefreshScoreDisplay();
	void RefreshTimerDisplay();
	void RefreshRoundLabel();
	void RefreshRelicDisplay(int32 DisplayPossessingTeam, bool bIsCarried);
	void RefreshHealthDisplay(float NewHealth, float MaxHealth, float HealthPercent);
	void RefreshTeamPortraits();

	FText BuildRelicStatusText(int32 DisplayPossessingTeam) const;
	void UpdateBoundScoreTexts(int32 Team1Score, int32 Team2Score);
	void UpdateBoundTimerText(int32 SecondsRemaining);
	TArray<UImage*> GetTeamPortraitImages(int32 DisplaySlotIndex) const;
};

