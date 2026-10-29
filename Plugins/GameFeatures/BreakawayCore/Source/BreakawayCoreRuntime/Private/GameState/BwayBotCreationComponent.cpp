#include "GameState/BwayBotCreationComponent.h"

#include "BreakawayGameMode.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "GameModes/LyraGameMode.h"
#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameSession.h"
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

void UBwayBotCreationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(PendingHumanTravelTimer);
	bInitializedStagingBotHeroQueues = false;
	PendingStagingBotHeroes[0].Reset();
	PendingStagingBotHeroes[1].Reset();
	Super::EndPlay(EndPlayReason);
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

void UBwayBotCreationComponent::InitializeStagingBotHeroQueuesIfNeeded()
{
	if (bInitializedStagingBotHeroQueues)
	{
		return;
	}
	bInitializedStagingBotHeroQueues = true;
	PendingStagingBotHeroes[0].Reset();
	PendingStagingBotHeroes[1].Reset();

	const FName OptionKeys[2] = { TEXT("StagingBotHeroes0"), TEXT("StagingBotHeroes1") };
	for (int32 TeamIdx = 0; TeamIdx < 2; ++TeamIdx)
	{
		FString SerializedList;
		if (!UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionString(this, OptionKeys[TeamIdx], SerializedList)
			|| SerializedList.IsEmpty())
		{
			continue;
		}

		TArray<FString> Tokens;
		SerializedList.ParseIntoArray(Tokens, TEXT(","), true);
		for (const FString& Token : Tokens)
		{
			const FString Trimmed = Token.TrimStartAndEnd();
			if (Trimmed.IsEmpty())
			{
				continue;
			}
			const FPrimaryAssetId ParsedId = Trimmed.Contains(TEXT(":"))
				? FPrimaryAssetId::FromString(Trimmed)
				: FPrimaryAssetId(FPrimaryAssetType(TEXT("HeroDataAsset")), FName(*Trimmed));
			if (ParsedId.IsValid())
			{
				PendingStagingBotHeroes[TeamIdx].Add(ParsedId);
			}
		}
	}
}

FPrimaryAssetId UBwayBotCreationComponent::PopNextPreAssignedStagingBotHero(
	int32 TeamIndex,
	const UBwayHeroSelectionManager* SelectionManager)
{
	if (TeamIndex < 0 || TeamIndex > 1 || !SelectionManager)
	{
		return FPrimaryAssetId();
	}

	while (PendingStagingBotHeroes[TeamIndex].Num() > 0)
	{
		const FPrimaryAssetId CandidateId = PendingStagingBotHeroes[TeamIndex][0];
		PendingStagingBotHeroes[TeamIndex].RemoveAt(0);
		if (CandidateId.IsValid()
			&& UBwayHeroRegistry::GetHeroDataById(CandidateId) != nullptr
			&& SelectionManager->IsHeroAvailableForTeam(CandidateId, TeamIndex))
		{
			return CandidateId;
		}
	}
	return FPrimaryAssetId();
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

	// Travelling controllers exist before their new PlayerStates join this
	// GameState. Wait for those carried teams/heroes before filling bot slots.
	const ABwayGameState* GameState = GetOwner<ABwayGameState>();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (IsValid(PC) && !PC->IsActorBeingDestroyed() && (!PC->HasClientLoadedCurrentWorld()
			|| !PC->PlayerState || !GameState || GameState->GetPlayerTeam(PC->PlayerState) < 0))
		{
			if (!bWaitingForHumanTravel)
				UE_LOG(LogTemp, Log, TEXT("BwayBotCreation: Waiting for travelling human teams before assigning bot heroes"));
			bWaitingForHumanTravel = true;
			if (!GetWorld()->GetTimerManager().IsTimerActive(PendingHumanTravelTimer))
				GetWorld()->GetTimerManager().SetTimer(PendingHumanTravelTimer, this, &ThisClass::SpawnMissingBots, 0.25f, true);
			return;
		}
	}
	GetWorld()->GetTimerManager().ClearTimer(PendingHumanTravelTimer);
	if (bWaitingForHumanTravel)
		UE_LOG(LogTemp, Log, TEXT("BwayBotCreation: Human travel complete; assigning remaining team heroes"));
	bWaitingForHumanTravel = false;

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

	ALyraGameMode* GameMode = GetWorld()->GetAuthGameMode<ALyraGameMode>();
	if (NewController->PlayerState)
	{
		if (GameMode && GameMode->GameSession)
		{
			// AI controllers do not go through GameSession::RegisterPlayer. Share
			// its allocator so bots also have unique, replicated player identities.
			NewController->PlayerState->SetPlayerId(GameMode->GameSession->GetNextPlayerID());
		}
		NewController->PlayerState->SetPlayerName(FString::Printf(TEXT("Breakaway Bot %d"), SpawnedBotList.Num() + 1));
	}

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

			const bool bDeferBotHeroForHumanSelection =
				UBwayGameplayUrlLibrary::HasGameplayUrlOption(this, TEXT("ArenaIntro")) &&
				GameState && GameState->HeroSelectionPhaseComponent &&
				!GameState->HeroSelectionPhaseComponent->HasHeroSelectionCompleted();

			if (!bDeferBotHeroForHumanSelection && (!BotPlayerState->GetSelectedHeroId().IsValid() || !BotPlayerState->IsHeroLocked()))
			{
				const bool bLockImmediately = SelectionManager->IsSelectionActive()
					|| !UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayWithoutHeroSelectFlow(this);

				InitializeStagingBotHeroQueuesIfNeeded();
				const int32 BotTeamIndex = GameState ? GameState->GetPlayerTeam(BotPlayerState) : -1;
				const FPrimaryAssetId PreAssignedStagingHero = PopNextPreAssignedStagingBotHero(BotTeamIndex, SelectionManager);
				bool bAssignedFromStaging = false;
				if (PreAssignedStagingHero.IsValid())
				{
					bAssignedFromStaging = SelectionManager->AssignHeroToPlayer(BotPlayerState, PreAssignedStagingHero, bLockImmediately);
					if (bAssignedFromStaging)
					{
						UE_LOG(LogTemp, Log, TEXT("BwayBotCreation: Carried pre-assigned staging hero %s to %s (Team %d)"),
							*PreAssignedStagingHero.ToString(), *BotPlayerState->GetPlayerName(), BotTeamIndex);
					}
				}

				if (!bAssignedFromStaging)
				{
					const FPrimaryAssetId DirectPieBotHero = UBwayHeroSelectionFlowLibrary::ResolveDirectPieBotHeroId(this);
					if (DirectPieBotHero.IsValid())
					{
						// The editor preference cannot leave a bot unassigned when that
						// hero is already taken by a teammate, including after rematch.
						if (!SelectionManager->AssignHeroToPlayer(BotPlayerState, DirectPieBotHero, bLockImmediately))
						{
							SelectionManager->AssignRandomHeroToPlayer(BotPlayerState, FPrimaryAssetId(), bLockImmediately);
						}
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
		FString DifficultyStr;
		if (UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionString(this, TEXT("BotDifficulty"), DifficultyStr) && !DifficultyStr.IsEmpty())
		{
			RelicBot->SetBotDifficultyTier(FName(*DifficultyStr));
		}
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
	GetWorld()->GetTimerManager().ClearTimer(PendingHumanTravelTimer);
	bWaitingForHumanTravel = false;
	bInitializedStagingBotHeroQueues = false;
	PendingStagingBotHeroes[0].Reset();
	PendingStagingBotHeroes[1].Reset();
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
