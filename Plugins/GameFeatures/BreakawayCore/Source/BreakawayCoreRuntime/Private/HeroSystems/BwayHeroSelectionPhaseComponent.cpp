#include "HeroSystems/BwayHeroSelectionPhaseComponent.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"
#include "HeroSystems/BwayHeroSelectWidget.h"
#include "BwayPlayerState.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "Player/LyraPlayerBotController.h"
#include "AIController.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

UBwayHeroSelectionPhaseComponent::UBwayHeroSelectionPhaseComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);

	// BreakawayCore tags may not be registered while the CDO is constructed.
	PhaseTag = FGameplayTag::RequestGameplayTag(FName("Breakaway.GamePhase.HeroSelection"), false);
	HeroSelectionWidgetClass = TSoftClassPtr<UCommonActivatableWidget>(
		FSoftObjectPath(TEXT("/BreakawayCore/UI/HeroSelect/W_HeroSelection.W_HeroSelection_C")));
}

void UBwayHeroSelectionPhaseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UBwayHeroSelectionPhaseComponent, bPostSelectionTravelFailed);
}

void UBwayHeroSelectionPhaseComponent::OnRep_PostSelectionTravelFailed()
{
	OnTravelFailureChanged.Broadcast();
}

void UBwayHeroSelectionPhaseComponent::SetPostSelectionTravelFailed(bool bFailed)
{
	if (bPostSelectionTravelFailed == bFailed) return;
	bPostSelectionTravelFailed = bFailed;
	GetOwner()->ForceNetUpdate();
	OnRep_PostSelectionTravelFailed();
}

bool UBwayHeroSelectionPhaseComponent::CanRetryPostSelectionTravel(const APlayerController* RequestingPlayer) const
{
	const UWorld* World = GetWorld();
	const auto* GS = Cast<ABwayGameState>(GetOwner());
	const auto* PS = IsValid(RequestingPlayer) ? RequestingPlayer->GetPlayerState<ABwayPlayerState>() : nullptr;
	return IsCanonicalInstance() && GetOwner()->HasAuthority() && bPhaseActive && bPostSelectionTravelFailed && !bTravelCommitted
		&& World && !World->bIsTearingDown && IsValid(RequestingPlayer) && RequestingPlayer->HasAuthority()
		&& RequestingPlayer->GetWorld() == World && RequestingPlayer->IsLocalController() && RequestingPlayer->GetLocalPlayer()
		&& GS && IsValid(PS) && GS->PlayerArray.Contains(PS) && !PS->IsInactive() && !PS->IsOnlyASpectator()
		&& ShouldTravelToPostHeroSelectionMap() && IsCurrentRosterReadyForTravel();
}

bool UBwayHeroSelectionPhaseComponent::RetryPostSelectionTravel(APlayerController* RequestingPlayer)
{
	if (!CanRetryPostSelectionTravel(RequestingPlayer) || LastExplicitRetryFrame == GFrameCounter) return false;
	// One click can reach both authored Lock and Confirm handlers. A failed
	// request must not turn those handlers into two travel attempts in one frame.
	LastExplicitRetryFrame = GFrameCounter;
	return TryTravelToPostHeroSelectionMap(/*bExplicitRetry=*/true);
}

void UBwayHeroSelectionPhaseComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!IsCanonicalInstance())
	{
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Ignoring duplicate %s; GameState owns the selection phase"), *GetName());
		return;
	}

	// Only server manages the phase
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// Listen for when Lyra's phase system starts the HeroSelection phase
	if (UWorld* World = GetWorld())
	{
		if (ULyraGamePhaseSubsystem* PhaseSubsystem = World->GetSubsystem<ULyraGamePhaseSubsystem>())
		{
			if (!PhaseTag.IsValid())
			{
				PhaseTag = FGameplayTag::RequestGameplayTag(FName("Breakaway.GamePhase.HeroSelection"), false);
			}

			if (!PhaseTag.IsValid())
			{
				UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: Breakaway.GamePhase.HeroSelection is not registered; cannot listen for hero selection phase"));
				return;
			}

			PhaseSubsystem->WhenPhaseStartsOrIsActive(
				PhaseTag,
				EPhaseTagMatchType::ExactMatch,
				FLyraGamePhaseTagDelegate::CreateUObject(this, &UBwayHeroSelectionPhaseComponent::HandleLyraPhaseActivated)
			);
			
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Listening for Lyra phase: %s"), *PhaseTag.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: Could not find LyraGamePhaseSubsystem - phase integration disabled"));
		}
	}



	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Initialized"));

	if (ShouldSkipHeroSelectionPhase())
	{
		bHeroSelectionCompleted = true;
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: SkipHeroSelection active — marking hero selection complete; phase progression deferred to RoundManagement"));
	}
	else if (ShouldTravelToPostHeroSelectionMap())
	{
		if (ULyraExperienceManagerComponent* ExperienceManager = GetOwner()->FindComponentByClass<ULyraExperienceManagerComponent>())
		{
			ExperienceManager->CallOrRegister_OnExperienceLoaded_LowPriority(
				FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::HandleExperienceLoaded));
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: HeroSelectStaging URL option found; waiting for experience load before starting hero selection"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: No LyraExperienceManagerComponent found; starting staging hero selection on next tick"));
			GetWorld()->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &ThisClass::StartHeroSelectionPhase));
		}
	}
	else if (UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayWithoutHeroSelectFlow(this)
		|| UBwayGameplayUrlLibrary::HasGameplayUrlOption(this, TEXT("ArenaIntro")))
	{
		if (ULyraExperienceManagerComponent* ExperienceManager = GetOwner()->FindComponentByClass<ULyraExperienceManagerComponent>())
		{
			ExperienceManager->CallOrRegister_OnExperienceLoaded_LowPriority(
				FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::HandleDevDirectPlayExperienceLoaded));
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Direct editor play detected; waiting for experience load before dev hero selection"));
		}
		else
		{
			GetWorld()->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateUObject(this, &ThisClass::StartDevDirectPlayHeroSelection));
		}
	}
}

void UBwayHeroSelectionPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// World teardown is not successful selection completion. Completing here can
	// spawn pawns or initiate ServerTravel while PIE/shutdown destroys the world.
	bPhaseActive = false;
	CancelPendingTravel();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RosterRecheckTimerHandle);
	if (UBwayHeroSelectionManager* Manager = GetSelectionManager())
	{
		Manager->OnAllPlayersReady.RemoveDynamic(this, &UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady);
	}
	Super::EndPlay(EndPlayReason);
}

// ============================================================
// Hero Selection Phase
// ============================================================

