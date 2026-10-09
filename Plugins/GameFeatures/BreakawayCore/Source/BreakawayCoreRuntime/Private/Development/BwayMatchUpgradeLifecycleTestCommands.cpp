#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "CoreGlobals.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Containers/Ticker.h"
#include "TimerManager.h"

namespace BwayMatchUpgradeLifecycleTest
{
FName UpgradeIds[] = { TEXT("AttackStrength"), TEXT("Armor"), TEXT("MoveSpeed"), TEXT("CooldownReduction") };
const FName AllUpgradeIds[] = { TEXT("AttackStrength"), TEXT("Armor"), TEXT("MoveSpeed"), TEXT("CooldownReduction"), TEXT("LifeLeech") };

TArray<FGameplayAttribute> UpgradeAttributes()
{
	return { UBwayHeroAttributeSet::GetAttackStrengthAttribute(), UBwayHeroAttributeSet::GetArmorAttribute(),
		UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute(), UBwayHeroAttributeSet::GetCooldownReductionAttribute(),
		UBwayHeroAttributeSet::GetLifeLeechAttribute() };
}

bool RosterModifiersMatch(const ABwayGameState* GameState, bool bPurchased)
{
	if (!GameState || GameState->PlayerArray.Num() != 4) return false;
	const APlayerController* Controller = GameState->GetWorld()->GetFirstPlayerController();
	const ABwayPlayerState* Owner = Controller ? Controller->GetPlayerState<ABwayPlayerState>() : nullptr;
	// Catalogs, like private ranks, replicate only to their owner. The shared catalog
	// describes public attribute bonuses without exposing another player's private state.
	const UBwayUpgradeCatalog* Catalog = Owner && Owner->UpgradeComponent ? Owner->UpgradeComponent->GetCatalog() : nullptr;
	if (!Catalog) return false;
	const TArray<FGameplayAttribute> Attributes = UpgradeAttributes();
	for (const APlayerState* State : GameState->PlayerArray)
	{
		const ABwayPlayerState* Player = Cast<ABwayPlayerState>(State);
		const ULyraAbilitySystemComponent* ASC = Player ? Player->GetLyraAbilitySystemComponent() : nullptr;
		if (!ASC || !ASC->GetSet<UBwayHeroAttributeSet>()) return false;
		for (int32 Index = 0; Index < Attributes.Num(); ++Index)
		{
			const FBwayUpgradeDefinition* Upgrade = Catalog->FindUpgrade(AllUpgradeIds[Index]);
			if (!Upgrade || Upgrade->Ranks.IsEmpty()) return false;
			bool bSelected = false;
			for (const FName Id : UpgradeIds) bSelected |= Id == AllUpgradeIds[Index];
			const float Base = ASC->GetNumericAttributeBase(Attributes[Index]);
			const float Current = ASC->GetNumericAttribute(Attributes[Index]);
			const float Expected = Base + (bPurchased && bSelected ? Upgrade->Ranks[0].Magnitude : 0.0f);
			if (!FMath::IsFinite(Base) || !FMath::IsFinite(Current)
				|| !FMath::IsNearlyEqual(Current, Expected, Index >= 2 ? 0.001f : 0.05f)) return false;
		}
	}
	return true;
}

void LogRosterModifiers(const ABwayGameState* GameState, int32 Observer, const TCHAR* Stage)
{
	const TArray<FGameplayAttribute> Attributes = UpgradeAttributes();
	for (const APlayerState* State : GameState->PlayerArray)
	{
		const ABwayPlayerState* Player = CastChecked<ABwayPlayerState>(State);
		const ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
		UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: RematchAttributes stage=%s observer=%d player=%d attack=%.3f attackBase=%.3f armor=%.3f armorBase=%.3f move=%.3f moveBase=%.3f cooldown=%.3f cooldownBase=%.3f leech=%.4f leechBase=%.4f"),
			Stage, Observer, Player->GetPlayerId(), ASC->GetNumericAttribute(Attributes[0]), ASC->GetNumericAttributeBase(Attributes[0]),
			ASC->GetNumericAttribute(Attributes[1]), ASC->GetNumericAttributeBase(Attributes[1]),
			ASC->GetNumericAttribute(Attributes[2]), ASC->GetNumericAttributeBase(Attributes[2]),
			ASC->GetNumericAttribute(Attributes[3]), ASC->GetNumericAttributeBase(Attributes[3]),
			ASC->GetNumericAttribute(Attributes[4]), ASC->GetNumericAttributeBase(Attributes[4]));
	}
}

