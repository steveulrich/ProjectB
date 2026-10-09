#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "BwayGameState.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace BwayHeroSelectionTests
{
UWorld* FindAuthorityPIEWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* Candidate = Context.World();
		if (Candidate && Candidate->IsGameWorld() && Candidate->WorldType == EWorldType::PIE && Candidate->GetNetMode() != NM_Client)
		{
			return Candidate;
		}
	}

	return nullptr;
}

void AutoLockAll()
{
	UWorld* World = nullptr;
	World = FindAuthorityPIEWorld();
	if (!World || !World->IsGameWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionTest: AutoLockAll could not resolve an authoritative PIE game world"));
		return;
	}

	ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	if (!GameState || !GameState->HasAuthority() || !GameState->HeroSelectionManager || !GameState->HeroSelectionPhaseComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionTest: AutoLockAll requires the authoritative Breakaway game state and selection components"));
		return;
	}

	UBwayHeroSelectionManager* Manager = GameState->HeroSelectionManager;
	if (!Manager->IsSelectionActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionTest: AutoLockAll requires an active hero-selection phase"));
		return;
	}

	const int32 RegisteredPlayers = Manager->GetTotalPlayers();
	const int32 ReadyBefore = Manager->GetNumPlayersReady();
	Manager->ResolveMissingSelectionsAndLockAll(GameState->HeroSelectionPhaseComponent->ResolveFallbackHeroId());

	UE_LOG(LogTemp, Display, TEXT("BwayHeroSelectionTest: AutoLockAll resolved authority selections (players=%d readyBefore=%d readyAfter=%d allReady=%d)"),
		RegisteredPlayers,
		ReadyBefore,
		Manager->GetNumPlayersReady(),
		Manager->AreAllPlayersReady() ? 1 : 0);
}

FAutoConsoleCommand AutoLockAllCommand(
	TEXT("bway.Test.AutoLockAllHeroes"),
	TEXT("Development test: resolve missing heroes and lock all registered players through the production selection manager."),
	FConsoleCommandDelegate::CreateStatic(&AutoLockAll));
}
#endif
