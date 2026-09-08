#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Engine/World.h"
#include "HeroSystems/BwayHeroSelectionManager.h"

namespace BwayResultsDiagnostics
{
void Setup(const TArray<FString>& Args, UWorld* World)
{
	const bool bUniqueWinner = Args.Num() == 1 && Args[0] == TEXT("unique-winner");
	if (Args.Num() != 1 || (!bUniqueWinner && Args[0] != TEXT("negative-tie"))
		|| !World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayResultsFixture: requires authority game world and negative-tie or unique-winner"));
		return;
	}
	ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	UBwayHeroSelectionManager* Selection = GameState ? GameState->FindComponentByClass<UBwayHeroSelectionManager>() : nullptr;
	if (!Selection || !Selection->IsSelectionActive() || GameState->PlayerArray.Num() != 8)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayResultsFixture: requires active selection and eight players"));
		return;
	}
	TArray<ABwayPlayerState*> Players;
	TSet<int32> PlayerIds;
	ABwayPlayerState* Winner = nullptr;
	for (APlayerState* State : GameState->PlayerArray)
	{
		ABwayPlayerState* Player = Cast<ABwayPlayerState>(State);
		if (!Player || PlayerIds.Contains(Player->GetPlayerId()))
		{
			UE_LOG(LogTemp, Error, TEXT("BwayResultsFixture: invalid or duplicate player identity"));
			return;
		}
		PlayerIds.Add(Player->GetPlayerId());
		Players.Add(Player);
		if (!Winner || Player->GetPlayerId() > Winner->GetPlayerId())
		{
			Winner = Player;
		}
	}

	// Transient fixture only: a fresh session restores selection timing and stats.
	Selection->bAutoLockOnTimeout = false;
	Selection->AssignRandomHeroToPlayers(false, FPrimaryAssetId(), false);
	for (ABwayPlayerState* Player : Players)
	{
		Player->SetPlayerName(TEXT("Duplicate player with a long display name"));
		Player->ResetMatchStats();
		for (int32 Death = 0; Death < 4; ++Death)
		{
			Player->AddDeath();
		}
		if (bUniqueWinner && Player == Winner)
		{
			for (int32 Kill = 0; Kill < 10; ++Kill)
			{
				Player->AddKill();
			}
		}
		Player->ForceNetUpdate();
	}
	UE_LOG(LogTemp, Display, TEXT("BwayResultsFixture: ready case=%s players=%d; synthetic stats, not gameplay evidence"), *Args[0], Players.Num());
}

FAutoConsoleCommandWithWorldAndArgs Command(
	TEXT("bway.Test.ResultsIdentity"),
	TEXT("Development authority fixture: during eight-player selection, assigns duplicate names and synthetic stats. Argument: negative-tie or unique-winner. Start a fresh session afterward."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Setup));
}
#endif
