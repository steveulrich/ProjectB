// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayRoundManagementComponent.h"
#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "HeroSystems/BwayHeroSelectionFlowLibrary.h"
#include "Stats/BwayMatchStatsLibrary.h"
#include "GameState/BwayBotCreationComponent.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "GameModes/LyraExperienceDefinition.h"
#include "GameState/BwayMidfieldRulesLibrary.h"
#include "GameState/BwayMidfieldDividerComponent.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "BwayGameState.h"
#include "BreakawayGameMode.h"
#include "BwayPlayerState.h"
#include "Relic/RelicActor.h"
#include "BwayCharacterWithAbilities.h"
#include "Character/LyraHealthComponent.h"
#include "GameModes/LyraGameMode.h"
#include "Buildable/BuildableBase.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayGameplayEffect_PassiveGold.h"
#include "GameState/BwayScoringComponent.h"
#include "SpawnSystem/BwaySpawnPointManagerComponent.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "GameModes/LyraGameState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "GameplayEffect.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "AIController.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRoundManagementComponent)

namespace BwayRoundManagement
{
#if !UE_BUILD_SHIPPING
TAutoConsoleVariable<int32> RespawnTimerTrace(TEXT("bway.Test.RespawnTimerTrace"), 0,
	TEXT("Development-only native respawn timer observations for an owned audit."));

void TraceRespawnTimer(UWorld* World, AController* Controller, const FTimerHandle& Timer, const TCHAR* Event)
{
	if (!RespawnTimerTrace.GetValueOnGameThread() || !World || !Controller) return;
	const auto* State = World->GetGameState<ABwayGameState>();
	const auto* Player = Cast<ABwayPlayerState>(Controller->PlayerState);
	if (!State || !Player) return;
	const auto& Manager = World->GetTimerManager();
	UE_LOG(LogTemp, Display, TEXT("BwayRespawnTimer: event=%s id=%d world=%s serverTime=%.6f wall=%.6f remaining=%.6f elapsed=%.6f rate=%.6f deadline=%.6f round=%d roundDeaths=%d"),
		Event, Player->GetPlayerId(), *World->GetPathName(), State->GetServerWorldTimeSeconds(), FPlatformTime::Seconds(),
		Manager.GetTimerRemaining(Timer), Manager.GetTimerElapsed(Timer), Manager.GetTimerRate(Timer),
		Player->GetRespawnSchedule().ServerEndTime, Player->GetRespawnSchedule().RoundNumber, Player->GetDeathsInCurrentRound());
}
#endif

bool IsHumanPlayerController(const AController* Controller)
{
	return Controller && Controller->IsA<APlayerController>() && !Controller->IsA<AAIController>();
}
}

UBwayRoundManagementComponent::UBwayRoundManagementComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);

	// Align with the existing BW_Phase_Playing phase ability tag (inherited from ShooterCore).
	// Override in component defaults if the project switches to a Breakaway-specific tag later.
	PlayingPhaseTag = FGameplayTag::RequestGameplayTag(FName("ShooterGame.GamePhase.Playing"), /*ErrorIfNotFound*/ false);
}

void UBwayRoundManagementComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBwayRoundManagementComponent, CurrentRoundState);
	DOREPLIFETIME(UBwayRoundManagementComponent, PointsToWin);
	DOREPLIFETIME(UBwayRoundManagementComponent, RoundDuration);
	DOREPLIFETIME(UBwayRoundManagementComponent, BaseRespawnDelaySeconds);
	DOREPLIFETIME(UBwayRoundManagementComponent, PerDeathRespawnIncrementSeconds);
	DOREPLIFETIME(UBwayRoundManagementComponent, MaxRespawnDelaySeconds);
	DOREPLIFETIME(UBwayRoundManagementComponent, bResetRespawnEscalationEachRound);
	DOREPLIFETIME(UBwayRoundManagementComponent, RoundStartTime);
	DOREPLIFETIME(UBwayRoundManagementComponent, CurrentRoundNumber);
	DOREPLIFETIME(UBwayRoundManagementComponent, CurrentMatchPhase);
	DOREPLIFETIME(UBwayRoundManagementComponent, ReplicatedPostRoundSummary);
	DOREPLIFETIME(UBwayRoundManagementComponent, bHasActivePostRoundSummary);
}

void UBwayRoundManagementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (!IsCanonicalRoundManagementInstance())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("BwayRoundManagement: Ignoring duplicate component '%s' on '%s' — orchestrator lives on canonical RoundManagementComponent"),
			*GetName(), *GetNameSafe(GetOwner()));
		return;
	}

	if (UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this))
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Hero staging owns selection; match rounds start after travel"));
		return;
	}

	if (AGameStateBase* GameState = GetGameStateChecked<AGameStateBase>())
	{
		if (ULyraExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<ULyraExperienceManagerComponent>())
		{
			ExperienceComponent->CallOrRegister_OnExperienceLoaded_HighPriority(
				FOnLyraExperienceLoaded::FDelegate::CreateUObject(this, &UBwayRoundManagementComponent::HandleExperienceLoadedForMatchRules));
		}
	}

	if (bAutoStartFirstRound)
	{
		GetWorld()->GetTimerManager().SetTimer(
			PreRoundTimerHandle, this, &UBwayRoundManagementComponent::StartRound, PreRoundDelay, false);

		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: First round will start in %.1f seconds (auto-start)"), PreRoundDelay);
	}
}

void UBwayRoundManagementComponent::RegisterPlayingPhaseListener()
{
	if (bPlayingPhaseListenerRegistered || bOrchestratorActive)
	{
		return;
	}

	if (!PlayingPhaseTag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: No PlayingPhaseTag set and bAutoStartFirstRound is false - round will not auto-start"));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (ULyraGamePhaseSubsystem* PhaseSubsystem = World->GetSubsystem<ULyraGamePhaseSubsystem>())
		{
			PhaseSubsystem->WhenPhaseStartsOrIsActive(
				PlayingPhaseTag,
				EPhaseTagMatchType::ExactMatch,
				FLyraGamePhaseTagDelegate::CreateUObject(this, &UBwayRoundManagementComponent::HandlePlayingPhaseActivated));

			bPlayingPhaseListenerRegistered = true;
			UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Legacy PlayingPhaseTag listener registered for %s"), *PlayingPhaseTag.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: LyraGamePhaseSubsystem not available - round will not auto-start"));
		}
	}
}

bool UBwayRoundManagementComponent::ShouldBlockPawnSpawning() const
{
	return bOrchestratorActive && (CurrentMatchPhase == EBwayMatchPhase::Prematch
		|| CurrentMatchPhase == EBwayMatchPhase::PostMatch);
}

void UBwayRoundManagementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearRespawnTimers();
	Super::EndPlay(EndPlayReason);
}

void UBwayRoundManagementComponent::SetMatchPhase(EBwayMatchPhase NewPhase)
{
	if (GetOwnerRole() != ROLE_Authority || CurrentMatchPhase == NewPhase)
	{
		return;
	}

	const EBwayMatchPhase PreviousPhase = CurrentMatchPhase;

	if (PreviousPhase == EBwayMatchPhase::Playing && NewPhase != EBwayMatchPhase::Playing)
	{
		ClearBetweenRoundTimer();
		ClearRespawnTimers();
	}

	CurrentMatchPhase = NewPhase;
	OnRep_MatchPhase();
}

void UBwayRoundManagementComponent::OnRep_MatchPhase()
{
	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Match phase -> %s"),
		*StaticEnum<EBwayMatchPhase>()->GetNameStringByValue(static_cast<int64>(CurrentMatchPhase)));

	OnMatchPhaseChanged.Broadcast(CurrentMatchPhase);

	if (CurrentMatchPhase != EBwayMatchPhase::PostRound)
	{
		ClearActivePostRoundSummary();
	}
}

