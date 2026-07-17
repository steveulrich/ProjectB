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

protected:
	virtual void NativePreConstruct() override;

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Portraits")
	TObjectPtr<UTexture2D> EmptyPortraitTexture = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Portraits")
	FLinearColor EmptyPortraitTint = FLinearColor(0.25f, 0.25f, 0.25f, 0.45f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Team Portraits")
	FLinearColor DeadPortraitTint = FLinearColor(0.22f, 0.22f, 0.22f, 1.0f);

	UPROPERTY(BlueprintReadOnly, Category = "Team Portraits")
	FBwayTeamPortraitSlotViewModel ViewModel;
};
