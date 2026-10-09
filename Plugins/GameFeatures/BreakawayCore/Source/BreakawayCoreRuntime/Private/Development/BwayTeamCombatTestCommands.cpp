// Development-only authority fixtures for team-aware damage and relic passing.
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING

#include "AbilitySystemComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Development/BwayCombatFixtureAbilities.h"
#include "Abilities/BwayGameplayAbility_MeleePrimary.h"
#include "Abilities/BwayGameplayAbility_RelicPass.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGoalVolume.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "GameState/BwayScoringComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "TimerManager.h"
#include "GameplayTagContainer.h"
#include "Containers/Ticker.h"

namespace BwayTeamCombatTest
{

void RunRemoteScore(UWorld* World);

bool GRemoteScorePollActive = false;
float GRemoteScorePollElapsed = 0.0f;

static TAutoConsoleVariable<int32> CVarRemoteRelicResetProbe(
	TEXT("bway.Test.RemoteRelicResetProbe"), 0,
	TEXT("One-shot remote score fixture probe: exercise repeated production reset/drop audio before passing."));

UWorld* FindAuthorityGameplayWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* CandidateWorld = Context.World();
		if (CandidateWorld && CandidateWorld->IsGameWorld() && CandidateWorld->GetNetMode() != NM_Client)
		{
			ABwayGameState* State = CandidateWorld->GetGameState<ABwayGameState>();
			const UBwayRoundManagementComponent* Rounds = State ? State->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
			if (!Rounds || Rounds->GetCurrentMatchPhase() != EBwayMatchPhase::Playing)
			{
				continue;
			}
			int32 SelectedHumans = 0;
			for (APlayerState* Player : State->PlayerArray)
			{
				const ABwayPlayerState* BwayPlayer = Cast<ABwayPlayerState>(Player);
				SelectedHumans += BwayPlayer && !BwayPlayer->IsABot() && BwayPlayer->IsHeroLocked()
					&& Cast<ABwayCharacterWithAbilities>(BwayPlayer->GetPawn()) ? 1 : 0;
			}
			if (SelectedHumans >= 4)
			{
				return CandidateWorld;
			}
		}
	}

	return nullptr;
}

struct FTeamPair
{
	ABwayCharacterWithAbilities* Carrier = nullptr;
	ABwayCharacterWithAbilities* Teammate = nullptr;
	ABwayCharacterWithAbilities* Enemy = nullptr;
};

ULyraAbilitySystemComponent* GetPlayerStateASC(ABwayCharacterWithAbilities* Character)
{
	ABwayPlayerState* PlayerState = Character ? Character->GetPlayerState<ABwayPlayerState>() : nullptr;
	return PlayerState ? PlayerState->GetLyraAbilitySystemComponent() : nullptr;
}

ULyraAbilitySystemComponent* GetCharacterASC(ABwayCharacterWithAbilities* Character)
{
	return Character ? Character->GetLyraAbilitySystemComponent() : nullptr;
}

bool FindCombatAbilityHandle(ULyraAbilitySystemComponent* ASC, FGameplayAbilitySpecHandle& OutHandle, FString& OutAbilityName)
{
	if (!ASC)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (!Spec.Ability)
		{
			continue;
		}

		const FString AbilityName = GetNameSafe(Spec.Ability);
		if (BwayCombatFixture::IsPrimaryAbility(Spec.Ability))
		{
			OutHandle = Spec.Handle;
			OutAbilityName = AbilityName;
			return true;
		}
	}

	return false;
}

bool IsAlive(ABwayCharacterWithAbilities* Character)
{
	if (!Character || !Character->GetPlayerState())
	{
		return false;
	}

	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character->GetPlayerState()))
	{
		const FGameplayTag DeathTag = FGameplayTag::RequestGameplayTag(FName("Status.Death"), false);
		const FGameplayTag DyingTag = FGameplayTag::RequestGameplayTag(FName("Status.Death.Dying"), false);
		if ((DeathTag.IsValid() && ASC->HasMatchingGameplayTag(DeathTag)) ||
			(DyingTag.IsValid() && ASC->HasMatchingGameplayTag(DyingTag)))
		{
			return false;
		}
	}

	return true;
}

bool FindTeamPair(UWorld* World, ABwayGameState* GameState, FTeamPair& OutPair, bool bRequireLocalCarrier = true, bool bRequireRemoteTeammate = false)
{
	TArray<ABwayCharacterWithAbilities*> Humans;
	for (TActorIterator<ABwayCharacterWithAbilities> It(World); It; ++It)
	{
		ABwayCharacterWithAbilities* Character = *It;
		if (Character && Cast<APlayerController>(Character->GetController()) && IsAlive(Character))
		{
			Humans.Add(Character);
		}
	}

	for (ABwayCharacterWithAbilities* Carrier : Humans)
	{
		APlayerController* CarrierPC = Cast<APlayerController>(Carrier->GetController());
		if (!CarrierPC || CarrierPC->IsLocalController() != bRequireLocalCarrier)
		{
			continue;
		}
		if (!bRequireLocalCarrier && !CarrierPC->GetNetConnection())
		{
			continue;
		}

		FGameplayAbilitySpecHandle CombatHandle;
		FString CombatAbilityName;
		if (!FindCombatAbilityHandle(GetCharacterASC(Carrier), CombatHandle, CombatAbilityName))
		{
			continue;
		}

		const int32 CarrierTeam = GameState->GetPlayerTeam(Carrier->GetPlayerState());
		if (CarrierTeam < 0)
		{
			continue;
		}

		for (ABwayCharacterWithAbilities* Teammate : Humans)
		{
			if (Teammate == Carrier || GameState->GetPlayerTeam(Teammate->GetPlayerState()) != CarrierTeam)
			{
				continue;
			}
			if (bRequireRemoteTeammate)
			{
				APlayerController* TeammatePC = Cast<APlayerController>(Teammate->GetController());
				if (!TeammatePC || TeammatePC->IsLocalController() || !TeammatePC->GetNetConnection())
				{
					continue;
				}
			}

			for (ABwayCharacterWithAbilities* Enemy : Humans)
			{
				if (Enemy != Carrier && Enemy != Teammate &&
					GameState->GetPlayerTeam(Enemy->GetPlayerState()) >= 0 &&
					GameState->GetPlayerTeam(Enemy->GetPlayerState()) != CarrierTeam)
				{
					OutPair = {Carrier, Teammate, Enemy};
					return true;
				}
			}
		}
	}

	return false;
}

bool TryActivateAbilityHandle(ULyraAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& AbilityHandle, const FString& ExpectedAbilityName)
{
	if (!ASC || !AbilityHandle.IsValid())
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Handle == AbilityHandle)
		{
			const bool bActivated = ASC->TryActivateAbility(Spec.Handle, false);
			FGameplayTagContainer OwnedTags;
			ASC->GetOwnedGameplayTags(OwnedTags);
			UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: AbilityAttempt class=%s expected=%s handle=%s activeBefore=%d activated=%d"),
				*GetNameSafe(Spec.Ability), *ExpectedAbilityName, *Spec.Handle.ToString(), Spec.IsActive(), bActivated);
			if (!bActivated)
			{
				UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: AbilityReject owner=%s avatar=%s tags=%s"),
					*GetNameSafe(ASC->GetOwnerActor()), *GetNameSafe(ASC->GetAvatarActor()), *OwnedTags.ToStringSimple());
			}
			return bActivated;
		}
	}

	return false;
}

