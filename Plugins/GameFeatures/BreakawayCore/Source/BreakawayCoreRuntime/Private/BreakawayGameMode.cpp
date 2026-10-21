// Copyright Epic Games, Inc. All Rights Reserved.

#include "BreakawayGameMode.h"
#include "GameModes/BwayExperienceLibrary.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "BwayPlayerController.h"
#include "BwayCharacterWithAbilities.h"
#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"
#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameState/BwayBotCreationComponent.h"
#include "SpawnSystem/BwaySpawnPointManagerComponent.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "Relic/RelicActor.h"
#include "GameState/BwayMidfieldDividerComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GameplayTagsManager.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "Character/LyraPawnExtensionComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogBreakawayGame);

#if !UE_BUILD_SHIPPING
namespace BwayBarrageTransportStarts
{
// This opt-in fixture selects existing starts before the ordinary pawn spawn.
// It never teleports a pawn or changes the player's view to create a safe lane.
struct FReservation
{
	const TCHAR* Hero;
	FVector Location;
	ABwayPlayerState* Player = nullptr;
	APlayerStart* Start = nullptr;
	int32 Team = INDEX_NONE;
};

void Fail(AController* Controller, const FString& Reason)
{
	UE_LOG(LogTemp, Error, TEXT("BwayBarrageTransport: FAIL reason=deterministic-start-%s request=%d"),
		*Reason, Controller && Controller->PlayerState ? Controller->PlayerState->GetPlayerId() : INDEX_NONE);
}

bool Resolve(UWorld* World, ABwayGameState* GameState, AController* Controller,
	FName Team1Tag, FName Team2Tag, TArray<FReservation>& Reservations, int32& Selected, FString& Reason)
{
	Selected = INDEX_NONE;
	Reservations = {
		{ TEXT("Argus"), FVector(100., 3500., 362.) },
		{ TEXT("Rawlins"), FVector(-180., -3610., 362.) },
		{ TEXT("Alona"), FVector(100., 3650., 362.) },
		{ TEXT("Korryn"), FVector(190., -3610., 362.) }
	};
	auto Reject = [&Reason](const TCHAR* Error) { Reason = Error; return false; };
	if (!World || World->GetNetMode() != NM_ListenServer
		|| World->GetOutermost()->GetName() != TEXT("/BreakawayCore/Maps/L_BW_Dorado"))
		return Reject(TEXT("wrong-world-or-topology"));
	if (!GameState || GameState->PlayerArray.Num() != 4 || !Controller || !Controller->PlayerState)
		return Reject(TEXT("four-player-roster-required"));
	if (Controller->GetPawn()) return Reject(TEXT("controller-already-possessed"));
	TSet<int32> PlayerIds;
	int32 LocalControllers = 0;
	for (APlayerState* Entry : GameState->PlayerArray)
	{
		auto* Player = Cast<ABwayPlayerState>(Entry);
		auto* PC = Player ? Cast<APlayerController>(Player->GetOwner()) : nullptr;
		if (!Player || Player->IsABot() || !Player->IsHeroLocked() || !PC || PC->PlayerState != Player
			|| PC->GetWorld() != World || PlayerIds.Contains(Player->GetPlayerId()))
			return Reject(TEXT("invalid-human-controller-or-hero-lock"));
		PlayerIds.Add(Player->GetPlayerId());
		LocalControllers += PC->IsLocalController() ? 1 : 0;
		const int32 Role = Reservations.IndexOfByPredicate([Player](const FReservation& Item)
		{
			const FPrimaryAssetId Expected = UBwayHeroRegistry::ResolveHeroIdByName(Item.Hero);
			return Expected.IsValid() && Player->GetSelectedHeroId() == Expected;
		});
		if (Role == INDEX_NONE || Reservations[Role].Player) return Reject(TEXT("missing-or-duplicate-fixture-hero"));
		auto& Item = Reservations[Role];
		Item.Player = Player; Item.Team = GameState->GetPlayerTeam(Player);
		if (Item.Team != 0 && Item.Team != 1) return Reject(TEXT("invalid-fixture-team"));
		if (PC == Controller) Selected = Role;
		if (Role == 1 && PC->IsLocalController()) return Reject(TEXT("rawlins-must-be-remote"));
	}
	if (Selected == INDEX_NONE || LocalControllers != 1) return Reject(TEXT("listen-controller-roster-mismatch"));
	if (Reservations[0].Team != Reservations[2].Team || Reservations[1].Team != Reservations[3].Team
		|| Reservations[0].Team == Reservations[1].Team) return Reject(TEXT("fixture-team-pairing-mismatch"));
	TSet<APlayerStart*> ReservedStarts;
	for (auto& Item : Reservations)
	{
		const FName Tag = Item.Team == 0 ? Team1Tag : Team2Tag;
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			if (It->PlayerStartTag != Tag || !It->GetActorLocation().Equals(Item.Location, .25)) continue;
			if (Item.Start || It->GetActorRotation().ContainsNaN()) return Reject(TEXT("ambiguous-or-invalid-tagged-start"));
			Item.Start = *It;
		}
		if (!Item.Start || ReservedStarts.Contains(Item.Start)) return Reject(TEXT("missing-or-duplicate-tagged-start"));
		ReservedStarts.Add(Item.Start);
	}
	return true;
}

void LogReservation(const TCHAR* Event, int32 Request, const FReservation& Item, const TCHAR* Suffix = TEXT(""))
{
	const FVector Position = Item.Start->GetActorLocation();
	const FRotator Rotation = Item.Start->GetActorRotation();
	UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: %s request=%d hero=%s player=%d team=%d start=%s tag=%s x=%.6f y=%.6f z=%.6f pitch=%.6f yaw=%.6f roll=%.6f%s"),
		Event, Request, Item.Hero, Item.Player->GetPlayerId(), Item.Team, *Item.Start->GetPathName(),
		*Item.Start->PlayerStartTag.ToString(), Position.X, Position.Y, Position.Z, Rotation.Pitch, Rotation.Yaw, Rotation.Roll, Suffix);
}
}
#endif

