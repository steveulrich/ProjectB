// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonSessionSubsystem.h"
#include "Matchmaking/LyraMatchmakingTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "LyraMatchmakingSubsystem.generated.h"

#define UE_API LYRAGAME_API

class UCommonSession_HostSessionRequest;
class UObject;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLyraMockMatchFoundDelegate);

/**
 * Front-end matchmaking queue (mock backend in 11-MM-3; session travel in 11-MM-4).
 */
UCLASS(MinimalAPI)
class ULyraMatchmakingSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UE_API ULyraMatchmakingSubsystem();

	UFUNCTION(BlueprintCallable, Category = "Lyra|Matchmaking", meta = (WorldContext = "WorldContextObject"))
	static UE_API ULyraMatchmakingSubsystem* Get(const UObject* WorldContextObject);

	/** Queue path: serialize payload, mock backend handoff after MockMatchDelaySeconds. */
	UFUNCTION(BlueprintCallable, Category = "Lyra|Matchmaking")
	UE_API void StartMatchmakingQueue(const FLyraMatchmakingQueuePayload& Payload);

	/** Marks CurrentCustomGameConfig as a custom lobby (used by 11-MM-4 immediate host path). */
	UFUNCTION(BlueprintCallable, Category = "Lyra|Matchmaking")
	UE_API void CreateCustomLobby();

	/**
	 * Custom-game path: host immediately from UCustomGameConfig overrides (no mock queue timer).
	 * Requires bIsCustomGameDefinition plus valid SelectedMapOverride / SelectedExperienceOverride.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lyra|Matchmaking")
	UE_API void ExecuteCustomSessionCreation();

	/**
	 * Phase 3 mock backend entry point. Logs JSON and schedules OnMockMatchFound.
	 * Phase 4+ may replace internals with a real HTTP call without changing callers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Lyra|Matchmaking")
	UE_API void SendMatchmakingPayloadToBackend(const FString& JsonPayload);

	UFUNCTION(BlueprintCallable, Category = "Lyra|Matchmaking")
	UE_API void CancelPendingMatchmaking();

	UFUNCTION(BlueprintPure, Category = "Lyra|Matchmaking")
	UE_API bool IsMatchmakingPending() const;

	UFUNCTION(BlueprintPure, Category = "Lyra|Matchmaking")
	UE_API FLyraMatchmakingQueuePayload GetActiveQueuePayload() const { return ActiveQueuePayload; }

	UFUNCTION(BlueprintPure, Category = "Lyra|Matchmaking")
	UE_API FString GetLastPayloadJson() const { return LastPayloadJson; }

	UPROPERTY(BlueprintAssignable, Category = "Lyra|Matchmaking")
	FLyraMockMatchFoundDelegate OnMockMatchFound;

	/** Mock queue delay before OnMockMatchFound (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Matchmaking", meta = (ClampMin = "0.1"))
	float MockMatchDelaySeconds = 3.0f;

	/** Session host mode for queue/custom travel. The slice uses LAN discovery. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Matchmaking")
	ECommonSessionOnlineMode DevHostOnlineMode = ECommonSessionOnlineMode::LAN;

	/** Merged into queue travel URL (dev goals expect SkipHeroSelection=1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lyra|Matchmaking")
	TMap<FString, FString> DefaultQueueExtraArgs;

private:
	UE_API void ExecuteQueueSessionCreation();
	UE_API UCommonSession_HostSessionRequest* BuildHostSessionRequest(
		FPrimaryAssetId MapID,
		FPrimaryAssetId ExperienceID,
		int32 MaxPlayerCount,
		const FString& ModeNameForAdvertisement,
		const TMap<FString, FString>& AdditionalExtraArgs) const;
	UE_API bool HostSessionFromRequest(UCommonSession_HostSessionRequest* Request);
	UE_API void HandleMockMatchFound();
	UE_API FString BuildJsonPayload(const FLyraMatchmakingQueuePayload& Payload) const;
	UE_API void ShowMatchmakingScreenMessage(const FString& Message, const FColor& Color, float DurationSeconds = 5.0f) const;

	UPROPERTY(Transient)
	FLyraMatchmakingQueuePayload ActiveQueuePayload;

	UPROPERTY(Transient)
	FString LastPayloadJson;

	FTimerHandle MockMatchFoundTimerHandle;

	bool bMatchmakingPending = false;
};

#undef UE_API