void UBwayHeroSelectionPhaseComponent::StartHeroSelectionPhase()
{
	if (!IsCanonicalInstance())
	{
		return;
	}

	if (!GetOwner()->HasAuthority())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: StartHeroSelectionPhase called on client"));
		return;
	}

	if (ShouldSkipHeroSelectionPhase())
	{
		bHeroSelectionCompleted = true;
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: SkipHeroSelection active — ignoring hero selection phase start; not calling EndPhaseAndProgressToNext (RoundManagement owns phase flow)"));
		return;
	}

	if (bPhaseActive || bTravelCommitted)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: Phase already active"));
		return;
	}

	bPhaseActive = true;
	bHeroSelectionCompleted = false;
	bTravelCommitted = false;
	LastExplicitRetryFrame = MAX_uint64;
	SetPostSelectionTravelFailed(false);
	CancelPendingTravel();

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Starting hero selection phase"));

	// Get selection manager
	UBwayHeroSelectionManager* Manager = GetSelectionManager();
	if (!Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: No selection manager found!"));
		EndHeroSelectionPhase(); // Fail gracefully
		return;
	}

	// IMPORTANT: Bind to the all-ready delegate FIRST, before any locking calls.
	// This ensures we receive the broadcast even if all players are pre-selected from Frontend.
	Manager->OnAllPlayersReady.AddUniqueDynamic(this, &UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady);

	// A fresh manager has no entries yet. Register returning PlayerStates before
	// resetting their carried locks, so the UI never sees a previous match's lock.
	if (ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			Manager->RegisterPlayer(PS);
		}
	}

	Manager->StartHeroSelection();

	// Now apply any pre-selections from the Frontend subsystem (after registration)
	if (ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
	{
		for (APlayerState* PS : GameState->PlayerArray)
		{
			if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS))
			{
				if (ABwayPlayerController* PC = Cast<ABwayPlayerController>(BwayPS->GetPlayerController()))
				{
					PC->Client_RequestPreSelectedHero();
				}
			}
		}
	}

	// Bots do not have a local hero-selection UI. When launched via ArenaIntro from the
	// Front-End Lobby, defer bot hero locking until all human players lock in (or timeout)
	// so human players have first choice of all heroes on W_HeroSelection.
	if (!UBwayGameplayUrlLibrary::HasGameplayUrlOption(this, TEXT("ArenaIntro")))
	{
		AssignRandomHeroes(/*bOnlyBots=*/true, /*bLockImmediately=*/true);
	}

	// Let Blueprint run cosmetic/audio/camera hooks, but always perform the C++
	// CommonUI push as well so overrides cannot swallow the client RPC.
	ShowHeroSelectionUI();
	if (!ShouldSuppressAutoHeroSelectUI())
	{
		PushHeroSelectionUIToPlayers();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Auto hero select UI suppressed for direct editor play"));
	}

	// Broadcast event
	OnHeroSelectionPhaseStarted.Broadcast();

	// Explicitly check if all players are already ready (e.g. all pre-selected from Frontend).
	// The delegate may have already fired during the lock calls above, but if not, check now.
	if (Manager->AreAllPlayersReady())
	{
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: All players pre-selected from Frontend — ending phase immediately"));
		HandleAllPlayersReady();
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Hero selection phase started"));
}

void UBwayHeroSelectionPhaseComponent::EndHeroSelectionPhase()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	if (!bPhaseActive)
	{
		return;
	}
	if (ShouldTravelToPostHeroSelectionMap())
	{
		// Keep admission, the UI model, and the spawn gate alive throughout the
		// replication settle window. Only an accepted travel completes this phase.
		SchedulePostHeroSelectionTravel();
		return;
	}

	bPhaseActive = false;
	bHeroSelectionCompleted = true;

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Ending hero selection phase"));

	// Get selection manager
	UBwayHeroSelectionManager* Manager = GetSelectionManager();
	if (Manager)
	{
		// Unbind from events
		Manager->OnAllPlayersReady.RemoveDynamic(this, &UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady);

		// Assign random/fallback heroes to anyone who didn't select
		AssignRandomHeroes(/*bOnlyBots=*/false, /*bLockImmediately=*/true);

		// End selection phase
		Manager->EndHeroSelection();
	}

	// Let Blueprint run cosmetic cleanup, then always remove the C++ CommonUI widget.
	HideHeroSelectionUI();
	PopHeroSelectionUIFromPlayers();

	if (!ShouldTravelToPostHeroSelectionMap())
	{
		if (const ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
		{
			if (UBwayRoundManagementComponent* RoundManagement = GameState->GetRoundManagement())
			{
				if (RoundManagement->IsOrchestratingMatchFlow())
				{
					UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: RM orchestrator active — deferring spawn to Warmup"));
				}
				else
				{
					SpawnHeroesForAllPlayers();
				}
			}
			else
			{
				SpawnHeroesForAllPlayers();
			}
		}
	}

	// Broadcast event
	OnHeroSelectionPhaseEnded.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Hero selection phase ended"));
}

