// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonGameInstance.h"

#include "LyraGameInstance.generated.h"

#define UE_API LYRAGAME_API

class ALyraPlayerController;
class UObject;
class UCustomGameConfig;

UCLASS(MinimalAPI, Config = Game)
class ULyraGameInstance : public UCommonGameInstance
{
	GENERATED_BODY()

public:

	UE_API ULyraGameInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UE_API ALyraPlayerController* GetPrimaryPlayerController() const;

	/** Creates CurrentCustomGameConfig if needed. Safe to call from front-end Blueprints on boot. */
	UFUNCTION(BlueprintCallable, Category = "Lyra|Custom Game")
	UE_API void InitializeCustomGameSettings();

	/** Returns persisted custom-game settings; creates defaults if unset. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Lyra|Custom Game")
	UE_API UCustomGameConfig* GetCustomGameSettings();

	UE_API virtual bool CanJoinRequestedSession() const override;
	UE_API virtual void HandlerUserInitialized(const UCommonUserInfo* UserInfo, bool bSuccess, FText Error, ECommonUserPrivilege RequestedPrivilege, ECommonUserOnlineContext OnlineContext) override;

	UE_API virtual void ReceivedNetworkEncryptionToken(const FString& EncryptionToken, const FOnEncryptionKeyResponse& Delegate) override;
	UE_API virtual void ReceivedNetworkEncryptionAck(const FOnEncryptionKeyResponse& Delegate) override;

	/** Survives front-end map changes for the lifetime of this game instance. */
	UPROPERTY(Transient)
	TObjectPtr<UCustomGameConfig> CurrentCustomGameConfig;

protected:

	UE_API virtual void Init() override;
	UE_API virtual void Shutdown() override;

	UE_API void OnPreClientTravelToSession(FString& URL);

	/** A hard-coded encryption key used to try out the encryption code. This is NOT SECURE, do not use this technique in production! */
	TArray<uint8> DebugTestEncryptionKey;
};

#undef UE_API