struct FProbeState
{
	TWeakObjectPtr<ABwayGameState> PreviousGameState;
	TWeakObjectPtr<ABwayGameState> ResetGameState;
	FString OriginalMatchMap;
	bool bSawReset = false;
	double ClearStartedAt = 0.0;
	int32 PreviousPlayerCount = 0;
	int32 PurchasedPlayerId = INDEX_NONE;
	FName PurchasedUpgradeId;
	int32 PurchasedRank = 0;
	float PurchasedGold = 0.0f;
	FBwayPlayerMatchStats PurchasedStats;
	double StartedAt = FPlatformTime::Seconds();
	float Elapsed = 0.0f;
};

TSharedPtr<FProbeState> ActiveProbe;

struct FSetupState
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<ABwayPlayerState> PurchasedPlayer;
	FName PurchasedUpgradeId;
	int32 WinningTeam = 0;
	bool bPurchaseComplete = false;
	bool bPostMatchEndRequested = false;
	double StartedAt = FPlatformTime::Seconds();
	float Elapsed = 0.0f;
};

TSharedPtr<FSetupState> ActiveSetup;

void StartProbe();

UWorld* FindGameWorld(bool bClient)
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (World && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
			&& (World->GetNetMode() == NM_Client) == bClient)
		{
			return World;
		}
	}

	return nullptr;
}

bool HasNonzeroStats(const FBwayPlayerMatchStats& Stats)
{
	return Stats.Kills != 0 || Stats.Deaths != 0 || Stats.Assists != 0 || Stats.GoldEarned != 0
		|| Stats.DamageDealt != 0 || Stats.HealingDone != 0 || Stats.RelicScores != 0
		|| Stats.ForcedFumbles != 0 || Stats.Interceptions != 0 || Stats.BuildablesDestroyed != 0;
}

bool AreStatsClear(const FBwayPlayerMatchStats& Stats)
{
	return !HasNonzeroStats(Stats);
}