TSubclassOf<ULyraGamePhaseAbility> UBwayRoundManagementComponent::ResolvePrematchPhaseAbilityClass() const
{
	if (ResolvedMatchFlowSettings.PrematchPhaseAbility)
	{
		return ResolvedMatchFlowSettings.PrematchPhaseAbility;
	}

	const TSoftClassPtr<ULyraGamePhaseAbility> FallbackPhaseAbilityClass(
		FSoftObjectPath(TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_Prematch.BW_Phase_Prematch_C")));
	return FallbackPhaseAbilityClass.LoadSynchronous();
}

TSubclassOf<ULyraGamePhaseAbility> UBwayRoundManagementComponent::ResolveWarmupPhaseAbilityClass() const
{
	if (ResolvedMatchFlowSettings.WarmupPhaseAbility)
	{
		return ResolvedMatchFlowSettings.WarmupPhaseAbility;
	}

	const TSoftClassPtr<ULyraGamePhaseAbility> FallbackPhaseAbilityClass(
		FSoftObjectPath(TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_Warmup.BW_Phase_Warmup_C")));
	return FallbackPhaseAbilityClass.LoadSynchronous();
}

TSubclassOf<ULyraGamePhaseAbility> UBwayRoundManagementComponent::ResolvePlayingPhaseAbilityClass() const
{
	if (ResolvedMatchFlowSettings.PlayingPhaseAbility)
	{
		return ResolvedMatchFlowSettings.PlayingPhaseAbility;
	}

	const TSoftClassPtr<ULyraGamePhaseAbility> FallbackPhaseAbilityClass(
		FSoftObjectPath(TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_Playing.BW_Phase_Playing_C")));
	return FallbackPhaseAbilityClass.LoadSynchronous();
}

TSubclassOf<ULyraGamePhaseAbility> UBwayRoundManagementComponent::ResolvePostRoundPhaseAbilityClass() const
{
	if (ResolvedMatchFlowSettings.PostRoundPhaseAbility)
	{
		return ResolvedMatchFlowSettings.PostRoundPhaseAbility;
	}

	const TSoftClassPtr<ULyraGamePhaseAbility> FallbackPhaseAbilityClass(
		FSoftObjectPath(TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_PostRound.BW_Phase_PostRound_C")));
	return FallbackPhaseAbilityClass.LoadSynchronous();
}

TSubclassOf<ULyraGamePhaseAbility> UBwayRoundManagementComponent::ResolvePostMatchPhaseAbilityClass() const
{
	if (ResolvedMatchFlowSettings.PostMatchPhaseAbility)
	{
		return ResolvedMatchFlowSettings.PostMatchPhaseAbility;
	}

	const TSoftClassPtr<ULyraGamePhaseAbility> FallbackPhaseAbilityClass(
		FSoftObjectPath(TEXT("/BreakawayCore/Experiences/Phases/BW_Phase_PostMatch.BW_Phase_PostMatch_C")));
	return FallbackPhaseAbilityClass.LoadSynchronous();
}

bool UBwayRoundManagementComponent::IsCanonicalRoundManagementInstance() const
{
	if (const ABwayGameState* BwayGS = GetBwayGameState())
	{
		return BwayGS->IsCanonicalRoundManagement(this);
	}

	TArray<UBwayRoundManagementComponent*> RoundManagers;
	GetOwner()->GetComponents<UBwayRoundManagementComponent>(RoundManagers);
	return RoundManagers.Num() <= 1 || (RoundManagers.Num() > 0 && RoundManagers[0] == this);
}

void UBwayRoundManagementComponent::EnterPrematch()
{
	if (GetOwnerRole() != ROLE_Authority || bOrchestratorActive)
	{
		return;
	}

	bOrchestratorActive = true;
	bPrematchCompletionHandled = false;

	SetMatchPhase(EBwayMatchPhase::Prematch);
	StartPrematchPhaseAbility();

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: EnterPrematch — orchestrator active; pawn spawn frozen until Warmup (11-4)"));
}

void UBwayRoundManagementComponent::StartPrematchPhaseAbility()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Defer one tick — high-priority OnExperienceLoaded can run before GameState ASC is ready to activate.
	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UBwayRoundManagementComponent::StartPrematchPhaseAbilityImpl));
}

void UBwayRoundManagementComponent::StartPrematchPhaseAbilityImpl()
{
	UWorld* World = GetWorld();
	ULyraGamePhaseSubsystem* PhaseSubsystem = World ? World->GetSubsystem<ULyraGamePhaseSubsystem>() : nullptr;
	if (!PhaseSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayRoundManagement: LyraGamePhaseSubsystem unavailable — prematch freeze active without GAS phase"));
	}

	if (PhaseSubsystem)
	{
		const TSubclassOf<ULyraGamePhaseAbility> PhaseAbilityClass = ResolvePrematchPhaseAbilityClass();
		if (!PhaseAbilityClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: No PrematchPhaseAbility configured — prematch freeze active without GAS phase (create BW_Phase_Prematch or set on DA_BW_MatchFlow_Dev)"));
		}
		else
		{
			if (const ULyraGamePhaseAbility* PhaseCDO = PhaseAbilityClass->GetDefaultObject<ULyraGamePhaseAbility>())
			{
				UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Prematch phase ability %s — GamePhaseTag=%s"),
					*GetNameSafe(PhaseAbilityClass), *PhaseCDO->GetGamePhaseTag().ToString());
			}

			if (const ALyraGameState* LyraGS = World->GetGameState<ALyraGameState>())
			{
				if (const ULyraAbilitySystemComponent* ASC = LyraGS->GetLyraAbilitySystemComponent())
				{
					if (!ASC->GetOwnerActor())
					{
						UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: GameState ASC owner not ready for prematch StartPhase"));
					}
				}
			}

			PhaseSubsystem->StartPhase(
				PhaseAbilityClass,
				FLyraGamePhaseDelegate::CreateUObject(this, &UBwayRoundManagementComponent::HandlePrematchPhaseEnded));

			UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Started prematch GAS phase via %s"), *GetNameSafe(PhaseAbilityClass));
		}
	}

	const float PrematchDuration = ResolvedMatchFlowSettings.PrematchDuration;
	if (PrematchDuration > 0.0f && World)
	{
		World->GetTimerManager().SetTimer(
			MatchPhaseTimerHandle,
			this,
			&UBwayRoundManagementComponent::HandlePrematchTimerExpired,
			PrematchDuration,
			false);

		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Prematch config timer started (%.1fs, authoritative)"), PrematchDuration);
	}
}

void UBwayRoundManagementComponent::HandlePrematchPhaseEnded(const ULyraGamePhaseAbility* PhaseAbility)
{
	if (!PhaseAbility)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: Prematch GAS phase failed to activate — waiting on config timer"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Prematch GAS phase ended early via %s — shortening prematch"),
		*GetNameSafe(PhaseAbility));
	CompletePrematchPhase();
}

void UBwayRoundManagementComponent::HandlePrematchTimerExpired()
{
	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Prematch config timer expired (authoritative)"));
	CompletePrematchPhase();
}

void UBwayRoundManagementComponent::CompletePrematchPhase()
{
	if (!bOrchestratorActive || CurrentMatchPhase != EBwayMatchPhase::Prematch || bPrematchCompletionHandled)
	{
		return;
	}

	if (ShouldWaitForDirectPieHeroLocks() && !AreAllHumanPlayersHeroLocked())
	{
		bWaitingForDirectPieHeroLock = true;
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Prematch completion deferred — waiting for all human players to lock heroes"));
		return;
	}

	bWaitingForDirectPieHeroLock = false;
	bPrematchCompletionHandled = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchPhaseTimerHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Prematch complete — advancing to Warmup (11-4)"));
	EnterWarmup();
}

void UBwayRoundManagementComponent::TryCompletePrematchAfterHeroLocks()
{
	if (!bOrchestratorActive || CurrentMatchPhase != EBwayMatchPhase::Prematch || bPrematchCompletionHandled)
	{
		return;
	}

	if (ShouldWaitForDirectPieHeroLocks() && AreAllHumanPlayersHeroLocked())
	{
		CompletePrematchPhase();
	}
}

bool UBwayRoundManagementComponent::ShouldWaitForDirectPieHeroLocks() const
{
	if (UBwayHeroSelectionFlowLibrary::ShouldForceHumanoidWorld(this))
	{
		return false;
	}

	if (UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this))
	{
		return false;
	}

	return UBwayHeroSelectionFlowLibrary::IsDirectEditorPlayMatchMap(this);
}

bool UBwayRoundManagementComponent::AreAllHumanPlayersHeroLocked() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	bool bFoundHuman = false;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!BwayRoundManagement::IsHumanPlayerController(PC))
		{
			continue;
		}

		bFoundHuman = true;
		const ABwayPlayerState* BwayPS = PC->GetPlayerState<ABwayPlayerState>();
		if (!BwayPS || !BwayPS->IsHeroLocked())
		{
			return false;
		}
	}

	return bFoundHuman;
}