ABreakawayGameMode::ABreakawayGameMode()
{
	// Set default game state class
	GameStateClass = ABwayGameState::StaticClass();
	PlayerStateClass = ABwayPlayerState::StaticClass();
	PlayerControllerClass = ABwayPlayerController::StaticClass();
}

void ABreakawayGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	UBwayExperienceLibrary::RegisterPackagedBreakawayExperiences();

	Super::InitGame(MapName, Options, ErrorMessage);

	UBwayGameplayUrlLibrary::AugmentGameModeOptionsString(this);

	UE_LOG(LogBreakawayGame, Log, TEXT("Initializing Breakaway Game Mode (OptionsString='%s')"), *OptionsString);
}

void ABreakawayGameMode::InitGameState()
{
	Super::InitGameState();

	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (BwayGS)
	{
		SpawnPointManager = BwayGS->FindComponentByClass<UBwaySpawnPointManagerComponent>();
		if (!SpawnPointManager)
		{
			SpawnPointManager = Cast<UBwaySpawnPointManagerComponent>(
				BwayGS->AddComponentByClass(UBwaySpawnPointManagerComponent::StaticClass(), false, FTransform::Identity, false));

			if (SpawnPointManager)
			{
				SpawnPointManager->RegisterComponent();
				UE_LOG(LogBreakawayGame, Log, TEXT("Spawn Point Manager added to Game State"));
			}
		}
		else
		{
			UE_LOG(LogBreakawayGame, Log, TEXT("Spawn Point Manager already present on Game State"));
		}

		UE_LOG(LogBreakawayGame, Log, TEXT("Breakaway Game State initialized"));
	}
}

void ABreakawayGameMode::GetSeamlessTravelActorList(bool bToTransition, TArray<AActor*>& ActorList)
{
	Super::GetSeamlessTravelActorList(bToTransition, ActorList);

	// Keep outgoing PlayerStates in ActorList so APlayerController::SeamlessTravelFrom(OldPC)
	// copies SelectedHeroId, bHeroLocked, PlayerNum, and GenericTeamId onto the fresh
	// destination PlayerState before destroying the outgoing instance. Quiesce replication on
	// the outgoing staging PlayerStates prior to NotifyActorIsTraveling so ServerReplicateActors
	// in the destination world never reopens an ActorChannel for a slower client's outgoing
	// PlayerState after client PostSeamlessTravelGarbageCollect marks its NetGUID read-only.
	UWorld* World = GetWorld();
	if (GameState && World)
	{
		int32 QuiescedCount = 0;
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (IsValid(PS))
			{
				PS->SetReplicates(false);
				World->RemoveNetworkActor(PS);
				++QuiescedCount;
			}
		}
		UE_LOG(LogBreakawayGame, Log,
			TEXT("GetSeamlessTravelActorList: Quiesced %d outgoing PlayerStates for seamless travel (bToTransition=%s)"),
			QuiescedCount, bToTransition ? TEXT("true") : TEXT("false"));
	}
}

void ABreakawayGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogBreakawayGame, Warning, TEXT("Breakaway GameMode Loaded"));

	RefreshGameplayUrlOptions();

	// Initialize spawn point tags
	InitializeSpawnPointTags();

	// Let level actors/components finish BeginPlay and register with the spawn
	// manager before spawning match-owned objects like the relic.
	GetWorldTimerManager().SetTimerForNextTick(this, &ABreakawayGameMode::SpawnInitialGameObjects);

	bGameInitialized = true;
}

void ABreakawayGameMode::RefreshGameplayUrlOptions()
{
	if (!HasAuthority())
	{
		return;
	}

	UBwayGameplayUrlLibrary::AugmentGameModeOptionsString(this);
}

void ABreakawayGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearPendingPlayerRestarts();
	if (auto* GS = GetBreakawayGameState(); GS && GS->HeroSelectionManager)
	{
		GS->HeroSelectionManager->OnPlayerHeroLocked.RemoveDynamic(this, &ThisClass::HandleLateJoinHeroLocked);
	}
	PlayersAwaitingLateJoinHeroSelection.Empty();
	PlayersShownLateJoinHeroSelection.Empty();
	PendingHeroSelectionCompletions.Empty();
	Super::EndPlay(EndPlayReason);
}

void ABreakawayGameMode::CancelPlayerRestart(AController* Controller)
{
	FTimerHandle Timer;
	if (PendingPlayerRestarts.RemoveAndCopyValue(Controller, Timer) && GetWorld())
	{
		GetWorldTimerManager().ClearTimer(Timer);
	}
}

void ABreakawayGameMode::ClearPendingPlayerRestarts()
{
	if (GetWorld())
	{
		for (auto& Entry : PendingPlayerRestarts) GetWorldTimerManager().ClearTimer(Entry.Value);
	}
	PendingPlayerRestarts.Empty();
}

void ABreakawayGameMode::QueuePlayerRestart(AController* Controller)
{
	ABwayGameState* GS = GetBreakawayGameState();
	if (!HasAuthority() || !GetWorld() || GetWorld()->bIsTearingDown || !IsValid(Controller)
		|| Controller->GetWorld() != GetWorld() || Controller->GetPawn() || !GS
		|| !GS->PlayerArray.Contains(Controller->PlayerState) || !ControllerCanRestart(Controller)
		|| !GetDefaultPawnClassForController(Controller) || PendingPlayerRestarts.Contains(Controller))
	{
		return;
	}
	const TWeakObjectPtr<AController> WeakController(Controller);
	FTimerHandle& Timer = PendingPlayerRestarts.Add(WeakController);
	GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
	{
		PendingPlayerRestarts.Remove(WeakController);
		AController* Pending = WeakController.Get();
		ABwayGameState* CurrentGS = GetBreakawayGameState();
		if (IsValid(Pending) && !Pending->GetPawn() && CurrentGS
			&& CurrentGS->PlayerArray.Contains(Pending->PlayerState) && ControllerCanRestart(Pending))
		{
			RestartPlayer(Pending);
		}
	}), 0.25f, false);
}

