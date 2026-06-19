// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "BwayRelicStatusWidget.generated.h"

class UTextBlock;

/**
 * Lyra HUD slot widget for relic possession status and carrier name.
 * Injected via UGameFeatureAction_AddWidgets into HUD.Slot.ModeStatus.
 *
 * Polls replicated relic state client-side (OnRelicCarrierChangedEvent is server-only).
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Relic Status Widget"))
class BREAKAWAYCORERUNTIME_API UBwayRelicStatusWidget : public UBwayMatchHUDWidgetBase
{
	GENERATED_BODY()

public:
	UBwayRelicStatusWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Poll relic possession and update bound widgets / BP event. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Relic")
	void RefreshRelicStatus();

	/** Called after each poll. PossessingTeamIndex is display slot (0 = left/friendly, 1 = right/enemy). */
	UFUNCTION(BlueprintImplementableEvent, Category = "HUD|Events")
	void OnRelicStatusUpdated(int32 PossessingTeamIndex, bool bIsCarried, const FText& StatusText, const FText& CarrierName);

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Status;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Carrier;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	float RelicPollInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	FText NeutralStatusText = NSLOCTEXT("BwayRelicStatus", "Neutral", "NEUTRAL");

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	FText Team1StatusText = NSLOCTEXT("BwayRelicStatus", "Team1", "TEAM 1 POSSESSION");

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Relic")
	FText Team2StatusText = NSLOCTEXT("BwayRelicStatus", "Team2", "TEAM 2 POSSESSION");

private:
	FText BuildStatusText(int32 DisplayPossessingTeam) const;
	void UpdateBoundStatusTexts(int32 DisplayPossessingTeam, bool bIsCarried, const FText& StatusText, const FText& CarrierName);

	FTimerHandle RelicPollHandle;

	int32 LastPossessingTeamIndex = INDEX_NONE;
	bool bLastIsCarried = false;
	FText LastCarrierName;
};
