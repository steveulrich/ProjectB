#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Combat/BwayDamageLibrary.h"
#include "Containers/Ticker.h"
#include "Economy/BwayBaseZone.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/PlatformTime.h"
#include "LyraGameplayTags.h"

namespace BwayLifeLeechTest
{
const FName UpgradeId(TEXT("LifeLeech"));
constexpr float RawDamage = 121.25f;
struct FRestorePlayer
{
	TWeakObjectPtr<ABwayPlayerState> Player;
	float Health = 0;
	float Gold = 0;
};
struct FProbe
{
	bool bClient = false;
	bool bRequested = false;
	bool bPurchased = false;
	int32 Step = 0;
	int32 Samples = 0;
	int32 Observer = INDEX_NONE;
	int32 Source = INDEX_NONE;
	int32 Victim = INDEX_NONE;
	double Started = FPlatformTime::Seconds();
	double Changed = 0;
	double StableSince = 0;
	float BeforeGold = 0;
	TArray<FRestorePlayer> RestorePlayers;
	TMap<TWeakObjectPtr<ABwayBaseZone>, float> RestoreBases;
};
TSharedPtr<FProbe> Active;

UWorld* FindWorld(bool bClient)
{
	if (GEngine)
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->IsGameWorld() && World->GetMapName().Contains(TEXT("L_BW_Dorado"))
				&& (World->GetNetMode() == NM_Client) == bClient) return World;
		}
	return nullptr;
}

float Health(const ABwayPlayerState* Player)
{
	return Player->GetLyraAbilitySystemComponent()->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
}

float MaxHealth(const ABwayPlayerState* Player)
{
	return Player->GetLyraAbilitySystemComponent()->GetNumericAttribute(ULyraHealthSet::GetMaxHealthAttribute());
}

void Cleanup(const TSharedPtr<FProbe>& Probe)
{
	if (Probe->bClient) return;
	int32 Players = 0, Bases = 0;
	for (const FRestorePlayer& Entry : Probe->RestorePlayers)
	{
		if (ABwayPlayerState* Player = Entry.Player.Get())
		{
			Player->UpgradeComponent->ResetUpgrades();
			ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
			ASC->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), Entry.Health);
			ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), Entry.Gold);
			++Players;
		}
	}
	for (const auto& Entry : Probe->RestoreBases)
		if (ABwayBaseZone* Base = Entry.Key.Get()) { Base->HealingPerSecond = Entry.Value; ++Bases; }
	UE_LOG(LogTemp, Display, TEXT("BwayLifeLeechTest: Cleanup players=%d bases=%d"), Players, Bases);
}

bool Finish(const TSharedPtr<FProbe>& Probe, const TCHAR* Error = nullptr)
{
	if (Error)
	{
		UE_LOG(LogTemp, Error, TEXT("BwayLifeLeechTest: FAIL role=%s step=%d error=%s elapsed=%.2f"), Probe->bClient ? TEXT("Client") : TEXT("Host"), Probe->Step, Error, FPlatformTime::Seconds() - Probe->Started);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("BwayLifeLeechTest: %s PASS observer=%d source=%d victim=%d samples=%d"), Probe->bClient ? TEXT("Client") : TEXT("Host"), Probe->Observer, Probe->Source, Probe->Victim, Probe->Samples);
	}
	Cleanup(Probe);
	Active.Reset();
	return false;
}

void Advance(FProbe& Probe, int32 Step)
{
	Probe.Step = Step;
	Probe.Changed = FPlatformTime::Seconds();
	Probe.StableSince = 0;
}

bool Stable(FProbe& Probe, bool bMatches, double Seconds)
{
	const double Now = FPlatformTime::Seconds();
	if (!bMatches) Probe.StableSince = 0;
	else if (!Probe.StableSince) Probe.StableSince = Now;
	return Probe.StableSince && Now - Probe.StableSince >= Seconds;
}

void LogPeers(const FProbe& Probe, const TArray<ABwayPlayerState*>& Players, const TCHAR* Stage)
{
	for (const ABwayPlayerState* Player : Players)
	{
		const ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
		const UBwayHeroAttributeSet* Hero = ASC->GetSet<UBwayHeroAttributeSet>();
		UE_LOG(LogTemp, Display, TEXT("BwayLifeLeechTest: Peer stage=%s observer=%d player=%d source=%d victim=%d rank=%d owned=%d leech=%.4f health=%.4f maxHealth=%.4f armor=%.4f incoming=%.4f team=%d"),
			Stage, Probe.Observer, Player->GetPlayerId(), Probe.Source, Probe.Victim,
			Player->UpgradeComponent->GetOwnedRank(UpgradeId), Player->UpgradeComponent->GetOwnedUpgrades().Num(),
			Hero->GetLifeLeech(), Health(Player), MaxHealth(Player), Hero->GetArmor(), Hero->GetIncomingDamageMultiplier(), Player->GetTeamId());
	}
}

