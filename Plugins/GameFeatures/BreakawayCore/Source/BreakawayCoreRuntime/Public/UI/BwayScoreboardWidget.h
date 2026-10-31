// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Economy/BwayUpgradeComponent.h"
#include "BwayScoreboardWidget.generated.h"

class ABwayGameState;
class ABwayPlayerState;
class UBwayUpgradeCatalog;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
struct FBwayMatchBreakdownPlayerColumn;

/**
 * Player data structure for 8-player Tab scoreboard display.
 */
USTRUCT(BlueprintType)
struct FScoreboardPlayerData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 PlayerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 TeamIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 DisplayTeamSlot = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 Assists = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 ObjectiveScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 DamageDealt = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 HealingDone = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 ForcedFumbles = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 Interceptions = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 BuildablesDestroyed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 MatchGoldEarned = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	FPrimaryAssetId HeroId;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	FText HeroName;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	TObjectPtr<UTexture2D> HeroPortrait = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	TArray<FBwayOwnedUpgrade> EquippedUpgrades;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	float MVPScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	bool bIsMVP = false;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	bool bIsLocalPlayer = false;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	bool bIsAlive = true;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	bool bIsConnected = true;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 Ping = 0;
};

/**
 * UBwayScoreboardWidget
 *
 * Tab-held 8-player scoreboard showing Friendly (Blue) and Enemy (Red) teams,
 * Hero portrait, K/D/A, Goals, Damage, Healing, Forced Fumbles, Interceptions,
 * Buildables Destroyed, Match Gold Earned, Equipped Upgrades (icons + ranks), and MVP highlight.
 */
UCLASS(Blueprintable, meta = (DisplayName = "Breakaway Scoreboard"))
class BREAKAWAYCORERUNTIME_API UBwayScoreboardWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UBwayScoreboardWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Get all players for a specific authoritative team (0 or 1), including disconnected participants. */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	TArray<FScoreboardPlayerData> GetTeamPlayers(int32 TeamIndex) const;

	/** Get all 8 scoreboard rows ordered friendly-first, reconciled with authoritative MVP. */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	TArray<FScoreboardPlayerData> GetAllScoreboardPlayers() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	int32 GetMVPPlayerId() const { return CachedMVPPlayerId; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	FText GetMVPPlayerName() const { return CachedMVPPlayerName; }

	/** Refresh all scoreboard data and visual rows. */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	void RefreshScoreboardData();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	int32 GetTeam1Score() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	int32 GetTeam2Score() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	int32 GetCurrentRound() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	FText GetMatchTimeElapsed() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Scoreboard|Events")
	void OnScoreboardDataRefreshed(
		const TArray<FScoreboardPlayerData>& Team1Players,
		const TArray<FScoreboardPlayerData>& Team2Players);

	UPROPERTY(EditDefaultsOnly, Category = "Scoreboard")
	float RefreshInterval = 0.25f;

private:
	void EnsureScoreboardLayout();
	void RebuildVisualRows(
		const TArray<FScoreboardPlayerData>& FriendlyPlayers,
		const TArray<FScoreboardPlayerData>& EnemyPlayers);
	void BuildTeamRowsInContainer(
		UVerticalBox* Container,
		const TArray<FScoreboardPlayerData>& Rows,
		bool bFriendlyTeam);
	const UBwayUpgradeCatalog* ResolveUpgradeCatalog() const;

	ABwayGameState* GetBwayGameState() const;
	FScoreboardPlayerData BuildPlayerData(ABwayPlayerState* PlayerState, int32 TeamIndex, int32 LocalTeam) const;
	FScoreboardPlayerData BuildDisconnectedPlayerData(
		const FBwayMatchBreakdownPlayerColumn& Snapshot,
		int32 LocalTeam) const;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_ScoreboardHeader;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> Text_ScoreboardSubheader;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> FriendlyRowsBox;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> EnemyRowsBox;

	UPROPERTY(Transient)
	mutable TObjectPtr<UBwayUpgradeCatalog> CachedUpgradeCatalog;

	mutable int32 CachedMVPPlayerId = INDEX_NONE;
	mutable FText CachedMVPPlayerName;
	float TimeSinceLastRefresh = 0.0f;
};