bool VerifyDestination(const TSharedPtr<FProbeState>& Probe, float DeltaTime)
{
	if (!Probe.IsValid())
	{
		return false;
	}

	Probe->Elapsed = static_cast<float>(FPlatformTime::Seconds() - Probe->StartedAt);
	UWorld* World = FindGameWorld(false);
	ABwayGameState* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	if (Probe->bSawReset)
	{
		const UBwayRoundManagementComponent* Rounds = GameState ? GameState->GetRoundManagement() : nullptr;
		const bool bReady = World && GameState && GameState != Probe->ResetGameState.Get()
			&& UWorld::RemovePIEPrefix(World->URL.Map) == Probe->OriginalMatchMap
			&& Rounds && Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::Playing
			&& RosterModifiersMatch(GameState, false);
		if (!bReady) Probe->ClearStartedAt = 0.0;
		else if (Probe->ClearStartedAt == 0.0) Probe->ClearStartedAt = FPlatformTime::Seconds();
		if (Probe->ClearStartedAt > 0.0 && FPlatformTime::Seconds() - Probe->ClearStartedAt >= 2.0)
		{
			const APlayerController* Controller = World->GetFirstPlayerController();
			const ABwayPlayerState* Owner = Controller ? Controller->GetPlayerState<ABwayPlayerState>() : nullptr;
			LogRosterModifiers(GameState, Owner ? Owner->GetPlayerId() : INDEX_NONE, TEXT("NextMatch"));
			UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: HostNextMatch PASS players=4 modifiers=0 phase=Playing world=%s"), *World->GetPathName());
			ActiveProbe.Reset();
			return false;
		}
		if (Probe->Elapsed >= 180.0f)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: HostNextMatch TIMEOUT waiting for cleared modifiers in the next playable match"));
			ActiveProbe.Reset();
			return false;
		}
		return true;
	}
	if (!World || !GameState || GameState == Probe->PreviousGameState.Get())
	{
		if (Probe->Elapsed >= 60.0f)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: TIMEOUT waiting for a fresh authoritative match world"));
			ActiveProbe.Reset();
			return false;
		}
		return true;
	}

	if (GameState->PlayerArray.Num() < Probe->PreviousPlayerCount)
	{
		if (Probe->Elapsed >= 60.0f)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: TIMEOUT waiting for players old=%d new=%d"),
				Probe->PreviousPlayerCount, GameState->PlayerArray.Num());
			ActiveProbe.Reset();
			return false;
		}
		return true;
	}

	int32 NonzeroRanks = 0;
	int32 NonzeroStats = 0;
	int32 NonzeroWallets = 0;
	int32 InvalidPlayers = 0;
	for (APlayerState* State : GameState->PlayerArray)
	{
		ABwayPlayerState* Player = Cast<ABwayPlayerState>(State);
		if (!Player || !Player->UpgradeComponent)
		{
			++InvalidPlayers;
			continue;
		}

		if (!Player->UpgradeComponent->GetOwnedUpgrades().IsEmpty())
		{
			++NonzeroRanks;
		}
		if (!AreStatsClear(Player->GetMatchStatsSnapshot()))
		{
			++NonzeroStats;
		}
		const ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
		const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
		if (!Gold || !FMath::IsFinite(Gold->GetCurrentGold()) || !FMath::IsNearlyZero(Gold->GetCurrentGold()))
		{
			++NonzeroWallets;
		}
	}

	const bool bPassed = InvalidPlayers == 0 && NonzeroRanks == 0 && NonzeroStats == 0 && NonzeroWallets == 0;
	UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: RematchReset verify oldPlayers=%d newPlayers=%d invalid=%d nonzeroRanks=%d nonzeroStats=%d nonzeroWallets=%d elapsed=%.1f"),
		Probe->PreviousPlayerCount, GameState->PlayerArray.Num(), InvalidPlayers, NonzeroRanks, NonzeroStats, NonzeroWallets, Probe->Elapsed);
	if (bPassed)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: RematchReset PASS purchasedPlayer=%d id=%s priorRank=%d priorGold=%.1f priorStatsNonzero=1"),
			Probe->PurchasedPlayerId, *Probe->PurchasedUpgradeId.ToString(), Probe->PurchasedRank, Probe->PurchasedGold);
		Probe->bSawReset = true;
		Probe->ResetGameState = GameState;
		return true;
	}
	else if (Probe->Elapsed < 60.0f)
	{
		return true;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: RematchReset FAIL after %.1fs"), Probe->Elapsed);
	}

	ActiveProbe.Reset();
	return false;
}

bool TickSetup(const TSharedPtr<FSetupState>& Setup, float DeltaTime);

void StartResetProbeAfterSetup(const TSharedPtr<FSetupState>& Setup)
{
	if (!Setup.IsValid())
	{
		return;
	}
	ActiveSetup.Reset();
	StartProbe();
}

