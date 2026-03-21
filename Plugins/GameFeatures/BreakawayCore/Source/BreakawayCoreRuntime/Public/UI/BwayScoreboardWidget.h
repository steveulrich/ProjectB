// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "BwayScoreboardWidget.generated.h"

class ABwayGameState;
class ABwayPlayerState;

/**
 * Player data structure for scoreboard display
 */
USTRUCT(BlueprintType)
struct FScoreboardPlayerData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 TeamIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 Assists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 ObjectiveScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	FText HeroName;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	bool bIsLocalPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	bool bIsAlive = true;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 Ping = 0;
};

/**
 * UBwayScoreboardWidget
 * 
 * Tab-held scoreboard showing all players, teams, and statistics.
 * Designed to be extended in Blueprint for visual implementation.
 */
UCLASS(Abstract, Blueprintable, meta = (DisplayName = "Breakaway Scoreboard"))
class BREAKAWAYCORERUNTIME_API UBwayScoreboardWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayScoreboardWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	//~ UUserWidget interface
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidget interface

	// ========== DATA ACCESS ==========

	/** Get all players for a specific team */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	TArray<FScoreboardPlayerData> GetTeamPlayers(int32 TeamIndex) const;

	/** Get the team 1 score */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	int32 GetTeam1Score() const;

	/** Get the team 2 score */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	int32 GetTeam2Score() const;

	/** Get the current round number */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	int32 GetCurrentRound() const;

	/** Get match time elapsed (formatted) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	FText GetMatchTimeElapsed() const;

	/** Refresh all scoreboard data - call this periodically */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	void RefreshScoreboardData();

	// ========== BLUEPRINT EVENTS ==========

	/** Called when scoreboard data has been refreshed */
	UFUNCTION(BlueprintImplementableEvent, Category = "Scoreboard|Events")
	void OnScoreboardDataRefreshed(const TArray<FScoreboardPlayerData>& Team1Players, 
								   const TArray<FScoreboardPlayerData>& Team2Players);

protected:
	/** How often to auto-refresh data (in seconds) */
	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard")
	float RefreshInterval = 1.0f;

private:
	/** Get the game state */
	ABwayGameState* GetBwayGameState() const;

	/** Build player data from player state */
	FScoreboardPlayerData BuildPlayerData(ABwayPlayerState* PlayerState, int32 TeamIndex) const;

	/** Time since last refresh */
	float TimeSinceLastRefresh = 0.0f;
};

