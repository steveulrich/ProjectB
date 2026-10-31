// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "BwayTeamPortraitSlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UTexture2D;

/**
 * Presentation state for one stable roster portrait position on a team row.
 */
USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayTeamPortraitSlotViewModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	FString PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	TObjectPtr<UTexture2D> PortraitTexture = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	bool bIsOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	bool bIsAlive = true;

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	bool bHasRelic = false;

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	bool bIsFriendly = true;

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	bool bIsWaitingToRespawn = false;

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	int32 RespawnRemainingSeconds = 0;

	/** Normalized live health [0..1] for the per-portrait mini health bar (0 when dead). */
	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	float HealthPercent = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	FLinearColor FrameColor = FLinearColor(0.10f, 0.12f, 0.16f, 0.75f);
};

/**
 * Non-interactive visual for one match roster portrait.
 * Blueprint owns layout/style; C++ owns the view-model contract.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Team Portrait Slot"))
class BREAKAWAYCORERUNTIME_API UBwayTeamPortraitSlotWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Team Portraits")
	void SetSlotViewModel(const FBwayTeamPortraitSlotViewModel& InViewModel);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Team Portraits")
	const FBwayTeamPortraitSlotViewModel& GetSlotViewModel() const { return ViewModel; }

	FText GetDisplayedRespawnText() const;
	bool IsRespawnTimerVisible() const;
	bool IsDeathOverlayVisible() const;
	bool IsRelicBadgeVisible() const;

protected:
	virtual void NativePreConstruct() override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	void ApplyViewModel();

	UFUNCTION(BlueprintImplementableEvent, Category = "Team Portraits", meta = (DisplayName = "On Slot View Model Changed"))
	void BP_OnSlotViewModelChanged(const FBwayTeamPortraitSlotViewModel& NewViewModel);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Team Portraits|Widgets")
	TObjectPtr<UBorder> Frame;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Team Portraits|Widgets")
	TObjectPtr<UImage> Image_Portrait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Team Portraits|Widgets")
	TObjectPtr<UImage> Image_DeathOverlay;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Team Portraits|Widgets")
	TObjectPtr<UTextBlock> Text_RelicBadge;

	/** Centered live respawn countdown seconds over dead player's portrait (Sept 2017 broadcast HUD parity). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Team Portraits|Widgets")
	TObjectPtr<UTextBlock> Text_RespawnTimer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Portraits")
	FLinearColor RelicCarrierFrameColor = FLinearColor(0.95f, 0.78f, 0.18f, 0.98f);

	void EnsureRespawnTimerWidget();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Portraits")
	TObjectPtr<UTexture2D> EmptyPortraitTexture = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Portraits")
	FLinearColor EmptyPortraitTint = FLinearColor(0.25f, 0.25f, 0.25f, 0.45f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Portraits")
	FLinearColor DeadPortraitTint = FLinearColor(0.22f, 0.22f, 0.22f, 1.0f);

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	FBwayTeamPortraitSlotViewModel ViewModel;
};