bool TickSetup(const TSharedPtr<FSetupState>& Setup, float DeltaTime)
{
	if (!Setup.IsValid())
	{
		return false;
	}

	Setup->Elapsed = static_cast<float>(FPlatformTime::Seconds() - Setup->StartedAt);
	UWorld* World = Setup->World.Get();
	if (!World && !Setup->bPurchaseComplete)
	{
		World = FindGameWorld(false);
		Setup->World = World;
	}
	ABwayGameState* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	UBwayRoundManagementComponent* Rounds = GameState ? GameState->GetRoundManagement() : nullptr;
	if (!World || !GameState || !Rounds)
	{
		if (Setup->Elapsed >= 60.0f)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: Seed TIMEOUT waiting for the authoritative match state"));
			ActiveSetup.Reset();
			return false;
		}
		return true;
	}

	if (!Setup->bPurchaseComplete)
	{
		TArray<ABwayPlayerState*> Humans;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			ABwayPlayerState* Player = Controller ? Controller->GetPlayerState<ABwayPlayerState>() : nullptr;
			if (Player && Player->UpgradeComponent && Player->GetLyraAbilitySystemComponent()
				&& Player->GetLyraAbilitySystemComponent()->GetSet<UBwayGoldAttributeSet>()
				&& Player->UpgradeComponent->GetCatalog())
			{
				Humans.Add(Player);
			}
		}
		if (Humans.Num() < 4)
		{
			if (Setup->Elapsed >= 60.0f)
			{
				UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: Seed TIMEOUT waiting for four ready humans; ready=%d"), Humans.Num());
				ActiveSetup.Reset();
				return false;
			}
			return true;
		}
		if (Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::Playing)
		{
			Rounds->EndRound(Setup->WinningTeam, EBwayWinCondition::GoalScored);
			UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: Seed ended one production round to open the planning purchase window"));
			return true;
		}

		if (Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::PostRound && !Rounds->CheckMatchEnd())
		{
			for (ABwayPlayerState* Player : Humans)
			{
				const UBwayUpgradeCatalog* Catalog = Player->UpgradeComponent->GetCatalog();
				int32 TotalCost = 0;
				for (const FName Id : UpgradeIds)
				{
					const FBwayUpgradeDefinition* Upgrade = Catalog ? Catalog->FindUpgrade(Id) : nullptr;
					if (!Upgrade || Upgrade->Ranks.IsEmpty() || Upgrade->Ranks[0].Cost < 0)
					{
						UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: Seed catalog missing valid rank for %s"), *Id.ToString());
						ActiveSetup.Reset();
						return false;
					}
					TotalCost += Upgrade->Ranks[0].Cost;
				}
				ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
				const UBwayGoldAttributeSet* Gold = ASC->GetSet<UBwayGoldAttributeSet>();
				if (!Player->UpgradeComponent->IsPurchaseWindowOpen())
				{
					UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: Seed purchase window or cost invalid"));
					ActiveSetup.Reset();
					return false;
				}
				const float SeedGold = FMath::Max(500.0f, static_cast<float>(TotalCost) + 100.0f);
				ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), SeedGold);
				Player->AddDamageDealt(1);
				for (const FName UpgradeId : UpgradeIds)
				{
					const int32 Cost = Catalog->FindUpgrade(UpgradeId)->Ranks[0].Cost;
					const float Before = Gold->GetCurrentGold();
					Player->UpgradeComponent->ServerPurchaseUpgrade(UpgradeId, 0);
					const int32 Rank = Player->UpgradeComponent->GetOwnedRank(UpgradeId);
					const float Balance = Gold->GetCurrentGold();
					if (Rank != 1 || !FMath::IsFinite(Balance) || !FMath::IsNearlyEqual(Balance, Before - Cost))
					{
						UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: Seed purchase failed id=%s rank=%d gold=%.1f expected=%.1f"),
							*UpgradeId.ToString(), Rank, Balance, Before - Cost);
						ActiveSetup.Reset();
						return false;
					}
					Setup->PurchasedPlayer = Player;
					Setup->PurchasedUpgradeId = UpgradeId;
					UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: RematchResetSeed purchase player=%d id=%s rank=%d gold=%.1f statsDamage=%d cost=%d before=%.1f"),
						Player->GetPlayerId(), *UpgradeId.ToString(), Rank, Balance, Player->GetDamageDealt(), Cost, Before);
				}
			}
			if (!RosterModifiersMatch(GameState, true))
			{
				UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: Seed four-upgrade modifiers did not match the catalog"));
				ActiveSetup.Reset();
				return false;
			}
			const ABwayPlayerState* Owner = World->GetFirstPlayerController()->GetPlayerState<ABwayPlayerState>();
			LogRosterModifiers(GameState, Owner ? Owner->GetPlayerId() : INDEX_NONE, TEXT("Purchased"));
			Setup->bPurchaseComplete = true;
			Rounds->SetPointsToWin(1);
			UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: Seed purchases complete humans=%d"), Humans.Num());
			return true;
		}
	}

	if (Setup->bPurchaseComplete && !Setup->bPostMatchEndRequested
		&& Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::Playing)
	{
		Setup->bPostMatchEndRequested = true;
		Rounds->EndRound(Setup->WinningTeam, EBwayWinCondition::GoalScored);
		UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: RematchResetSeed requested production match-ending round"));
		return true;
	}

	if (Setup->bPurchaseComplete && Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::PostMatch)
	{
		UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: RematchResetSeed reached PostMatch; starting production reset probe"));
		StartResetProbeAfterSetup(Setup);
		return false;
	}

	if (Setup->Elapsed >= 60.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: RematchResetSeed TIMEOUT phase=%d purchaseComplete=%d"),
			static_cast<int32>(Rounds->GetCurrentMatchPhase()), Setup->bPurchaseComplete ? 1 : 0);
		ActiveSetup.Reset();
		return false;
	}

	return true;
}

