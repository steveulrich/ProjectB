#include "GameState/BwayBotCreationComponent.h"

#include "BreakawayGameMode.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "GameModes/LyraGameMode.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "GameFramework/PlayerState.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Player/LyraPlayerBotController.h"
#include "TimerManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayBotCreationComponent)

namespace
{
bool IsHumanPlayerController(const AController* Controller)
{
	return Controller && !Controller->IsA<AAIController>();
}
}

UBwayBotCreationComponent::UBwayBotCreationComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumBotsToCreate = 7;
	BotControllerClass = ALyraPlayerBotController::StaticClass();
}

void UBwayBotCreationComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!GetOwner()->HasAuthority() || UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this))
	{
		return;
	}

	if (AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>())
	{
		if (ULyraExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyraExperienceManagerComponent>())
		{
			ExperienceComponent->CallOrRegister_OnExperienceLoaded(
				FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &UBwayBotCreationComponent::OnExperienceLoaded));
		}
	}
}

void UBwayBotCreationComponent::OnExperienceLoaded(const ULyraExperienceDefinition* Experience)
{
	// Spawn bot controllers once the experience (pawn data, actions) is ready.
	// Round start will place them at team spawns via EnsureBotsForRound.
	SpawnMissingBots();
}

int32 UBwayBotCreationComponent::GetTargetBotCount() const
{
	int32 BotsToSpawn = NumBotsToCreate;

	if (bScaleBotsToTargetPlayerCount && GetWorld())
	{
		int32 HumanCount = 0;
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (IsHumanPlayerController(It->Get()))
			{
				++HumanCount;
			}
		}

		// Listen-server PIE creates bots before the human client connects; reserve one slot.
		const int32 OccupiedHumanSlots = FMath::Max(HumanCount, 1);
		BotsToSpawn = FMath::Max(0, TargetPlayerCount - OccupiedHumanSlots);
	}

	return BotsToSpawn;
}

void UBwayBotCreationComponent::SpawnMissingBots()
{
	if (!GetOwner()->HasAuthority() || UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this))
	{
		return;
	}

	SpawnedBotList.RemoveAll([](const TObjectPtr<AAIController>& Bot)
	{
		return !IsValid(Bot);
	});

	const int32 BotsToSpawn = GetTargetBotCount();
	for (int32 Index = SpawnedBotList.Num(); Index < BotsToSpawn; ++Index)
	{
		SpawnOneBot();
	}
}

void UBwayBotCreationComponent::EnsureBotsForRound()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	SpawnMissingBots();
	RestartAllBots();
}

void UBwayBotCreationComponent::SpawnOneBot()
{
	if (!BotControllerClass || UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this))
	{
		return;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.OverrideLevel = GetComponentLevel();
	SpawnInfo.ObjectFlags |= RF_Transient;

	AAIController* NewController = GetWorld()->SpawnActor<AAIController>(BotControllerClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnInfo);
	if (!NewController)
	{
		return;
	}

	if (NewController->PlayerState)
	{
		NewController->PlayerState->SetPlayerName(FString::Printf(TEXT("Breakaway Bot %d"), SpawnedBotList.Num() + 1));
	}

	ALyraGameMode* GameMode = GetWorld()->GetAuthGameMode<ALyraGameMode>();
	if (GameMode)
	{
		GameMode->GenericPlayerInitialization(NewController);
	}

	if (ABreakawayGameMode* BwayGameMode = GetWorld()->GetAuthGameMode<ABreakawayGameMode>())
	{
		BwayGameMode->AssignControllerToTeam(NewController);
	}

	if (!UBwayHeroSelectionFlowLibrary::ShouldSkipHeroSelectionWorld(this))
	{
		ABwayGameState* GameState = GetOwner<ABwayGameState>();
		UBwayHeroSelectionManager* SelectionManager = GameState ? GameState->FindComponentByClass<UBwayHeroSelectionManager>() : nullptr;
		ABwayPlayerState* BotPlayerState = NewController->GetPlayerState<ABwayPlayerState>();

		if (SelectionManager && BotPlayerState)
		{
			if (SelectionManager->IsSelectionActive())
			{
				SelectionManager->RegisterPlayer(BotPlayerState);
			}

			if (!BotPlayerState->GetSelectedHeroId().IsValid() || !BotPlayerState->IsHeroLocked())
			{
				const bool bLockImmediately = SelectionManager->IsSelectionActive()
					|| !UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayWithoutHeroSelectFlow(this);
				SelectionManager->AssignRandomHeroToPlayer(
					BotPlayerState,
					FPrimaryAssetId(),
					bLockImmediately);
			}
		}
	}

	if (GameMode)
	{
		GameMode->RestartPlayer(NewController);
	}

	if (APawn* Pawn = NewController->GetPawn())
	{
		if (ULyraPawnExtensionComponent* PawnExtComponent = Pawn->FindComponentByClass<ULyraPawnExtensionComponent>())
		{
			PawnExtComponent->CheckDefaultInitialization();
		}
	}

	SpawnedBotList.Add(NewController);
}

void UBwayBotCreationComponent::RestartAllBots()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	AGameModeBase* GM = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr;
	if (!GM)
	{
		return;
	}

	SpawnedBotList.RemoveAll([](const TObjectPtr<AAIController>& Bot)
	{
		return !IsValid(Bot);
	});

	for (AAIController* BotController : SpawnedBotList)
	{
		if (!BotController)
		{
			continue;
		}

		if (APawn* OldBotPawn = BotController->GetPawn())
		{
			BotController->UnPossess();
			OldBotPawn->Destroy();
		}

		GM->RestartPlayer(BotController);

		if (APawn* Pawn = BotController->GetPawn())
		{
			if (ULyraPawnExtensionComponent* PawnExtComponent = Pawn->FindComponentByClass<ULyraPawnExtensionComponent>())
			{
				PawnExtComponent->CheckDefaultInitialization();
			}
		}
	}
}
