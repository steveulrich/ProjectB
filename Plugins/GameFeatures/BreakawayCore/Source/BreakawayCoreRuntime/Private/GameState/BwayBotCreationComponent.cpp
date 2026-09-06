#include "GameState/BwayBotCreationComponent.h"

#include "BreakawayGameMode.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "GameModes/LyraGameMode.h"
#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "GameFramework/PlayerState.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "AI/BwayRelicBotController.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BrainComponent.h"
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
	BotControllerClass = ABwayRelicBotController::StaticClass();
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

void UBwayBotCreationComponent::SetNumBotsOverride(int32 InNumBots)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	NumBotsOverride = InNumBots;
}

void UBwayBotCreationComponent::ApplyMatchRulesFromExperience(const ULyraExperienceDefinition* Experience)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// Re-augment at experience load — PIE LastURL / server game options may not exist at InitGame.
	if (AGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr)
	{
		UBwayGameplayUrlLibrary::AugmentGameModeOptionsString(GameMode);
	}

	ABwayGameState* BwayGS = GetOwner<ABwayGameState>();
	UBwayRoundManagementComponent* RoundManagement = BwayGS ? BwayGS->GetRoundManagement() : nullptr;

	const FBwayResolvedMatchFlowSettings Resolved = UBwayMatchFlowLibrary::ResolveMatchFlowSettings(this, nullptr, Experience);
	UBwayMatchFlowLibrary::LogResolvedMatchFlowSettings(Resolved);
	UBwayMatchFlowLibrary::ApplyMatchRulesOnly(this, Resolved, RoundManagement, this);
}

void UBwayBotCreationComponent::OnExperienceLoaded(const ULyraExperienceDefinition* Experience)
{
	ApplyMatchRulesFromExperience(Experience);

	// Spawn bot controllers once the experience (pawn data, actions) is ready.
	// Round start will place them at team spawns via EnsureBotsForRound.
	SpawnMissingBots();
}

int32 UBwayBotCreationComponent::GetUrlNumBotsOverride(int32 CurrentDefault) const
{
	int32 UrlNumBots = 0;
	if (UBwayMatchFlowLibrary::GetUrlOptionInt(this, TEXT("NumBots"), UrlNumBots))
	{
		return UrlNumBots;
	}

	return CurrentDefault;
}

int32 UBwayBotCreationComponent::GetTargetBotCount() const
{
	int32 BotsToSpawn = NumBotsOverride >= 0 ? NumBotsOverride : NumBotsToCreate;

	if (NumBotsOverride < 0 && bScaleBotsToTargetPlayerCount && GetWorld())
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

	return GetUrlNumBotsOverride(BotsToSpawn);
}

void UBwayBotCreationComponent::TrimExcessBots(int32 TargetBotCount)
{
	SpawnedBotList.RemoveAll([](const TObjectPtr<AAIController>& Bot)
	{
		return !IsValid(Bot);
	});

	while (SpawnedBotList.Num() > TargetBotCount)
	{
		AAIController* BotController = SpawnedBotList.Pop(EAllowShrinking::No);
		if (!IsValid(BotController))
		{
			continue;
		}

		if (ABwayRelicBotController* RelicBot = Cast<ABwayRelicBotController>(BotController))
		{
			RelicBot->StopRelicBotLogic();
		}
		else if (UBrainComponent* Brain = BotController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("HumanJoined"));
		}
		if (ABwayGameState* GameState = GetOwner<ABwayGameState>())
		{
			GameState->RemovePlayerFromTeam(BotController->PlayerState);
		}

		if (APawn* OldPawn = BotController->GetPawn())
		{
			BotController->UnPossess();
			OldPawn->Destroy();
		}

		BotController->Destroy();
	}
}

void UBwayBotCreationComponent::MakeRoomForHumanPlayer()
{
	if (GetOwner()->HasAuthority() && !UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this))
	{
		// Only remove existing fillers here; experience load owns initial creation.
		TrimExcessBots(FMath::Max(0, GetTargetBotCount()));
	}
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
	TrimExcessBots(BotsToSpawn);

	UE_LOG(LogTemp, Log,
		TEXT("BwayMatchFlow: SpawnMissingBots — target=%d override=%d default=%d scale4v4=%s current=%d"),
		BotsToSpawn,
		NumBotsOverride,
		NumBotsToCreate,
		bScaleBotsToTargetPlayerCount ? TEXT("true") : TEXT("false"),
		SpawnedBotList.Num());

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

	if (!UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidWorld(this))
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
				const FPrimaryAssetId DirectPieBotHero = UBwayHeroSelectionFlowLibrary::ResolveDirectPieBotHeroId(this);
				if (DirectPieBotHero.IsValid())
				{
					SelectionManager->AssignHeroToPlayer(BotPlayerState, DirectPieBotHero, bLockImmediately);
				}
				else
				{
					SelectionManager->AssignRandomHeroToPlayer(
						BotPlayerState,
						FPrimaryAssetId(),
						bLockImmediately);
				}
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

	ApplyRelicAIToBot(NewController);

	SpawnedBotList.Add(NewController);
}

void UBwayBotCreationComponent::ApplyRelicAIToBot(AAIController* BotController)
{
	if (!BotController || RelicBehaviorTreeAsset.IsNull())
	{
		return;
	}

	int32 DisableRelicBotAI = 0;
	if (UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionInt(this, TEXT("DisableRelicBotAI"), DisableRelicBotAI)
		&& DisableRelicBotAI != 0)
	{
		UE_LOG(LogTemp, Log, TEXT("BwayBotCreation: DisableRelicBotAI=1 — skipping relic BT for %s"), *GetNameSafe(BotController));
		return;
	}

	UBehaviorTree* BehaviorTree = RelicBehaviorTreeAsset.LoadSynchronous();
	UBlackboardData* Blackboard = RelicBlackboardAsset.IsNull() ? nullptr : RelicBlackboardAsset.LoadSynchronous();

	if (ABwayRelicBotController* RelicBot = Cast<ABwayRelicBotController>(BotController))
	{
		RelicBot->ConfigureRelicAI(BehaviorTree, Blackboard);
		RelicBot->StartRelicBotLogic();
	}
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

		ApplyRelicAIToBot(BotController);
	}
}

void UBwayBotCreationComponent::StopAllBotLogic()
{
	if (!GetOwner()->HasAuthority())
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

		if (ABwayRelicBotController* RelicBot = Cast<ABwayRelicBotController>(BotController))
		{
			RelicBot->StopRelicBotLogic();
		}
		else if (UBrainComponent* Brain = BotController->GetBrainComponent())
		{
			if (Brain->IsRunning())
			{
				Brain->StopLogic(TEXT("MatchEnded"));
			}
		}
	}
}

void UBwayBotCreationComponent::ShutdownAllBotsForTravel()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	StopAllBotLogic();

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

		if (APawn* BotPawn = BotController->GetPawn())
		{
			BotController->UnPossess();
			BotPawn->Destroy();
		}

		BotController->Destroy();
	}

	SpawnedBotList.Empty();
}
