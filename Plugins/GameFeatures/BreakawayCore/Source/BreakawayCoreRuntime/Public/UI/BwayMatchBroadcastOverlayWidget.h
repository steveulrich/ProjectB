// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "BwayMatchBroadcastOverlayWidget.generated.h"

class UInputAction;
class UTexture2D;

/**
 * Match-layer broadcast HUD overlay mounted on WBP_BW_MatchHUDLayout's CanvasPanel_0 (ZOrder = 0).
 *
 * Paints the surrounding Breakaway broadcast chrome behind slot widgets (top portrait wings
 * and bottom hexagonal ability pedestal) plus:
 *  - Center Breakaway 3-spoke T-crosshair with outgoing-hit confirmation
 *  - Mid-left Match Event Toast (GOAL SCORED!, INTERCEPTION!, FUMBLE FORCED!, RELIC TAKEN!, ELIMINATION!)
 *  - Upper-right Live Arena Minimap (rounded dashed stadium, red Relic progress line, buildables, X markers, local chevron)
 *  - Bottom-left UPGRADES [Key] Shop Prompt (visible only near friendly team goal / healing zone)
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayMatchBroadcastOverlayWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UBwayMatchBroadcastOverlayWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Deterministically push an event toast (also used by headless HUD automation tests). */
	UFUNCTION(BlueprintCallable, Category = "HUD|Broadcast")
	void PushEventToast(const FText& Headline, const FString& PlayerName, UTexture2D* PortraitTexture, bool bIsFriendly);

	/** Refresh stat/relic event deltas, shop-zone visibility, and dynamic shop keybind immediately. */
	UFUNCTION(BlueprintCallable, Category = "HUD|Broadcast")
	void RefreshBroadcastState();

	bool IsShopPromptVisible() const;
	bool IsCrosshairVisible() const;
	bool IsEventToastActive() const;
	FText GetActiveToastHeadline() const { return ActiveToast.Headline; }
	FString GetActiveToastPlayerName() const { return ActiveToast.PlayerName; }
	FText GetResolvedShopKeyLabel() const;

	void SetInputDeviceOverrideForTest(bool bEnableOverride, bool bPreferGamepad);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

private:
	struct FPlayerStatSnapshot
	{
		int32 ObjectiveScore = 0;
		int32 Interceptions = 0;
		int32 ForcedFumbles = 0;
		int32 Kills = 0;
		bool bHasRelic = false;
	};

	struct FActiveEventToast
	{
		FText Headline;
		FString PlayerName;
		TWeakObjectPtr<UTexture2D> PortraitTexture;
		bool bIsFriendly = true;
		double ExpireRealTime = -1.0;
		double StartRealTime = -1.0;
	};

	void PollMatchEventDeltas();
	double GetOverlayClockSeconds() const;

	int32 PaintTopPortraitBackings(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FVector2D& LocalSize) const;

	int32 PaintBottomAbilityPedestal(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FVector2D& LocalSize) const;

	int32 PaintCrosshair(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FVector2D& LocalSize) const;

	int32 PaintShopPrompt(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FVector2D& LocalSize) const;

	int32 PaintEventToast(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FVector2D& LocalSize) const;

	int32 PaintLiveMinimap(
		const FGeometry& AllottedGeometry,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FVector2D& LocalSize) const;

	UPROPERTY(Transient)
	TObjectPtr<const UInputAction> CachedUpgradeShopAction;

	TMap<int32, FPlayerStatSnapshot> PlayerSnapshots;
	FActiveEventToast ActiveToast;
	int32 LastTrackedRoundNumber = INDEX_NONE;
	bool bHasBaselineSnapshots = false;
	bool bTestOverrideInputDevice = false;
	bool bTestPreferGamepad = false;
};
