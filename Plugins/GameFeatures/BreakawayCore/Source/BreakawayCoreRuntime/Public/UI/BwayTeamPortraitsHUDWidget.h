// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "GameplayTagContainer.h"
#include "UI/BwayHUDHelpers.h"
#include "UI/BwayMatchHUDWidgetBase.h"
#include "UI/BwayTeamPortraitSlotWidget.h"
#include "BwayTeamPortraitsHUDWidget.generated.h"

class UHorizontalBox;
class UTextBlock;
class UTexture2D;
class UAbilitySystemComponent;
class ABwayGameState;
class ABwayPlayerState;

/**
 * Single-team portrait row for the match HUD.
 *
 * Inject one instance per side:
 * - HUD.Slot.TeamPortraits.Friendly (DisplaySlotIndex = 0, local-team-left)
 * - HUD.Slot.TeamPortraits.Enemy (DisplaySlotIndex = 1)
 *
 * Runtime-spawns four UBwayTeamPortraitSlotWidget children into TeamPortraitsRow.
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
	virtual void NativePreConstruct() override;
	virtual void NotifyRoundStateChanged(FName NewState) override;

	UFUNCTION(BlueprintCallable, Category = "HUD|Portraits")
	void RefreshTeamPortraits();

	UFUNCTION()
	void HandlePortraitHeroChanged(FPrimaryAssetId NewHeroId);

	UFUNCTION()
	void HandlePortraitRelicChanged(bool bHasRelic);

	UFUNCTION()
	void HandlePortraitPlayerNumChanged(int32 PlayerNum);

	UFUNCTION()
	void HandleTeamsUpdated();

	/** Horizontal attach point that receives the four runtime portrait slots. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UHorizontalBox> TeamPortraitsRow;

	/** Optional label above the row ("FRIENDLY TEAM" / "ENEMY TEAM"). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "HUD|Widgets")
	TObjectPtr<UTextBlock> Text_TeamLabel;

	/** Visual Blueprint class created once for each of the four roster positions. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Portraits")
	TSubclassOf<UBwayTeamPortraitSlotWidget> PortraitSlotWidgetClass;

	/**
	 * Display column this instance owns.
	 * 0 = friendly / left, 1 = enemy / right.
	 * Bake this on the Friendly vs Enemy Blueprint CDOs used by EAS.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Portraits", meta = (ClampMin = "0", ClampMax = "1"))
	int32 DisplaySlotIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Portraits", meta = (ClampMin = "1", ClampMax = "4"))
	int32 PortraitSlotCount = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Portraits")
	FMargin PortraitSlotPadding = FMargin(3.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Portraits")
	float PortraitRefreshInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Portraits")
	TObjectPtr<UTexture2D> EmptyPortraitTexture = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Portraits")
	FLinearColor FriendlyFrameColor = FLinearColor(0.18f, 0.45f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Portraits")
	FLinearColor EnemyFrameColor = FLinearColor(1.0f, 0.28f, 0.12f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Portraits")
	FLinearColor EmptyFrameColor = FLinearColor(0.10f, 0.12f, 0.16f, 0.75f);

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Portraits")
	FText FriendlyTeamLabel;

	UPROPERTY(EditDefaultsOnly, Category = "HUD|Portraits")
	FText EnemyTeamLabel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBwayTeamPortraitSlotWidget>> SlotWidgets;

private:
	struct FBoundPortraitPlayer
	{
		TWeakObjectPtr<ABwayPlayerState> PlayerState;
		TWeakObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
		FDelegateHandle DeathTagChangedHandle;
	};

	void RebuildSlotWidgets();
	void ApplyTeamLabel();
	void BindToRosterEvents();
	void RefreshPlayerEventBindings(const TArray<FTeamPlayerHUDData>& TeamData);
	void ClearPlayerEventBindings();
	void HandleDeathTagChanged(const FGameplayTag ChangedTag, int32 NewCount);

	float TimeSinceLastPortraitRefresh = 0.0f;
	TArray<FBoundPortraitPlayer> BoundPortraitPlayers;

	UPROPERTY(Transient)
	TWeakObjectPtr<ABwayGameState> BoundRosterGameState;
};