void ABreakawayGameMode::FailedToRestartPlayer(AController* NewPlayer)
{
	if (!IsValid(NewPlayer)) return;
	// Preserve the engine's failure notification, but own the retry for every
	// controller type; Lyra's helper does not handle modular relic bots.
	AGameModeBase::FailedToRestartPlayer(NewPlayer);
	QueuePlayerRestart(NewPlayer);
}

void ABreakawayGameMode::PostLogin(APlayerController* NewPlayer)
{
	RefreshGameplayUrlOptions();

	// Super::PostLogin can immediately call HandleStartingNewPlayer. Establish
	// identity and any individual spawn gate before that first restart attempt.
	if (NewPlayer)
	{
		AssignPlayerToTeam(NewPlayer);
		TryApplyHeroUrlOptionAtLogin(NewPlayer);
		PreparePlayerHeroSelection(NewPlayer);
	}

	Super::PostLogin(NewPlayer);

	if (!NewPlayer)
	{
		return;
	}

	if (ABwayGameState* GS = GetBreakawayGameState(); GS && GS->GetRoundManagement()
		&& GS->GetRoundManagement()->GetCurrentMatchPhase() == EBwayMatchPhase::PostMatch)
	{
		const bool bSentResults = GS->SendResultsToPlayer(Cast<ABwayPlayerController>(NewPlayer));
		UE_LOG(LogBreakawayGame, Log, TEXT("PostMatchArrival: player=%d pawn=%s resultsSent=%d"),
			NewPlayer->PlayerState ? NewPlayer->PlayerState->GetPlayerId() : INDEX_NONE,
			*GetNameSafe(NewPlayer->GetPawn()), bSentResults);
		return;
	}

	if (PlayersAwaitingLateJoinHeroSelection.Contains(NewPlayer))
	{
		ShowPendingHeroSelection(NewPlayer);
		return;
	}

	if (UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidGameMode(this))
	{
		UE_LOG(LogBreakawayGame, Log, TEXT("Player %s logged in with ForceHumanoid — using experience DefaultPawnData (no hero apply)"),
			*NewPlayer->GetName());
		return;
	}

	if (UBwayHeroSelectionFlowLibrary::ShouldSkipHeroSelectionGameMode(this))
	{
		ABwayPlayerState* PS = NewPlayer->GetPlayerState<ABwayPlayerState>();
		if (PS && PS->GetSelectedHeroId().IsValid() && PS->IsHeroLocked())
		{
			UE_LOG(LogBreakawayGame, Log, TEXT("Player %s logged in with SkipHeroSelection — locked hero %s will apply on spawn"),
				*NewPlayer->GetName(), *PS->GetSelectedHeroId().ToString());
		}
		else
		{
			UE_LOG(LogBreakawayGame, Log, TEXT("Player %s logged in with SkipHeroSelection — hero-select UI/phase skipped"),
				*NewPlayer->GetName());
		}
		return;
	}

	ABwayPlayerState* PS = NewPlayer->GetPlayerState<ABwayPlayerState>();
	if (PS && PS->GetSelectedHeroId().IsValid())
	{
		UE_LOG(LogBreakawayGame, Log, TEXT("Player %s logged in with hero %s"),
			*NewPlayer->GetName(), *PS->GetSelectedHeroId().ToString());
	}
	else
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("Player %s logged in without a selected hero"),
			*NewPlayer->GetName());
	}
}

void ABreakawayGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Also runs for seamless arrivals, which do not receive PostLogin.
	// Register the carried team before prematch can defer pawn spawning.
	AssignPlayerToTeam(NewPlayer);
	PreparePlayerHeroSelection(NewPlayer);
	if (PlayersAwaitingLateJoinHeroSelection.Contains(NewPlayer))
	{
		ShowPendingHeroSelection(NewPlayer);
	}
	else if (ABwayGameState* BwayGS = GetGameState<ABwayGameState>())
	{
		if (BwayGS->HeroSelectionManager && BwayGS->HeroSelectionManager->IsSelectionActive() && BwayGS->HeroSelectionPhaseComponent)
		{
			if (ABwayPlayerController* BwayPC = Cast<ABwayPlayerController>(NewPlayer))
			{
				BwayPC->Client_ShowHeroSelection(BwayGS->HeroSelectionPhaseComponent->HeroSelectionWidgetClass);
			}
		}
	}

	if (ShouldDeferPlayerRestartForMatchFlow(NewPlayer))
	{
		UE_LOG(LogBreakawayGame, Log, TEXT("HandleStartingNewPlayer: match phase blocks pawn spawning for %s"), *GetNameSafe(NewPlayer));
		return;
	}

	if (ShouldDeferPlayerRestartForHeroSelection(NewPlayer))
	{
		UE_LOG(LogBreakawayGame, Log, TEXT("HandleStartingNewPlayer: deferring spawn for %s until hero selection completes"), *GetNameSafe(NewPlayer));
		return;
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

void ABreakawayGameMode::Logout(AController* Exiting)
{
	CancelPlayerRestart(Exiting);
	PlayersAwaitingLateJoinHeroSelection.Remove(Exiting);
	PlayersShownLateJoinHeroSelection.Remove(Exiting);
	PendingHeroSelectionCompletions.Remove(Exiting);
	if (APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		ABwayGameState* BwayGS = GetBreakawayGameState();
		if (BwayGS && PC->PlayerState)
		{
			BwayGS->RemovePlayerFromTeam(PC->PlayerState);
		}
	}

	Super::Logout(Exiting);
}

bool ABreakawayGameMode::ControllerCanRestart(AController* Controller)
{
	if (ShouldDeferPlayerRestartForMatchFlow(Controller))
	{
		return false;
	}

	if (ShouldDeferPlayerRestartForHeroSelection(Controller))
	{
		return false;
	}

	return Super::ControllerCanRestart(Controller);
}

AActor* ABreakawayGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS || !Player || !Player->PlayerState)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	const int32 TeamIndex = BwayGS->GetPlayerTeam(Player->PlayerState);
	if (TeamIndex < 0)
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("ChoosePlayerStart: %s has no team assignment; using default spawn"), *GetNameSafe(Player));
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	FName SpawnTag = (TeamIndex == 0) ? Team1SpawnPointTag : Team2SpawnPointTag;
	TArray<AActor*> TeamSpawns = GetPlayerStartsWithTag(SpawnTag);

	if (TeamSpawns.Num() > 0)
	{
		// Preserve team boundaries while applying the engine's occupancy policy.
		// Randomly selecting an occupied start can strand a bot during round reset
		// even when another start on its own team is completely clear.
		UClass* PawnClass = GetDefaultPawnClassForController(Player);
		APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;
		TArray<AActor*> ClearStarts;
		TArray<AActor*> AdjustableStarts;
		for (AActor* Start : TeamSpawns)
		{
			FVector Location = Start->GetActorLocation();
			const FRotator Rotation = Start->GetActorRotation();
			if (!GetWorld()->EncroachingBlockingGeometry(PawnToFit, Location, Rotation))
			{
				ClearStarts.Add(Start);
			}
			else if (GetWorld()->FindTeleportSpot(PawnToFit, Location, Rotation))
			{
				AdjustableStarts.Add(Start);
			}
		}
		const TArray<AActor*>& Candidates = ClearStarts.IsEmpty() ? AdjustableStarts : ClearStarts;
		return Candidates.IsEmpty() ? nullptr : Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	}

	// Fall back to default behavior if no tagged spawns found
	UE_LOG(LogBreakawayGame, Warning, TEXT("No spawn points found with tag %s, using default spawn"), *SpawnTag.ToString());
	return Super::ChoosePlayerStart_Implementation(Player);
}

void ABreakawayGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!IsValid(NewPlayer) || NewPlayer->GetWorld() != GetWorld()) return;
	if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
	{
		// Seamless travel retains replaced remote controllers until client acknowledgment.
		// Match-flow restart iteration can still see them; CanRestartPlayer rejects them too.
		if (PC->PendingSwapConnection != nullptr)
		{
			CancelPlayerRestart(PC);
#if !UE_BUILD_SHIPPING
			if (FParse::Param(FCommandLine::Get(), TEXT("BwayBarrageTransportDeterministicStarts")))
			{
				UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: SpawnSkipped reason=obsolete-seamless-travel-controller controller=%s request=%d playerState=%s pawn=%s pendingSwap=1"),
					*GetPathNameSafe(PC), PC->PlayerState ? PC->PlayerState->GetPlayerId() : INDEX_NONE,
					*GetPathNameSafe(PC->PlayerState), *GetPathNameSafe(PC->GetPawn()));
			}
#endif
			return;
		}
		PreparePlayerHeroSelection(PC);
		ShowPendingHeroSelection(PC);
	}
	if (!IsExperienceLoaded())
	{
		UE_LOG(LogBreakawayGame, Log, TEXT("RestartPlayer: deferring spawn for %s until experience loads"), *GetNameSafe(NewPlayer));
		FTimerDelegate RestartDelegate;
		RestartDelegate.BindUObject(this, &ThisClass::RestartPlayer, NewPlayer);
		GetWorldTimerManager().SetTimerForNextTick(RestartDelegate);
		return;
	}

	if (ShouldDeferPlayerRestartForMatchFlow(NewPlayer))
	{
		UE_LOG(LogBreakawayGame, Log, TEXT("RestartPlayer: match phase blocks pawn spawning for %s"), *GetNameSafe(NewPlayer));
		return;
	}

	if (ShouldDeferPlayerRestartForHeroSelection(NewPlayer))
	{
		UE_LOG(LogBreakawayGame, Log, TEXT("RestartPlayer: deferring spawn for %s until hero selection completes"), *GetNameSafe(NewPlayer));
		return;
	}

	UE_LOG(LogBreakawayGame, Log, TEXT("RestartPlayer: BEGIN for %s"), *GetNameSafe(NewPlayer));

	// Check for selected hero before spawning
	if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
	{
		if (ABwayPlayerState* BwayPS = PC->GetPlayerState<ABwayPlayerState>())
		{
			FPrimaryAssetId HeroId = BwayPS->GetSelectedHeroId();
			UE_LOG(LogBreakawayGame, Log, TEXT("RestartPlayer: PlayerState has SelectedHeroId = %s"), 
				HeroId.IsValid() ? *HeroId.ToString() : TEXT("NONE"));
		}
	}

	// Only the explicitly opted-in transport fixture bypasses random start selection.
	// A null ChoosePlayerStart result is not fail-closed: engine FindPlayerStart can
	// substitute WorldSettings or a previous StartSpot, so validate before dispatch.
