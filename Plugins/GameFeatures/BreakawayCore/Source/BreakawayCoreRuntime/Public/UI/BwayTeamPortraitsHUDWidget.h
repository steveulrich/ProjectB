// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "BwayTeamPortraitsHUDWidget.generated.h"

class UImage;
class UTexture2D;

/**
 * Lyra HUD slot widget for team portrait rows (display-slot aware).
 * Injected via UGameFeatureAction_AddWidgets into HUD.Slot.TeamPortraits.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Team Portraits HUD Widget"))
class BREAKAWAYCORERUNTIME_API UBwayTeamPortraitsHUDWidget : public UBwayMatchHUDWidgetBase
{
	GENERATED_BODY()

public:
	UBwayTeamPortraitsHUDWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NotifyRoundStateChanged(FName NewState) override;

	UFUNCTION(BlueprintCallable, Category = "HUD|Portraits")
	void RefreshTeamPortraits();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UImage> Team1_Portrait_1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UImage> Team1_Portrait_2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UImage> Team1_Portrait_3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UImage> Team1_Portrait_4;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UImage> Team2_Portrait_1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UImage> Team2_Portrait_2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UImage> Team2_Portrait_3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UImage> Team2_Portrait_4;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Portraits")
	float PortraitRefreshInterval = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Portraits")
	TObjectPtr<UTexture2D> EmptyPortraitTexture = nullptr;

private:
	TArray<UImage*> GetTeamPortraitImages(int32 DisplaySlotIndex) const;

	float TimeSinceLastPortraitRefresh = 0.0f;
};
