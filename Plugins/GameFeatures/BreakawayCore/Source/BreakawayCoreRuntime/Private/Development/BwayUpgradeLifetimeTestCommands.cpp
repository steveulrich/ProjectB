#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/PlatformTime.h"
#include "Relic/RelicActor.h"

namespace BwayUpgradeLifetimeTest
{
struct FPlayerBaseline
{
	TWeakObjectPtr<ABwayPlayerState> Player;
	TWeakObjectPtr<ULyraAbilitySystemComponent> ASC;
	TWeakObjectPtr<APawn> BeforeDeathPawn;
	float Attack = 0;
	float Armor = 0;
	float Move = 0;
	float Cooldown = 0;
	float Leech = 0;
	int32 Deaths = 0;
};

enum class EStep { Setup, Baseline, Planning, Purchased, Death, RespawnHold, FinalPlanning, FinalRound };
struct FProbe
{
	bool bClient = false;
	bool bLifeLeech = false;
	TArray<FName> UpgradeIds = { TEXT("AttackStrength"), TEXT("Armor"), TEXT("MoveSpeed"), TEXT("CooldownReduction") };
	EStep Step = EStep::Setup;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<UBwayRoundManagementComponent> Rounds;
	TWeakObjectPtr<UBwayUpgradeCatalog> Catalog;
	TMap<int32, FPlayerBaseline> Players;
	int32 LocalPlayer = INDEX_NONE;
	int32 Victim = INDEX_NONE;
	int32 FirstRound = 0;
	int32 RespawnRound = 0;
	int32 Samples = 0;
	float OriginalDuration = 0;
	float AttackBonus = 0;
	float ArmorBonus = 0;
	float MoveBonus = 0;
	float CooldownBonus = 0;
	float LeechBonus = 0;
	double StartedAt = FPlatformTime::Seconds();
	double StageAt = 0;
	double StableAt = 0;
	double DiagnosticPlayingAt = 0;
	bool bLoggedWait = false;
	FString StableKey;
};
TSharedPtr<FProbe> ActiveProbe;

UWorld* FindWorld(bool bClient)
{
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game)
				&& (World->GetNetMode() == NM_Client) == bClient) return World;
		}
	}
	return nullptr;
}

bool Finish(const TSharedPtr<FProbe>& Probe, const TCHAR* Error = nullptr)
{
	if (!Probe->bClient && Probe->Rounds.IsValid() && Probe->OriginalDuration > 0)
		Probe->Rounds->SetRoundDuration(Probe->OriginalDuration);
	if (Error)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayUpgradeLifetimeTest: FAIL role=%s step=%d reason=%s samples=%d"),
			Probe->bClient ? TEXT("client") : TEXT("host"), static_cast<int32>(Probe->Step), Error, Probe->Samples);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("BwayUpgradeLifetimeTest: %s PASS local=%d victim=%d players=%d samples=%d round=%d durationRestored=1"),
			Probe->bClient ? TEXT("Client") : TEXT("Host"), Probe->LocalPlayer, Probe->Victim, Probe->Players.Num(), Probe->Samples,
			Probe->Rounds->GetCurrentRoundNumber());
	}
	ActiveProbe.Reset();
	return false;
}

void Advance(FProbe& Probe, EStep Step)
{
	Probe.Step = Step;
	Probe.StageAt = FPlatformTime::Seconds();
	Probe.StableAt = 0;
	Probe.DiagnosticPlayingAt = 0;
	Probe.bLoggedWait = false;
	Probe.StableKey.Reset();
}

bool Stable(FProbe& Probe, bool bQualifies, double Seconds = 2.0)
{
	FString Key = FString::Printf(TEXT("%d:%d"), static_cast<int32>(Probe.Rounds->GetCurrentMatchPhase()), Probe.Rounds->GetCurrentRoundNumber());
	for (const auto& Entry : Probe.Players)
	{
		const ABwayPlayerState* Player = Entry.Value.Player.Get();
		Key += FString::Printf(TEXT("|%d:%s:%d"), Entry.Key, *GetPathNameSafe(Player ? Player->GetPawn() : nullptr), Player ? Player->GetDeaths() : -1);
	}
	// Readiness may recover after the pawn/phase key has already replicated.
	// A prior unqualified sample clears the clock; rearm it even if the key is unchanged.
	if (!bQualifies || Key != Probe.StableKey || Probe.StableAt == 0)
	{
		Probe.StableKey = Key;
		Probe.StableAt = bQualifies ? FPlatformTime::Seconds() : 0;
		return false;
	}
	return Probe.StableAt > 0 && FPlatformTime::Seconds() - Probe.StableAt >= Seconds;
}