void UBwayRoundManagementComponent::EnterWarmup()
{
	if (GetOwnerRole() != ROLE_Authority || !bOrchestratorActive || CurrentMatchPhase != EBwayMatchPhase::Prematch)
	{
		return;
	}

	bWarmupCompletionHandled = false;
	SetMatchPhase(EBwayMatchPhase::Warmup);
	StartWarmupPhaseAbility();
	RestartDeferredPlayersForWarmup();

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: EnterWarmup — pawn spawn unblocked; warmup runs once per match"));
}

void UBwayRoundManagementComponent::StartWarmupPhaseAbility()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UBwayRoundManagementComponent::StartWarmupPhaseAbilityImpl));
}

void UBwayRoundManagementComponent::StartWarmupPhaseAbilityImpl()
{
	UWorld* World = GetWorld();
	ULyraGamePhaseSubsystem* PhaseSubsystem = World ? World->GetSubsystem<ULyraGamePhaseSubsystem>() : nullptr;
	if (!PhaseSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayRoundManagement: LyraGamePhaseSubsystem unavailable — warmup active without GAS phase"));
	}

	if (PhaseSubsystem)
	{
		const TSubclassOf<ULyraGamePhaseAbility> PhaseAbilityClass = ResolveWarmupPhaseAbilityClass();
		if (!PhaseAbilityClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: No WarmupPhaseAbility configured — warmup active without GAS phase (set on DA_BW_MatchFlow_Dev)"));
		}
		else
		{
			if (const ULyraGamePhaseAbility* PhaseCDO = PhaseAbilityClass->GetDefaultObject<ULyraGamePhaseAbility>())
			{
				UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Warmup phase ability %s — GamePhaseTag=%s"),
					*GetNameSafe(PhaseAbilityClass), *PhaseCDO->GetGamePhaseTag().ToString());
			}

			PhaseSubsystem->StartPhase(
				PhaseAbilityClass,
				FLyraGamePhaseDelegate::CreateUObject(this, &UBwayRoundManagementComponent::HandleWarmupPhaseEnded));

			UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Started warmup GAS phase via %s"), *GetNameSafe(PhaseAbilityClass));
		}
	}

	const float WarmupDuration = ResolvedMatchFlowSettings.WarmupDuration;
	if (WarmupDuration > 0.0f && World)
	{
		World->GetTimerManager().SetTimer(
			MatchPhaseTimerHandle,
			this,
			&UBwayRoundManagementComponent::HandleWarmupTimerExpired,
			WarmupDuration,
			false);

		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Warmup config timer started (%.1fs, authoritative)"), WarmupDuration);
	}
}

void UBwayRoundManagementComponent::HandleWarmupPhaseEnded(const ULyraGamePhaseAbility* PhaseAbility)
{
	if (!PhaseAbility)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: Warmup GAS phase failed to activate — waiting on config timer"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Warmup GAS phase ended early via %s — shortening warmup"),
		*GetNameSafe(PhaseAbility));
	CompleteWarmupPhase();
}

void UBwayRoundManagementComponent::HandleWarmupTimerExpired()
{
	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Warmup config timer expired (authoritative)"));
	CompleteWarmupPhase();
}

void UBwayRoundManagementComponent::CompleteWarmupPhase()
{
	if (!bOrchestratorActive || CurrentMatchPhase != EBwayMatchPhase::Warmup || bWarmupCompletionHandled)
	{
		return;
	}

	bWarmupCompletionHandled = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchPhaseTimerHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Warmup complete — advancing to Playing (11-4)"));
	EnterPlaying();
}

void UBwayRoundManagementComponent::EnterPlaying()
{
	if (GetOwnerRole() != ROLE_Authority || !bOrchestratorActive)
	{
		return;
	}

	const bool bFirstEntryFromWarmup = CurrentMatchPhase == EBwayMatchPhase::Warmup;
	const bool bNextRoundFromPostRound = CurrentMatchPhase == EBwayMatchPhase::PostRound;

	if (!bFirstEntryFromWarmup && !bNextRoundFromPostRound)
	{
		return;
	}

	SetMatchPhase(EBwayMatchPhase::Playing);
	StartPlayingPhaseAbility();

	if (bFirstEntryFromWarmup)
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: EnterPlaying — round FSM active (PointsToWin=%d, RoundDuration=%.0fs); subsequent rounds use PostRound loop"),
			PointsToWin, RoundDuration);

		if (CurrentRoundState == ERoundState::WaitingToStart)
		{
			StartRound();
		}
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: EnterPlaying — starting round %d after PostRound (PointsToWin=%d)"),
			CurrentRoundNumber + 1, PointsToWin);
		StartRound();
	}
}

void UBwayRoundManagementComponent::StartPlayingPhaseAbility()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UBwayRoundManagementComponent::StartPlayingPhaseAbilityImpl));
}

void UBwayRoundManagementComponent::StartPlayingPhaseAbilityImpl()
{
	UWorld* World = GetWorld();
	ULyraGamePhaseSubsystem* PhaseSubsystem = World ? World->GetSubsystem<ULyraGamePhaseSubsystem>() : nullptr;
	if (!PhaseSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayRoundManagement: LyraGamePhaseSubsystem unavailable — playing phase active without GAS phase"));
		return;
	}

	const TSubclassOf<ULyraGamePhaseAbility> PhaseAbilityClass = ResolvePlayingPhaseAbilityClass();
	if (!PhaseAbilityClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: No PlayingPhaseAbility configured — playing phase active without GAS phase (set on DA_BW_MatchFlow_Dev)"));
		return;
	}

	PhaseSubsystem->StartPhase(
		PhaseAbilityClass,
		FLyraGamePhaseDelegate::CreateUObject(this, &UBwayRoundManagementComponent::HandlePlayingPhaseEnded));

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Started playing GAS phase via %s (round timer authoritative via RoundDuration=%.0fs)"),
		*GetNameSafe(PhaseAbilityClass), RoundDuration);
}

void UBwayRoundManagementComponent::HandlePlayingPhaseEnded(const ULyraGamePhaseAbility* PhaseAbility)
{
	if (!PhaseAbility)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: Playing GAS phase failed to activate — round FSM continues on config timer"));
		return;
	}

	if (CurrentMatchPhase == EBwayMatchPhase::Playing)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("BwayRoundManagement: Playing GAS phase ended early via %s while match phase still Playing — round FSM unchanged (11-5)"),
			*GetNameSafe(PhaseAbility));
	}
}

void UBwayRoundManagementComponent::EnterPostRound()
{
	if (GetOwnerRole() != ROLE_Authority || !bOrchestratorActive || CurrentMatchPhase != EBwayMatchPhase::Playing)
	{
		return;
	}

	bPostRoundCompletionHandled = false;
	SetMatchPhase(EBwayMatchPhase::PostRound);
	StartPostRoundPhaseAbility();

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: EnterPostRound — between-round pause (PostRoundDuration=%.1fs from config)"),
		ResolvedMatchFlowSettings.PostRoundDuration);
}

void UBwayRoundManagementComponent::StartPostRoundPhaseAbility()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UBwayRoundManagementComponent::StartPostRoundPhaseAbilityImpl));
}

void UBwayRoundManagementComponent::StartPostRoundPhaseAbilityImpl()
{
	UWorld* World = GetWorld();
	ULyraGamePhaseSubsystem* PhaseSubsystem = World ? World->GetSubsystem<ULyraGamePhaseSubsystem>() : nullptr;
	if (!PhaseSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayRoundManagement: LyraGamePhaseSubsystem unavailable — PostRound active without GAS phase"));
	}

	if (PhaseSubsystem)
	{
		const TSubclassOf<ULyraGamePhaseAbility> PhaseAbilityClass = ResolvePostRoundPhaseAbilityClass();
		if (!PhaseAbilityClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: No PostRoundPhaseAbility configured — PostRound active without GAS phase (set on DA_BW_MatchFlow_Dev)"));
		}
		else
		{
			if (const ULyraGamePhaseAbility* PhaseCDO = PhaseAbilityClass->GetDefaultObject<ULyraGamePhaseAbility>())
			{
				UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: PostRound phase ability %s — GamePhaseTag=%s"),
					*GetNameSafe(PhaseAbilityClass), *PhaseCDO->GetGamePhaseTag().ToString());
			}

			PhaseSubsystem->StartPhase(
				PhaseAbilityClass,
				FLyraGamePhaseDelegate::CreateUObject(this, &UBwayRoundManagementComponent::HandlePostRoundPhaseEnded));

			UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Started PostRound GAS phase via %s"), *GetNameSafe(PhaseAbilityClass));
		}
	}

	const float PostRoundDuration = ResolvedMatchFlowSettings.PostRoundDuration;
	if (PostRoundDuration > 0.0f && World)
	{
		World->GetTimerManager().SetTimer(
			MatchPhaseTimerHandle,
			this,
			&UBwayRoundManagementComponent::HandlePostRoundTimerExpired,
			PostRoundDuration,
			false);

		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: PostRound config timer started (%.1fs, authoritative)"), PostRoundDuration);
	}
	else if (World)
	{
		// Zero skips the pause without depending on a Blueprint delay or reentering StartPhase.
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UBwayRoundManagementComponent::HandlePostRoundTimerExpired));
	}
}

