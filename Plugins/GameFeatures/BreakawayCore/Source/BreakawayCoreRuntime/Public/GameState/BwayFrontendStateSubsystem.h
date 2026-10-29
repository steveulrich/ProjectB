// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "BwayFrontendStateSubsystem.generated.h"

UENUM(BlueprintType)
enum class EBwayBotDifficulty : uint8
{
	Normal UMETA(DisplayName = "NORMAL"),
	Elite  UMETA(DisplayName = "ELITE")
};

UENUM(BlueprintType)
enum class EBwayLobbyInviteMode : uint8
{
	HostOnly          UMETA(DisplayName = "HOST ONLY"),
	FriendsAndInvited UMETA(DisplayName = "FRIENDS & INVITED"),
	OpenToAll         UMETA(DisplayName = "OPEN TO ALL")
};

UENUM(BlueprintType)
enum class EBwayLobbyTeam : uint8
{
	BlueTeam   = 0 UMETA(DisplayName = "BLUE TEAM"),
	RedTeam    = 1 UMETA(DisplayName = "RED TEAM"),
	Spectators = 2 UMETA(DisplayName = "SPECTATORS")
};

USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayMapOptionEntry : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby|Map")
	FPrimaryAssetId MapAssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby|Map")
	FString MapPackagePath = TEXT("/BreakawayCore/Maps/L_BW_Dorado");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby|Map")
	FPrimaryAssetId ExperienceAssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby|Map")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby|Map")
	FText Description;
};

USTRUCT(BlueprintType)
struct BREAKAWAYCORERUNTIME_API FBwayMatchLobbySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby")
	int32 SelectedMapIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby")
	bool bBotsEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby")
	EBwayBotDifficulty BotDifficulty = EBwayBotDifficulty::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby")
	EBwayLobbyInviteMode InviteMode = EBwayLobbyInviteMode::HostOnly;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby")
	EBwayLobbyTeam LocalHostTeam = EBwayLobbyTeam::BlueTeam;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby")
	int32 LocalHostSlotIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby")
	TArray<FString> BlueTeamSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby")
	TArray<FString> RedTeamSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby")
	TArray<FString> SpectatorSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby")
	bool bLaunchedFromLobby = false;

	/** True when hosting an advertised LAN session; false for offline Dev Practice / Bot Match. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lobby")
	bool bIsLanMatch = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBwayLobbySettingsChanged);

/**
 * Subsystem to hold state in the FrontEnd (like selected hero, custom options)
 * that persists right until the player calls OpenLevel to enter a match.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayFrontendStateSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	UBwayFrontendStateSubsystem();

	/** Gets the hero the local player currently has selected in the frontend */
	UFUNCTION(BlueprintCallable, Category = "Frontend State")
	FPrimaryAssetId GetSelectedHeroId() const { return FrontEndSelectedHeroId; }

	/** Sets the hero the local player currently has selected in the frontend */
	UFUNCTION(BlueprintCallable, Category = "Frontend State")
	void SetSelectedHeroId(FPrimaryAssetId HeroId) { FrontEndSelectedHeroId = HeroId; }

	/** Returns the available arena maps for the Match Settings carousel */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Frontend State|Lobby")
	const TArray<FBwayMapOptionEntry>& GetAvailableMaps() const { return AvailableMaps; }

	/** Returns the currently selected map option entry */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Frontend State|Lobby")
	FBwayMapOptionEntry GetSelectedMapEntry() const;

	/** Returns the current match lobby settings */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Frontend State|Lobby")
	const FBwayMatchLobbySettings& GetLobbySettings() const { return LobbySettings; }

	/** Replaces the match lobby settings and broadcasts OnLobbySettingsChanged */
	UFUNCTION(BlueprintCallable, Category = "Frontend State|Lobby")
	void SetLobbySettings(const FBwayMatchLobbySettings& NewSettings);

	/** Configures whether the lobby hosts an advertised LAN session or offline Dev Practice match. */
	UFUNCTION(BlueprintCallable, Category = "Frontend State|Lobby")
	void SetLobbyOnlineMode(bool bInIsLanMatch);

	/** Commits Match Settings Modal options (Map, Enable Bots, Bot Difficulty) */
	UFUNCTION(BlueprintCallable, Category = "Frontend State|Lobby")
	void ApplyMatchSettingsModal(int32 InMapIndex, bool bInEnableBots, EBwayBotDifficulty InBotDifficulty);

	/** Cycles the Invite Mode selector in the lobby */
	UFUNCTION(BlueprintCallable, Category = "Frontend State|Lobby")
	void CycleInviteMode(int32 Direction = 1);

	/** Initializes or resets the 3x4 team roster slots with the local host in Blue Team Slot 1 */
	UFUNCTION(BlueprintCallable, Category = "Frontend State|Lobby")
	void InitializeDefaultLobbyRoster();

	/** Moves the local host to the requested team & slot (0..3) */
	UFUNCTION(BlueprintCallable, Category = "Frontend State|Lobby")
	bool MoveLocalHostToSlot(EBwayLobbyTeam TargetTeam, int32 TargetSlotIndex);

	/** Populates travel URL extra arguments for UCommonSessionSubsystem or OpenLevel */
	UFUNCTION(BlueprintCallable, Category = "Frontend State|Lobby")
	void BuildMatchExtraArgs(TMap<FString, FString>& OutExtraArgs) const;

	/** Builds a full OpenLevel/ServerTravel URL string for the configured match */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Frontend State|Lobby")
	FString BuildMatchTravelUrl(bool bListenServer = true) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Frontend State|Lobby")
	static FText GetBotDifficultyDisplayText(EBwayBotDifficulty Difficulty);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Frontend State|Lobby")
	static FText GetInviteModeDisplayText(EBwayLobbyInviteMode Mode);

	/** Returns the resolved local player display name */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Frontend State|Lobby")
	FString GetResolvedLocalPlayerName() const;

	UPROPERTY(BlueprintAssignable, Category = "Frontend State|Lobby")
	FOnBwayLobbySettingsChanged OnLobbySettingsChanged;

private:
	UPROPERTY(Transient)
	FPrimaryAssetId FrontEndSelectedHeroId;

	UPROPERTY(Transient)
	TArray<FBwayMapOptionEntry> AvailableMaps;

	UPROPERTY(Transient)
	FBwayMatchLobbySettings LobbySettings;
};