void LogPlayers(const FProbe& Probe, const TCHAR* Stage)
{
	for (const auto& Entry : Probe.Players)
	{
		const ABwayPlayerState* Player = Entry.Value.Player.Get();
		const ULyraAbilitySystemComponent* ASC = Entry.Value.ASC.Get();
		const UBwayHeroAttributeSet* Hero = ASC->GetSet<UBwayHeroAttributeSet>();
		const ULyraHealthSet* HealthSet = ASC->GetSet<ULyraHealthSet>();
		APawn* ObservedPawn = FCString::Strcmp(Stage, TEXT("Dead")) == 0 && Entry.Value.BeforeDeathPawn.IsValid()
			? Entry.Value.BeforeDeathPawn.Get() : Player->GetPawn();
		const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(ObservedPawn);
		UE_LOG(LogTemp, Display, TEXT("BwayUpgradeLifetimeTest: %s stage=%s player=%d owner=%d attackRank=%d armorRank=%d attack=%.3f attackBase=%.3f attackBonus=%.3f armor=%.3f armorBase=%.3f armorBonus=%.3f health=%.1f deaths=%d pawn=%s round=%d settled=%.3f ascHealth=%.1f avatar=%s dying=%d moveRank=%d cooldownRank=%d move=%.3f moveBase=%.3f moveBonus=%.3f cooldown=%.3f cooldownBase=%.3f cooldownBonus=%.3f asc=%s ps=%s leechRank=%d leech=%.4f leechBase=%.4f leechBonus=%.4f owned=%d"),
			FCString::Strcmp(Stage, TEXT("Diagnostic")) == 0 ? TEXT("State") : TEXT("Sample"), Stage, Entry.Key, Entry.Key == Probe.LocalPlayer,
			Player->UpgradeComponent->GetOwnedRank(TEXT("AttackStrength")), Player->UpgradeComponent->GetOwnedRank(TEXT("Armor")),
			Hero ? Hero->GetAttackStrength() : -999.0f, Entry.Value.Attack, Probe.AttackBonus, Hero ? Hero->GetArmor() : -999.0f, Entry.Value.Armor, Probe.ArmorBonus,
			Health ? Health->GetHealth() : -1.0f, Player->GetDeaths(), *GetNameSafe(ObservedPawn), Probe.Rounds->GetCurrentRoundNumber(),
			Probe.StableAt > 0 ? FPlatformTime::Seconds() - Probe.StableAt : 0.0,
			HealthSet ? HealthSet->GetHealth() : -1.0f, *GetNameSafe(ASC->GetAvatarActor()), Health ? Health->IsDeadOrDying() : -1,
			Player->UpgradeComponent->GetOwnedRank(TEXT("MoveSpeed")), Player->UpgradeComponent->GetOwnedRank(TEXT("CooldownReduction")),
			Hero ? Hero->GetMoveSpeedMultiplier() : -999.f, Entry.Value.Move, Probe.MoveBonus,
			Hero ? Hero->GetCooldownReduction() : -999.f, Entry.Value.Cooldown, Probe.CooldownBonus, *GetNameSafe(ASC), *GetNameSafe(Player),
            Player->UpgradeComponent->GetOwnedRank(TEXT("LifeLeech")), Hero ? Hero->GetLifeLeech() : -999.f,
            Entry.Value.Leech, Probe.LeechBonus, Player->UpgradeComponent->GetOwnedUpgrades().Num());
	}
}

bool AllAlive(const FProbe& Probe)
{
	for (const auto& Entry : Probe.Players)
	{
		const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Entry.Value.Player->GetPawn());
		if (!Health || Health->GetHealth() <= 0 || Health->IsDeadOrDying()) return false;
	}
	return true;
}