#if !UE_BUILD_SHIPPING
	TArray<BwayBarrageTransportStarts::FReservation> BarrageReservations;
	int32 BarrageSelected = INDEX_NONE;
	TWeakObjectPtr<APawn> BarrageSpawnedPawn;
	if (FParse::Param(FCommandLine::Get(), TEXT("BwayBarrageTransportDeterministicStarts")))
	{
		FString Reason;
		if (!BwayBarrageTransportStarts::Resolve(GetWorld(), GetBreakawayGameState(), NewPlayer,
			Team1SpawnPointTag, Team2SpawnPointTag, BarrageReservations, BarrageSelected, Reason))
		{
			CancelPlayerRestart(NewPlayer);
			BwayBarrageTransportStarts::Fail(NewPlayer, Reason);
			return;
		}
		const auto& Selected = BarrageReservations[BarrageSelected];
		const int32 Request = NewPlayer->PlayerState->GetPlayerId();
		for (const auto& Item : BarrageReservations)
			BwayBarrageTransportStarts::LogReservation(TEXT("SpawnReservation"), Request, Item);
		UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
		APawn* PawnToFit = PawnClass ? PawnClass->GetDefaultObject<APawn>() : nullptr;
		// Only the requested reservation must be empty. Previously spawned teammates
		// already occupy their own distinct reservations, independent of restart order.
		if (!PawnToFit || GetWorld()->EncroachingBlockingGeometry(PawnToFit,
			Selected.Start->GetActorLocation(), Selected.Start->GetActorRotation()))
		{
			CancelPlayerRestart(NewPlayer);
			BwayBarrageTransportStarts::Fail(NewPlayer, TEXT("reserved-start-not-clear"));
			return;
		}
		BwayBarrageTransportStarts::LogReservation(TEXT("SpawnSelected"), Request, Selected, TEXT(" clear=1 pawnBefore=0"));
		RestartPlayerAtPlayerStart(NewPlayer, Selected.Start);
		BarrageSpawnedPawn = NewPlayer->GetPawn();
		if (!BarrageSpawnedPawn.IsValid())
		{
			CancelPlayerRestart(NewPlayer);
			BwayBarrageTransportStarts::Fail(NewPlayer, TEXT("exact-start-spawn-failed"));
			return;
		}
	}
	else
#endif
	{
		Super::RestartPlayer(NewPlayer);
	}
	if (!NewPlayer->GetPawn())
	{
		// No available start can return without FailedToRestartPlayer being called.
		QueuePlayerRestart(NewPlayer);
		if (ABwayGameState* GS = GetBreakawayGameState())
		{
			GS->RecalculateTeamAliveCount(GS->GetPlayerTeam(NewPlayer->PlayerState));
		}
		return;
	}
	CancelPlayerRestart(NewPlayer);
	NewPlayer->ResetIgnoreInputFlags();

	if (APlayerController* PC = Cast<APlayerController>(NewPlayer))
	{
		if (APawn* RestartedPawn = PC->GetPawn())
		{
			// Remote clients: retry RPC when possession has not caught up yet.
			// Listen-server host: possession is already correct, but ClientRetryClientRestart
			// skips ClientRestart when GetPawn() == NewPawn — leaving input/mesh broken.
			if (PC->IsLocalPlayerController())
			{
				PC->ClientRestart(RestartedPawn);
			}
			else
			{
				PC->ClientRetryClientRestart(RestartedPawn);
			}

			if (ULyraPawnExtensionComponent* PawnExtComp = ULyraPawnExtensionComponent::FindPawnExtensionComponent(RestartedPawn))
			{
				PawnExtComp->CheckDefaultInitialization();
			}

			RestartedPawn->ForceNetUpdate();
		}
	}

	if (!UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidGameMode(this))
	{
		ApplyHeroDataToNewPawn(NewPlayer);
	}
	if (ABwayGameState* GS = GetBreakawayGameState())
	{
		// A delayed restart also completes initial and round-reset spawns, outside
		// the individual death timer. Recount after hero health is initialized.
		GS->OnPlayerRespawned(NewPlayer->PlayerState);
	}

#if !UE_BUILD_SHIPPING
	if (BarrageSelected != INDEX_NONE)
	{
		const auto& Selected = BarrageReservations[BarrageSelected];
		auto* PC = Cast<APlayerController>(NewPlayer);
		APawn* Pawn = NewPlayer->GetPawn();
		const bool SamePawn = PC && Pawn && Pawn == BarrageSpawnedPawn.Get()
			&& Pawn->GetController() == NewPlayer && Pawn->GetPlayerState() == Selected.Player;
		const FVector Position = Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector;
		const FRotator Rotation = Pawn ? Pawn->GetActorRotation() : FRotator::ZeroRotator;
		const FRotator Control = NewPlayer->GetControlRotation();
		FVector ViewLocation; FRotator View = FRotator::ZeroRotator;
		if (PC) PC->GetPlayerViewPoint(ViewLocation, View);
		const bool ExactXY = Pawn && !Position.ContainsNaN()
			&& FVector::DistSquaredXY(Position, Selected.Start->GetActorLocation()) <= 1.;
		UE_LOG(LogTemp, Display, TEXT("BwayBarrageTransport: SpawnActual request=%d hero=%s player=%d team=%d start=%s pawn=%s x=%.6f y=%.6f z=%.6f pawnPitch=%.6f pawnYaw=%.6f pawnRoll=%.6f controlPitch=%.6f controlYaw=%.6f controlRoll=%.6f viewPitch=%.6f viewYaw=%.6f viewRoll=%.6f exactXY=%d samePawn=%d"),
			Selected.Player->GetPlayerId(), Selected.Hero, Selected.Player->GetPlayerId(), Selected.Team,
			*Selected.Start->GetPathName(), *GetPathNameSafe(Pawn), Position.X, Position.Y, Position.Z,
			Rotation.Pitch, Rotation.Yaw, Rotation.Roll, Control.Pitch, Control.Yaw, Control.Roll,
			View.Pitch, View.Yaw, View.Roll, ExactXY, SamePawn);
		if (!SamePawn || !ExactXY || Rotation.ContainsNaN() || Control.ContainsNaN() || View.ContainsNaN())
		{
			BwayBarrageTransportStarts::Fail(NewPlayer, TEXT("post-spawn-readback-mismatch"));
			return;
		}
	}
