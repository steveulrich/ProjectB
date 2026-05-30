// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"

#include "LyraUserFacingExperienceDefinition.generated.h"

#define UE_API LYRAGAME_API

class FString;
class UCommonSession_HostSessionRequest;
class UObject;
class UTexture2D;
class UUserWidget;
struct FFrame;

/** Network/session behavior requested by a user-facing experience tile. */
UENUM(BlueprintType)
enum class ELyraUserFacingExperienceOnlineMode : uint8
{
	Offline,
	LAN,
	Online
};

/** Description of settings used to display experiences in the UI and start a new session */
UCLASS(BlueprintType, MinimalAPI)
class ULyraUserFacingExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UE_API ULyraUserFacingExperienceDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	/** The specific map to load */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience, meta=(AllowedTypes="Map"))
	FPrimaryAssetId MapID;

	/** The gameplay experience to load */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience, meta=(AllowedTypes="LyraExperienceDefinition"))
	FPrimaryAssetId ExperienceID;

	/** Extra arguments passed as URL options to the game */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	TMap<FString, FString> ExtraArgs;

	/** Online behavior for the session created from this tile. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Session)
	ELyraUserFacingExperienceOnlineMode OnlineMode = ELyraUserFacingExperienceOnlineMode::Online;

	/** True if this session should use lobbies when the active online subsystem supports them. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Session)
	bool bUseLobbies = true;

	/** True if this session should advertise presence when the active online subsystem supports it. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Session)
	bool bUsePresence = true;

	/** Primary title in the UI */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	FText TileTitle;

	/** Secondary title */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	FText TileSubTitle;

	/** Full description */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	FText TileDescription;

	/** Icon used in the UI */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	TObjectPtr<UTexture2D> TileIcon;

	/** The loading screen widget to show when loading into (or back out of) a given experience */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=LoadingScreen)
	TSoftClassPtr<UUserWidget> LoadingScreenWidget;

	/** If true, this is a default experience that should be used for quick play and given priority in the UI */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	bool bIsDefaultExperience = false;

	/** If true, this will show up in the experiences list in the front-end */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	bool bShowInFrontEnd = true;

	/** If true, a replay will be recorded of the game */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	bool bRecordReplay = false;

	/** Max number of players for this session */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	int32 MaxPlayerCount = 16;

public:
	/** Create a request object that is used to actually start a session with these settings */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, meta = (WorldContext = "WorldContextObject"))
	virtual UE_API UCommonSession_HostSessionRequest* CreateHostingRequest(const UObject* WorldContextObject) const;
};

#undef UE_API