bool BonusesMatch(const FProbe& Probe)
{
	for (const auto& Entry : Probe.Players)
	{
		const ABwayPlayerState* Player = Entry.Value.Player.Get();
		const ULyraAbilitySystemComponent* ASC = Entry.Value.ASC.Get();
		if (!Player || !ASC || Player->GetLyraAbilitySystemComponent() != ASC || !Player->UpgradeComponent) return false;
		const UBwayHeroAttributeSet* Hero = ASC->GetSet<UBwayHeroAttributeSet>();
		const int32 Rank = !Probe.bClient || Entry.Key == Probe.LocalPlayer ? 1 : 0;
		if (!Hero || Player->UpgradeComponent->GetOwnedRank(TEXT("AttackStrength")) != Rank
			|| Player->UpgradeComponent->GetOwnedRank(TEXT("Armor")) != Rank
			|| Player->UpgradeComponent->GetOwnedRank(TEXT("MoveSpeed")) != Rank
			|| Player->UpgradeComponent->GetOwnedRank(TEXT("CooldownReduction")) != (Probe.bLifeLeech ? 0 : Rank)
			|| Player->UpgradeComponent->GetOwnedRank(TEXT("LifeLeech")) != (Probe.bLifeLeech ? Rank : 0)
			|| Player->UpgradeComponent->GetOwnedUpgrades().Num() != Rank * Probe.UpgradeIds.Num()
			|| !FMath::IsNearlyEqual(Hero->GetAttackStrength(), Entry.Value.Attack + Probe.AttackBonus, 0.05f)
			|| !FMath::IsNearlyEqual(Hero->GetArmor(), Entry.Value.Armor + Probe.ArmorBonus, 0.05f)
			|| !FMath::IsNearlyEqual(Hero->GetMoveSpeedMultiplier(), Entry.Value.Move + Probe.MoveBonus, 0.001f)
			|| !FMath::IsNearlyEqual(Hero->GetCooldownReduction(), Entry.Value.Cooldown + Probe.CooldownBonus, 0.001f)
			|| !FMath::IsNearlyEqual(Hero->GetLifeLeech(), Entry.Value.Leech + Probe.LeechBonus, 0.001f)) return false;
	}
	return true;
}

