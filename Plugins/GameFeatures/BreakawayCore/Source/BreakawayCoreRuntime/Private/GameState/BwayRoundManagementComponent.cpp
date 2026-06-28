// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayRoundManagementComponent.h"
#include "GameModes/BwayMatchFlowLibrary.h"
#include "GameModes/BwayGameplayUrlLibrary.h"
#include "Stats/BwayMatchStatsLibrary.h"
#include "GameState/BwayBotCreationComponent.h"
#include "GameModes/LyraExperienceManagerComponent.h"
#include "GameModes/LyraExperienceDefinition.h"
#include "GameState/BwayMidfieldRulesLibrary.h"
#include "GameState/BwayMidfieldDividerComponent.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Relic/RelicActor.h"
#include "BwayCharacterWithAbilities.h"
#include "Buildable/BuildableBase.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "GameState/BwayScoringComponent.h"
#include "SpawnSystem/BwaySpawnPointManagerComponent.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
#include "AbilitySystem/Phases/LyraGamePhaseAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "GameModes/LyraGameState.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BwayRoundManagementComponent)

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
	return bOrchestratorActive && CurrentMatchPhase == EBwayMatchPhase::Prematch;
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

	bPrematchCompletionHandled = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchPhaseTimerHandle);
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Prematch complete — advancing to Warmup (11-4)"));
	EnterWarmup();
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

	if (bFirstEntryFromWarmup)
	{
		StartPlayingPhaseAbility();

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

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchPhaseTimerHandle);
		World->GetTimerManager().ClearTimer(BetweenRoundTimerHandle);
		World->GetTimerManager().ClearTimer(PreRoundTimerHandle);

		for (TPair<TObjectPtr<AController>, FTimerHandle>& Pair : RespawnTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
		RespawnTimers.Empty();
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

	RoundDuration = FMath::Max(1.0f, InRoundDuration);
}

bool UBwayRoundManagementComponent::IsRoundLifecycleActive() const
{
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

	// BotCreation applies full match rules on its own OnExperienceLoaded (after this high-priority hook).
	// Here we only ensure playing FSM settings are set early if BotCreation is absent.
	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS || !BwayGS->FindComponentByClass<UBwayBotCreationComponent>())
	{
		UBwayMatchFlowLibrary::ApplyMatchRulesOnly(this, ResolvedMatchFlowSettings, this, nullptr);
	}

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

	const float ElapsedTime = GetWorld()->GetTimeSeconds() - RoundStartTime;
	const float RemainingTime = FMath::Max(0.0f, RoundDuration - ElapsedTime);
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

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Round ended! Team %d wins"), WinningTeam + 1);

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
		}
	}
}

// ========================================
// Win Condition Checks
// ========================================

void UBwayRoundManagementComponent::OnRelicScored(int32 ScoringTeam)
{
	if (!IsRoundLifecycleActive() || CurrentRoundState != ERoundState::RoundActive)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Goal! Team %d scored"), ScoringTeam + 1);

	if (ABwayGameState* BwayGS = GetBwayGameState())
	{
		if (UBwayRelicManagerComponent* RelicMgr = BwayGS->RelicManagerComponent)
		{
			if (ARelicActor* Relic = RelicMgr->GetRelicActor())
			{
				if (ABwayCharacterWithAbilities* Carrier = Relic->CurrentCarrier)
				{
					if (ABwayPlayerState* ScorerPS = Cast<ABwayPlayerState>(Carrier->GetPlayerState()))
					{
						ScorerPS->AddObjectiveScore(1);
					}
				}
			}
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

void UBwayRoundManagementComponent::OnPlayerDied(AController* VictimController, AController* KillerController)
{
	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS || !VictimController || !VictimController->PlayerState)
	{
		return;
	}

	// Update alive count via GameState's team tracking
	BwayGS->OnPlayerDied(VictimController->PlayerState);

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Player %s died"),
		*VictimController->PlayerState->GetPlayerName());

	if (KillerController && KillerController != VictimController && KillerController->PlayerState)
	{
		ApplyGoldDeltaToPlayerState(KillerController->PlayerState, GoldAwardForKill);
	}

	// Check for team elimination
	CheckTeamElimination();

	// Schedule respawn
	FTimerHandle& RespawnTimer = RespawnTimers.FindOrAdd(VictimController);
	GetWorld()->GetTimerManager().SetTimer(RespawnTimer,
		[this, VictimController]() { RespawnPlayer(VictimController); },
		RespawnDelay, false);
}

void UBwayRoundManagementComponent::RespawnPlayer(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	ABwayGameState* BwayGS = GetBwayGameState();
	APlayerController* PC = Cast<APlayerController>(Controller);

	if (!PC || !PC->PlayerState)
	{
		return;
	}

	// Destroy old pawn if exists
	if (APawn* OldPawn = Controller->GetPawn())
	{
		OldPawn->Destroy();
	}

	// Spawn new pawn via GameMode (the only thing GameMode should do — spawn pawns)
	if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
	{
		GM->RestartPlayer(Controller);
	}

	// Update alive count
	if (BwayGS)
	{
		BwayGS->OnPlayerRespawned(PC->PlayerState);
	}

	// Clear respawn timer
	RespawnTimers.Remove(Controller);

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Player %s respawned"),
		*PC->PlayerState->GetPlayerName());
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
	if (!PlayerState || PassiveGoldPerSecond <= 0.0f)
	{
		return FActiveGameplayEffectHandle();
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PlayerState);
	if (!ASC || !ASC->GetSet<UBwayGoldAttributeSet>())
	{
		return FActiveGameplayEffectHandle();
	}

	UGameplayEffect* PassiveEffect = NewObject<UGameplayEffect>(GetTransientPackage(), NAME_None);
	PassiveEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;
	PassiveEffect->Period = FScalableFloat(1.0f);
	PassiveEffect->bExecutePeriodicEffectOnApplication = false;

	FGameplayModifierInfo& Modifier = PassiveEffect->Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UBwayGoldAttributeSet::GetCurrentGoldAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(PassiveGoldPerSecond);

	return ASC->ApplyGameplayEffectToSelf(PassiveEffect, 1.0f, ASC->MakeEffectContext());
}

void UBwayRoundManagementComponent::RemovePassiveGoldIncome()
{
	for (const TPair<TObjectPtr<APlayerState>, FActiveGameplayEffectHandle>& Pair : PassiveGoldEffectHandles)
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