bool Tick(const TSharedPtr<FProbe>& Probe)
{
	if (FPlatformTime::Seconds() - Probe->Started > 120.0) return Finish(Probe, TEXT("timeout"));
	UWorld* World = FindWorld(Probe->bClient);
	ABwayGameState* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
	APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	ABwayPlayerState* Local = Controller ? Controller->GetPlayerState<ABwayPlayerState>() : nullptr;
	const UBwayUpgradeCatalog* Catalog = Local && Local->UpgradeComponent ? Local->UpgradeComponent->GetCatalog() : nullptr;
	UBwayRoundManagementComponent* Rounds = GS ? GS->GetRoundManagement() : nullptr;
	if (!Catalog || !Rounds || GS->PlayerArray.Num() != 4) return true;
	const FBwayUpgradeDefinition* Definition = Catalog->FindUpgrade(UpgradeId);
	if (!Definition || Definition->Ranks.IsEmpty()) return Finish(Probe, TEXT("missing-catalog-definition"));
	const float Bonus = Definition->Ranks[0].Magnitude;
	const int32 Cost = Definition->Ranks[0].Cost;
	if (!FMath::IsFinite(Bonus) || Bonus <= 0 || Bonus > 1 || Cost < 0 || Cost > 400) return Finish(Probe, TEXT("unsupported-fixture-tuning"));
	TArray<ABwayPlayerState*> Players;
	for (APlayerState* State : GS->PlayerArray)
	{
		ABwayPlayerState* Player = Cast<ABwayPlayerState>(State);
		ULyraAbilitySystemComponent* ASC = Player ? Player->GetLyraAbilitySystemComponent() : nullptr;
		if (!Player || Player->IsABot() || !Player->GetPawn() || !ASC || !ASC->GetSet<UBwayHeroAttributeSet>()
			|| !ASC->GetSet<ULyraHealthSet>() || !ASC->GetSet<UBwayGoldAttributeSet>()
			|| !Player->UpgradeComponent || Health(Player) <= 0 || MaxHealth(Player) < 100
			|| GS->GetPlayerTeam(Player) == INDEX_NONE || ASC->HasMatchingGameplayTag(LyraGameplayTags::Status_Death)) return true;
		Players.Add(Player);
	}
	Players.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
	ABwayPlayerState* Source = Players[1];
	ABwayPlayerState* Victim = nullptr;
	for (ABwayPlayerState* Player : Players)
		if (GS->GetPlayerTeam(Player) != GS->GetPlayerTeam(Source)) { Victim = Player; break; }
	if (!Victim) return true;
	Probe->Observer = Local->GetPlayerId(); Probe->Source = Source->GetPlayerId(); Probe->Victim = Victim->GetPlayerId();
	++Probe->Samples;
	const bool bPlaying = Rounds->GetCurrentMatchPhase() == EBwayMatchPhase::Playing;
	ULyraAbilitySystemComponent* LocalASC = Local->GetLyraAbilitySystemComponent();
	const float Gold = LocalASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute());

	if (!Probe->bClient && Probe->Step == 0)
	{
		if (!bPlaying) return true;
		if (Source == Local || !Source->GetPlayerController() || Source->GetPlayerController()->IsLocalController()) return Finish(Probe, TEXT("source-must-be-remote-owner"));
		for (ABwayPlayerState* Player : Players)
			if (!Player->UpgradeComponent->GetOwnedUpgrades().IsEmpty()) return Finish(Probe, TEXT("unexpected-existing-upgrades"));
		for (TActorIterator<ABwayBaseZone> It(World); It; ++It)
		{
			Probe->RestoreBases.Add(*It, It->HealingPerSecond);
			It->HealingPerSecond = 0.0f;
		}
		for (ABwayPlayerState* Player : Players)
		{
			ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
			Probe->RestorePlayers.Add({Player, Health(Player), ASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute())});
			ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 500.0f);
		}
		Rounds->EndRound(0, EBwayWinCondition::GoalScored);
		UE_LOG(LogTemp, Display, TEXT("BwayLifeLeechTest: Fund players=4 gold=500 bases=%d source=%d victim=%d remoteSource=1"), Probe->RestoreBases.Num(), Probe->Source, Probe->Victim);
		Advance(*Probe, 1);
		return true;
	}
	if (!Probe->bRequested && Gold >= 450.0f && Local->UpgradeComponent->IsPurchaseWindowOpen())
	{
		Probe->bRequested = true; Probe->BeforeGold = Gold;
		UE_LOG(LogTemp, Display, TEXT("BwayLifeLeechTest: Request observer=%d id=LifeLeech before=%.4f cost=%d bonus=%.4f client=%d"), Probe->Observer, Gold, Cost, Bonus, Probe->bClient ? 1 : 0);
		Local->UpgradeComponent->ServerPurchaseUpgrade(UpgradeId, 0);
	}
	if (Probe->bRequested && !Probe->bPurchased && Local->UpgradeComponent->GetOwnedRank(UpgradeId) == 1
		&& FMath::IsNearlyEqual(LocalASC->GetNumericAttribute(UBwayHeroAttributeSet::GetLifeLeechAttribute()), Bonus, 0.001f)
		// Wallet, owner ranks and public attributes can arrive independently. Wait for all three.
		&& Gold >= Probe->BeforeGold - Cost - 0.01f && Gold <= Probe->BeforeGold - Cost + 10.0f)
	{
		Probe->bPurchased = true;
		UE_LOG(LogTemp, Display, TEXT("BwayLifeLeechTest: Purchased observer=%d id=LifeLeech rank=1 before=%.4f after=%.4f cost=%d bonus=%.4f"), Probe->Observer, Probe->BeforeGold, LocalASC->GetNumericAttribute(UBwayGoldAttributeSet::GetCurrentGoldAttribute()), Cost, Bonus);
	}
	bool bBonuses = Probe->bPurchased;
	for (ABwayPlayerState* Player : Players)
	{
		const bool bOwns = !Probe->bClient || Player == Local;
		bBonuses &= FMath::IsNearlyEqual(Player->GetLyraAbilitySystemComponent()->GetNumericAttribute(UBwayHeroAttributeSet::GetLifeLeechAttribute()), Bonus, 0.001f)
			&& Player->UpgradeComponent->GetOwnedRank(UpgradeId) == (bOwns ? 1 : 0)
			&& Player->UpgradeComponent->GetOwnedUpgrades().Num() == (bOwns ? 1 : 0);
	}
	if (!bBonuses || !bPlaying) return true;
	const float SourceBase = MaxHealth(Source) * 0.4f;
	const float VictimBase = MaxHealth(Victim) * 0.6f;
	const float Damage = FMath::Min(VictimBase, UBwayDamageLibrary::ResolveFinalDamage(RawDamage, Victim->GetLyraAbilitySystemComponent()));
	const float SourceAfter = FMath::Min(MaxHealth(Source), SourceBase + Damage * Bonus);
	const float VictimAfter = VictimBase - Damage;
	if (!FMath::IsFinite(Damage) || Damage < 1 || VictimAfter <= 1) return Finish(Probe, TEXT("invalid-damage-baseline"));
	const bool bBaseline = FMath::IsNearlyEqual(Health(Source), SourceBase, 0.02f) && FMath::IsNearlyEqual(Health(Victim), VictimBase, 0.02f);
	const bool bAfter = FMath::IsNearlyEqual(Health(Source), SourceAfter, 0.02f) && FMath::IsNearlyEqual(Health(Victim), VictimAfter, 0.02f);
	if (!Probe->bClient && Probe->Step == 1)
	{
		Source->GetLyraAbilitySystemComponent()->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), SourceBase);
		Victim->GetLyraAbilitySystemComponent()->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), VictimBase);
		LogPeers(*Probe, Players, TEXT("Baseline"));
		Advance(*Probe, 2);
	}
	else if (Probe->bClient && Probe->Step == 0 && Stable(*Probe, bBaseline, 1.0))
	{
		LogPeers(*Probe, Players, TEXT("Baseline"));
		Advance(*Probe, 3);
	}
	else if (!Probe->bClient && Probe->Step == 2 && Stable(*Probe, bBaseline, 6.0))
	{
		const float BeforeSource = Health(Source), BeforeVictim = Health(Victim);
		APawn* SourcePawn = Source->GetPawn();
		UBwayDamageLibrary::ApplyDamageFromSource(Source->GetLyraAbilitySystemComponent(), Victim->GetPawn(), RawDamage, SourcePawn, SourcePawn);
		UE_LOG(LogTemp, Display, TEXT("BwayLifeLeechTest: Damage source=%d victim=%d raw=%.4f sourceBefore=%.4f sourceAfter=%.4f victimBefore=%.4f victimAfter=%.4f bonus=%.4f expectedDamage=%.4f"), Probe->Source, Probe->Victim, RawDamage, BeforeSource, Health(Source), BeforeVictim, Health(Victim), Bonus, Damage);
		if (!FMath::IsNearlyEqual(BeforeVictim - Health(Victim), Damage, 0.02f)
			|| !FMath::IsNearlyEqual(Health(Source) - BeforeSource, Damage * Bonus, 0.02f)) return Finish(Probe, TEXT("actual-healing-delta"));
		Advance(*Probe, 3);
	}
	else if (Probe->Step == 3 && Stable(*Probe, bAfter, Probe->bClient ? 2.0 : 8.0))
	{
		LogPeers(*Probe, Players, TEXT("AfterDamage"));
		return Finish(Probe);
	}
	return true;
}

void Start(bool bClient)
{
	if (Active) return;
	Active = MakeShared<FProbe>(); Active->bClient = bClient;
	const TSharedPtr<FProbe> Probe = Active;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float) { return Tick(Probe); }), 0.1f);
}
FAutoConsoleCommand HostCommand(TEXT("bway.Test.LifeLeechHost"), TEXT("Development four-player life-leech authority fixture."), FConsoleCommandDelegate::CreateLambda([]() { Start(false); }));
FAutoConsoleCommand ClientCommand(TEXT("bway.Test.LifeLeechClient"), TEXT("Development owner purchase and replicated life-leech observer."), FConsoleCommandDelegate::CreateLambda([]() { Start(true); }));
}
#endif