void UBwayHeroSelectionPhaseComponent::SkipHeroSelection()
{
	if (!IsCanonicalInstance() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Skipping hero selection"));
	if (ShouldTravelToPostHeroSelectionMap())
	{
		// Explicit skip still resolves this roster, but it cannot bypass travel's
		// final admission check or leave a second pending timer behind.
		AssignRandomHeroes(/*bOnlyBots=*/false, /*bLockImmediately=*/true);
		CancelPendingTravel();
		TryTravelToPostHeroSelectionMap();
		return;
	}

	bPhaseActive = false;
	bHeroSelectionCompleted = true;

	// Assign random/fallback heroes to everyone
	AssignRandomHeroes(/*bOnlyBots=*/false, /*bLockImmediately=*/true);

	// Spawn heroes immediately
	if (!ShouldTravelToPostHeroSelectionMap())
	{
		if (const ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
		{
			if (UBwayRoundManagementComponent* RoundManagement = GameState->GetRoundManagement())
			{
				if (RoundManagement->IsOrchestratingMatchFlow())
				{
					UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: RM orchestrator active — deferring spawn to Warmup"));
				}
				else
				{
					SpawnHeroesForAllPlayers();
				}
			}
			else
			{
				SpawnHeroesForAllPlayers();
			}
		}
	}
	else
	{
		TryTravelToPostHeroSelectionMap();
	}
}

void UBwayHeroSelectionPhaseComponent::ShowHeroSelectionUI_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: ShowHeroSelectionUI base impl (override in Blueprint)"));
}

void UBwayHeroSelectionPhaseComponent::PushHeroSelectionUIToPlayers()
{
	if (ShouldSuppressAutoHeroSelectUI())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Pushing hero selection UI to players"));

	TSoftClassPtr<UCommonActivatableWidget> ResolvedWidgetClass = HeroSelectionWidgetClass;
	if (ResolvedWidgetClass.IsNull())
	{
		const FSoftClassPath WHeroSelectionPath(TEXT("/BreakawayCore/UI/HeroSelect/W_HeroSelection.W_HeroSelection_C"));
		if (UClass* LoadedWHeroSelect = WHeroSelectionPath.TryLoadClass<UCommonActivatableWidget>())
		{
			ResolvedWidgetClass = LoadedWHeroSelect;
		}
		else
		{
			ResolvedWidgetClass = UBwayHeroSelectWidget::StaticClass();
		}
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABwayPlayerController* PC = Cast<ABwayPlayerController>(It->Get()))
		{
			// Skip showing UI if this player already locked their selection (e.g. from Frontend)
			if (ABwayPlayerState* PS = PC->GetPlayerState<ABwayPlayerState>())
			{
				if (PS->IsHeroLocked())
				{
					continue;
				}
			}

			PC->Client_ShowHeroSelection(ResolvedWidgetClass);
			UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Sent hero select widget RPC to %s"), *PC->GetName());
		}
	}
}

void UBwayHeroSelectionPhaseComponent::HideHeroSelectionUI_Implementation()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: HideHeroSelectionUI base impl (override in Blueprint)"));
}

void UBwayHeroSelectionPhaseComponent::PopHeroSelectionUIFromPlayers()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Popping hero selection UI from players"));

	// Base C++ implementation: remove hero selection widgets and restore game input
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ABwayPlayerController* PC = Cast<ABwayPlayerController>(It->Get()))
		{
			PC->Client_HideHeroSelection();
		}
	}
}