void UBwayRoundManagementComponent::HandlePostRoundPhaseEnded(const ULyraGamePhaseAbility* PhaseAbility)
{
	if (!PhaseAbility)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: PostRound GAS phase failed to activate — waiting on config timer"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: PostRound GAS phase ended early via %s — shortening PostRound"),
		*GetNameSafe(PhaseAbility));
	CompletePostRoundPhase();
}

void UBwayRoundManagementComponent::HandlePostRoundTimerExpired()
{
	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: PostRound config timer expired (authoritative)"));
	CompletePostRoundPhase();
}

void UBwayRoundManagementComponent::CompletePostRoundPhase()
{
	if (!bOrchestratorActive || CurrentMatchPhase != EBwayMatchPhase::PostRound || bPostRoundCompletionHandled)
	{
		return;
	}

	bPostRoundCompletionHandled = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchPhaseTimerHandle);
	}

	if (bPendingMatchEndAfterPostRound)
	{
		const int32 WinningTeam = PendingMatchWinningTeam;
		bPendingMatchEndAfterPostRound = false;
		PendingMatchWinningTeam = INDEX_NONE;

		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: PostRound complete — match ended, advancing to PostMatch"));
		OnMatchEnded.Broadcast(WinningTeam, CurrentRoundNumber);
		EnterPostMatch();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: PostRound complete — advancing to Playing for next round (11-6)"));
	EnterPlaying();
}

void UBwayRoundManagementComponent::StopRoundFSM()
{
	RemovePassiveGoldIncome();
	ClearRespawnTimers();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchPhaseTimerHandle);
		World->GetTimerManager().ClearTimer(BetweenRoundTimerHandle);
		World->GetTimerManager().ClearTimer(PreRoundTimerHandle);
	}

	if (ABwayGameState* BwayGS = GetBwayGameState())
	{
		if (UBwayMidfieldDividerComponent* DividerComponent = BwayGS->MidfieldDividerComponent)
		{
			DividerComponent->SetSuddenDeathDividerVisible(false);
		}
	}

	if (CurrentRoundState != ERoundState::RoundComplete)
	{
		SetRoundState(ERoundState::RoundComplete);
	}
}

void UBwayRoundManagementComponent::EnterPostMatch()
{
	if (GetOwnerRole() != ROLE_Authority || !bOrchestratorActive)
	{
		return;
	}

	if (CurrentMatchPhase != EBwayMatchPhase::Playing && CurrentMatchPhase != EBwayMatchPhase::PostRound)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("BwayRoundManagement: EnterPostMatch ignored — expected Playing or PostRound, got %s"),
			*StaticEnum<EBwayMatchPhase>()->GetNameStringByValue(static_cast<int64>(CurrentMatchPhase)));
		return;
	}

	StopRoundFSM();
	SetMatchPhase(EBwayMatchPhase::PostMatch);

	if (ABwayGameState* BwayGS = GetBwayGameState())
	{
		if (UBwayBotCreationComponent* BotCreation = BwayGS->FindComponentByClass<UBwayBotCreationComponent>())
		{
			BotCreation->StopAllBotLogic();
		}
	}

	StartPostMatchPhaseAbility();

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: EnterPostMatch — round FSM stopped; PostMatch phase active (11-7)"));
}

void UBwayRoundManagementComponent::StartPostMatchPhaseAbility()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &UBwayRoundManagementComponent::StartPostMatchPhaseAbilityImpl));
}

void UBwayRoundManagementComponent::StartPostMatchPhaseAbilityImpl()
{
	UWorld* World = GetWorld();
	ULyraGamePhaseSubsystem* PhaseSubsystem = World ? World->GetSubsystem<ULyraGamePhaseSubsystem>() : nullptr;
	if (!PhaseSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayRoundManagement: LyraGamePhaseSubsystem unavailable — PostMatch active without GAS phase"));
		return;
	}

	const TSubclassOf<ULyraGamePhaseAbility> PhaseAbilityClass = ResolvePostMatchPhaseAbilityClass();
	if (!PhaseAbilityClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: No PostMatchPhaseAbility configured — PostMatch active without GAS phase (set on DA_BW_MatchFlow_Dev)"));
		return;
	}

	if (const ULyraGamePhaseAbility* PhaseCDO = PhaseAbilityClass->GetDefaultObject<ULyraGamePhaseAbility>())
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: PostMatch phase ability %s — GamePhaseTag=%s"),
			*GetNameSafe(PhaseAbilityClass), *PhaseCDO->GetGamePhaseTag().ToString());
	}

	PhaseSubsystem->StartPhase(
		PhaseAbilityClass,
		FLyraGamePhaseDelegate::CreateUObject(this, &UBwayRoundManagementComponent::HandlePostMatchPhaseEnded));

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Started PostMatch GAS phase via %s"), *GetNameSafe(PhaseAbilityClass));
}

void UBwayRoundManagementComponent::HandlePostMatchPhaseEnded(const ULyraGamePhaseAbility* PhaseAbility)
{
	if (!PhaseAbility)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: PostMatch GAS phase failed to activate"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: PostMatch GAS phase ended via %s — awaiting Continue / ReturnToFrontEnd"),
		*GetNameSafe(PhaseAbility));
}

void UBwayRoundManagementComponent::RestartDeferredPlayersForWarmup()
{
	UWorld* World = GetWorld();
	AGameModeBase* GM = World ? World->GetAuthGameMode() : nullptr;
	if (!World || !GM)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (!PC->GetPawn())
			{
				GM->RestartPlayer(PC);
			}
		}
	}

	if (ABwayGameState* BwayGS = GetBwayGameState())
	{
		if (UBwayBotCreationComponent* BotCreation = BwayGS->FindComponentByClass<UBwayBotCreationComponent>())
		{
			BotCreation->EnsureBotsForRound();
		}
	}
}

void UBwayRoundManagementComponent::SetPointsToWin(int32 InPointsToWin)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	PointsToWin = FMath::Max(1, InPointsToWin);
}

void UBwayRoundManagementComponent::SetRoundDuration(float InRoundDuration)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	const float NewDuration = FMath::Max(1.0f, InRoundDuration);
	if (!FMath::IsNearlyEqual(RoundDuration, NewDuration))
	{
		RoundDuration = NewDuration;
	}
}

void UBwayRoundManagementComponent::SetRespawnProgressionRules(
	float InBaseRespawnDelaySeconds,
	float InPerDeathRespawnIncrementSeconds,
	float InMaxRespawnDelaySeconds,
	bool bInResetEachRound)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	BaseRespawnDelaySeconds = FMath::Max(0.05f, InBaseRespawnDelaySeconds);
	PerDeathRespawnIncrementSeconds = FMath::Max(0.0f, InPerDeathRespawnIncrementSeconds);
	MaxRespawnDelaySeconds = FMath::Max(BaseRespawnDelaySeconds, InMaxRespawnDelaySeconds);
	bResetRespawnEscalationEachRound = bInResetEachRound;
	RespawnDelay = BaseRespawnDelaySeconds;
}

void UBwayRoundManagementComponent::OnRep_RoundDuration()
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		return;
	}

	if (ABwayGameState* BwayGS = GetBwayGameState())
	{
		// One-shot client HUD refresh when authoritative duration arrives; periodic ticks stay authority-only.
		BwayGS->OnRoundTimeChanged.Broadcast(GetRoundTimeRemaining());
	}
}

bool UBwayRoundManagementComponent::IsRoundLifecycleActive() const
{
	if (UBwayHeroSelectionFlowLibrary::IsHeroSelectStagingWorld(this))
	{
		return false;
	}

	if (!bOrchestratorActive)
	{
		return true;
	}

	return CurrentMatchPhase == EBwayMatchPhase::Playing;
}