#endif

	UE_LOG(LogBreakawayGame, Log, TEXT("RestartPlayer: END for %s"), *GetNameSafe(NewPlayer));
}

void ABreakawayGameMode::TryApplyHeroUrlOptionAtLogin(APlayerController* NewPlayer)
{
	if (!HasAuthority() || !NewPlayer || UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidGameMode(this))
	{
		return;
	}

	FString HeroName;
	if (!UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionString(this, TEXT("Hero"), HeroName))
	{
		return;
	}

	ABwayPlayerState* BwayPS = NewPlayer->GetPlayerState<ABwayPlayerState>();
	if (!BwayPS || BwayPS->IsHeroLocked())
	{
		return;
	}

	const FPrimaryAssetId HeroId = UBwayHeroRegistry::ResolveHeroIdByName(HeroName);
	if (!HeroId.IsValid())
	{
		UE_LOG(LogBreakawayGame, Error, TEXT("PostLogin: Hero= URL '%s' did not resolve to a HeroDataAsset"), *HeroName);
		return;
	}

	BwayPS->ServerSetSelectedHeroId_Implementation(HeroId);
	BwayPS->ServerLockHeroSelection_Implementation();

	UE_LOG(LogBreakawayGame, Log, TEXT("PostLogin: Applied Hero= URL '%s' -> %s (locked) for %s"),
		*HeroName, *HeroId.ToString(), *NewPlayer->GetName());
}

void ABreakawayGameMode::ApplyHeroDataToNewPawn(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	if (UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidGameMode(this))
	{
		return;
	}

	// Get the player state to check for selected hero
	ABwayPlayerState* BwayPS = Controller->GetPlayerState<ABwayPlayerState>();
	if (!BwayPS)
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("ApplyHeroDataToNewPawn: No BwayPlayerState for %s"), *Controller->GetName());
		return;
	}

	// Get the selected hero ID
	FPrimaryAssetId HeroId = BwayPS->GetSelectedHeroId();
	if (!HeroId.IsValid())
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("ApplyHeroDataToNewPawn: Player %s has no hero selected"), *Controller->GetName());
		return;
	}

	if (!BwayPS->IsHeroLocked())
	{
		UE_LOG(LogBreakawayGame, Verbose, TEXT("ApplyHeroDataToNewPawn: Player %s hero not locked yet — deferring apply"), *Controller->GetName());
		return;
	}

	// Load the hero data asset
	UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId);
	if (!HeroData)
	{
		UE_LOG(LogBreakawayGame, Error, TEXT("ApplyHeroDataToNewPawn: Could not load hero data for %s"), *HeroId.ToString());
		return;
	}

	// Get the spawned pawn
	APawn* Pawn = Controller->GetPawn();
	if (!Pawn)
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("ApplyHeroDataToNewPawn: No pawn for player %s after RestartPlayer"), *Controller->GetName());
		return;
	}

	// Cast to our character class
	ABwayCharacterWithAbilities* Character = Cast<ABwayCharacterWithAbilities>(Pawn);
	if (!Character)
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("ApplyHeroDataToNewPawn: Pawn is not ABwayCharacterWithAbilities (is %s)"), 
			*Pawn->GetClass()->GetName());
		return;
	}

	// Apply hero data to the character
	UE_LOG(LogBreakawayGame, Log, TEXT("ApplyHeroDataToNewPawn: Applying hero %s to character %s"), 
		*HeroData->DisplayName.ToString(), *Character->GetName());
	
	Character->InitializeHeroData(HeroData);
}

// ========================================
// Team Management
// ========================================

void ABreakawayGameMode::AssignPlayerToTeam(APlayerController* Player)
{
	if (Player)
	{
		if (ABwayGameState* GS = GetBreakawayGameState())
		{
			if (UBwayBotCreationComponent* Bots = GS->FindComponentByClass<UBwayBotCreationComponent>())
			{
				Bots->MakeRoomForHumanPlayer();
			}
		}
	}
	AssignControllerToTeam(Player);
}

void ABreakawayGameMode::AssignControllerToTeam(AController* Controller)
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS || !Controller || !Controller->PlayerState)
	{
		return;
	}

	if (BwayGS->GetPlayerTeam(Controller->PlayerState) >= 0)
	{
		return;
	}

	int32 TeamIndex = GetTeamWithFewerPlayers();
	if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Controller->PlayerState))
	{
		const uint8 CarriedTeamId = BwayPS->GetGenericTeamId().GetId();
		if (BwayPS->HasCarriedTeamAssignment() && (CarriedTeamId == 1 || CarriedTeamId == 2))
		{
			TeamIndex = CarriedTeamId - 1;
		}
		else if (const APlayerController* PC = Cast<APlayerController>(Controller); PC && PC->IsLocalController())
		{
			int32 UrlHostTeam = INDEX_NONE;
			if (UBwayGameplayUrlLibrary::TryGetGameplayUrlOptionInt(this, TEXT("HostTeam"), UrlHostTeam)
				&& (UrlHostTeam == 0 || UrlHostTeam == 1))
			{
				TeamIndex = UrlHostTeam;
			}
		}
	}
	BwayGS->AddPlayerToTeam(Controller->PlayerState, TeamIndex);

	UE_LOG(LogBreakawayGame, Log, TEXT("Assigned player %s to Team %d"),
		*Controller->GetName(), TeamIndex + 1);
}

int32 ABreakawayGameMode::GetTeamWithFewerPlayers() const
{
	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS)
	{
		return 0;
	}

	const FTeamInfo& Team1 = BwayGS->GetTeamInfo(0);
	const FTeamInfo& Team2 = BwayGS->GetTeamInfo(1);

	// Return team with fewer players, or Team 1 if equal
	return (Team1.TeamMembers.Num() <= Team2.TeamMembers.Num()) ? 0 : 1;
}

