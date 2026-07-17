// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BwayHUDHelpers.generated.h"

class UImage;
class UBwayHeroDataAsset;
class ABwayPlayerState;

/**
 * Data about a single player for HUD display
 */
USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FTeamPlayerHUDData
{
	GENERATED_BODY()

	/** The player's display name */
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	FString PlayerName;

	/** The player's selected hero data asset (may be null if not yet selected) */
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TObjectPtr<const UBwayHeroDataAsset> HeroData = nullptr;

	/** The hero portrait texture (convenience, same as HeroData->Portrait) */
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TObjectPtr<UTexture2D> PortraitTexture = nullptr;

	/** Whether this player is currently alive */
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	bool bIsAlive = true;

	/** Whether this player currently has the relic */
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	bool bHasRelic = false;

	/** The player state reference */
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	TObjectPtr<ABwayPlayerState> PlayerState = nullptr;

	/** Slot index (0-3 for team position) */
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	int32 SlotIndex = 0;

	FTeamPlayerHUDData() = default;
};

/**
 * Blueprint function library for HUD-related helper functions
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayHUDHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Get HUD data for all players on a specific team
	 * 
	 * @param WorldContextObject - World context for getting GameState
	 * @param TeamIndex - Which team to get (0 = Team 1, 1 = Team 2)
	 * @param OutPlayerData - Array of player HUD data, ordered by slot index
	 * @return True if successful, false if GameState not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|HUD", meta = (WorldContext = "WorldContextObject"))
	static bool GetTeamPlayerHUDData(
		const UObject* WorldContextObject,
		int32 TeamIndex,
		TArray<FTeamPlayerHUDData>& OutPlayerData);

	/** Replication-safe alive state for local or remote players, including the respawn pawn gap. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|HUD")
	static bool IsPlayerAliveForHUD(const ABwayPlayerState* PlayerState);

	/**
	 * Update an array of Image widgets with hero portraits for a team
	 * 
	 * @param WorldContextObject - World context for getting GameState
	 * @param TeamIndex - Which team to display (0 = Team 1, 1 = Team 2)
	 * @param PortraitImages - Array of Image widgets to update (should be 4 for 4v4)
	 * @param EmptySlotTexture - Optional texture to show for empty slots
	 * @param DeadPlayerTint - Color tint to apply to dead players (default: dark gray)
	 */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|HUD", meta = (WorldContext = "WorldContextObject"))
	static void UpdateTeamPortraits(
		const UObject* WorldContextObject,
		int32 TeamIndex,
		const TArray<UImage*>& PortraitImages,
		UTexture2D* EmptySlotTexture = nullptr,
		FLinearColor DeadPlayerTint = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f));

	/** Local player's authoritative game team (0/1), or -1 for spectator/unassigned. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|HUD", meta = (WorldContext = "WorldContextObject"))
	static int32 GetLocalPlayerTeamForHUD(const UObject* WorldContextObject);

	/** Map display slot (0 = left, 1 = right) to authoritative game team index. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|HUD")
	static int32 MapDisplaySlotToGameTeam(int32 DisplaySlotIndex, int32 LocalPlayerTeamIndex);

	/** HUD data for a display column (left = friendly when local team is assigned). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|HUD", meta = (WorldContext = "WorldContextObject"))
	static bool GetTeamPlayerHUDDataForDisplaySlot(
		const UObject* WorldContextObject,
		int32 DisplaySlotIndex,
		int32 LocalPlayerTeamIndex,
		TArray<FTeamPlayerHUDData>& OutPlayerData);

	/** Update portrait images for a display column (left = friendly when local team is assigned). */
	UFUNCTION(BlueprintCallable, Category = "Breakaway|HUD", meta = (WorldContext = "WorldContextObject"))
	static void UpdateTeamPortraitsForDisplaySlot(
		const UObject* WorldContextObject,
		int32 DisplaySlotIndex,
		int32 LocalPlayerTeamIndex,
		const TArray<UImage*>& PortraitImages,
		UTexture2D* EmptySlotTexture = nullptr,
		FLinearColor DeadPlayerTint = FLinearColor(0.3f, 0.3f, 0.3f, 1.0f));

	/**
	 * Get the hero data asset for a specific player
	 * 
	 * @param PlayerState - The player to get hero data for
	 * @return The hero data asset, or nullptr if not selected/loaded
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|HUD")
	static const UBwayHeroDataAsset* GetPlayerHeroData(const ABwayPlayerState* PlayerState);

	/**
	 * Get the hero portrait texture for a player
	 * 
	 * @param PlayerState - The player to get portrait for
	 * @return The portrait texture, or nullptr if not available
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Breakaway|HUD")
	static UTexture2D* GetPlayerHeroPortrait(const ABwayPlayerState* PlayerState);
};

