// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameState/BwayRoundManagementComponent.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Relic/RelicActor.h"
#include "BwayCharacterWithAbilities.h"
#include "Buildable/BuildableBase.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayScoringComponent.h"
#include "SpawnSystem/BwaySpawnPointManagerComponent.h"
#include "AbilitySystem/Phases/LyraGamePhaseSubsystem.h"
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
}

void UBwayRoundManagementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	if (bAutoStartFirstRound)
	{
		GetWorld()->GetTimerManager().SetTimer(
			PreRoundTimerHandle, this, &UBwayRoundManagementComponent::StartRound, PreRoundDelay, false);

		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: First round will start in %.1f seconds (auto-start)"), PreRoundDelay);
		return;
	}

	// Phase-driven flow: listen for the Playing phase to activate, then call StartRound().
	if (PlayingPhaseTag.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (ULyraGamePhaseSubsystem* PhaseSubsystem = World->GetSubsystem<ULyraGamePhaseSubsystem>())
			{
				PhaseSubsystem->WhenPhaseStartsOrIsActive(
					PlayingPhaseTag,
					EPhaseTagMatchType::ExactMatch,
					FLyraGamePhaseTagDelegate::CreateUObject(this, &UBwayRoundManagementComponent::HandlePlayingPhaseActivated));

				UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Listening for Playing phase tag: %s"), *PlayingPhaseTag.ToString());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: LyraGamePhaseSubsystem not available - round will not auto-start"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayRoundManagement: No PlayingPhaseTag set and bAutoStartFirstRound is false - round will not auto-start"));
	}
}

void UBwayRoundManagementComponent::HandlePlayingPhaseActivated(const FGameplayTag& ActivePhaseTag)
{
	if (GetOwnerRole() != ROLE_Authority)
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

	if (GetOwnerRole() != ROLE_Authority || CurrentRoundState != ERoundState::RoundActive)
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
	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayRoundManagement: Cannot start round - GameState is null"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Starting Round %d"), CurrentRoundNumber + 1);

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
}

void UBwayRoundManagementComponent::EndRound(int32 WinningTeam, EBwayWinCondition WinCondition)
{
	ABwayGameState* BwayGS = GetBwayGameState();
	if (!BwayGS)
	{
		return;
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

	if (CheckMatchEnd())
	{
		UE_LOG(LogTemp, Log, TEXT("BwayRoundManagement: Match Over! Team %d wins!"), WinningTeam + 1);
		SetRoundState(ERoundState::RoundComplete);

		// Broadcast match-end to any listeners (GameState uses this for PostGame)
		OnMatchEnded.Broadcast(WinningTeam, CurrentRoundNumber);
		return;
	}

	SetRoundState(ERoundState::RoundComplete);

	OnBetweenRoundPlanningStarted.Broadcast(CurrentRoundNumber, RoundEndDelay);

	// Schedule next round
	FTimerHandle UnusedHandle;
	GetWorld()->GetTimerManager().SetTimer(
		UnusedHandle, this, &UBwayRoundManagementComponent::StartRound, 
		RoundEndDelay, false);
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

	// Respawn all players
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			if (AGameModeBase* GM = GetWorld()->GetAuthGameMode())
			{
				GM->RestartPlayer(PC);
			}
		}
	}
}

// ========================================
// Win Condition Checks
// ========================================

void UBwayRoundManagementComponent::OnRelicScored(int32 ScoringTeam)
{
	if (CurrentRoundState != ERoundState::RoundActive)
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
	if (!BwayGS || CurrentRoundState != ERoundState::RoundActive)
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
	if (CurrentRoundState != ERoundState::RoundActive)
	{
		return;
	}

	const int32 PossessingTeam = DetermineRelicPossessionTeam();
	if (PossessingTeam >= 0)
	{
		EndRound(PossessingTeam, EBwayWinCondition::TimeExpired);
	}
	else
	{
		// Neutral — no winner, start new round
		SetRoundState(ERoundState::RoundComplete);
		FTimerHandle UnusedHandle;
		GetWorld()->GetTimerManager().SetTimer(
			UnusedHandle, this, &UBwayRoundManagementComponent::StartRound,
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
