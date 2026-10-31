// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "BwayMatchBreakdownWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPostMatchBreakdownActionRequested);

class UBwayUpgradeCatalog;
class UCommonButtonBase;
class UUserWidget;

/**
 * Match breakdown — horizontal 8-player columns (Step 16).
 * Blueprint child: WBP_BW_MatchBreakdown
 *
 * NOTE: WBP_BW_MatchBreakdown defines Blueprint variables named PlayerColumnsHost,
 * DimOverlay, and Btn_PlayAgain. Never declare C++ UPROPERTYs with those names.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Match Breakdown"))
class BREAKAWAYCORERUNTIME_API UBwayMatchBreakdownWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayMatchBreakdownWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "PostMatch")
	void ApplyBreakdownData(FBwayPostMatchSummaryData SummaryData);

	UFUNCTION(BlueprintPure, Category = "PostMatch")
	const TArray<FBwayMatchBreakdownPlayerColumn>& GetPlayerColumns() const { return CachedSummary.PlayerColumns; }

	UFUNCTION(BlueprintPure, Category = "PostMatch")
	FText GetMVPPlayerName() const { return CachedSummary.MVPPlayerName; }

	UFUNCTION(BlueprintPure, Category = "PostMatch")
	FText GetMVPHeroName() const { return CachedSummary.MVPHeroName; }

	UFUNCTION(BlueprintPure, Category = "PostMatch")
	int32 GetMVPPlayerId() const { return CachedSummary.MVPPlayerId; }

	UFUNCTION(BlueprintPure, Category = "PostMatch")
	bool IsPlayAgainButtonEnabled() const;

	UFUNCTION(BlueprintPure, Category = "PostMatch")
	bool IsReturnToLobbyButtonEnabled() const;

	UFUNCTION(BlueprintPure, Category = "PostMatch")
	int32 GetDisplayedColumnCount() const;

	UFUNCTION(BlueprintCallable, Category = "PostMatch")
	void RequestReturnToLobby();

	UFUNCTION(BlueprintCallable, Category = "PostMatch")
	void RequestPlayAgain();

	void SetRoleOverrideForTest(bool bEnableOverride, bool bSimulateHost);

	UPROPERTY(BlueprintAssignable, Category = "PostMatch|Events")
	FOnPostMatchBreakdownActionRequested OnReturnToLobbyRequested;

	UPROPERTY(BlueprintAssignable, Category = "PostMatch|Events")
	FOnPostMatchBreakdownActionRequested OnPlayAgainRequested;

protected:
	virtual void NativeConstruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UCommonButtonBase> Btn_ReturnToLobby;

	UFUNCTION(BlueprintImplementableEvent, Category = "PostMatch|Events")
	void OnBreakdownReady(const FBwayPostMatchSummaryData& SummaryData);

private:
	bool IsHostBreakdownRole() const;
	UCommonButtonBase* FindPlayAgainButton() const;
	void PolishBreakdownLayoutAndColumns();
	void DecoratePlayerColumnWidget(UUserWidget* ColumnWidget, const FBwayMatchBreakdownPlayerColumn& ColumnData);
	void UpdateRoleActionPermissions();
	const UBwayUpgradeCatalog* ResolveUpgradeCatalog() const;

	UPROPERTY()
	FBwayPostMatchSummaryData CachedSummary;

	UPROPERTY(Transient)
	mutable TObjectPtr<UBwayUpgradeCatalog> CachedUpgradeCatalog;

	bool bTestOverrideRole = false;
	bool bTestSimulateHost = true;
};