bool TryActivateAbilityClass(ULyraAbilitySystemComponent* ASC, TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!ASC || !AbilityClass)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA(AbilityClass))
		{
			return ASC->TryActivateAbility(Spec.Handle, false);
		}
	}

	return false;
}

ARelicActor* FindRelic(UWorld* World)
{
	for (TActorIterator<ARelicActor> It(World); It; ++It)
	{
		if (*It)
		{
			return *It;
		}
	}
	return nullptr;
}

ULyraHealthComponent* GetHealth(ABwayCharacterWithAbilities* Character)
{
	return Character ? ULyraHealthComponent::FindHealthComponent(Character) : nullptr;
}

ABwayCharacterWithAbilities* FindCharacterByPlayerId(UWorld* World, int32 PlayerId)
{
	if (!World || PlayerId == INDEX_NONE)
	{
		return nullptr;
	}

	for (TActorIterator<ABwayCharacterWithAbilities> It(World); It; ++It)
	{
		ABwayCharacterWithAbilities* Character = *It;
		if (Character && Character->GetPlayerState() && Character->GetPlayerState()->GetPlayerId() == PlayerId)
		{
			return Character;
		}
	}

	return nullptr;
}

struct FPeerConvergenceSample
{
	UWorld* World = nullptr;
	bool bAuthority = false;
	bool bFound = false;
	float Health = -1.0f;
	int32 Team = INDEX_NONE;
};

void RunPeerConvergenceCheck(UWorld* AuthorityWorld, int32 EnemyId, int32 Attempt)
{
	if (!AuthorityWorld)
	{
		return;
	}

	TArray<UWorld*> PeerWorlds;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* CandidateWorld = Context.World();
			if (Context.WorldType == EWorldType::PIE && CandidateWorld && !PeerWorlds.Contains(CandidateWorld))
			{
				PeerWorlds.Add(CandidateWorld);
			}
		}
	}
	if (!PeerWorlds.Contains(AuthorityWorld))
	{
		PeerWorlds.Add(AuthorityWorld);
	}

	TArray<FPeerConvergenceSample> Samples;
	Samples.Reserve(PeerWorlds.Num());
	for (UWorld* PeerWorld : PeerWorlds)
	{
		FPeerConvergenceSample& Sample = Samples.AddDefaulted_GetRef();
		Sample.World = PeerWorld;
		Sample.bAuthority = PeerWorld == AuthorityWorld;
		if (ABwayCharacterWithAbilities* Enemy = FindCharacterByPlayerId(PeerWorld, EnemyId))
		{
			if (ULyraHealthComponent* Health = GetHealth(Enemy))
			{
				Sample.Health = Health->GetHealth();
			}
			if (ABwayGameState* PeerGameState = PeerWorld->GetGameState<ABwayGameState>())
			{
				Sample.Team = PeerGameState->GetPlayerTeam(Enemy->GetPlayerState());
			}
			Sample.bFound = Sample.Health >= 0.0f && Sample.Team >= 0;
		}
	}

	const FPeerConvergenceSample* AuthoritySample = Samples.FindByPredicate(
		[](const FPeerConvergenceSample& Sample)
		{
			return Sample.bAuthority;
		});
	const bool bHasAuthoritySample = AuthoritySample && AuthoritySample->bFound;
	const bool bConverged = PeerWorlds.Num() >= 4 && bHasAuthoritySample &&
		Samples.ContainsByPredicate(
			[AuthoritySample](const FPeerConvergenceSample& Sample)
			{
				return !Sample.bFound ||
					!FMath::IsNearlyEqual(Sample.Health, AuthoritySample->Health, 0.5f) ||
					Sample.Team != AuthoritySample->Team;
			}) == false;

	if (bConverged)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: PeerConvergence COMPLETE worlds=%d enemy=%d authorityHealth=%.1f authorityTeam=%d attempts=%d"),
			PeerWorlds.Num(), EnemyId, AuthoritySample->Health, AuthoritySample->Team, Attempt + 1);
		for (const FPeerConvergenceSample& Sample : Samples)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: PeerConvergencePeer world=%s role=%s found=%d health=%.1f team=%d"),
				*GetNameSafe(Sample.World), Sample.bAuthority ? TEXT("Authority") : TEXT("Remote"), Sample.bFound,
				Sample.Health, Sample.Team);
		}
		return;
	}

	if (Attempt < 12)
	{
		TWeakObjectPtr<UWorld> WeakAuthorityWorld(AuthorityWorld);
		FTimerHandle RetryTimer;
		AuthorityWorld->GetTimerManager().SetTimer(
			RetryTimer,
			FTimerDelegate::CreateLambda([WeakAuthorityWorld, EnemyId, Attempt]()
			{
				if (UWorld* RetryWorld = WeakAuthorityWorld.Get())
				{
					RunPeerConvergenceCheck(RetryWorld, EnemyId, Attempt + 1);
				}
			}),
			0.25f,
			false);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatTest: PeerConvergence TIMEOUT worlds=%d enemy=%d authoritySample=%d attempts=%d"),
		PeerWorlds.Num(), EnemyId, bHasAuthoritySample, Attempt + 1);
}

void SchedulePeerConvergenceCheck(UWorld* AuthorityWorld, int32 EnemyId)
{
	if (!AuthorityWorld)
	{
		return;
	}

	TWeakObjectPtr<UWorld> WeakAuthorityWorld(AuthorityWorld);
	FTimerHandle ConvergenceTimer;
	AuthorityWorld->GetTimerManager().SetTimer(
		ConvergenceTimer,
		FTimerDelegate::CreateLambda([WeakAuthorityWorld, EnemyId]()
		{
			if (UWorld* CheckWorld = WeakAuthorityWorld.Get())
			{
				RunPeerConvergenceCheck(CheckWorld, EnemyId, 0);
			}
		}),
		0.25f,
		false);
}

void RequestPeerConvergenceSamples(UWorld* AuthorityWorld, int32 EnemyId)
{
	if (!AuthorityWorld || AuthorityWorld->GetNetMode() == NM_Client)
	{
		return;
	}

	if (ABwayGameState* GameState = AuthorityWorld->GetGameState<ABwayGameState>())
	{
		GameState->SetTeamPeerConvergenceEnemyId(EnemyId);

		// Advance the replicated sequence twice more after the first state has
		// settled. Reusing the same enemy id alone can be coalesced by the
		// replication system, which would make a one-shot probe look like a
		// convergence result. Keep the interval longer than the client settle
		// delay and the adverse-latency replication window so each sequence
		// produces exactly one local sample.
		TWeakObjectPtr<UWorld> WeakWorld(AuthorityWorld);
		for (int32 SampleIndex = 1; SampleIndex < 3; ++SampleIndex)
		{
			FTimerHandle SampleTimer;
			AuthorityWorld->GetTimerManager().SetTimer(
				SampleTimer,
				FTimerDelegate::CreateLambda([WeakWorld, EnemyId]()
				{
					if (UWorld* SampleWorld = WeakWorld.Get())
					{
						if (ABwayGameState* SampleGameState = SampleWorld->GetGameState<ABwayGameState>())
						{
							SampleGameState->SetTeamPeerConvergenceEnemyId(EnemyId);
						}
					}
				}),
				static_cast<float>(SampleIndex) * 3.0f,
				false);
		}
	}
}