bool Initialize(FProbe& Probe, UWorld* World)
{
	ABwayGameState* GameState = World->GetGameState<ABwayGameState>();
	UBwayRoundManagementComponent* Rounds = GameState ? GameState->GetRoundManagement() : nullptr;
	APlayerController* Local = World->GetFirstPlayerController();
	ABwayPlayerState* LocalState = Local ? Local->GetPlayerState<ABwayPlayerState>() : nullptr;
	UBwayUpgradeCatalog* Catalog = LocalState && LocalState->UpgradeComponent ? const_cast<UBwayUpgradeCatalog*>(LocalState->UpgradeComponent->GetCatalog()) : nullptr;
	if (!Rounds || !LocalState || !Catalog || GameState->PlayerArray.Num() != 4
		|| !World->GetMapName().Contains(TEXT("L_BW_Dorado")) || Rounds->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) return false;
	int32 Relics = 0;
	for (TActorIterator<ARelicActor> It(World); It; ++It) ++Relics;
	if (Relics != 1) return false;
	const FBwayUpgradeDefinition* Attack = Catalog->FindUpgrade(TEXT("AttackStrength"));
	const FBwayUpgradeDefinition* Armor = Catalog->FindUpgrade(TEXT("Armor"));
	const FBwayUpgradeDefinition* Move = Catalog->FindUpgrade(TEXT("MoveSpeed"));
	const FBwayUpgradeDefinition* Cooldown = Catalog->FindUpgrade(TEXT("CooldownReduction"));
	const FBwayUpgradeDefinition* Leech = Catalog->FindUpgrade(TEXT("LifeLeech"));
	if (!Attack || !Armor || !Move || !Cooldown || Attack->Ranks.IsEmpty() || Armor->Ranks.IsEmpty() || Move->Ranks.IsEmpty() || Cooldown->Ranks.IsEmpty()) return false;
	if (Probe.bLifeLeech && (!Leech || Leech->Ranks.IsEmpty())) return false;
	TMap<int32, FPlayerBaseline> Players;
	for (APlayerState* State : GameState->PlayerArray)
	{
		ABwayPlayerState* Player = Cast<ABwayPlayerState>(State);
		ULyraAbilitySystemComponent* ASC = Player ? Player->GetLyraAbilitySystemComponent() : nullptr;
		const UBwayHeroAttributeSet* Hero = ASC ? ASC->GetSet<UBwayHeroAttributeSet>() : nullptr;
		const ULyraHealthComponent* Health = Player ? ULyraHealthComponent::FindHealthComponent(Player->GetPawn()) : nullptr;
		if (!Player || Player->IsABot() || !Hero || !Health || Health->GetHealth() <= 0 || Health->IsDeadOrDying()
			|| !Player->UpgradeComponent || !Player->UpgradeComponent->GetOwnedUpgrades().IsEmpty()) return false;
		FPlayerBaseline& Baseline = Players.Add(Player->GetPlayerId());
		Baseline.Player = Player;
		Baseline.ASC = ASC;
		Baseline.Attack = Hero->GetAttackStrength();
		Baseline.Armor = Hero->GetArmor();
		Baseline.Move = Hero->GetMoveSpeedMultiplier();
		Baseline.Cooldown = Hero->GetCooldownReduction();
		Baseline.Leech = Hero->GetLifeLeech();
		Baseline.Deaths = Player->GetDeaths();
	}
	if (Players.Num() != 4) return false;
	Probe.World = World;
	Probe.Rounds = Rounds;
	Probe.Catalog = Catalog;
	Probe.Players = MoveTemp(Players);
	Probe.LocalPlayer = LocalState->GetPlayerId();
	Probe.FirstRound = Rounds->GetCurrentRoundNumber();
	Probe.OriginalDuration = Rounds->RoundDuration;
	Probe.AttackBonus = Attack->Ranks[0].Magnitude;
	Probe.ArmorBonus = Armor->Ranks[0].Magnitude;
	Probe.MoveBonus = Move->Ranks[0].Magnitude;
	Probe.CooldownBonus = Probe.bLifeLeech ? 0.0f : Cooldown->Ranks[0].Magnitude;
	Probe.LeechBonus = Probe.bLifeLeech ? Leech->Ranks[0].Magnitude : 0.0f;
	Advance(Probe, EStep::Baseline);
	UE_LOG(LogTemp, Display, TEXT("BwayUpgradeLifetimeTest: Ready role=%s world=%s players=4 relics=%d"), Probe.bClient ? TEXT("client") : TEXT("host"), *World->GetMapName(), Relics);
	LogPlayers(Probe, TEXT("Baseline"));
	return true;
}

