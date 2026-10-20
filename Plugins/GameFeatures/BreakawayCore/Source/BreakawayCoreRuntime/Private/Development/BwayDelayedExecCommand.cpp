// Development-only timed console execution for packaged evidence capture (screenshots, timed cvar
// changes) where -ExecCmds only runs at startup. Not compiled into Shipping.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace BwayDelayedExec
{
static UWorld* FindGameWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World && World->IsGameWorld())
		{
			return World;
		}
	}
	return nullptr;
}

FAutoConsoleCommand Command(
	TEXT("bway.Test.DelayedExec"),
	TEXT("Development: bway.Test.DelayedExec <seconds> <console command...> runs the command on the current game world after the delay."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() < 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayDelayedExec: usage bway.Test.DelayedExec <seconds> <command...>"));
			return;
		}
		const float Delay = FMath::Max(0.0f, FCString::Atof(*Args[0]));
		TArray<FString> Rest;
		for (int32 Index = 1; Index < Args.Num(); ++Index)
		{
			Rest.Add(Args[Index]);
		}
		const FString Pending = FString::Join(Rest, TEXT(" "));
		UE_LOG(LogTemp, Display, TEXT("BwayDelayedExec: scheduled delay=%.2f command=%s"), Delay, *Pending);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Pending](float)
		{
			UWorld* World = FindGameWorld();
			// Route through the local player so viewport-client commands (HighResShot, shot) resolve.
			APlayerController* LocalPC = World ? World->GetFirstPlayerController() : nullptr;
			const bool bViaPlayer = LocalPC && LocalPC->IsLocalController();
			UE_LOG(LogTemp, Display, TEXT("BwayDelayedExec: run command=%s world=%s viaPlayer=%d"), *Pending, *GetPathNameSafe(World), bViaPlayer ? 1 : 0);
			if (bViaPlayer)
			{
				LocalPC->ConsoleCommand(Pending, true);
			}
			else if (GEngine)
			{
				GEngine->Exec(World, *Pending);
			}
			return false;
		}), Delay);
	}));
}
#endif