void UBwayRoundManagementComponent::ClearBetweenRoundTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BetweenRoundTimerHandle);
	}
}

void UBwayRoundManagementComponent::HandleExperienceLoadedForMatchRules(const ULyraExperienceDefinition* Experience)
{
	if (GetOwnerRole() != ROLE_Authority || !IsCanonicalRoundManagementInstance())
	{
		return;
	}

	if (bMatchFlowExperienceHandled)
	{
		UE_LOG(LogTemp, Verbose, TEXT("BwayRoundManagement: Match-flow experience hook already handled — skipping duplicate"));
		return;
	}

	bMatchFlowExperienceHandled = true;

	if (AGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr)
	{
		UBwayGameplayUrlLibrary::AugmentGameModeOptionsString(GameMode);
	}

	ResolvedMatchFlowSettings = UBwayMatchFlowLibrary::ResolveMatchFlowSettings(this, nullptr, Experience);
	UBwayMatchFlowLibrary::LogResolvedMatchFlowSettings(ResolvedMatchFlowSettings);

	// Apply PointsToWin, RoundDuration, and RespawnProgressionRules immediately on high-priority experience load.
	UBwayMatchFlowLibrary::ApplyMatchRulesOnly(this, ResolvedMatchFlowSettings, this, nullptr);

	if (ResolvedMatchFlowSettings.bOrchestrateMatchFlow)
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: bOrchestrateMatchFlow=true — RM owns StartPhase; legacy PlayingPhaseTag listener disabled"));
		EnterPrematch();
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: bOrchestrateMatchFlow=false — using legacy PlayingPhaseTag listener"));
		RegisterPlayingPhaseListener();
	}
}

void UBwayRoundManagementComponent::HandlePlayingPhaseActivated(const FGameplayTag& ActivePhaseTag)
{
	if (GetOwnerRole() != ROLE_Authority || bOrchestratorActive)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Playing phase activated (%s) - starting first round"), *ActivePhaseTag.ToString());

	// Only start the first round this way. Subsequent rounds are scheduled internally by EndRound().
	if (CurrentRoundState == ERoundState::WaitingToStart)
	{
		StartRound();
	}
}

void UBwayRoundManagementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (GetOwnerRole() == ROLE_Authority
		&& bOrchestratorActive
		&& CurrentMatchPhase == EBwayMatchPhase::Prematch
		&& bWaitingForDirectPieHeroLock
		&& !bPrematchCompletionHandled)
	{
		TryCompletePrematchAfterHeroLocks();
	}

	if (GetOwnerRole() != ROLE_Authority || !IsRoundLifecycleActive() || CurrentRoundState != ERoundState::RoundActive)
	{
		return;
	}

	// Broadcast round time updates every second
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	if (CurrentTime - LastRoundTimeUpdateBroadcast >= 1.0f)
	{
		LastRoundTimeUpdateBroadcast = CurrentTime;
		const int32 RemainingSeconds = GetRoundTimeRemaining();

		// Broadcast to GameState delegates for backward compatibility
		if (ABwayGameState* BwayGS = GetBwayGameState())
		{
			BwayGS->OnRoundTimeChanged.Broadcast(RemainingSeconds);
		}

		if (!bSuddenDeathWarningBroadcastThisRound
			&& SuddenDeathWarningSeconds > 0
			&& RemainingSeconds > 0
			&& RemainingSeconds <= SuddenDeathWarningSeconds)
		{
			bSuddenDeathWarningBroadcastThisRound = true;
			OnSuddenDeathWarning.Broadcast(RemainingSeconds);
			UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Sudden death warning — %d seconds remaining"), RemainingSeconds);

			if (ABwayGameState* BwayGS = GetBwayGameState())
			{
				if (UBwayMidfieldDividerComponent* DividerComponent = BwayGS->MidfieldDividerComponent)
				{
					DividerComponent->SetSuddenDeathDividerVisible(true);
				}
			}
		}
	}

	// Check if round time has expired
	if (GetRoundTimeRemaining() <= 0)
	{
		OnRoundTimerExpired();
	}
}

ABwayGameState* UBwayRoundManagementComponent::GetBwayGameState() const
{
	return Cast<ABwayGameState>(GetOwner());
}

// ========================================
// Round State Management
// ========================================

void UBwayRoundManagementComponent::SetRoundState(ERoundState NewState)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (CurrentRoundState != NewState)
	{
		CurrentRoundState = NewState;
		if (NewState != ERoundState::RoundActive)
		{
			ClearRespawnTimers();
		}

		// Handle state transitions
		switch (NewState)
		{
		case ERoundState::RoundActive:
			RoundStartTime = GetWorld()->GetTimeSeconds();
			CurrentRoundNumber++;
			UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Round %d started"), CurrentRoundNumber);
			break;

		case ERoundState::RoundEnding:
			UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Round ending..."));
			break;

		case ERoundState::RoundComplete:
			UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Round complete"));
			break;

		default:
			break;
		}

		// Broadcast state change (fires OnRep on clients, direct call on server)
		OnRep_RoundState();

		// Also broadcast to GameState delegates for backward compatibility
		if (ABwayGameState* BwayGS = GetBwayGameState())
		{
			BwayGS->OnRoundStateChanged.Broadcast(
				FName(*StaticEnum<ERoundState>()->GetNameStringByValue((int64)CurrentRoundState)));
		}
	}
}

void UBwayRoundManagementComponent::OnRep_RoundState()
{
	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Round state replicated to: %s"),
		*StaticEnum<ERoundState>()->GetNameStringByValue((int64)CurrentRoundState));

	OnRoundStateChanged.Broadcast(
		FName(*StaticEnum<ERoundState>()->GetNameStringByValue((int64)CurrentRoundState)));
}

int32 UBwayRoundManagementComponent::GetRoundTimeRemaining() const
{
	if (CurrentRoundState != ERoundState::RoundActive)
	{
		return 0;
	}

	const AGameStateBase* GameState = GetOwner<AGameStateBase>();
	const double CurrentServerTime = GameState
		? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
	const double ElapsedTime = CurrentServerTime - RoundStartTime;
	const double RemainingTime = FMath::Max(0.0, RoundDuration - ElapsedTime);
	return FMath::CeilToInt(RemainingTime);
}

// ========================================
// Round Lifecycle
// ========================================

void UBwayRoundManagementComponent::StartRound()
{
	if (!IsRoundLifecycleActive())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("BwayRoundManagement: StartRound ignored — orchestrator phase is %s (expected Playing)"),
			*StaticEnum<EBwayMatchPhase>()->GetNameStringByValue(static_cast<int64>(CurrentMatchPhase)));
		return;
	}

	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayRoundManagement: Cannot start round - GameState is null"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Starting Round %d"), CurrentRoundNumber + 1);

	bSuddenDeathWarningBroadcastThisRound = false;

	if (UBwayMidfieldDividerComponent* DividerComponent = BwayGS->MidfieldDividerComponent)
	{
		DividerComponent->SetSuddenDeathDividerVisible(false);
	}

	ResetRoundState();
	SetRoundState(ERoundState::RoundActive);

	RemovePassiveGoldIncome();
	for (APlayerState* PlayerState : BwayGS->PlayerArray)
	{
		if (FActiveGameplayEffectHandle Handle = ApplyPassiveGoldIncomeToPlayerState(PlayerState); Handle.IsValid())
		{
			PassiveGoldEffectHandles.Add(PlayerState, Handle);
		}
	}

	OnRoundStarted.Broadcast(CurrentRoundNumber, RoundDuration);

	BeginRoundStatTrackingForAllPlayers();
}

void UBwayRoundManagementComponent::StartNewRound()
{
	if (GetOwnerRole() == ROLE_Authority && CurrentRoundState == ERoundState::RoundActive)
	{
		CurrentRoundState = ERoundState::RoundComplete;
	}
	StartRound();
}

void UBwayRoundManagementComponent::BeginRoundStatTrackingForAllPlayers()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (ABwayGameState* BwayGS = GetBwayGameState())
	{
		for (APlayerState* PlayerState : BwayGS->PlayerArray)
		{
			if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState))
			{
				BwayPS->BeginRoundStatTracking();
			}
		}
	}
}

void UBwayRoundManagementComponent::FinalizeRoundStatsForAllPlayers()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (ABwayGameState* BwayGS = GetBwayGameState())
	{
		for (APlayerState* PlayerState : BwayGS->PlayerArray)
		{
			if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState))
			{
				BwayPS->FinalizeRoundStats();
			}
		}
	}
}