void StartProbe()
{
	if (ActiveProbe.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayMatchUpgradeLifecycleTest: probe already active"));
		return;
	}

	UWorld* World = FindGameWorld(false);
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayMatchUpgradeLifecycleTest: requires an authoritative game world"));
		return;
	}

	FTimerHandle Timer;
	World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(World, [World]()
	{
		if (GAllowActorScriptExecutionInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: refusing probe under editor script guard"));
			return;
		}

		ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
		UBwayRoundManagementComponent* Rounds = GameState ? GameState->GetRoundManagement() : nullptr;
		if (!GameState || !Rounds || Rounds->GetCurrentMatchPhase() != EBwayMatchPhase::PostMatch)
		{
			UE_LOG(LogTemp, Warning, TEXT("BwayMatchUpgradeLifecycleTest: requires idle PostMatch; invoke after a completed match"));
			return;
		}

		ABwayPlayerState* PurchasedPlayer = nullptr;
		FName UpgradeId;
		int32 Rank = 0;
		for (APlayerState* State : GameState->PlayerArray)
		{
			ABwayPlayerState* Player = Cast<ABwayPlayerState>(State);
			if (!Player || !Player->UpgradeComponent)
			{
				continue;
			}
			const TArray<FBwayOwnedUpgrade> Owned = Player->UpgradeComponent->GetOwnedUpgrades();
			if (Owned.Num() > 0 && Owned[0].Rank > 0)
			{
				PurchasedPlayer = Player;
				UpgradeId = Owned[0].Id;
				Rank = Owned[0].Rank;
				break;
			}
		}
		if (!PurchasedPlayer)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: setup requires at least one purchased upgrade"));
			return;
		}

		bool bNonzeroStats = false;
		for (APlayerState* State : GameState->PlayerArray)
		{
			if (const ABwayPlayerState* Player = Cast<ABwayPlayerState>(State))
			{
				bNonzeroStats |= HasNonzeroStats(Player->GetMatchStatsSnapshot());
			}
		}
		if (!bNonzeroStats)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: setup requires nonzero match stats"));
			return;
		}

		const ULyraAbilitySystemComponent* ASC = PurchasedPlayer->GetLyraAbilitySystemComponent();
		const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
		ActiveProbe = MakeShared<FProbeState>();
		ActiveProbe->PreviousGameState = GameState;
		ActiveProbe->OriginalMatchMap = UWorld::RemovePIEPrefix(World->URL.Map);
		ActiveProbe->PreviousPlayerCount = GameState->PlayerArray.Num();
		ActiveProbe->PurchasedPlayerId = PurchasedPlayer->GetPlayerId();
		ActiveProbe->PurchasedUpgradeId = UpgradeId;
		ActiveProbe->PurchasedRank = Rank;
		ActiveProbe->PurchasedGold = Gold ? Gold->GetCurrentGold() : -1.0f;
		ActiveProbe->PurchasedStats = PurchasedPlayer->GetMatchStatsSnapshot();
		UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: RematchReset setup players=%d purchasedPlayer=%d id=%s rank=%d gold=%.1f statsNonzero=1"),
			ActiveProbe->PreviousPlayerCount, ActiveProbe->PurchasedPlayerId, *UpgradeId.ToString(), Rank, ActiveProbe->PurchasedGold);

		GameState->RestartMatchFromResults();
		UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: RematchReset requested production RestartMatchFromResults"));
		const TSharedPtr<FProbeState> Probe = ActiveProbe;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float DeltaTime)
		{
			return VerifyDestination(Probe, DeltaTime);
		}), 0.5f);
	}), 0.2f, false);
}