void UBwayHeroSelectionPhaseComponent::HandleLyraPhaseActivated(const FGameplayTag& InPhaseTag)
{
	if (ShouldTravelToPostHeroSelectionMap())
	{
		UE_LOG(LogTemp, Verbose, TEXT("BwayHeroSelectionPhaseComponent: Ignoring Lyra hero-selection phase on staging map; experience load handles staging start"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: HeroSelection phase activated by Lyra phase system! (Tag: %s)"), *InPhaseTag.ToString());
	
	// Now start our hero selection logic
	StartHeroSelectionPhase();
}

void UBwayHeroSelectionPhaseComponent::HandleDevDirectPlayExperienceLoaded(const ULyraExperienceDefinition* Experience)
{
	StartDevDirectPlayHeroSelection();
}

void UBwayHeroSelectionPhaseComponent::StartDevDirectPlayHeroSelection()
{
	if (!GetOwner()->HasAuthority() || ShouldSkipHeroSelectionPhase() || ShouldTravelToPostHeroSelectionMap() || bPhaseActive || bHeroSelectionCompleted)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Starting direct editor-play hero selection (auto UI)"));
	StartHeroSelectionPhase();
}

void UBwayHeroSelectionPhaseComponent::HandleExperienceLoaded(const ULyraExperienceDefinition* Experience)
{
	if (!GetOwner()->HasAuthority() || bPhaseActive || bHeroSelectionCompleted || !ShouldTravelToPostHeroSelectionMap())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Experience loaded; starting staging hero selection"));
	StartHeroSelectionPhase();
}

void UBwayHeroSelectionPhaseComponent::EndPhaseAndProgressToNext()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	// In Lyra's phase system, starting a new sibling phase automatically ends the
	// current one (phases are nested by tag — sibling phases cancel each other).
	// So we start the next phase, which auto-cancels HeroSelection.
	TSubclassOf<ULyraGamePhaseAbility> PhaseAbilityClass = NextPhaseAbilityClass;
	if (!PhaseAbilityClass)
	{
		const TSoftClassPtr<ULyraGamePhaseAbility> FallbackPhaseAbilityClass(
			FSoftObjectPath(TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_Warmup.BW_Phase_Warmup_C")));
		PhaseAbilityClass = FallbackPhaseAbilityClass.LoadSynchronous();
		if (PhaseAbilityClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: NextPhaseAbilityClass is not set; using BW_Phase_Warmup fallback"));
		}
	}

	if (PhaseAbilityClass)
	{
		if (UWorld* World = GetWorld())
		{
			if (ULyraGamePhaseSubsystem* PhaseSubsystem = World->GetSubsystem<ULyraGamePhaseSubsystem>())
			{
				PhaseSubsystem->StartPhase(PhaseAbilityClass);
				UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Started next phase via %s"),
					*PhaseAbilityClass->GetName());
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: NextPhaseAbilityClass is not set and BW_Phase_Warmup fallback failed to load! Cannot progress beyond hero selection."));
	}
}

bool UBwayHeroSelectionPhaseComponent::ShouldTravelToPostHeroSelectionMap() const
{
	return UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this);
}

bool UBwayHeroSelectionPhaseComponent::IsCurrentRosterReadyForTravel() const
{
	const ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner());
	if (!GameState) return false;
	int32 CurrentPlayers = 0;
	for (const APlayerState* Player : GameState->PlayerArray)
	{
		if (!IsValid(Player) || Player->IsInactive() || Player->IsOnlyASpectator()) continue;
		const auto* PS = Cast<ABwayPlayerState>(Player);
		if (!PS || !PS->IsHeroLocked() || !UBwayHeroRegistry::GetHeroDataById(PS->GetSelectedHeroId())) return false;
		++CurrentPlayers;
	}
	return CurrentPlayers > 0;
}

void UBwayHeroSelectionPhaseComponent::CancelPendingTravel()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(PendingTravelTimerHandle);
}

void UBwayHeroSelectionPhaseComponent::HandleSelectionRosterChanged(APlayerState* AddedPlayer)
{
	UWorld* World = GetWorld();
	if (!IsCanonicalInstance() || !GetOwner()->HasAuthority() || !bPhaseActive || bTravelCommitted
		|| !World || World->bIsTearingDown || !ShouldTravelToPostHeroSelectionMap()) return;
	CancelPendingTravel();
	if (const auto* PS = Cast<ABwayPlayerState>(AddedPlayer); PS && !PS->IsInactive() && !PS->IsOnlyASpectator()
		&& (!PS->IsHeroLocked() || !UBwayHeroRegistry::GetHeroDataById(PS->GetSelectedHeroId())))
	{
		if (auto* Manager = GetSelectionManager()) Manager->RefreshSelectionDeadline();
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: late staging arrival keeps existing locks; selection window refreshed player=%d"), PS->GetPlayerId());
	}
	if (auto* Manager = GetSelectionManager())
	{
		Manager->ReconcileStagingBotSelections();
	}
	// Finish logout and replacement before evaluating the live roster. Multiple
	// notifications in the same frame share one owned callback.
	World->GetTimerManager().ClearTimer(RosterRecheckTimerHandle);
	RosterRecheckTimerHandle = World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::SchedulePostHeroSelectionTravel);
	if (bPostSelectionTravelFailed) OnTravelFailureChanged.Broadcast();
}

void UBwayHeroSelectionPhaseComponent::SchedulePostHeroSelectionTravel()
{
	UWorld* World = GetWorld();
	if (!GetOwner()->HasAuthority() || !bPhaseActive || bTravelCommitted || bPostSelectionTravelFailed || !World || World->bIsTearingDown
		|| !ShouldTravelToPostHeroSelectionMap()) return;
	if (!IsCurrentRosterReadyForTravel()) { CancelPendingTravel(); return; }
	if (World->GetTimerManager().TimerExists(PendingTravelTimerHandle)) return;
	World->GetTimerManager().SetTimer(PendingTravelTimerHandle, this, &ThisClass::CommitPendingTravel, 5.f, false);
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: delaying staging travel 5.0 seconds for remote selection replication"));
}

void UBwayHeroSelectionPhaseComponent::CommitPendingTravel()
{
	CancelPendingTravel();
	if (!bPhaseActive || bTravelCommitted) return;
	TryTravelToPostHeroSelectionMap();
}

bool UBwayHeroSelectionPhaseComponent::TryTravelToPostHeroSelectionMap(bool bExplicitRetry)
{
	if (!GetOwner()->HasAuthority() || bTravelCommitted || (bPostSelectionTravelFailed && !bExplicitRetry))
	{
		return false;
	}

	UWorld* World = GetWorld();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	if (!World || World->bIsTearingDown || !GameMode)
	{
		return false;
	}

	const FString& OptionsString = GameMode->OptionsString;
	if (!UGameplayStatics::HasOption(OptionsString, TEXT("HeroSelectStaging")))
	{
		return false;
	}
	if (!IsCurrentRosterReadyForTravel())
	{
		CancelPendingTravel();
		UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: staging travel deferred; current roster has unlocked or invalid heroes"));
		return false;
	}

	FString TargetMap = UGameplayStatics::ParseOption(OptionsString, TEXT("HeroSelectTargetMap"));
	if (TargetMap.IsEmpty() && PostHeroSelectionMapID.IsValid())
	{
		if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
		{
			TargetMap = AssetManager->GetPrimaryAssetPath(PostHeroSelectionMapID).GetLongPackageName();
		}
	}

	if (TargetMap.IsEmpty())
	{
		SetPostSelectionTravelFailed(true);
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: HeroSelectStaging is set but no HeroSelectTargetMap or PostHeroSelectionMapID was provided"));
		return false;
	}

	FString TargetExperience = UGameplayStatics::ParseOption(OptionsString, TEXT("HeroSelectTargetExperience"));
	if (TargetExperience.IsEmpty() && PostHeroSelectionExperienceID.IsValid())
	{
		TargetExperience = PostHeroSelectionExperienceID.PrimaryAssetName.ToString();
	}

	FString TravelURL = TargetMap;
	if (!TargetExperience.IsEmpty())
	{
		TravelURL += FString::Printf(TEXT("?Experience=%s"), *TargetExperience);
	}

	if (UGameplayStatics::HasOption(OptionsString, TEXT("listen")))
	{
		// Keep the bare listen marker. ProcessClientTravel strips '?listen' before
		// sending the URL to remote clients; '?listen=1' is not stripped and makes
		// a client derive listen-server mode during the destination travel.
		TravelURL += TEXT("?listen");
	}

	TravelURL += TEXT("?SkipHeroSelection=1");
	TravelURL += TEXT("?SeamlessTravel");
	UBwayGameplayUrlLibrary::AppendMatchRulesToTravelUrl(TravelURL, OptionsString);

	if (UBwayHeroSelectionManager* Manager = GetSelectionManager())
	{
		Manager->ReconcileStagingBotSelections();
		const FString Team0Bots = Manager->BuildStagingBotHeroesOptionForTeam(0);
		if (!Team0Bots.IsEmpty())
		{
			TravelURL += FString::Printf(TEXT("?StagingBotHeroes0=%s"), *Team0Bots);
		}
		const FString Team1Bots = Manager->BuildStagingBotHeroesOptionForTeam(1);
		if (!Team1Bots.IsEmpty())
		{
			TravelURL += FString::Printf(TEXT("?StagingBotHeroes1=%s"), *Team1Bots);
		}
	}

	// Mark the commit before crossing into engine travel callbacks. A failed
	// request leaves selection open, with no unchanged automatic travel retry.
	bTravelCommitted = true;
	CancelPendingTravel();
	if (!World->ServerTravel(TravelURL, /*bAbsolute=*/true, /*bShouldSkipGameNotify=*/false))
	{
		bTravelCommitted = false;
		SetPostSelectionTravelFailed(true);
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: staging travel request failed; selection remains open URL=%s"), *TravelURL);
		return false;
	}
	bPhaseActive = false;
	bHeroSelectionCompleted = true;
	SetPostSelectionTravelFailed(false);
	World->GetTimerManager().ClearTimer(RosterRecheckTimerHandle);
	if (auto* Manager = GetSelectionManager())
	{
		Manager->OnAllPlayersReady.RemoveDynamic(this, &ThisClass::HandleAllPlayersReady);
		Manager->EndHeroSelection();
	}
	HideHeroSelectionUI();
	PopHeroSelectionUIFromPlayers();
	OnHeroSelectionPhaseEnded.Broadcast();
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Hero selection complete; travelling to match URL %s"), *TravelURL);
	return true;
}

bool UBwayHeroSelectionPhaseComponent::ShouldSkipHeroSelectionPhase() const
{
	return UBwayHeroSelectionFlowLibrary::ShouldSkipHeroSelectionWorld(this);
}

void UBwayHeroSelectionPhaseComponent::HandleAllPlayersReady()
{
	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: All players ready - ending phase"));

	const bool bWillTravelToMatchMap = ShouldTravelToPostHeroSelectionMap();

	// End our phase logic (assigns defaults, hides UI, spawns heroes)
	EndHeroSelectionPhase();
	if (bWillTravelToMatchMap)
	{
		return;
	}

	if (const ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
	{
		if (UBwayRoundManagementComponent* RoundManagement = GameState->GetRoundManagement())
		{
			if (RoundManagement->IsOrchestratingMatchFlow())
			{
				RoundManagement->TryCompletePrematchAfterHeroLocks();
				return;
			}
		}
	}

	// Progress Lyra's phase to the next one (e.g. Warmup / Playing)
	EndPhaseAndProgressToNext();
}

// ============================================================
// Internal Helpers
// ============================================================

void UBwayHeroSelectionPhaseComponent::SpawnHeroesForAllPlayers()
{
	ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner());
	if (!GameState)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: No game state"));
		return;
	}

	// Spawn hero for each player
	for (APlayerState* PS : GameState->PlayerArray)
	{
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS))
		{
			SpawnHeroForPlayer(BwayPS);
		}
	}
}