// ========================================
// Spawn System Integration
// ========================================

void ABreakawayGameMode::InitializeSpawnPointTags()
{
	// Initialize gameplay tags for spawn points
	Goal1SpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Goal.Team1"));
	Goal2SpawnTag = FGameplayTag::RequestGameplayTag(FName("SpawnPoint.Goal.Team2"));
	
	UE_LOG(LogBreakawayGame, Log, TEXT("Initialized spawn point tags"));
}

void ABreakawayGameMode::SpawnInitialGameObjects()
{
	if (bInitialGameObjectsSpawned)
	{
		return;
	}

	if (UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingGameMode(this))
	{
		UE_LOG(LogBreakawayGame, Log, TEXT("Skipping initial match object spawn on hero-select staging map"));
		bInitialGameObjectsSpawned = true;
		return;
	}

	++InitialGameObjectSpawnAttempts;

	if (!SpawnPointManager)
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("No spawn point manager - cannot spawn initial objects"));
		return;
	}

	ABwayGameState* BwayGS = GetBreakawayGameState();
	if (!BwayGS)
	{
		return;
	}

	if (UBwayRelicManagerComponent* RelicMgr = BwayGS->FindComponentByClass<UBwayRelicManagerComponent>())
	{
		if (!RelicMgr->RelicClass)
		{
			UE_LOG(LogBreakawayGame, Log, TEXT("RelicClass not configured; skipping initial relic spawn"));
		}
		else if (!RelicMgr->SpawnRelic())
		{
			if (InitialGameObjectSpawnAttempts < 5)
			{
				UE_LOG(LogBreakawayGame, Warning, TEXT("Relic manager could not spawn relic; initial objects will retry next tick"));
				GetWorldTimerManager().SetTimerForNextTick(this, &ABreakawayGameMode::SpawnInitialGameObjects);
			}
			else
			{
				UE_LOG(LogBreakawayGame, Error, TEXT("Relic manager could not spawn relic after %d attempts"), InitialGameObjectSpawnAttempts);
			}
			return;
		}
	}
	else
	{
		UE_LOG(LogBreakawayGame, Warning, TEXT("No relic manager found - relic was not spawned"));
	}

	// Spawn goals
	SpawnPointManager->SpawnObjectsAtPoints(Goal1SpawnTag);
	SpawnPointManager->SpawnObjectsAtPoints(Goal2SpawnTag);

	if (UBwayMidfieldDividerComponent* DividerComponent = BwayGS->MidfieldDividerComponent)
	{
		DividerComponent->SpawnMidfieldDivider();
	}
	
	bInitialGameObjectsSpawned = true;
	UE_LOG(LogBreakawayGame, Log, TEXT("Spawned initial game objects"));
}

// ========================================
// Helper Functions
// ========================================

ABwayGameState* ABreakawayGameMode::GetBreakawayGameState() const
{
	return GetGameState<ABwayGameState>();
}

bool ABreakawayGameMode::PreparePlayerHeroSelection(APlayerController* Player)
{
	if (!HasAuthority() || !IsValid(Player) || !GetWorld() || GetWorld()->bIsTearingDown
		|| Player->GetWorld() != GetWorld() || UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidGameMode(this)
		|| UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingGameMode(this)) return false;
	auto* GS = GetBreakawayGameState();
	auto* PS = Player->GetPlayerState<ABwayPlayerState>();
	const auto* Rounds = GS ? GS->GetRoundManagement() : nullptr;
	const EBwayMatchPhase Phase = Rounds ? Rounds->GetCurrentMatchPhase() : EBwayMatchPhase::None;
	if (!GS || !PS || PS->IsABot() || PS->IsInactive() || PS->IsOnlyASpectator() || Phase == EBwayMatchPhase::PostMatch) return false;
	const bool bValidHero = UBwayHeroRegistry::GetHeroDataById(PS->GetSelectedHeroId()) != nullptr;
	if (PS->IsHeroLocked() && !bValidHero) PS->UnlockHeroSelection();
	if (PlayersAwaitingLateJoinHeroSelection.Contains(Player)) return true;
	if (PS->IsHeroLocked() && bValidHero) return false;
	const bool bMatchAdmission = Phase == EBwayMatchPhase::Prematch || Phase == EBwayMatchPhase::Warmup
		|| Phase == EBwayMatchPhase::Playing || Phase == EBwayMatchPhase::PostRound
		|| UBwayHeroSelectionFlowLibrary::ShouldSkipHeroSelectionGameMode(this);
	// A shared phase already owns ordinary initial selection. SkipHeroSelection
	// skips that global phase, never an uncovered destination player's own choice.
	if (!bMatchAdmission || (GS->HeroSelectionManager && GS->HeroSelectionManager->IsSelectionActive())) return false;
	PlayersAwaitingLateJoinHeroSelection.Add(Player);
	if (GS->HeroSelectionManager)
	{
		GS->HeroSelectionManager->RegisterPlayer(PS);
		GS->HeroSelectionManager->OnPlayerHeroLocked.AddUniqueDynamic(this, &ThisClass::HandleLateJoinHeroLocked);
	}
	UE_LOG(LogBreakawayGame, Log, TEXT("LateJoinHeroSelection: admission gated player=%d phase=%s"), PS->GetPlayerId(), *UEnum::GetValueAsString(Phase));
	return true;
}