void StartSeed()
{
	if (ActiveSetup.IsValid() || ActiveProbe.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayMatchUpgradeLifecycleTest: setup or reset probe already active"));
		return;
	}

	// Startup ExecCmds can precede the gameplay world. Resolve it on native ticks,
	// outside the Python script guard, and preserve the setup's bounded timeout.
	ActiveSetup = MakeShared<FSetupState>();
	const TSharedPtr<FSetupState> Setup = ActiveSetup;
	UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: RematchResetSeed admitted; waiting for four humans and production Playing/PostRound state"));
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Setup](float DeltaTime)
	{
		if (GAllowActorScriptExecutionInEditor)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: Seed refusing execution under editor script guard"));
			ActiveSetup.Reset();
			return false;
		}
		return TickSetup(Setup, DeltaTime);
	}), 0.5f);
}

struct FClientProbeState
{
	TWeakObjectPtr<ABwayGameState> PreviousGameState;
	TWeakObjectPtr<ABwayPlayerState> PreviousPlayer;
	TWeakObjectPtr<ABwayGameState> SettlingGameState;
	int32 PreviousPlayerId = INDEX_NONE;
	bool bSawPurchase = false;
	bool bSawReset = false;
	bool bContinueToNextMatch = false;
	FString ExpectedMatchMap;
	double StartedAt = FPlatformTime::Seconds();
	double ClearStartedAt = 0.0;
	double NextDiagnosticAt = 0.0;
	float Elapsed = 0.0f;
	float ClearSeconds = 0.0f;
};

TSharedPtr<FClientProbeState> ActiveClientProbe;