void UBwayHeroSelectionPhaseComponent::SpawnHeroForPlayer_Implementation(ABwayPlayerState* PlayerState)
{
	if (!PlayerState)
	{
		return;
	}

	// Get selected hero
	FPrimaryAssetId HeroId = PlayerState->GetSelectedHeroId();
	if (!HeroId.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayHeroSelectionPhaseComponent: Player %s has no hero selected"), 
			*PlayerState->GetPlayerName());
		return;
	}

	// Load hero data
	UBwayHeroDataAsset* HeroData = UBwayHeroRegistry::GetHeroDataById(HeroId);
	if (!HeroData)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: Could not load hero data for %s"), 
			*HeroId.ToString());
		return;
	}

	// Get player controller
	APlayerController* PC = Cast<APlayerController>(PlayerState->GetOwner());
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayHeroSelectionPhaseComponent: No controller for player %s"), 
			*PlayerState->GetPlayerName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayHeroSelectionPhaseComponent: Spawning hero %s for player %s"), 
		*HeroData->DisplayName.ToString(), *PlayerState->GetPlayerName());

	// Trigger a respawn through game mode (which calls ApplyHeroDataToNewPawn)
	if (AGameModeBase* GameMode = GetWorld()->GetAuthGameMode())
	{
		GameMode->RestartPlayer(PC);
	}
}