bool Poll(float)
{
	const TSharedPtr<FProbe> Probe = ActiveProbe;
	if (!Probe) return false;
	if (FPlatformTime::Seconds() - Probe->StartedAt > 120) return Finish(Probe, TEXT("timeout"));
	if (GAllowActorScriptExecutionInEditor) return Finish(Probe, TEXT("editor-script-guard"));
	if (Probe->Step == EStep::Setup)
	{
		if (UWorld* World = FindWorld(Probe->bClient)) Initialize(*Probe, World);
		return true;
	}
	if (!Probe->World.IsValid() || !Probe->Rounds.IsValid()) return Finish(Probe, TEXT("world-lost"));
	for (const auto& Entry : Probe->Players)
		if (!Entry.Value.Player.IsValid() || !Entry.Value.ASC.IsValid()) return Finish(Probe, TEXT("persistent-owner-lost"));
	++Probe->Samples;
	UBwayRoundManagementComponent* Rounds = Probe->Rounds.Get();
	const EBwayMatchPhase Phase = Rounds->GetCurrentMatchPhase();
	const bool bPlaying = Phase == EBwayMatchPhase::Playing;
	const bool bPostRound = Phase == EBwayMatchPhase::PostRound;
	if (bPlaying && Probe->DiagnosticPlayingAt == 0) Probe->DiagnosticPlayingAt = FPlatformTime::Seconds();
	if (!bPlaying) Probe->DiagnosticPlayingAt = 0;
	if (!Probe->bLoggedWait && Probe->DiagnosticPlayingAt > 0 && FPlatformTime::Seconds() - Probe->DiagnosticPlayingAt >= 4)
	{
		Probe->bLoggedWait = true;
		UE_LOG(LogTemp, Display, TEXT("BwayUpgradeLifetimeTest: Observe step=%d phase=%d bonuses=%d alive=%d victim=%d"),
			static_cast<int32>(Probe->Step), static_cast<int32>(Phase), BonusesMatch(*Probe), AllAlive(*Probe), Probe->Victim);
		LogPlayers(*Probe, TEXT("Diagnostic"));
	}
	if (Probe->Step > EStep::Purchased && !BonusesMatch(*Probe)) return Finish(Probe, TEXT("lost-rank-or-modifier"));
	if (Probe->Step == EStep::Baseline)
	{
		if (Probe->bClient)
		{
			if (BonusesMatch(*Probe)) Advance(*Probe, EStep::Purchased);
		}
		else if (FPlatformTime::Seconds() - Probe->StageAt >= 8 && AllAlive(*Probe))
		{
			Rounds->SetRoundDuration(1);
			Advance(*Probe, EStep::Planning);
		}
	}
	else if (Probe->Step == EStep::Planning && bPostRound)
	{
		Rounds->SetRoundDuration(Probe->OriginalDuration);
		for (const auto& Entry : Probe->Players)
		{
			ABwayPlayerState* Player = Entry.Value.Player.Get();
			ULyraAbilitySystemComponent* ASC = Entry.Value.ASC.Get();
			const UBwayGoldAttributeSet* Gold = ASC->GetSet<UBwayGoldAttributeSet>();
			if (!Gold) return Finish(Probe, TEXT("missing-wallet"));
			int32 TotalCost = 0;
			for (const FName Id : Probe->UpgradeIds) TotalCost += Probe->Catalog->FindUpgrade(Id)->Ranks[0].Cost;
			ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), TotalCost + 100.0f);
			for (const FName Id : Probe->UpgradeIds)
			{
				const int32 Cost = Probe->Catalog->FindUpgrade(Id)->Ranks[0].Cost;
				const float Before = Gold->GetCurrentGold();
				Player->UpgradeComponent->ServerPurchaseUpgrade(Id, 0);
				if (Player->UpgradeComponent->GetOwnedRank(Id) != 1 || !FMath::IsNearlyEqual(Gold->GetCurrentGold(), Before - Cost, 0.01f)) return Finish(Probe, TEXT("purchase-debit"));
				UE_LOG(LogTemp, Display, TEXT("BwayUpgradeLifetimeTest: Purchase player=%d id=%s rank=1 cost=%d before=%.1f after=%.1f"), Entry.Key, *Id.ToString(), Cost, Before, Gold->GetCurrentGold());
			}
		}
		Advance(*Probe, EStep::Purchased);
	}
	else if (Probe->Step == EStep::Purchased)
	{
		// Clients get their own two-second interval before authority issues damage.
		if (Stable(*Probe, BonusesMatch(*Probe) && bPlaying && Rounds->GetCurrentRoundNumber() > Probe->FirstRound && AllAlive(*Probe), Probe->bClient ? 2.0 : 6.0))
		{
			for (auto& Entry : Probe->Players)
			{
				Entry.Value.BeforeDeathPawn = Entry.Value.Player->GetPawn();
				Entry.Value.Deaths = Entry.Value.Player->GetDeaths();
			}
			LogPlayers(*Probe, TEXT("Purchased"));
			Advance(*Probe, EStep::Death);
			if (!Probe->bClient)
			{
				for (const auto& Entry : Probe->Players)
				{
					if (Entry.Key == Probe->LocalPlayer) continue;
					Probe->Victim = Entry.Key;
					APawn* Pawn = Entry.Value.Player->GetPawn();
					UBwayDamageLibrary::ApplyDamageFromSource(Entry.Value.ASC.Get(), Pawn, 100000.0f, Pawn, Pawn);
					UE_LOG(LogTemp, Display, TEXT("BwayUpgradeLifetimeTest: NativeDamage victim=%d guard=%d health=%.1f"), Entry.Key, GAllowActorScriptExecutionInEditor, ULyraHealthComponent::FindHealthComponent(Pawn)->GetHealth());
					LogPlayers(*Probe, TEXT("Dead"));
					break;
				}
			}
		}
	}
	else if (Probe->Step == EStep::Death)
	{
		if (Probe->bClient && Probe->Victim == INDEX_NONE)
		{
			for (const auto& Entry : Probe->Players)
			{
				// Possession can clear PlayerState::GetPawn before the dying actor is removed.
				const ULyraHealthComponent* Health = ULyraHealthComponent::FindHealthComponent(Entry.Value.BeforeDeathPawn.Get());
				if (Health && Health->GetHealth() <= 0 && Health->IsDeadOrDying())
				{
					Probe->Victim = Entry.Key;
					LogPlayers(*Probe, TEXT("Dead"));
					break;
				}
			}
		}
		const FPlayerBaseline* Victim = Probe->Players.Find(Probe->Victim);
		const bool bRespawned = Victim && Victim->Player->GetPawn() && Victim->Player->GetPawn() != Victim->BeforeDeathPawn.Get()
			&& Victim->Player->GetDeaths() == Victim->Deaths + 1;
		if (Stable(*Probe, bRespawned && bPlaying && AllAlive(*Probe)))
		{
			Probe->RespawnRound = Rounds->GetCurrentRoundNumber();
			LogPlayers(*Probe, TEXT("Respawn"));
			Advance(*Probe, EStep::RespawnHold);
		}
	}
	else if (Probe->Step == EStep::RespawnHold)
	{
		if (!Probe->bClient && FPlatformTime::Seconds() - Probe->StageAt >= 4)
		{
			Rounds->SetRoundDuration(1);
			Advance(*Probe, EStep::FinalPlanning);
		}
		else if (Probe->bClient && bPostRound) Advance(*Probe, EStep::FinalRound);
	}
	else if (Probe->Step == EStep::FinalPlanning && bPostRound)
	{
		Rounds->SetRoundDuration(Probe->OriginalDuration);
		Advance(*Probe, EStep::FinalRound);
	}
	else if (Probe->Step == EStep::FinalRound && Stable(*Probe, bPlaying && Rounds->GetCurrentRoundNumber() > Probe->RespawnRound && AllAlive(*Probe)))
	{
		LogPlayers(*Probe, TEXT("Final"));
		return Finish(Probe);
	}
	return true;
}

