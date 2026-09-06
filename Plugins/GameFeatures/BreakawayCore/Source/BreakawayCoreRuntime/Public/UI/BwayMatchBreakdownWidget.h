// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Stats/BwayMatchStatsTypes.h"
#include "BwayMatchBreakdownWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPostMatchBreakdownActionRequested);

class UCommonButtonBase;

/**
 * Match breakdown — horizontal per-player columns (Step 16).
 * Blueprint child: WBP_BW_MatchBreakdown
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

	UFUNCTION(BlueprintCallable, Category = "PostMatch")
	void RequestReturnToLobby();

	UFUNCTION(BlueprintCallable, Category = "PostMatch")
	void RequestPlayAgain();

	UPROPERTY(BlueprintAssignable, Category = "PostMatch|Events")
	FOnPostMatchBreakdownActionRequested OnReturnToLobbyRequested;

	UPROPERTY(BlueprintAssignable, Category = "PostMatch|Events")
	FOnPostMatchBreakdownActionRequested OnPlayAgainRequested;

protected:
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "PostMatch|Widgets")
	TObjectPtr<UCommonButtonBase> Btn_ReturnToLobby;

	UFUNCTION(BlueprintImplementableEvent, Category = "PostMatch|Events")
	void OnBreakdownReady(const FBwayPostMatchSummaryData& SummaryData);

private:
	UPROPERTY()
	FBwayPostMatchSummaryData CachedSummary;
};