void UBwayRoundManagementComponent::BuildAndBroadcastPostRoundSummary(
	const int32 RoundWinningTeam,
	const float DisplayDurationSeconds)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	ReplicatedPostRoundSummary = UBwayMatchStatsLibrary::BuildPostRoundSummaryData(
		this, CurrentRoundNumber, RoundWinningTeam, DisplayDurationSeconds);
	bHasActivePostRoundSummary = true;

	NotifyPostRoundSummaryListeners();

	OnBetweenRoundPlanningStarted.Broadcast(CurrentRoundNumber, DisplayDurationSeconds);
}

void UBwayRoundManagementComponent::ClearActivePostRoundSummary()
{
	if (!bHasActivePostRoundSummary)
	{
		return;
	}

	bHasActivePostRoundSummary = false;
	ReplicatedPostRoundSummary = FBwayPostRoundSummaryData();
}

void UBwayRoundManagementComponent::NotifyPostRoundSummaryListeners()
{
	if (!bHasActivePostRoundSummary)
	{
		return;
	}

	OnPostRoundSummaryStarted.Broadcast(ReplicatedPostRoundSummary);
}

void UBwayRoundManagementComponent::OnRep_PostRoundSummary()
{
	if (bHasActivePostRoundSummary)
	{
		NotifyPostRoundSummaryListeners();
	}
}

void UBwayRoundManagementComponent::EndRound(int32 WinningTeam, EBwayWinCondition WinCondition)
{
	if (!IsRoundLifecycleActive())
	{
		UE_LOG(LogTemp, Verbose, TEXT("BwayRoundManagement: EndRound ignored — not in Playing match phase"));
		return;
	}

	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS)
	{
		return;
	}

	if (UBwayMidfieldDividerComponent* DividerComponent = BwayGS->MidfieldDividerComponent)
	{
		DividerComponent->SetSuddenDeathDividerVisible(false);
	}

	RemovePassiveGoldIncome();

	SetRoundState(ERoundState::RoundEnding);

	// Award points via ScoringComponent
	if (UBwayScoringComponent* Scoring = GetOwner()->FindComponentByClass<UBwayScoringComponent>())
	{
		Scoring->AddScore(WinningTeam, 1);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BwayRoundManagement: No ScoringComponent found! Cannot award score."));
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Round ended! Team %d wins (round=%d condition=%d)"),
		WinningTeam + 1, CurrentRoundNumber, int32(WinCondition));

	OnRoundEnded.Broadcast(WinningTeam, WinCondition, CurrentRoundNumber);

	const bool bMatchEnding = CheckMatchEnd();
	if (bMatchEnding)
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Match Over! Team %d wins — showing final PostRound before PostMatch"),
			WinningTeam + 1);
	}

	SetRoundState(ERoundState::RoundComplete);

	FinalizeRoundStatsForAllPlayers();

	if (bOrchestratorActive)
	{
		if (bMatchEnding)
		{
			bPendingMatchEndAfterPostRound = true;
			PendingMatchWinningTeam = WinningTeam;
		}

		const float PostRoundDuration = ResolvedMatchFlowSettings.PostRoundDuration;
		EnterPostRound();
		BuildAndBroadcastPostRoundSummary(WinningTeam, PostRoundDuration);
	}
	else if (bMatchEnding)
	{
		OnMatchEnded.Broadcast(WinningTeam, CurrentRoundNumber);
	}
	else
	{
		BuildAndBroadcastPostRoundSummary(WinningTeam, RoundEndDelay);

		ClearBetweenRoundTimer();
		GetWorld()->GetTimerManager().SetTimer(
			BetweenRoundTimerHandle, this, &UBwayRoundManagementComponent::StartRound,
			RoundEndDelay, false);
	}
}

void UBwayRoundManagementComponent::ResetRoundState()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}
	ClearRespawnTimers();
	if (bResetRespawnEscalationEachRound)
	{
		ResetRoundRespawnEscalation();
	}
	// Remove buildables that should not persist between rounds before respawning players.
	for (TActorIterator<ABuildableActor> It(GetWorld()); It; ++It)
	{
		ABuildableActor* Buildable = *It;
		if (Buildable && !Buildable->ShouldPersistBetweenRounds())
		{
			Buildable->Destroy();
		}
	}

	// Reset relic via RelicManagerComponent if available
	if (UBwayRelicManagerComponent* RelicMgr = GetOwner()->FindComponentByClass<UBwayRelicManagerComponent>())
	{
		RelicMgr->ResetRelic();
	}

	// Respawn all human players (destroy first — same pattern as RespawnPlayer).
	if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* PC = It->Get();
			if (!PC)
			{
				continue;
			}

			if (APawn* OldPawn = PC->GetPawn())
			{
				PC->UnPossess();
				OldPawn->Destroy();
			}

			GM->RestartPlayer(PC);
		}

		if (ABwayGameState* BwayGS = GetBwayGameState())
		{
			if (UBwayBotCreationComponent* BotCreation = BwayGS->FindComponentByClass<UBwayBotCreationComponent>())
			{
				BotCreation->EnsureBotsForRound();
			}

			// Round reset replaces every pawn outside the individual death timers.
			// Recount actual successful spawns so the prior elimination's zero cannot
			// persist into the next round (or credit a failed spawn as living).
			for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
			{
				const FTeamInfo& Team = BwayGS->GetTeamInfo(TeamIndex);
				int32 Alive = 0;
				for (APlayerState* PS : Team.TeamMembers)
				{
					const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(IsValid(PS) ? PS->GetPawn() : nullptr);
					Alive += Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0.0f ? 1 : 0;
				}
				BwayGS->UpdateTeamAliveCount(TeamIndex, Alive);
				UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Round reset team=%d alive=%d members=%d"), TeamIndex, Alive, Team.TeamMembers.Num());
			}
			BwayGS->ForceNetUpdate();
		}
	}
}

void UBwayRoundManagementComponent::ResetRoundRespawnEscalation()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	for (auto& Pair : PlayerRoundDeathCounts)
	{
		if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(Pair.Key.Get()))
		{
			BwayPS->SetDeathsInCurrentRound(0);
		}
	}
	PlayerRoundDeathCounts.Empty();

	if (ABwayGameState* BwayGS = GetBwayGameState())
	{
		for (APlayerState* PlayerState : BwayGS->PlayerArray)
		{
			if (ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState))
			{
				BwayPS->SetDeathsInCurrentRound(0);
			}
		}
	}
}

void UBwayRoundManagementComponent::ResetForNextMatch()
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	ClearBetweenRoundTimer();
	ClearRespawnTimers();
	ResetRoundRespawnEscalation();
	CurrentRoundNumber = 0;
	bSuddenDeathWarningBroadcastThisRound = false;
}

// ========================================
// Win Condition Checks
// ========================================

void UBwayRoundManagementComponent::OnRelicScored(int32 ScoringTeam, ABwayPlayerState* Scorer)
{
	if (GetOwnerRole() != ROLE_Authority || !IsRoundLifecycleActive() || CurrentRoundState != ERoundState::RoundActive)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Goal! Team %d scored"), ScoringTeam + 1);

	if (ABwayGameState* BwayGS = GetBwayGameState())
	{
		// The goal captured identity before OnEnteredGoal detached the carrier.
		// Validate membership/team before crediting a still-live PlayerState.
		if (IsValid(Scorer) && BwayGS->PlayerArray.Contains(Scorer) && BwayGS->GetPlayerTeam(Scorer) == ScoringTeam)
		{
			Scorer->AddObjectiveScore(1);
			UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Goal credited player=%d team=%d total=%d"),
				Scorer->GetPlayerId(), ScoringTeam, Scorer->GetObjectiveScore());
		}

		for (APlayerState* PlayerState : BwayGS->PlayerArray)
		{
			if (BwayGS->GetPlayerTeam(PlayerState) == ScoringTeam)
			{
				ApplyGoldDeltaToPlayerState(PlayerState, GoldAwardForGoalScored);
			}
		}
	}

	EndRound(ScoringTeam, EBwayWinCondition::GoalScored);
}

void UBwayRoundManagementComponent::CheckTeamElimination()
{
	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS || !IsRoundLifecycleActive() || CurrentRoundState != ERoundState::RoundActive)
	{
		return;
	}

	for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
	{
		const FTeamInfo& TeamInfo = BwayGS->GetTeamInfo(TeamIndex);
		if (TeamInfo.TeamMembers.Num() > 0 && TeamInfo.AlivePlayerCount == 0)
		{
			const int32 WinningTeam = (TeamIndex == 0) ? 1 : 0;
			UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Team %d eliminated!"), TeamIndex + 1);
			EndRound(WinningTeam, EBwayWinCondition::TeamEliminated);
			return;
		}
	}
}