void CompleteTeamCombatPass(
	UWorld* World,
	TWeakObjectPtr<ABwayCharacterWithAbilities> WeakCarrier,
	TWeakObjectPtr<ABwayCharacterWithAbilities> WeakTeammate,
	TWeakObjectPtr<ABwayCharacterWithAbilities> WeakEnemy,
	FGameplayAbilitySpecHandle CombatAbilityHandle,
	FString CombatAbilityName,
	int32 CarrierId,
	int32 TeammateId,
	int32 EnemyId,
	int32 CarrierTeam,
	int32 EnemyTeam,
	FVector CarrierLocation,
	FRotator CarrierRotation,
	FVector TeammateLocation,
	FRotator TeammateRotation,
	FVector EnemyLocation,
	FRotator EnemyRotation,
	FVector Forward)
{
	ABwayCharacterWithAbilities* Carrier = WeakCarrier.Get();
	ABwayCharacterWithAbilities* Teammate = WeakTeammate.Get();
	ABwayCharacterWithAbilities* Enemy = WeakEnemy.Get();
	ABwayGameState* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	TArray<TPair<TWeakObjectPtr<ABwayCharacterWithAbilities>, FTransform>> MovedActors;
	if (!World || !Carrier || !Teammate || !Enemy || !GameState)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatTest: delayed team/pass phase lost its actors or game state"));
		return;
	}

	// The first primary may have a short fire interval. Wait for that real ability
	// to finish before exercising the same-team rejection path.
	Teammate->SetActorLocation(CarrierLocation + Carrier->GetActorForwardVector().GetSafeNormal2D() * 120.0f, false, nullptr, ETeleportType::TeleportPhysics);
	Enemy->SetActorLocation(CarrierLocation + FVector(0.0f, 700.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	ULyraHealthComponent* TeammateHealth = GetHealth(Teammate);
	const float TeammateBefore = TeammateHealth ? TeammateHealth->GetHealth() : -1.0f;
	ULyraAbilitySystemComponent* CarrierCharacterASC = GetCharacterASC(Carrier);
	const bool bSameTeamActivated = TryActivateAbilityHandle(CarrierCharacterASC, CombatAbilityHandle, CombatAbilityName);
	const float TeammateAfter = TeammateHealth ? TeammateHealth->GetHealth() : -1.0f;
	UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: SameTeamDamage carrier=%d target=%d sharedTeam=%d activated=%d healthBefore=%.1f healthAfter=%.1f rejected=%d"),
		CarrierId, TeammateId, CarrierTeam, bSameTeamActivated, TeammateBefore, TeammateAfter,
		TeammateBefore >= 0.0f && TeammateAfter >= 0.0f && FMath::IsNearlyEqual(TeammateBefore, TeammateAfter));

	ARelicActor* Relic = FindRelic(World);
	if (!Relic)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatTest: no relic actor found for pass fixture"));
	}
	else
	{
		// Keep unrelated humans/bots from racing the authoritative receive call.
		for (TActorIterator<ABwayCharacterWithAbilities> It(World); It; ++It)
		{
			ABwayCharacterWithAbilities* Other = *It;
			if (Other && Other != Carrier && Other != Teammate && Other != Enemy)
			{
				MovedActors.Emplace(Other, Other->GetActorTransform());
				Other->SetActorLocation(CarrierLocation + FVector(0.0f, 10000.0f, 2000.0f), false, nullptr, ETeleportType::TeleportPhysics);
			}
		}

		// Always force a loose state first, then move the relic away from bot
		// overlap volumes before invoking the production pickup path.
		Relic->OnDropped();
		Relic->SetActorLocation(CarrierLocation + FVector(0.0f, 0.0f, 1000.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Carrier->SetActorLocation(CarrierLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Teammate->SetActorLocation(CarrierLocation + Forward * 400.0f, false, nullptr, ETeleportType::TeleportPhysics);
		Carrier->SetActorRotation((Teammate->GetActorLocation() - CarrierLocation).Rotation());
		Relic->OnPickedUp(Carrier);
		ULyraAbilitySystemComponent* CarrierPlayerStateASC = GetPlayerStateASC(Carrier);
		const bool bPassActivated = TryActivateAbilityClass(CarrierPlayerStateASC, UBwayGameplayAbility_RelicPass::StaticClass());
		const bool bPassStarted = bPassActivated && Relic->GetCurrentState() == ERelicState::BeingPassed && Relic->CurrentCarrier == nullptr;
		UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: PassActivation carrier=%d teammate=%d enemy=%d sharedTeam=%d enemyTeam=%d activated=%d started=%d state=%d"),
			CarrierId, TeammateId, EnemyId, CarrierTeam, EnemyTeam, bPassActivated, bPassStarted, static_cast<int32>(Relic->GetCurrentState()));

		if (bPassStarted)
		{
			Relic->OnPickedUp(Teammate);
		}
		const int32 ReceivedTeam = Relic->CurrentCarrier && Relic->CurrentCarrier->GetPlayerState()
			? GameState->GetPlayerTeam(Relic->CurrentCarrier->GetPlayerState()) : INDEX_NONE;
		const bool bCarrierMatches = Relic->CurrentCarrier == Teammate;
		const bool bSameTeam = ReceivedTeam == CarrierTeam;
		const bool bReceived = bCarrierMatches && bSameTeam && GameState->AreOnSameTeam(Carrier, Teammate);
		UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: PassReceive teammate=%d received=%d carrierMatches=%d sameTeam=%d receivedTeam=%d carrierNow=%s state=%d"),
			TeammateId, bReceived, bCarrierMatches, bSameTeam, ReceivedTeam, *GetNameSafe(Relic->CurrentCarrier), static_cast<int32>(Relic->GetCurrentState()));

		if (Relic->CurrentCarrier)
		{
			Relic->OnDropped();
		}
	}

	Carrier->SetActorLocation(CarrierLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Carrier->SetActorRotation(CarrierRotation);
	Teammate->SetActorLocation(TeammateLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Teammate->SetActorRotation(TeammateRotation);
	Enemy->SetActorLocation(EnemyLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Enemy->SetActorRotation(EnemyRotation);
	for (const TPair<TWeakObjectPtr<ABwayCharacterWithAbilities>, FTransform>& MovedActor : MovedActors)
	{
		if (ABwayCharacterWithAbilities* Other = MovedActor.Key.Get())
		{
			Other->SetActorTransform(MovedActor.Value, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
	RequestPeerConvergenceSamples(World, EnemyId);
	UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: COMPLETE authority fixture restored actor transforms"));
}

struct FRemoteFixtureContext
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ABwayPlayerController> Controller;
	TWeakObjectPtr<ABwayCharacterWithAbilities> Carrier;
	TWeakObjectPtr<ABwayCharacterWithAbilities> Teammate;
	TWeakObjectPtr<ABwayCharacterWithAbilities> Enemy;
	TWeakObjectPtr<ARelicActor> Relic;
	TWeakObjectPtr<ABwayGoalVolume> EnemyGoal;
	int32 CarrierId = INDEX_NONE;
	int32 TeammateId = INDEX_NONE;
	int32 EnemyId = INDEX_NONE;
	int32 CarrierTeam = INDEX_NONE;
	int32 EnemyTeam = INDEX_NONE;
	int32 ScoreTeamBefore = INDEX_NONE;
	int32 ScorerObjectiveBefore = INDEX_NONE;
	int32 ScorerId = INDEX_NONE;
	float EnemyBefore = -1.0f;
	FVector CarrierLocation = FVector::ZeroVector;
	FRotator CarrierRotation = FRotator::ZeroRotator;
	FVector TeammateLocation = FVector::ZeroVector;
	FRotator TeammateRotation = FRotator::ZeroRotator;
	FVector EnemyLocation = FVector::ZeroVector;
	FRotator EnemyRotation = FRotator::ZeroRotator;
	FVector Forward = FVector::ForwardVector;
	FGameplayTag TeammateRequestTag;
	bool bScoreAfterPass = false;
	uint8 TeammateMovementMode = MOVE_Walking;
	uint8 TeammateCustomMovementMode = 0;
	bool bTeammateMovementFrozen = false;
	TArray<TPair<TWeakObjectPtr<ABwayCharacterWithAbilities>, FTransform>> MovedActors;
};

void RestoreRemoteFixture(const TSharedPtr<FRemoteFixtureContext>& Context);
void PrepareRemotePass(const TSharedPtr<FRemoteFixtureContext>& Context);
void WaitForRemoteDamage(const TSharedPtr<FRemoteFixtureContext>& Context, int32 Attempt);
void VerifyRemoteScore(const TSharedPtr<FRemoteFixtureContext>& Context, int32 Attempt);

void PrepareRemoteScore(const TSharedPtr<FRemoteFixtureContext>& Context)
{
	UWorld* World = Context.IsValid() ? Context->World.Get() : nullptr;
	ABwayCharacterWithAbilities* Scorer = Context.IsValid() ? Context->Teammate.Get() : nullptr;
	ARelicActor* Relic = Context.IsValid() ? Context->Relic.Get() : nullptr;
	ABwayGameState* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	if (!World || !Scorer || !Relic || !GameState || Relic->CurrentCarrier != Scorer || Relic->GetCurrentState() != ERelicState::Carried)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatRemote: RemoteScoreSetup requires the passed relic to remain carried by the intended remote scorer"));
		RestoreRemoteFixture(Context);
		return;
	}

	ABwayGoalVolume* EnemyGoal = nullptr;
	for (TActorIterator<ABwayGoalVolume> It(World); It; ++It)
	{
		if (*It && (*It)->OwningTeam == Context->EnemyTeam)
		{
			EnemyGoal = *It;
			break;
		}
	}
	if (!EnemyGoal)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatRemote: RemoteScoreSetup could not find the opposing goal for team=%d"), Context->CarrierTeam);
		RestoreRemoteFixture(Context);
		return;
	}

	Context->EnemyGoal = EnemyGoal;
	Context->ScorerId = Scorer->GetPlayerState() ? Scorer->GetPlayerState()->GetPlayerId() : INDEX_NONE;
	Context->ScoreTeamBefore = INDEX_NONE;
	if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
	{
		Context->ScoreTeamBefore = Scoring->GetTeamScore(Context->CarrierTeam);
	}
	Context->ScorerObjectiveBefore = Scorer->GetPlayerState<ABwayPlayerState>()
		? Scorer->GetPlayerState<ABwayPlayerState>()->GetObjectiveScore() : INDEX_NONE;

	// The receiver remains frozen from pass preparation so a remote movement
	// correction cannot pull it back out of the overlap before the score delay.
	Scorer->SetActorLocation(EnemyGoal->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
	Scorer->SetActorRotation((EnemyGoal->GetActorLocation() - Scorer->GetActorLocation()).Rotation());
	for (UActorComponent* Component : EnemyGoal->GetComponents())
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
		{
			Primitive->UpdateOverlaps();
		}
	}
	UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: RemoteScoreSetup scorer=%d scoringTeam=%d goalTeam=%d scoreBefore=%d objectiveBefore=%d goal=%s"),
		Context->ScorerId, Context->CarrierTeam, EnemyGoal->OwningTeam, Context->ScoreTeamBefore,
		Context->ScorerObjectiveBefore, *GetNameSafe(EnemyGoal));

	FTimerHandle VerifyTimer;
	World->GetTimerManager().SetTimer(
		VerifyTimer,
		FTimerDelegate::CreateLambda([Context]()
		{
			VerifyRemoteScore(Context, 0);
		}),
		0.75f,
		false);
}

void RestoreRemoteFixture(const TSharedPtr<FRemoteFixtureContext>& Context)
{
	if (!Context.IsValid())
	{
		return;
	}
	if (Context->TeammateRequestTag.IsValid())
	{
		if (ULyraAbilitySystemComponent* TeammateASC = GetCharacterASC(Context->Teammate.Get()))
		{
			TeammateASC->RemoveLooseGameplayTag(Context->TeammateRequestTag);
		}
		Context->TeammateRequestTag = FGameplayTag();
	}
	if (Context->bTeammateMovementFrozen)
	{
		if (ABwayCharacterWithAbilities* Teammate = Context->Teammate.Get())
		{
			if (UCharacterMovementComponent* Movement = Teammate->GetCharacterMovement())
			{
				Movement->SetMovementMode(static_cast<EMovementMode>(Context->TeammateMovementMode), Context->TeammateCustomMovementMode);
			}
		}
		Context->bTeammateMovementFrozen = false;
	}

	if (ARelicActor* Relic = Context->Relic.Get())
	{
		if (Relic->CurrentCarrier)
		{
			Relic->OnDropped();
		}
	}
	if (ABwayCharacterWithAbilities* Carrier = Context->Carrier.Get())
	{
		Carrier->SetActorLocation(Context->CarrierLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Carrier->SetActorRotation(Context->CarrierRotation);
	}
	if (ABwayCharacterWithAbilities* Teammate = Context->Teammate.Get())
	{
		Teammate->SetActorLocation(Context->TeammateLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Teammate->SetActorRotation(Context->TeammateRotation);
	}
	if (ABwayCharacterWithAbilities* Enemy = Context->Enemy.Get())
	{
		Enemy->SetActorLocation(Context->EnemyLocation, false, nullptr, ETeleportType::TeleportPhysics);
		Enemy->SetActorRotation(Context->EnemyRotation);
	}
	for (const TPair<TWeakObjectPtr<ABwayCharacterWithAbilities>, FTransform>& MovedActor : Context->MovedActors)
	{
		if (ABwayCharacterWithAbilities* Other = MovedActor.Key.Get())
		{
			Other->SetActorTransform(MovedActor.Value, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void VerifyRemoteScore(const TSharedPtr<FRemoteFixtureContext>& Context, int32 Attempt)
{
	UWorld* World = Context.IsValid() ? Context->World.Get() : nullptr;
	ABwayCharacterWithAbilities* Scorer = Context.IsValid() ? Context->Teammate.Get() : nullptr;
	ABwayGameState* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	if (!World || !Scorer || !GameState)
	{
		return;
	}

	int32 ScoreAfter = INDEX_NONE;
	if (UBwayScoringComponent* Scoring = GameState->FindComponentByClass<UBwayScoringComponent>())
	{
		ScoreAfter = Scoring->GetTeamScore(Context->CarrierTeam);
	}
	const int32 ObjectiveAfter = Scorer->GetPlayerState<ABwayPlayerState>()
		? Scorer->GetPlayerState<ABwayPlayerState>()->GetObjectiveScore() : INDEX_NONE;
	const bool bTeamScoreChanged = Context->ScoreTeamBefore >= 0 && ScoreAfter == Context->ScoreTeamBefore + 1;
	const bool bObjectiveScoreChanged = Context->ScorerObjectiveBefore >= 0 && ObjectiveAfter == Context->ScorerObjectiveBefore + 1;
	const bool bRoundComplete = GameState->FindComponentByClass<UBwayRoundManagementComponent>()
		&& GameState->FindComponentByClass<UBwayRoundManagementComponent>()->GetCurrentRoundState() == ERoundState::RoundComplete;
	if (bTeamScoreChanged && bObjectiveScoreChanged)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: RemoteGoalOverlap scorer=%d entered=1 goalTeam=%d scoringTeam=%d"),
			Context->ScorerId, Context->EnemyTeam, Context->CarrierTeam);
		UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: RemoteScore scorer=%d scoringTeam=%d scoreBefore=%d scoreAfter=%d objectiveBefore=%d objectiveAfter=%d roundComplete=%d credited=%d"),
			Context->ScorerId, Context->CarrierTeam, Context->ScoreTeamBefore, ScoreAfter,
			Context->ScorerObjectiveBefore, ObjectiveAfter, bRoundComplete ? 1 : 0, bObjectiveScoreChanged ? 1 : 0);
		RestoreRemoteFixture(Context);
		UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: RemoteScore COMPLETE remote-owned damage/pass/goal fixture restored actor transforms"));
		return;
	}

	if (Attempt < 40)
	{
		FTimerHandle RetryTimer;
		World->GetTimerManager().SetTimer(
			RetryTimer,
			FTimerDelegate::CreateLambda([Context, Attempt]()
			{
				VerifyRemoteScore(Context, Attempt + 1);
			}),
			0.25f,
			false);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: RemoteScore TIMEOUT scorer=%d scoreBefore=%d scoreAfter=%d objectiveBefore=%d objectiveAfter=%d roundComplete=%d"),
		Context->ScorerId, Context->ScoreTeamBefore, ScoreAfter, Context->ScorerObjectiveBefore, ObjectiveAfter,
		bRoundComplete ? 1 : 0);
	RestoreRemoteFixture(Context);
}

void VerifyRemotePass(const TSharedPtr<FRemoteFixtureContext>& Context, int32 Attempt)
{
	UWorld* World = Context.IsValid() ? Context->World.Get() : nullptr;
	ARelicActor* Relic = Context.IsValid() ? Context->Relic.Get() : nullptr;
	ABwayCharacterWithAbilities* Teammate = Context.IsValid() ? Context->Teammate.Get() : nullptr;
	ABwayGameState* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	if (!World || !Relic || !Teammate || !GameState)
	{
		return;
	}

	const int32 ReceivedTeam = Relic->CurrentCarrier && Relic->CurrentCarrier->GetPlayerState()
		? GameState->GetPlayerTeam(Relic->CurrentCarrier->GetPlayerState()) : INDEX_NONE;
	const bool bCarrierMatches = Relic->CurrentCarrier == Teammate;
	const bool bSameTeam = ReceivedTeam == Context->CarrierTeam;
	const bool bReceived = bCarrierMatches && bSameTeam && GameState->AreOnSameTeam(Context->Carrier.Get(), Teammate);
	if (bReceived)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: PassReceive carrier=%d teammate=%d received=%d carrierMatches=%d sameTeam=%d receivedTeam=%d state=%d"),
			Context->CarrierId, Context->TeammateId, bReceived ? 1 : 0, bCarrierMatches ? 1 : 0,
			bSameTeam ? 1 : 0, ReceivedTeam, static_cast<int32>(Relic->GetCurrentState()));
		if (Context->bScoreAfterPass)
		{
			// Collect the three adverse-latency damage/team samples before the
			// goal ends the round and legitimately resets pawn health. This keeps
			// the replication probe paired on one stable gameplay state while the
			// score markers independently prove the subsequent round transition.
			RequestPeerConvergenceSamples(World, Context->EnemyId);
			FTimerHandle ScoreTimer;
			World->GetTimerManager().SetTimer(
				ScoreTimer,
				FTimerDelegate::CreateLambda([Context]()
				{
					PrepareRemoteScore(Context);
				}),
				8.0f,
				false);
			return;
		}
		RestoreRemoteFixture(Context);
		UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: COMPLETE remote-owned damage/pass fixture restored actor transforms"));
		return;
	}

	// A server-only pass can remain in flight for several seconds while the
	// reliable activation and movement corrections drain under 100 ms lag.
	// Keep the retry bounded, but long enough to observe a valid receive.
	if (Attempt < 60)
	{
		FTimerHandle RetryTimer;
		World->GetTimerManager().SetTimer(
			RetryTimer,
			FTimerDelegate::CreateLambda([Context, Attempt]()
			{
				VerifyRemotePass(Context, Attempt + 1);
			}),
			0.25f,
			false);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: PassReceive TIMEOUT carrier=%d teammate=%d carrierMatches=%d sameTeam=%d state=%d"),
		Context->CarrierId, Context->TeammateId, bCarrierMatches ? 1 : 0, bSameTeam ? 1 : 0,
		static_cast<int32>(Relic->GetCurrentState()));
	RestoreRemoteFixture(Context);
}

void PrepareRemotePass(const TSharedPtr<FRemoteFixtureContext>& Context)
{
	UWorld* World = Context.IsValid() ? Context->World.Get() : nullptr;
	ABwayCharacterWithAbilities* Carrier = Context.IsValid() ? Context->Carrier.Get() : nullptr;
	ABwayCharacterWithAbilities* Teammate = Context.IsValid() ? Context->Teammate.Get() : nullptr;
	ABwayCharacterWithAbilities* Enemy = Context.IsValid() ? Context->Enemy.Get() : nullptr;
	ABwayPlayerController* Controller = Context.IsValid() ? Context->Controller.Get() : nullptr;
	if (!World || !Carrier || !Teammate || !Enemy || !Controller)
	{
		return;
	}

	const float EnemyAfter = GetHealth(Enemy) ? GetHealth(Enemy)->GetHealth() : -1.0f;
	UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: RemoteDamage carrier=%d target=%d sourceTeam=%d targetTeam=%d healthBefore=%.1f healthAfter=%.1f changed=%d"),
		Context->CarrierId, Context->EnemyId, Context->CarrierTeam, Context->EnemyTeam,
		Context->EnemyBefore, EnemyAfter, Context->EnemyBefore >= 0.0f && EnemyAfter >= 0.0f && !FMath::IsNearlyEqual(Context->EnemyBefore, EnemyAfter));

	// Put the intended teammate in the pass cone and keep all other humans away
	// so the server validates the same production target-selection path.
	Enemy->SetActorLocation(Context->CarrierLocation + FVector(0.0f, 700.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	Teammate->SetActorLocation(Context->CarrierLocation + Context->Forward * 400.0f, false, nullptr, ETeleportType::TeleportPhysics);
	Carrier->SetActorLocation(Context->CarrierLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Carrier->SetActorRotation((Teammate->GetActorLocation() - Context->CarrierLocation).Rotation());

	ARelicActor* Relic = FindRelic(World);
	if (!Relic)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatRemote: no relic actor found for remote pass fixture"));
		RestoreRemoteFixture(Context);
		return;
	}
	Context->Relic = Relic;
	FGameplayTag RequestTag = Relic->GetRelicSettings() ? Relic->GetRelicSettings()->RequestingTag : FGameplayTag();
	if (!RequestTag.IsValid())
	{
		RequestTag = FGameplayTag::RequestGameplayTag(FName("State.RequestingRelic"), false);
	}
	if (RequestTag.IsValid())
	{
		if (ULyraAbilitySystemComponent* TeammateASC = GetCharacterASC(Teammate))
		{
			TeammateASC->AddLooseGameplayTag(RequestTag);
			Context->TeammateRequestTag = RequestTag;
		}
	}
	if (UCharacterMovementComponent* Movement = Teammate->GetCharacterMovement())
	{
		Context->TeammateMovementMode = static_cast<uint8>(Movement->MovementMode);
		Context->TeammateCustomMovementMode = Movement->CustomMovementMode;
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		Context->bTeammateMovementFrozen = true;
	}
	const bool bResetFeedbackProbe = CVarRemoteRelicResetProbe.GetValueOnGameThread() != 0;
	if (bResetFeedbackProbe)
	{
		CVarRemoteRelicResetProbe->Set(0, ECVF_SetByConsole);
		ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
		UBwayRelicManagerComponent* Manager = GameState ? GameState->FindComponentByClass<UBwayRelicManagerComponent>() : nullptr;
		if (!Manager || Manager->GetRelicActor() != Relic)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatRemote: reset feedback probe needs the production manager's active relic"));
			RestoreRemoteFixture(Context);
			return;
		}
		Relic->OnDropped();
		Relic->SetActorLocation(Context->CarrierLocation + FVector(1000.0f, 1000.0f, 1800.0f), false, nullptr, ETeleportType::TeleportPhysics);
		const int32 BeforeState = int32(Relic->GetCurrentState());
		UE_LOG(LogTemp, Display, TEXT("BwayRelicFeedback: ResetProbeStart before=%d location=%s"),
			BeforeState, *Relic->GetActorLocation().ToCompactString());
		Relic->BeginResettingState();
		Relic->BeginResettingState();
		UE_LOG(LogTemp, Display, TEXT("BwayRelicFeedback: ResetBegin before=%d after=%d requests=2"),
			BeforeState, int32(Relic->GetCurrentState()));
		Manager->ResetRelic();
		Relic->OnDropped();
		UE_LOG(LogTemp, Display, TEXT("BwayRelicFeedback: ResetProbe after=%d requests=2 managerCalls=1 carrierClear=%d location=%s"),
			int32(Relic->GetCurrentState()), Relic->CurrentCarrier == nullptr, *Relic->GetActorLocation().ToCompactString());
		// A genuine pickup/release must still emit the drop sound, unlike reset.
		Relic->SetActorLocation(Context->CarrierLocation + FVector(0.0f, 0.0f, 1000.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Relic->OnPickedUp(Carrier);
		UE_LOG(LogTemp, Display, TEXT("BwayRelicFeedback: DropProbeStart before=%d carrierMatches=%d"),
			int32(Relic->GetCurrentState()), Relic->CurrentCarrier == Carrier);
	}
	Relic->OnDropped();
	if (bResetFeedbackProbe)
	{
		Relic->OnDropped();
		UE_LOG(LogTemp, Display, TEXT("BwayRelicFeedback: DropProbe after=%d requests=2"), int32(Relic->GetCurrentState()));
	}
	Relic->SetActorLocation(Context->CarrierLocation + FVector(0.0f, 0.0f, 1000.0f), false, nullptr, ETeleportType::TeleportPhysics);
	Relic->OnPickedUp(Carrier);
	UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: PassSetup carrier=%d teammate=%d enemy=%d state=%d"),
		Context->CarrierId, Context->TeammateId, Context->EnemyId, static_cast<int32>(Relic->GetCurrentState()));

	Controller->Client_RunTeamRemoteAction(Context->TeammateId, Context->EnemyId, true);
	FTimerHandle VerifyTimer;
	World->GetTimerManager().SetTimer(
		VerifyTimer,
		FTimerDelegate::CreateLambda([Context]()
		{
			VerifyRemotePass(Context, 0);
		}),
		1.5f,
		false);
}

void WaitForRemoteDamage(const TSharedPtr<FRemoteFixtureContext>& Context, int32 Attempt)
{
	UWorld* World = Context.IsValid() ? Context->World.Get() : nullptr;
	ABwayCharacterWithAbilities* Enemy = Context.IsValid() ? Context->Enemy.Get() : nullptr;
	if (!World || !Enemy)
	{
		return;
	}

	const float EnemyAfter = GetHealth(Enemy) ? GetHealth(Enemy)->GetHealth() : -1.0f;
	if (Context->EnemyBefore >= 0.0f && EnemyAfter >= 0.0f && !FMath::IsNearlyEqual(Context->EnemyBefore, EnemyAfter))
	{
		PrepareRemotePass(Context);
		return;
	}

	if (Attempt < 20)
	{
		FTimerHandle RetryTimer;
		World->GetTimerManager().SetTimer(
			RetryTimer,
			FTimerDelegate::CreateLambda([Context, Attempt]()
			{
				WaitForRemoteDamage(Context, Attempt + 1);
			}),
			0.5f,
			false);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: RemoteDamage TIMEOUT carrier=%d target=%d healthBefore=%.1f healthAfter=%.1f"),
		Context->CarrierId, Context->EnemyId, Context->EnemyBefore, EnemyAfter);
	RestoreRemoteFixture(Context);
}

void RunRemoteFixture(UWorld* World, int32 Attempt, bool bScoreAfterPass = false)
{
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: requires an authoritative game world"));
		return;
	}

	ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	UBwayRoundManagementComponent* RoundManagement = GameState
		? GameState->FindComponentByClass<UBwayRoundManagementComponent>()
		: nullptr;
	if (!RoundManagement || RoundManagement->GetCurrentMatchPhase() != EBwayMatchPhase::Playing)
	{
		if (Attempt < 120)
		{
			TWeakObjectPtr<UWorld> WeakWorld(World);
			FTimerHandle RetryTimer;
			World->GetTimerManager().SetTimer(
				RetryTimer,
				FTimerDelegate::CreateLambda([WeakWorld, Attempt, bScoreAfterPass]()
				{
					if (UWorld* RetryWorld = WeakWorld.Get())
					{
						RunRemoteFixture(RetryWorld, Attempt + 1, bScoreAfterPass);
					}
				}),
				0.5f,
				false);
			if (Attempt == 0)
			{
				UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: waiting for Playing phase before selecting fixture actors"));
			}
			return;
		}

		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatRemote: timed out waiting for Playing phase before selecting fixture actors"));
		return;
	}

	FTeamPair Pair;
	if (!GameState || !FindTeamPair(World, GameState, Pair, false, bScoreAfterPass))
	{
		if (GameState && Attempt < 20)
		{
			FTimerHandle RetryTimer;
			World->GetTimerManager().SetTimer(
				RetryTimer,
				FTimerDelegate::CreateLambda([World, Attempt, bScoreAfterPass]()
				{
					RunRemoteFixture(World, Attempt + 1, bScoreAfterPass);
				}),
				0.5f,
				false);
			if (Attempt == 0)
			{
				UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: waiting for two alive same-team humans with a remote carrier"));
			}
			return;
		}
		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatRemote: could not find a remote carrier, teammate, and enemy after %.1f seconds"), Attempt * 0.5f);
		return;
	}

	ABwayPlayerController* RemoteController = Cast<ABwayPlayerController>(Pair.Carrier->GetController());
	if (!RemoteController || RemoteController->IsLocalController())
	{
		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatRemote: selected carrier is not owned by a remote BwayPlayerController"));
		return;
	}

	const int32 CarrierTeam = GameState->GetPlayerTeam(Pair.Carrier->GetPlayerState());
	const int32 EnemyTeam = GameState->GetPlayerTeam(Pair.Enemy->GetPlayerState());
	const int32 CarrierId = Pair.Carrier->GetPlayerState()->GetPlayerId();
	const int32 TeammateId = Pair.Teammate->GetPlayerState()->GetPlayerId();
	const int32 EnemyId = Pair.Enemy->GetPlayerState()->GetPlayerId();
	const FVector CarrierLocation = Pair.Carrier->GetActorLocation();
	const FVector Forward = Pair.Carrier->GetActorForwardVector().GetSafeNormal2D();
	const FVector EnemyTestLocation = CarrierLocation + Forward * 120.0f;

	const FGameplayTag CarrierTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.State.RelicCarrier"), false);
	if (CarrierTag.IsValid() && GetPlayerStateASC(Pair.Carrier) && GetPlayerStateASC(Pair.Carrier)->HasMatchingGameplayTag(CarrierTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: selected remote carrier already has the relic; retry after a fresh round"));
		return;
	}

	auto Context = MakeShared<FRemoteFixtureContext>();
	Context->World = World;
	Context->Controller = RemoteController;
	Context->Carrier = Pair.Carrier;
	Context->Teammate = Pair.Teammate;
	Context->Enemy = Pair.Enemy;
	Context->CarrierId = CarrierId;
	Context->TeammateId = TeammateId;
	Context->EnemyId = EnemyId;
	Context->CarrierTeam = CarrierTeam;
	Context->EnemyTeam = EnemyTeam;
	Context->bScoreAfterPass = bScoreAfterPass;
	Context->CarrierLocation = CarrierLocation;
	Context->CarrierRotation = Pair.Carrier->GetActorRotation();
	Context->TeammateLocation = Pair.Teammate->GetActorLocation();
	Context->TeammateRotation = Pair.Teammate->GetActorRotation();
	Context->EnemyLocation = Pair.Enemy->GetActorLocation();
	Context->EnemyRotation = Pair.Enemy->GetActorRotation();
	Context->Forward = Forward.IsNearlyZero() ? FVector::ForwardVector : Forward;
	Context->EnemyBefore = GetHealth(Pair.Enemy) ? GetHealth(Pair.Enemy)->GetHealth() : -1.0f;

	for (TActorIterator<ABwayCharacterWithAbilities> It(World); It; ++It)
	{
		ABwayCharacterWithAbilities* Other = *It;
		if (Other && Other != Pair.Carrier && Other != Pair.Teammate && Other != Pair.Enemy)
		{
			Context->MovedActors.Emplace(Other, Other->GetActorTransform());
			Other->SetActorLocation(CarrierLocation + FVector(0.0f, 10000.0f, 2000.0f), false, nullptr, ETeleportType::TeleportPhysics);
		}
	}

	Pair.Carrier->SetActorRotation((EnemyTestLocation - CarrierLocation).Rotation());
	Pair.Enemy->SetActorLocation(EnemyTestLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Pair.Teammate->SetActorLocation(CarrierLocation + FVector(0.0f, 500.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: Setup carrier=%s/%d teammate=%s/%d enemy=%s/%d teams=%d/%d controllerLocal=%d netMode=%d role=%d remoteRole=%d netConnection=%s"),
		*GetNameSafe(Pair.Carrier), CarrierId, *GetNameSafe(Pair.Teammate), TeammateId,
		*GetNameSafe(Pair.Enemy), EnemyId, CarrierTeam, EnemyTeam,
		RemoteController->IsLocalController() ? 1 : 0, static_cast<int32>(World->GetNetMode()),
		static_cast<int32>(RemoteController->GetLocalRole()), static_cast<int32>(RemoteController->GetRemoteRole()),
		*GetNameSafe(RemoteController->GetNetConnection()));

	TWeakObjectPtr<ABwayPlayerController> WeakRemoteController(RemoteController);
	FTimerHandle ActionTimer;
	World->GetTimerManager().SetTimer(
		ActionTimer,
		FTimerDelegate::CreateLambda([WeakRemoteController, TeammateId, EnemyId]()
		{
			if (ABwayPlayerController* Controller = WeakRemoteController.Get())
			{
				Controller->Client_RunTeamRemoteAction(TeammateId, EnemyId, false);
			}
		}),
		1.00f,
		false);
	FTimerHandle ContinueTimer;
	World->GetTimerManager().SetTimer(
		ContinueTimer,
		FTimerDelegate::CreateLambda([Context]()
		{
			WaitForRemoteDamage(Context, 0);
		}),
		2.50f,
		false);
}

void RunFixture(UWorld* World, int32 Attempt)
{
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatTest: requires an authoritative game world"));
		return;
	}

	ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	FTeamPair Pair;
	if (!GameState || !FindTeamPair(World, GameState, Pair))
	{
		if (GameState && Attempt < 20)
		{
			TWeakObjectPtr<UWorld> WeakWorld(World);
			FTimerHandle RetryTimer;
			World->GetTimerManager().SetTimer(
				RetryTimer,
				FTimerDelegate::CreateLambda([WeakWorld, Attempt]()
				{
					if (UWorld* RetryWorld = WeakWorld.Get())
					{
						RunFixture(RetryWorld, Attempt + 1);
					}
				}),
				0.5f,
				false);
			if (Attempt == 0)
			{
				UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: waiting for two alive same-team humans and one alive enemy"));
			}
			return;
		}

		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatTest: could not find two alive same-team humans and one alive enemy after %.1f seconds"), Attempt * 0.5f);
		return;
	}

	const int32 CarrierTeam = GameState->GetPlayerTeam(Pair.Carrier->GetPlayerState());
	const int32 EnemyTeam = GameState->GetPlayerTeam(Pair.Enemy->GetPlayerState());
	const int32 CarrierId = Pair.Carrier->GetPlayerState()->GetPlayerId();
	const int32 TeammateId = Pair.Teammate->GetPlayerState()->GetPlayerId();
	const int32 EnemyId = Pair.Enemy->GetPlayerState()->GetPlayerId();
	UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: Pair carrier=%s/%d teammate=%s/%d enemy=%s/%d teams=%d/%d"),
		*GetNameSafe(Pair.Carrier), CarrierId, *GetNameSafe(Pair.Teammate), TeammateId,
		*GetNameSafe(Pair.Enemy), EnemyId, CarrierTeam, EnemyTeam);

	const FVector CarrierLocation = Pair.Carrier->GetActorLocation();
	const FRotator CarrierRotation = Pair.Carrier->GetActorRotation();
	const FVector TeammateLocation = Pair.Teammate->GetActorLocation();
	const FRotator TeammateRotation = Pair.Teammate->GetActorRotation();
	const FVector EnemyLocation = Pair.Enemy->GetActorLocation();
	const FRotator EnemyRotation = Pair.Enemy->GetActorRotation();

	ULyraAbilitySystemComponent* CarrierCharacterASC = GetCharacterASC(Pair.Carrier);
	ULyraAbilitySystemComponent* CarrierPlayerStateASC = GetPlayerStateASC(Pair.Carrier);
	if (!CarrierCharacterASC || !CarrierPlayerStateASC)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatTest: carrier character or PlayerState ASC is unavailable"));
		return;
	}

	FGameplayAbilitySpecHandle CombatAbilityHandle;
	FString CombatAbilityName;
	if (!FindCombatAbilityHandle(CarrierCharacterASC, CombatAbilityHandle, CombatAbilityName))
	{
		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatTest: carrier has no primary or melee ability"));
		return;
	}

	const FGameplayTag CarrierTag = FGameplayTag::RequestGameplayTag(FName("Gameplay.State.RelicCarrier"), false);
	if (CarrierTag.IsValid() && CarrierPlayerStateASC->HasMatchingGameplayTag(CarrierTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatTest: selected carrier already has the relic; retry after a fresh round"));
		return;
	}

	// Put the enemy directly in the production combat path, then activate the real ability.
	const FVector Forward = Pair.Carrier->GetActorForwardVector().GetSafeNormal2D();
	const FVector EnemyTestLocation = CarrierLocation + Forward * 120.0f;
	Pair.Carrier->SetActorRotation((EnemyTestLocation - CarrierLocation).Rotation());
	Pair.Enemy->SetActorLocation(EnemyTestLocation, false, nullptr, ETeleportType::TeleportPhysics);
	Pair.Teammate->SetActorLocation(CarrierLocation + FVector(0.0f, 500.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);

	ULyraHealthComponent* EnemyHealth = GetHealth(Pair.Enemy);
	const float EnemyBefore = EnemyHealth ? EnemyHealth->GetHealth() : -1.0f;
	const bool bEnemyActivated = TryActivateAbilityHandle(CarrierCharacterASC, CombatAbilityHandle, CombatAbilityName);
	const float EnemyAfter = EnemyHealth ? EnemyHealth->GetHealth() : -1.0f;
	UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatTest: EnemyDamage carrier=%d target=%d sourceTeam=%d targetTeam=%d activated=%d healthBefore=%.1f healthAfter=%.1f changed=%d"),
		CarrierId, EnemyId, CarrierTeam, EnemyTeam, bEnemyActivated, EnemyBefore, EnemyAfter,
		EnemyBefore >= 0.0f && EnemyAfter >= 0.0f && !FMath::IsNearlyEqual(EnemyBefore, EnemyAfter));
	// Defer the second activation until any multi-shot primary has finished its
	// real fire interval; this keeps the same-team check on the production path.
	TWeakObjectPtr<UWorld> WeakWorld(World);
	TWeakObjectPtr<ABwayCharacterWithAbilities> WeakCarrier(Pair.Carrier);
	TWeakObjectPtr<ABwayCharacterWithAbilities> WeakTeammate(Pair.Teammate);
	TWeakObjectPtr<ABwayCharacterWithAbilities> WeakEnemy(Pair.Enemy);
	FTimerHandle ContinueTimer;
	World->GetTimerManager().SetTimer(
		ContinueTimer,
		FTimerDelegate::CreateLambda([WeakWorld, WeakCarrier, WeakTeammate, WeakEnemy, CombatAbilityHandle, CombatAbilityName,
			CarrierId, TeammateId, EnemyId, CarrierTeam, EnemyTeam, CarrierLocation, CarrierRotation,
			TeammateLocation, TeammateRotation, EnemyLocation, EnemyRotation, Forward]()
		{
			if (UWorld* ContinueWorld = WeakWorld.Get())
			{
				CompleteTeamCombatPass(ContinueWorld, WeakCarrier, WeakTeammate, WeakEnemy, CombatAbilityHandle, CombatAbilityName,
					CarrierId, TeammateId, EnemyId, CarrierTeam, EnemyTeam, CarrierLocation, CarrierRotation,
					TeammateLocation, TeammateRotation, EnemyLocation, EnemyRotation, Forward);
			}
		}),
		1.0f,
		false);
}

void Run(UWorld* World)
{
	RunFixture(World, 0);
}

void RunRemote(UWorld* World)
{
	RunRemoteFixture(World, 0, false);
}

void RunRemoteScore(UWorld* World)
{
	RunRemoteFixture(World, 0, true);
}

bool PollRemoteScoreWorld(float DeltaTime)
{
	if (!GRemoteScorePollActive)
	{
		GRemoteScorePollElapsed = 0.0f;
		return false;
	}

	GRemoteScorePollElapsed += DeltaTime;
	if (UWorld* World = FindAuthorityGameplayWorld())
	{
		GRemoteScorePollActive = false;
		GRemoteScorePollElapsed = 0.0f;
		RunRemoteScore(World);
		return false;
	}

	// Normal hero selection can take 60 seconds before travel and warmup. Keep the
	// startup observer alive across those worlds instead of binding a timer to the
	// frontend/staging world or starting with an incomplete selected roster.
	if (GRemoteScorePollElapsed >= 180.0f)
	{
		GRemoteScorePollActive = false;
		GRemoteScorePollElapsed = 0.0f;
		UE_LOG(LogTemp, Error, TEXT("BwayTeamCombatRemote: delayed score command timed out waiting for Playing with four selected human pawns"));
		return false;
	}

	return true;
}

void StartRemoteScoreDelayed()
{
	if (GRemoteScorePollActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayTeamCombatRemote: delayed score command already waiting for a gameplay world"));
		return;
	}

	GRemoteScorePollActive = true;
	GRemoteScorePollElapsed = 0.0f;
	UE_LOG(LogTemp, Display, TEXT("BwayTeamCombatRemote: delayed score command waiting for an authoritative gameplay world"));
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&PollRemoteScoreWorld), 0.25f);
}

FAutoConsoleCommandWithWorld Command(
	TEXT("bway.Test.TeamCombatPass"),
	TEXT("Development test: exercise real team-filtered combat damage and native relic pass on the authority."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&Run));

FAutoConsoleCommandWithWorld RemoteCommand(
	TEXT("bway.Test.TeamCombatRemote"),
	TEXT("Development test: ask a remote-owned client to run production damage and relic-pass input."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&RunRemote));

FAutoConsoleCommandWithWorld RemoteScoreCommand(
	TEXT("bway.Test.TeamCombatRemoteScore"),
	TEXT("Development test: exercise remote-owned damage/pass followed by natural goal-overlap scoring."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&RunRemoteScore));

FAutoConsoleCommand RemoteScoreDelayedCommand(
	TEXT("bway.Test.TeamCombatRemoteScoreDelayed"),
	TEXT("Development test: wait for an authoritative gameplay world, then exercise remote-owned damage/pass and natural goal-overlap scoring."),
	FConsoleCommandDelegate::CreateStatic(&StartRemoteScoreDelayed));

} // namespace BwayTeamCombatTest

#endif