void UBwayHeroSelectionPhaseComponent::AssignDefaultHeroes(bool bOnlyBots)
{
	AssignRandomHeroes(bOnlyBots, /*bLockImmediately=*/true);
}

void UBwayHeroSelectionPhaseComponent::AssignRandomHeroes(bool bOnlyBots, bool bLockImmediately)
{
	if (UBwayHeroSelectionManager* Manager = GetSelectionManager())
	{
		const FPrimaryAssetId DirectPieBotHero = UBwayHeroSelectionFlowLibrary::ResolveDirectPieBotHeroId(this);
		if (bOnlyBots && DirectPieBotHero.IsValid())
		{
			if (ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner()))
			{
				for (APlayerState* PS : GameState->PlayerArray)
				{
					ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PS);
					if (!BwayPS)
					{
						continue;
					}

					AController* Controller = Cast<AController>(BwayPS->GetOwner());
					if (!Controller || !Controller->IsA<AAIController>())
					{
						continue;
					}

					if (!BwayPS->GetSelectedHeroId().IsValid() || !BwayPS->IsHeroLocked())
					{
						if (!Manager->AssignHeroToPlayer(BwayPS, DirectPieBotHero, bLockImmediately))
						{
							Manager->AssignRandomHeroToPlayer(BwayPS, FPrimaryAssetId(), bLockImmediately);
						}
					}
				}
			}
			return;
		}

		Manager->AssignRandomHeroToPlayers(bOnlyBots, ResolveFallbackHeroId(), bLockImmediately);
	}
}