void UBwayRoundManagementComponent::OnRoundTimerExpired()
{
	if (!IsRoundLifecycleActive() || CurrentRoundState != ERoundState::RoundActive)
	{
		return;
	}

	const UBwayRelicManagerComponent* RelicMgr = GetOwner()->FindComponentByClass<UBwayRelicManagerComponent>();
	const ARelicActor* Relic = RelicMgr ? RelicMgr->GetRelicActor() : nullptr;

	const int32 LosingTeam = UBwayMidfieldRulesLibrary::GetLosingTeamAtMidfieldFromRelic(Relic, MidfieldTolerance);
	if (LosingTeam >= 0)
	{
		const int32 WinningTeam = (LosingTeam == 0) ? 1 : 0;
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Timer expired — relic in Team %d half, Team %d wins (sudden death)"),
			LosingTeam + 1, WinningTeam + 1);
		EndRound(WinningTeam, EBwayWinCondition::SuddenDeath);
		return;
	}

	// Relic on midfield line with no last possessor — no winner, start a new round without scoring.
	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Timer expired — relic on midfield, no winner"));
	SetRoundState(ERoundState::RoundComplete);

	if (bOrchestratorActive)
	{
		EnterPostRound();
	}
	else
	{
		ClearBetweenRoundTimer();
		GetWorld()->GetTimerManager().SetTimer(
			BetweenRoundTimerHandle, this, &UBwayRoundManagementComponent::StartRound,
			RoundEndDelay, false);
	}
}

bool UBwayRoundManagementComponent::CheckMatchEnd() const
{
	// Use ScoringComponent as the canonical source of truth for scores
	if (const UBwayScoringComponent* Scoring = GetOwner()->FindComponentByClass<UBwayScoringComponent>())
	{
		for (int32 TeamIndex = 0; TeamIndex < 2; ++TeamIndex)
		{
			if (Scoring->GetTeamScore(TeamIndex) >= PointsToWin)
			{
				return true;
			}
		}
	}
	return false;
}

int32 UBwayRoundManagementComponent::DetermineRelicPossessionTeam() const
{
	// Try the RelicManagerComponent first
	if (const UBwayRelicManagerComponent* RelicMgr = GetOwner()->FindComponentByClass<UBwayRelicManagerComponent>())
	{
		return RelicMgr->GetRelicPossessingTeam();
	}
	return -1;
}

// ========================================
// Player Death/Respawn
// ========================================

int32 UBwayRoundManagementComponent::GetPlayerDeathsInCurrentRound(const APlayerState* PlayerState) const
{
	if (!PlayerState)
	{
		return 0;
	}

	if (const int32* TrackedCount = PlayerRoundDeathCounts.Find(PlayerState))
	{
		return FMath::Max(0, *TrackedCount);
	}

	if (const ABwayPlayerState* BwayPS = Cast<ABwayPlayerState>(PlayerState))
	{
		return FMath::Max(0, BwayPS->GetDeathsInCurrentRound());
	}

	return 0;
}

float UBwayRoundManagementComponent::GetConfiguredBaseRespawnDelaySeconds() const
{
	constexpr float MinimumDelay = 0.05f;
	const float ConfiguredBase = FMath::IsFinite(BaseRespawnDelaySeconds) ? BaseRespawnDelaySeconds : 10.0f;
	const float LegacyAlias = FMath::IsFinite(RespawnDelay) ? RespawnDelay : ConfiguredBase;
	if (!FMath::IsNearlyEqual(ConfiguredBase, 10.0f))
	{
		return FMath::Max(MinimumDelay, ConfiguredBase);
	}
	if (!FMath::IsNearlyEqual(LegacyAlias, 10.0f))
	{
		return FMath::Max(MinimumDelay, LegacyAlias);
	}
	return FMath::Max(MinimumDelay, ConfiguredBase);
}

float UBwayRoundManagementComponent::ComputeEscalatedRespawnBaseDelay(int32 PriorDeathsInRound) const
{
	constexpr float MinimumDelay = 0.05f;
	const float Base = GetConfiguredBaseRespawnDelaySeconds();
	const float Increment = FMath::IsFinite(PerDeathRespawnIncrementSeconds)
		? FMath::Max(0.0f, PerDeathRespawnIncrementSeconds)
		: 0.0f;
	const float MaxDelay = FMath::IsFinite(MaxRespawnDelaySeconds)
		? FMath::Max(Base, MaxRespawnDelaySeconds)
		: Base;
	const int32 ClampedPriorDeaths = FMath::Max(0, PriorDeathsInRound);
	const float UnclampedDelay = Base + static_cast<float>(ClampedPriorDeaths) * Increment;
	return FMath::Max(MinimumDelay, FMath::Clamp(UnclampedDelay, Base, MaxDelay));
}

float UBwayRoundManagementComponent::ComputeEffectiveRespawnDelayForPlayer(const APlayerState* PlayerState, int32 PriorDeathsInRound) const
{
	constexpr float MinimumDelay = 0.05f;
	const float EscalatedBaseDelay = ComputeEscalatedRespawnBaseDelay(PriorDeathsInRound);
	const UAbilitySystemComponent* ASC = PlayerState
		? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState, true)
		: nullptr;
	const UBwayHeroAttributeSet* Hero = ASC ? ASC->GetSet<UBwayHeroAttributeSet>() : nullptr;
	const float RawReduction = Hero ? Hero->GetRespawnTimeReduction() : 0.0f;
	const float ClampedReduction = FMath::IsFinite(RawReduction) ? FMath::Clamp(RawReduction, 0.0f, 1.0f) : 0.0f;
	return FMath::Max(MinimumDelay, EscalatedBaseDelay * (1.0f - ClampedReduction));
}

void UBwayRoundManagementComponent::OnPlayerDied(AController* VictimController, AController* KillerController)
{
	ABwayGameState* BwayGS = GetBwayGameState();
	if (GetOwnerRole() != ROLE_Authority || !BwayGS || !IsValid(VictimController)
		|| !VictimController->PlayerState || !IsRoundLifecycleActive() || CurrentRoundState != ERoundState::RoundActive)
	{
		return;
	}

	// Update alive count via GameState's team tracking
	// Keep the Blueprint signature, but derive awards from server-observed health loss,
	// not a caller-supplied killer controller or the lifetime of the attacker's pawn.
	auto* Victim = Cast<ABwayPlayerState>(VictimController->PlayerState);
	auto* Pawn = Cast<ABwayCharacterWithAbilities>(VictimController->GetPawn());
	ABwayPlayerState* Killer = nullptr;
	TArray<ABwayPlayerState*> Assisters;
	if (!Victim || !BwayGS->PlayerArray.Contains(Victim) || !Pawn || !Pawn->ConsumeCombatCredit(Killer, Assisters))
	{
		return;
	}
	Victim->AddDeath();
	if (Killer)
	{
		Killer->AddKill();
		ApplyGoldDeltaToPlayerState(Killer, GoldAwardForKill);
	}
	for (auto* Assister : Assisters)
	{
		Assister->AddAssist();
		ApplyGoldDeltaToPlayerState(Assister, GoldAwardForAssist);
	}

	BwayGS->OnPlayerDied(VictimController->PlayerState);

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Player %s died"),
		*VictimController->PlayerState->GetPlayerName());

	// Check for team elimination
	CheckTeamElimination();

	// Elimination can end the round synchronously. Never add a timer after its cleanup.
	if (!IsRoundLifecycleActive() || CurrentRoundState != ERoundState::RoundActive)
	{
		return;
	}

	const int32 PriorDeathsInRound = GetPlayerDeathsInCurrentRound(Victim);
	const int32 NewDeathsInRound = PriorDeathsInRound + 1;
	PlayerRoundDeathCounts.Add(Victim, NewDeathsInRound);
	Victim->SetDeathsInCurrentRound(NewDeathsInRound);

	const float EscalatedBaseDelay = ComputeEscalatedRespawnBaseDelay(PriorDeathsInRound);
	FPendingRespawn& Pending = RespawnTimers.FindOrAdd(VictimController);
	Pending.StartTime = BwayGS->GetServerWorldTimeSeconds();
	Pending.BaseDelay = EscalatedBaseDelay;
	Pending.Round = CurrentRoundNumber;
	Pending.AppliedReduction = -1.0f;
	RefreshRespawnSchedule(Victim);
}