bool TickClientProbe(const TSharedPtr<FClientProbeState>& Probe, float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	Probe->Elapsed = static_cast<float>(Now - Probe->StartedAt);
	UWorld* World = FindGameWorld(true);
	ABwayGameState* GameState = World ? World->GetGameState<ABwayGameState>() : nullptr;
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	ABwayPlayerState* Player = Controller ? Controller->GetPlayerState<ABwayPlayerState>() : nullptr;
	const ULyraAbilitySystemComponent* ASC = Player ? Player->GetLyraAbilitySystemComponent() : nullptr;
	const UBwayGoldAttributeSet* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
	if (GameState && Player && Gold && Player->UpgradeComponent)
	{
		if (Probe->bSawReset)
		{
			UBwayRoundManagementComponent* Rounds = GameState->GetRoundManagement();
			APawn* Pawn = Controller->GetPawn();
			const ULyraHealthComponent* Health = Pawn ? ULyraHealthComponent::FindHealthComponent(Pawn) : nullptr;
			const bool bNextMatchReady = GameState != Probe->SettlingGameState.Get()
				&& UWorld::RemovePIEPrefix(World->URL.Map) == Probe->ExpectedMatchMap
				&& Rounds && Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::Playing
				&& Health && Health->GetHealth() > 0.0f && !Health->IsDeadOrDying()
				&& Player->UpgradeComponent->GetOwnedUpgrades().IsEmpty() && Player->GetDamageDealt() == 0
				&& RosterModifiersMatch(GameState, false);
			if (!bNextMatchReady) { Probe->ClearStartedAt = 0.0; }
			else if (Probe->ClearStartedAt == 0.0) { Probe->ClearStartedAt = Now; }
			if (Probe->ClearStartedAt > 0.0 && Now - Probe->ClearStartedAt >= 2.0)
			{
				LogRosterModifiers(GameState, Player->GetPlayerId(), TEXT("NextMatch"));
				UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: ClientNextMatch PASS player=%d owned=0 damage=0 health=%.1f phase=Playing world=%s"),
					Player->GetPlayerId(), Health->GetHealth(), *World->GetPathName());
				ActiveClientProbe.Reset();
				return false;
			}
		}
		else if (!Probe->bSawPurchase)
		{
			const TArray<FBwayOwnedUpgrade> Owned = Player->UpgradeComponent->GetOwnedUpgrades();
			bool bAllRanks = Owned.Num() == UE_ARRAY_COUNT(UpgradeIds);
			for (const FName Id : UpgradeIds) bAllRanks &= Player->UpgradeComponent->GetOwnedRank(Id) == 1;
			if (bAllRanks && Gold->GetCurrentGold() > 0.0f && FMath::IsFinite(Gold->GetCurrentGold())
				&& Player->GetDamageDealt() > 0 && RosterModifiersMatch(GameState, true))
			{
				Probe->PreviousGameState = GameState;
				Probe->PreviousPlayer = Player;
				Probe->PreviousPlayerId = Player->GetPlayerId();
				Probe->bSawPurchase = true;
				Probe->ExpectedMatchMap = UWorld::RemovePIEPrefix(World->URL.Map);
				for (const FName Id : UpgradeIds)
				{
					UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: ClientBefore player=%d id=%s rank=%d gold=%.1f statsDamage=%d world=%s"),
						Player->GetPlayerId(), *Id.ToString(), Player->UpgradeComponent->GetOwnedRank(Id), Gold->GetCurrentGold(), Player->GetDamageDealt(), *World->GetPathName());
				}
				LogRosterModifiers(GameState, Player->GetPlayerId(), TEXT("Purchased"));
			}
		}
		else if (GameState != Probe->PreviousGameState.Get() && Player != Probe->PreviousPlayer.Get()
			&& GameState->PlayerArray.Num() >= 4)
		{
			int32 InvalidPlayers = 0;
			int32 NonzeroStats = 0;
			int32 NonzeroWallets = 0;
			for (APlayerState* State : GameState->PlayerArray)
			{
				const ABwayPlayerState* Peer = Cast<ABwayPlayerState>(State);
				const ULyraAbilitySystemComponent* PeerASC = Peer ? Peer->GetLyraAbilitySystemComponent() : nullptr;
				const UBwayGoldAttributeSet* PeerGold = PeerASC ? PeerASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
				if (!Peer || !PeerGold)
				{
					++InvalidPlayers;
					continue;
				}
				NonzeroStats += !AreStatsClear(Peer->GetMatchStatsSnapshot()) ? 1 : 0;
				NonzeroWallets += !FMath::IsFinite(PeerGold->GetCurrentGold()) || !FMath::IsNearlyZero(PeerGold->GetCurrentGold()) ? 1 : 0;
			}
			const bool bClear = InvalidPlayers == 0 && NonzeroStats == 0 && NonzeroWallets == 0
				&& Player->UpgradeComponent->GetOwnedUpgrades().IsEmpty();
			if (Probe->SettlingGameState != GameState)
			{
				Probe->SettlingGameState = GameState;
				Probe->ClearStartedAt = 0.0;
			}
			// FTSTicker supplies the frame delta, not the interval since this
			// periodic callback. Start a real-time window only after a clear sample.
			if (!bClear) { Probe->ClearStartedAt = 0.0; }
			else if (Probe->ClearStartedAt == 0.0) { Probe->ClearStartedAt = Now; }
			Probe->ClearSeconds = Probe->ClearStartedAt > 0.0 ? static_cast<float>(Now - Probe->ClearStartedAt) : 0.0f;
			if (Now >= Probe->NextDiagnosticAt)
			{
				Probe->NextDiagnosticAt = Now + 5.0;
				UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: ClientDestination player=%d players=%d invalid=%d owned=%d nonzeroStats=%d nonzeroWallets=%d world=%s"),
					Player->GetPlayerId(), GameState->PlayerArray.Num(), InvalidPlayers, Player->UpgradeComponent->GetOwnedUpgrades().Num(), NonzeroStats, NonzeroWallets, *World->GetPathName());
			}
			if (Probe->ClearSeconds >= 2.0f)
			{
				UE_LOG(LogTemp, Display, TEXT("BwayMatchUpgradeLifecycleTest: ClientReset PASS oldPlayer=%d newPlayer=%d players=%d owned=0 nonzeroStats=0 nonzeroWallets=0 settled=%.1f world=%s"),
					Probe->PreviousPlayerId, Player->GetPlayerId(), GameState->PlayerArray.Num(), Probe->ClearSeconds, *World->GetPathName());
				if (!Probe->bContinueToNextMatch)
				{
					ActiveClientProbe.Reset();
					return false;
				}
				Probe->bSawReset = true;
				Probe->ClearStartedAt = 0.0;
			}
		}
		else
		{
			Probe->ClearStartedAt = 0.0;
			Probe->ClearSeconds = 0.0f;
		}
	}
	else
	{
		Probe->ClearStartedAt = 0.0;
		Probe->ClearSeconds = 0.0f;
	}
	if (Probe->Elapsed >= 180.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayMatchUpgradeLifecycleTest: ClientReset TIMEOUT sawPurchase=%d sawReset=%d freshState=%d players=%d elapsed=%.1f"),
			Probe->bSawPurchase ? 1 : 0, Probe->bSawReset ? 1 : 0, GameState && GameState != Probe->PreviousGameState.Get() ? 1 : 0,
			GameState ? GameState->PlayerArray.Num() : 0, Probe->Elapsed);
		ActiveClientProbe.Reset();
		return false;
	}
	return true;
}