FPrimaryAssetId UBwayHeroSelectionPhaseComponent::ResolveFallbackHeroId() const
{
	if (!DefaultHeroData.IsNull())
	{
		if (UBwayHeroDataAsset* DefaultHero = DefaultHeroData.LoadSynchronous())
		{
			return DefaultHero->GetPrimaryAssetId();
		}
	}

	if (UBwayHeroRegistry* HeroRegistry = UBwayHeroRegistry::Get(this))
	{
		for (const TSoftObjectPtr<UBwayHeroDataAsset>& HeroSoftObject : HeroRegistry->GetAllHeroSoftObjects())
		{
			if (UBwayHeroDataAsset* CandidateHero = HeroRegistry->LoadHeroSync(HeroSoftObject))
			{
				const FPrimaryAssetId CandidateId = CandidateHero->GetPrimaryAssetId();
				if (CandidateId.IsValid())
				{
					return CandidateId;
				}
			}
		}
	}

	return FPrimaryAssetId();
}

bool UBwayHeroSelectionPhaseComponent::ShouldSuppressAutoHeroSelectUI() const
{
	return false;
}

bool UBwayHeroSelectionPhaseComponent::IsCanonicalInstance() const
{
	const ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner());
	return !GameState || GameState->HeroSelectionPhaseComponent == this;
}

UBwayHeroSelectionManager* UBwayHeroSelectionPhaseComponent::GetSelectionManager() const
{
	ABwayGameState* GameState = Cast<ABwayGameState>(GetOwner());
	if (!GameState)
	{
		return nullptr;
	}

	return GameState->FindComponentByClass<UBwayHeroSelectionManager>();
}
