// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "BwayRelicStatusWidget.generated.h"

class UBorder;
class UEnhancedInputLocalPlayerSubsystem;
class UInputAction;
class UTextBlock;

/**
 * Lyra HUD slot widget for relic possession, territory status, carrier name,
 * and clean CommonUI Prematch/Warmup phase banners (replacing debug PrintString spam).
 * Injected via UGameFeatureAction_AddWidgets into HUD.Slot.ModeStatus.
 *
 * Polls replicated relic/round state client-side and updates key/gamepad labels dynamically.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Relic Status Widget"))
class BREAKAWAYCORERUNTIME_API UBwayRelicStatusWidget : public UBwayMatchHUDWidgetBase
{
	GENERATED_BODY()

public:
	UBwayRelicStatusWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Poll relic possession / match phase banner and update bound widgets / BP event. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Relic")
	void RefreshRelicStatus();

	UFUNCTION(BlueprintPure, Category = "HUD|Relic")
	FText GetDisplayedPrimaryStatusText() const;

	UFUNCTION(BlueprintPure, Category = "HUD|Relic")
	FText GetDisplayedSecondaryStatusText() const;

	UFUNCTION(BlueprintPure, Category = "HUD|Relic")
	FText GetResolvedShopKeyLabel() const;

	UFUNCTION(BlueprintPure, Category = "HUD|Relic")
	bool IsBannerVisible() const;

	void SetInputDeviceOverrideForTest(bool bEnableOverride, bool bPreferGamepad);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NotifyRoundStateChanged(FName NewState) override;

	/** Called after each poll. PossessingTeamIndex is display slot (0 = left/friendly, 1 = right/enemy). */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnRelicStatusUpdated(int32 PossessingTeamIndex, bool bIsCarried, const FText& StatusText, const FText& CarrierName);

	UFUNCTION()
	void HandleControlMappingsRebuilt();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UTextBlock> Text_Carrier;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	float RelicPollInterval = 0.20f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	FText NeutralStatusText = NSLOCTEXT("BwayRelicStatus", "Neutral", "RELIC NEUTRAL");

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	FText Team1StatusText = NSLOCTEXT("BwayRelicStatus", "Team1", "FRIENDLY POSSESSION");

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	FText Team2StatusText = NSLOCTEXT("BwayRelicStatus", "Team2", "ENEMY POSSESSION");

private:
	void BindInputDelegates();
	void UnbindInputDelegates();
	void EnsureBannerStyling();
	FText BuildStatusText(int32 DisplayPossessingTeam, int32 DisplayTerritoryHalf) const;
	FText BuildTerritoryText(int32 DisplayTerritoryHalf) const;
	void UpdateBoundStatusTexts(
		int32 DisplayPossessingTeam,
		bool bHasSecondaryLine,
		const FText& PrimaryText,
		const FText& SecondaryText,
		const FLinearColor& AccentColor);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> StatusBannerBorder;

	UPROPERTY(Transient)
	TObjectPtr<const UInputAction> CachedUpgradeShopAction;

	UPROPERTY(Transient)
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> BoundInputSubsystem;

	FTimerHandle RelicPollHandle;
	FDelegateHandle InputMethodChangedHandle;

	int32 LastPossessingTeamIndex = INDEX_NONE;
	bool bLastIsCarried = false;
	FText LastPrimaryText;
	FText LastSecondaryText;
	bool bHasInitializedState = false;
	bool bTestOverrideInputDevice = false;
	bool bTestPreferGamepad = false;
};