void StartClientProbe(bool bContinueToNextMatch)
{
	if (ActiveClientProbe.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayMatchUpgradeLifecycleTest: client observer already active"));
		return;
	}
	ActiveClientProbe = MakeShared<FClientProbeState>();
	ActiveClientProbe->bContinueToNextMatch = bContinueToNextMatch;
	const TSharedPtr<FClientProbeState> Probe = ActiveClientProbe;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float DeltaTime)
	{
		return TickClientProbe(Probe, DeltaTime);
	}), 0.2f);
}

bool ConfigureLoadout(bool bLifeLeech)
{
	if (ActiveSetup || ActiveProbe || ActiveClientProbe)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayMatchUpgradeLifecycleTest: refusing to change loadout while a probe is active"));
		return false;
	}
	// Four equipment slots remain fixed; the alternative replaces cooldown with life leech.
	UpgradeIds[3] = bLifeLeech ? TEXT("LifeLeech") : TEXT("CooldownReduction");
	return true;
}

FAutoConsoleCommand ClientObserveCommand(
	TEXT("bway.Test.MatchUpgradeLifecycleObserve"),
	TEXT("Development client observer: require owned purchased state before rematch, then settled owner-upgrade and four-player stat/wallet reset in a fresh destination."),
	FConsoleCommandDelegate::CreateLambda([]() { if (ConfigureLoadout(false)) StartClientProbe(false); }));

FAutoConsoleCommand ClientFlowObserveCommand(
	TEXT("bway.Test.MatchUpgradeRematchFlowObserve"),
	TEXT("Development client observer: verify purchased state, rematch reset, and a fresh playable match after normal selection."),
	FConsoleCommandDelegate::CreateLambda([]() { if (ConfigureLoadout(false)) StartClientProbe(true); }));

FAutoConsoleCommand RematchResetCommand(
	TEXT("bway.Test.MatchUpgradeRematchReset"),
	TEXT("Development authority fixture: after a completed match with a real purchased upgrade and nonzero stats, request a production rematch and verify fresh players have no upgrades, stats, or wallet."),
	FConsoleCommandDelegate::CreateStatic(&StartProbe));

FAutoConsoleCommand RematchResetSeedCommand(
	TEXT("bway.Test.MatchUpgradeLifecycleSeed"),
	TEXT("Development authority fixture: wait for four ready humans, use production round and purchase APIs to buy each an upgrade and seed stats, reach PostMatch, then verify rematch reset."),
	FConsoleCommandDelegate::CreateLambda([]() { if (ConfigureLoadout(false)) StartSeed(); }));

FAutoConsoleCommand LifeLeechSeedCommand(
	TEXT("bway.Test.MatchUpgradeLifeLeechSeed"),
	TEXT("Development authority fixture: verify rematch reset with attack, armor, movement, and life leech equipped."),
	FConsoleCommandDelegate::CreateLambda([]() { if (ConfigureLoadout(true)) StartSeed(); }));
FAutoConsoleCommand LifeLeechObserveCommand(
	TEXT("bway.Test.MatchUpgradeLifeLeechObserve"),
	TEXT("Development client observer: life-leech loadout, production rematch reset, selection, and fresh playable match."),
	FConsoleCommandDelegate::CreateLambda([]() { if (ConfigureLoadout(true)) StartClientProbe(true); }));
}
#endif
