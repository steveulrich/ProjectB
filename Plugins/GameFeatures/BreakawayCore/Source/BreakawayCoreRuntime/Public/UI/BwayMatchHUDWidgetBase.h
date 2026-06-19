// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "BwayMatchHUDWidgetBase.generated.h"

class ABwayGameState;
class APlayerController;
class UBwayRelicManagerComponent;
class UBwayScoringComponent;
class UWorld;

/**
 * Shared match HUD data access and delegate binding for Lyra slot widgets
 * (score bar, relic status) and reused by UBwayCoreHUDWidget via static helpers.
 */
UCLASS(Abstract, Blueprintable)
class BREAKAWAYCORERUNTIME_API UBwayMatchHUDWidgetBase : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	/** Resolve ABwayGameState from a widget's world. */
	static ABwayGameState* ResolveBwayGameState(const UUserWidget* Widget);

	/** Format seconds as MM:SS for round timer display. */
	static FText FormatRoundTime(int32 TotalSeconds);

	/** Read a team score from GameState scoring component (-1 team index returns 0). */
	static int32 GetTeamScoreFromWorld(const UWorld* World, int32 TeamIndex);

	/** Read round time remaining from GameState. */
	static int32 GetRoundTimeRemainingFromWorld(const UWorld* World);

	/** Read relic possessing team (-1 = neutral) from GameState. */
	static int32 GetRelicPossessingTeamFromWorld(const UWorld* World);

	/** True when the relic actor exists and is in Carried state. */
	static bool IsRelicCarriedInWorld(const UWorld* World);

	/** Carrier display name when carried; empty when neutral or unknown. */
	static FText GetRelicCarrierNameFromWorld(const UWorld* World);

	/** Local player's authoritative game team (0/1), or -1 for spectator/unassigned. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Team")
	static int32 GetLocalPlayerTeamForWidget(const UUserWidget* Widget);

	/** Local player's authoritative game team from a player controller. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Team")
	static int32 GetLocalPlayerTeamForPlayerController(const APlayerController* PlayerController);

	/** True when UI should swap left/right columns (local player on game team 1). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Team")
	static bool ShouldSwapTeamsForDisplay(int32 LocalPlayerTeamIndex);

	/** Map authoritative game team index to display slot (0 = left, 1 = right). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Team")
	static int32 MapGameTeamToDisplaySlot(int32 GameTeamIndex, int32 LocalPlayerTeamIndex);

	/** Map display slot (0 = left, 1 = right) to authoritative game team index. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Team")
	static int32 MapDisplaySlotToGameTeam(int32 DisplaySlotIndex, int32 LocalPlayerTeamIndex);

	/** Score for a display slot, accounting for local-team-left perspective. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Team")
	static int32 GetDisplayTeamScore(const UWorld* World, int32 DisplaySlotIndex, int32 LocalPlayerTeamIndex);

	/** Relic possessing team in display slot space (-1 = neutral). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "HUD|Team")
	static int32 GetDisplayRelicPossessingTeam(const UWorld* World, int32 LocalPlayerTeamIndex);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	ABwayGameState* GetBwayGameState() const;
	UBwayScoringComponent* GetScoringComponent() const;
	UBwayRelicManagerComponent* GetRelicManagerComponent() const;

	void BindToMatchData();
	void UnbindFromMatchData();

	/** Override in slot widgets to react to score changes. */
	virtual void NotifyTeamScoreChanged(int32 TeamIndex, int32 NewScore) {}

	/** Override in slot widgets to react to round timer broadcasts (authority only). */
	virtual void NotifyRoundTimeChanged(int32 SecondsRemaining) {}

private:
	UFUNCTION()
	void HandleTeamScoreChanged(int32 TeamIndex, int32 NewScore);

	UFUNCTION()
	void HandleRoundTimeChanged(int32 SecondsRemaining);

	UPROPERTY()
	TWeakObjectPtr<ABwayGameState> CachedGameState;

	UPROPERTY()
	TWeakObjectPtr<UBwayScoringComponent> CachedScoringComponent;

	bool bBoundToMatchData = false;
};
