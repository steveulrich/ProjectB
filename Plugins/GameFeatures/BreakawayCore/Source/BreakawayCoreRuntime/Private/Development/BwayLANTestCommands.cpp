// Development harness for exercising real LAN advertisement from a separate process.
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "CommonSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/BwayUserFacingExperienceDefinition.h"
#include "TimerManager.h"

namespace BwayLANTest
{
void HostLAN(UWorld* World)
{
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayLANTest: HostLAN requires a game world"));
		return;
	}

	// Allow startup commands to run before the front-end experience finishes loading.
	FTimerHandle Timer;
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(World, [World]()
	{
		UGameInstance* GI = World->GetGameInstance();
		APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
		UCommonSessionSubsystem* Sessions = GI ? GI->GetSubsystem<UCommonSessionSubsystem>() : nullptr;
		const UBwayUserFacingExperienceDefinition* Playlist = LoadObject<UBwayUserFacingExperienceDefinition>(
			nullptr, TEXT("/Game/System/DA_Playlist_CTR_LAN.DA_Playlist_CTR_LAN"));
		if (!PC || !Sessions || !Playlist)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayLANTest: Missing local player, session subsystem, or LAN playlist"));
			return;
		}
		UCommonSession_HostSessionRequest* Request = Playlist->CreateHostingRequest(World);
		if (!Request || Request->OnlineMode != ECommonSessionOnlineMode::LAN)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayLANTest: Playlist did not produce a LAN request"));
			return;
		}
		UE_LOG(LogTemp, Display, TEXT("BwayLANTest: Hosting through the LAN playlist and CommonSessionSubsystem"));
		Sessions->HostSession(PC, Request);
	}), 10.0f, false);
}

FAutoConsoleCommandWithWorld HostCommand(
	TEXT("bway.Test.HostLAN"),
	TEXT("Development test: host the LAN playlist after a 10-second front-end startup delay."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&HostLAN));
}
#endif