void Start(bool bClient, bool bLifeLeech = false)
{
	if (ActiveProbe)
	{
		UE_LOG(LogTemp, Warning, TEXT("BwayUpgradeLifetimeTest: probe already running"));
		return;
	}
	ActiveProbe = MakeShared<FProbe>();
	ActiveProbe->bClient = bClient;
	ActiveProbe->bLifeLeech = bLifeLeech;
	if (bLifeLeech) ActiveProbe->UpgradeIds.Last() = TEXT("LifeLeech");
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Poll), 0.1f);
}

FAutoConsoleCommand AuthorityCommand(TEXT("bway.Test.UpgradeLifetimeAuthority"),
	TEXT("Development test: four-human Dorado upgrade purchases, native death, respawn, and round lifetime; use an owned disposable match."),
	FConsoleCommandDelegate::CreateLambda([] { Start(false); }));
FAutoConsoleCommand ClientCommand(TEXT("bway.Test.UpgradeLifetimeObserve"),
	TEXT("Development test: observe four-player public attributes and private ranks through death/respawn/rounds."),
	FConsoleCommandDelegate::CreateLambda([] { Start(true); }));
FAutoConsoleCommand LifeLeechAuthorityCommand(TEXT("bway.Test.UpgradeLifeLeechLifetimeAuthority"),
	TEXT("Development test: four-item loadout including life leech through actual death, respawn, and rounds."),
	FConsoleCommandDelegate::CreateLambda([] { Start(false, true); }));
FAutoConsoleCommand LifeLeechClientCommand(TEXT("bway.Test.UpgradeLifeLeechLifetimeObserve"),
	TEXT("Development test: observe four-item life-leech loadout lifetime and owner privacy."),
	FConsoleCommandDelegate::CreateLambda([] { Start(true, true); }));
}
#endif