void ABreakawayGameMode::ShowPendingHeroSelection(APlayerController* Player)
{
	if (!HasAuthority() || !IsValid(Player) || !GetWorld() || GetWorld()->bIsTearingDown || Player->GetWorld() != GetWorld()
		|| !PlayersAwaitingLateJoinHeroSelection.Contains(Player) || PlayersShownLateJoinHeroSelection.Contains(Player)) return;
	auto* GS = GetBreakawayGameState();
	auto* PC = Cast<ABwayPlayerController>(Player);
	auto* PS = Player->GetPlayerState<ABwayPlayerState>();
	if (!GS || !IsValid(PS) || !GS->PlayerArray.Contains(PS) || !GS->HeroSelectionManager || !GS->HeroSelectionPhaseComponent || !PC
		|| GS->HeroSelectionPhaseComponent->HeroSelectionWidgetClass.IsNull()) return;
	// PostLogin and seamless startup may share this path. Push once for this
	// admission so a repeated lifecycle notification cannot reset the widget.
	PlayersShownLateJoinHeroSelection.Add(Player);
	PC->Client_ShowHeroSelection(GS->HeroSelectionPhaseComponent->HeroSelectionWidgetClass);
	UE_LOG(LogBreakawayGame, Log, TEXT("LateJoinHeroSelection: awaiting choice for player=%d pawn=%s"), PS->GetPlayerId(), *GetNameSafe(Player->GetPawn()));
}

bool ABreakawayGameMode::ShouldDeferPlayerRestartForHeroSelection(const AController* Controller) const
{
	if (!Controller)
	{
		return false;
	}

	if (PlayersAwaitingLateJoinHeroSelection.Contains(Controller))
	{
		return true;
	}

	if (UBwayHeroSelectionFlowLibrary::ShouldSkipHeroSelectionGameMode(this))
	{
		return false;
	}

	if (UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidGameMode(this))
	{
		return false;
	}

	const ABwayGameState* BwayGS = GetBreakawayGameState();
	const UBwayHeroSelectionPhaseComponent* HeroSelectionPhase = BwayGS ? BwayGS->HeroSelectionPhaseComponent : nullptr;
	return HeroSelectionPhase && HeroSelectionPhase->ShouldBlockPlayerSpawning();
}

void ABreakawayGameMode::HandleLateJoinHeroLocked(APlayerState* PlayerState, FPrimaryAssetId HeroId)
{
	ABwayPlayerState* PS = Cast<ABwayPlayerState>(PlayerState);
	ABwayPlayerController* PC = PS ? Cast<ABwayPlayerController>(PS->GetOwner()) : nullptr;
	if (!HasAuthority() || !PC || !PlayersAwaitingLateJoinHeroSelection.Contains(PC)
		|| PendingHeroSelectionCompletions.Contains(PC) || !PS->IsHeroLocked()
		|| PS->GetSelectedHeroId() != HeroId || !UBwayHeroRegistry::GetHeroDataById(HeroId))
	{
		return;
	}

	// The manager is iterating its selection array during this delegate. Defer
	// spawning until that update finishes, and do not retain a disconnected player.
	PendingHeroSelectionCompletions.Add(PC);
	const TWeakObjectPtr<ABwayPlayerController> WeakPC(PC);
	const TWeakObjectPtr<ABwayPlayerState> WeakPS(PS);
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, WeakPC, WeakPS, HeroId]()
	{
		ABwayPlayerController* Player = WeakPC.Get();
		if (!Player || !PendingHeroSelectionCompletions.Remove(Player) || !PlayersAwaitingLateJoinHeroSelection.Contains(Player)) return;
		ABwayGameState* GS = GetBreakawayGameState();
		ABwayPlayerState* CurrentPS = Player->GetPlayerState<ABwayPlayerState>();
		if (!GetWorld() || GetWorld()->bIsTearingDown || Player->GetWorld() != GetWorld() || !GS || !CurrentPS
			|| !GS->PlayerArray.Contains(CurrentPS) || CurrentPS != WeakPS.Get() || !CurrentPS->IsHeroLocked()
			|| CurrentPS->GetSelectedHeroId() != HeroId || !UBwayHeroRegistry::GetHeroDataById(HeroId)) return;
		PlayersAwaitingLateJoinHeroSelection.Remove(Player);
		PlayersShownLateJoinHeroSelection.Remove(Player);
		UBwayRoundManagementComponent* Rounds = GS->GetRoundManagement();
		if (Rounds && Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::Prematch)
		{
			Rounds->TryCompletePrematchAfterHeroLocks();
		}
		if (Rounds && Rounds->GetCurrentMatchPhase() != EBwayMatchPhase::PostMatch)
		{
			if (!Player->GetPawn() && !ShouldDeferPlayerRestartForMatchFlow(Player)) RestartPlayer(Player);
		}
		Player->Client_HideHeroSelection();
		UE_LOG(LogBreakawayGame, Log, TEXT("LateJoinHeroSelection: completed player=%d hero=%s pawn=%s"),
			Player->PlayerState->GetPlayerId(),
			*Player->GetPlayerState<ABwayPlayerState>()->GetSelectedHeroId().ToString(), *GetNameSafe(Player->GetPawn()));
	}));
}

bool ABreakawayGameMode::ShouldDeferPlayerRestartForMatchFlow(const AController* Controller) const
{
	if (!Controller)
	{
		return false;
	}

	const ABwayGameState* BwayGS = GetBreakawayGameState();
	const UBwayRoundManagementComponent* RoundManagement = BwayGS ? BwayGS->GetRoundManagement() : nullptr;
	return RoundManagement && RoundManagement->ShouldBlockPawnSpawning();
}

TArray<AActor*> ABreakawayGameMode::GetPlayerStartsWithTag(const FName& Tag) const
{
	TArray<AActor*> TaggedStarts;

	if (!GetWorld())
	{
		return TaggedStarts;
	}

	// Find all PlayerStart actors
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		APlayerStart* Start = *It;
		if (Start && Start->PlayerStartTag == Tag)
		{
			TaggedStarts.Add(Start);
		}
	}

	return TaggedStarts;
}