void UBwayRoundManagementComponent::RefreshRespawnSchedule(APlayerState* PlayerState)
{
	ABwayGameState* BwayGS = GetBwayGameState();
	ABwayPlayerState* Player = Cast<ABwayPlayerState>(PlayerState);
	AController* Controller = Player ? Cast<AController>(Player->GetOwner()) : nullptr;
	if (GetOwnerRole() != ROLE_Authority || !BwayGS || !IsValid(Controller)
		|| Controller->PlayerState != Player || Controller->GetWorld() != GetWorld()
		|| !BwayGS->PlayerArray.Contains(Player) || !IsRoundLifecycleActive()
		|| CurrentRoundState != ERoundState::RoundActive) return;
	FPendingRespawn* Pending = RespawnTimers.Find(Controller);
	if (!Pending || Pending->Round != CurrentRoundNumber) return;
	const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Player, true);
	const UBwayHeroAttributeSet* Hero = ASC ? ASC->GetSet<UBwayHeroAttributeSet>() : nullptr;
	const float RawReduction = Hero ? Hero->GetRespawnTimeReduction() : 0.0f;
	const float Reduction = FMath::IsFinite(RawReduction) ? FMath::Clamp(RawReduction, 0.0f, 1.0f) : 0.0f;
	if (Pending->AppliedReduction == Reduction) return;
	Pending->AppliedReduction = Reduction;
	// A zero timer rate cancels a timer in Unreal. Keep even a fully reduced or
	// already elapsed deadline asynchronous and positive, outside the shop transaction.
	constexpr float MinimumDelay = 0.05f;
	const float BaseDelay = FMath::IsFinite(Pending->BaseDelay) ? FMath::Max(MinimumDelay, Pending->BaseDelay) : MinimumDelay;
	const float EffectiveDelay = BaseDelay * (1.0f - Reduction);
	const double Now = BwayGS->GetServerWorldTimeSeconds();
	const double Elapsed = FMath::Max(0.0, Now - Pending->StartTime);
	const float RemainingDelay = static_cast<float>(FMath::Max(static_cast<double>(MinimumDelay), EffectiveDelay - Elapsed));
	const TWeakObjectPtr<AController> WeakVictim(Controller);
	const int32 DeathRound = Pending->Round;
	FTimerHandle& RespawnTimer = Pending->Timer;
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, WeakVictim, DeathRound]()
		{
			if (WeakVictim.IsValid() && CurrentRoundNumber == DeathRound)
			{
#if !UE_BUILD_SHIPPING
				if (const FPendingRespawn* Scheduled = RespawnTimers.Find(WeakVictim.Get()))
					BwayRoundManagement::TraceRespawnTimer(GetWorld(), WeakVictim.Get(), Scheduled->Timer, TEXT("fire"));
#endif
				RespawnPlayer(WeakVictim.Get());
			}
		}),
		RemainingDelay, false);
	const float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(RespawnTimer);
	Player->SetRespawnSchedule(Remaining > 0.0f ? Now + Remaining : 0.0, DeathRound);
	UE_LOG(LogTemp, Log,
		TEXT("BwayRoundManagement: Scheduled respawn for %s (round=%d roundDeaths=%d baseDelay=%.2fs reduction=%.2f effectiveDelay=%.2fs remaining=%.2fs)"),
		*Player->GetPlayerName(), DeathRound, GetPlayerDeathsInCurrentRound(Player), BaseDelay, Reduction, EffectiveDelay, Remaining);
#if !UE_BUILD_SHIPPING
	BwayRoundManagement::TraceRespawnTimer(GetWorld(), Controller, RespawnTimer, TEXT("schedule"));
#endif
}

void UBwayRoundManagementComponent::ClearRespawnTimers()
{
	if (UWorld* World = GetWorld())
	{
		if (ABreakawayGameMode* Mode = World->GetAuthGameMode<ABreakawayGameMode>())
		{
			Mode->ClearPendingPlayerRestarts();
		}
		for (auto& Pair : RespawnTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value.Timer);
			if (AController* Controller = Pair.Key.Get())
			{
				if (ABwayPlayerState* Player = Cast<ABwayPlayerState>(Controller->PlayerState))
				{
					Player->SetRespawnSchedule(0.0, 0);
				}
			}
		}
	}
	RespawnTimers.Empty();
}

void UBwayRoundManagementComponent::RespawnPlayer(AController* Controller)
{
	if (GetOwnerRole() != ROLE_Authority || !IsValid(Controller))
	{
		return;
	}

	FPendingRespawn PendingTimer;
	if (!RespawnTimers.RemoveAndCopyValue(Controller, PendingTimer))
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(PendingTimer.Timer);
	// Consume the presentation schedule with its timer, including rejected restarts.
	if (ABwayPlayerState* Player = Cast<ABwayPlayerState>(Controller->PlayerState))
	{
		Player->SetRespawnSchedule(0.0, 0);
	}

	ABwayGameState* BwayGS = GetBwayGameState();
	ALyraGameMode* GM = GetWorld()->GetAuthGameMode<ALyraGameMode>();
	if (!BwayGS || !GM || !Controller->PlayerState || Controller->GetWorld() != GetWorld()
		|| !BwayGS->PlayerArray.Contains(Controller->PlayerState)
		|| !IsRoundLifecycleActive() || CurrentRoundState != ERoundState::RoundActive
		|| !GM->ControllerCanRestart(Controller))
	{
		return;
	}

	// A different restart path must never cause us to destroy an already living pawn.
	if (APawn* OldPawn = Controller->GetPawn())
	{
		const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(OldPawn);
		if (Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0.0f)
		{
			return;
		}
		Controller->UnPossess();
		OldPawn->Destroy();
	}

	// Both human and bot controllers use the same authoritative restart path.
	Controller->ResetIgnoreInputFlags();
	GM->RestartPlayer(Controller);

	const ULyraHealthComponent* NewHealth = ULyraHealthComponent::FindHealthComponent(Controller->GetPawn());
	if (!NewHealth || NewHealth->IsDeadOrDying() || NewHealth->GetHealth() <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: Restart did not produce a living pawn for %s"),
			*GetNameSafe(Controller));
		return;
	}

	BwayGS->OnPlayerRespawned(Controller->PlayerState);
	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Player %s respawned"),
		*Controller->PlayerState->GetPlayerName());
}

void UBwayRoundManagementComponent::ApplyGoldDeltaToPlayerState(APlayerState* PlayerState, float GoldDelta) const
{
	if (!PlayerState || FMath::IsNearlyZero(GoldDelta))
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
	if (!ASC || !ASC->GetSet<UBwayGoldAttributeSet>())
	{
		return;
	}

	UGameplayEffect* GoldEffect = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None);
	GoldEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo& Modifier = GoldEffect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UBwayGoldAttributeSet::GetCurrentGoldAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(GoldDelta);

	ASC->ApplyGameplayEffectToSelf(GoldEffect, 1.0f, ASC->MakeEffectContext());
}

FActiveGameplayEffectHandle UBwayRoundManagementComponent::ApplyPassiveGoldIncomeToPlayerState(APlayerState* PlayerState) const
{
	if (GetOwnerRole() != ROLE_Authority || !PlayerState || PassiveGoldPerSecond <= 0.0f)
	{
		return FActiveGameplayEffectHandle();
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
	if (!ASC || !ASC->GetSet<UBwayGoldAttributeSet>())
	{
		return FActiveGameplayEffectHandle();
	}

	// Active effects replicate their definition; a transient server-only definition
	// cannot be resolved by clients. Keep the rate on the spec, not the shared CDO.
	FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(
		UBwayGameplayEffect_PassiveGold::StaticClass(), 1.0f, ASC->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	Spec.Data->SetSetByCallerMagnitude(UBwayGameplayEffect_PassiveGold::IncomeMagnitudeName, PassiveGoldPerSecond);
	return ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void UBwayRoundManagementComponent::RemovePassiveGoldIncome()
{
	for (const TPair<TWeakObjectPtr<APlayerState>, FActiveGameplayEffectHandle>& Pair : PassiveGoldEffectHandles)
	{
		if (APlayerState* PlayerState = Pair.Key.Get())
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState))
			{
				ASC->RemoveActiveGameplayEffect(Pair.Value);
			}
		}
	}

	PassiveGoldEffectHandles.Reset();
}
