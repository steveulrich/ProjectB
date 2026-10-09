// Development-only real-damage credit, results, rematch and departing-client observation.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "BwayPlayerController.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "Containers/Ticker.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "UI/BwayMatchBreakdownWidget.h"
#include "UI/BwayRespawnHUDWidget.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace BwayCreditLifecycle
{
bool Running = false;
struct FHold
{
	TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
	EMovementMode Mode;
	uint8 Custom;
	TWeakObjectPtr<UBrainComponent> Brain;
	bool WasRunning;
};
struct FRun
{
	double Started = FPlatformTime::Seconds(), Since = Started, NextSample = 0.0;
	int32 Stage = 0, Sample = 0, ReadyResultSamples = 0;
	bool Packaged = false;
	double ActiveStarted = 0;
	TWeakObjectPtr<UBwayRoundManagementComponent> RematchConfigOwner;
	float RematchOriginalWindow = 0;
	TWeakObjectPtr<ABwayGameState> Original, Staging;
	TWeakObjectPtr<UBwayRoundManagementComponent> ConfigOwner;
	int32 OriginalPoints = 0;
	float OriginalWindow = 0, DisconnectDamageTime = 0;
	TArray<FHold> Holds;
	// Assist source must be remote so only that connection leaves in the final case.
	TWeakObjectPtr<ABwayPlayerState> Assister, Killer, Victim;
	int32 AssisterId = INDEX_NONE, KillerId = INDEX_NONE, VictimId = INDEX_NONE;
	TWeakObjectPtr<APawn> Round1Victim, BeforeRematchVictim;

	ABwayGameState* Authority() const
	{
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->IsGameWorld() && World->GetNetMode() == NM_ListenServer)
				if (auto* GS = World->GetGameState<ABwayGameState>()) return GS;
		}
		return nullptr;
	}
	bool Alive(const ABwayPlayerState* Player) const
	{
		const auto* Health = Player && Player->GetPawn() ? ULyraHealthComponent::FindHealthComponent(Player->GetPawn()) : nullptr;
		return Health && Health->GetHealth() > 0 && !Health->IsDeadOrDying();
	}
	void HoldAll(ABwayGameState* GS)
	{
		for (APlayerState* Player : GS->PlayerArray)
		{
			auto* Pawn = Player ? Cast<ABwayCharacterWithAbilities>(Player->GetPawn()) : nullptr;
			if (!Pawn || Holds.ContainsByPredicate([Pawn](const FHold& H) { return H.Pawn == Pawn; })) continue;
			auto* Move = Pawn->GetCharacterMovement();
			auto* AI = Cast<AAIController>(Pawn->GetController());
			auto* Brain = AI ? AI->GetBrainComponent() : nullptr;
			Holds.Add({Pawn, Move->MovementMode, Move->CustomMovementMode, Brain, Brain && Brain->IsRunning()});
			Move->StopMovementImmediately(); Move->DisableMovement();
			if (Brain) Brain->StopLogic(TEXT("Owned combat-credit lifecycle fixture"));
		}
	}
	bool Stop(const TCHAR* Reason)
	{
		int32 Restored = 0, Expired = 0;
		for (const auto& H : Holds)
		{
			if (H.Pawn.IsValid()) { H.Pawn->GetCharacterMovement()->SetMovementMode(H.Mode, H.Custom); ++Restored; }
			else ++Expired;
			if (H.WasRunning && H.Brain.IsValid()) H.Brain->RestartLogic();
		}
		if (ConfigOwner.IsValid())
		{
			ConfigOwner->SetPointsToWin(OriginalPoints);
			ConfigOwner->AssistCreditWindowSeconds = OriginalWindow;
		}
		if (RematchConfigOwner.IsValid())
		{
			RematchConfigOwner->AssistCreditWindowSeconds = RematchOriginalWindow;
			UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: RestoredPolicy window=%.3f"), RematchConfigOwner->AssistCreditWindowSeconds);
		}
		UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: COMPLETE reason=%s stage=%d samples=%d restored=%d expired=%d held=%d configOwnerExpired=%d"),
			Reason, Stage, Sample, Restored, Expired, Holds.Num(), !ConfigOwner.IsValid());
		Running = false;
		return false;
	}
	void Advance(int32 Next)
	{
		Stage = Next; Since = FPlatformTime::Seconds();
		UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Stage value=%d"), Stage);
	}
	bool Choose(ABwayGameState* GS)
	{
		TArray<ABwayPlayerState*> Team[2];
		int32 Humans = 0;
		for (APlayerState* State : GS->PlayerArray)
		{
			auto* Player = Cast<ABwayPlayerState>(State);
			const int32 T = Player ? GS->GetPlayerTeam(Player) : INDEX_NONE;
			if (!Player || T < 0 || T > 1 || !Alive(Player) || !Player->IsHeroLocked()) return false;
			Team[T].Add(Player); Humans += !Player->IsABot();
		}
		if (Humans != 4 || Team[0].Num() != 4 || Team[1].Num() != 4) return false;
		for (auto& Players : Team) Players.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B)
			{ return A.IsABot() != B.IsABot() ? !A.IsABot() : A.GetPlayerId() < B.GetPlayerId(); });
		Killer = Team[0][0]; Assister = Team[0][1]; Victim = Team[1][0];
		if (!Killer->GetPlayerController() || !Killer->GetPlayerController()->IsLocalController()
			|| Assister->IsABot() || !Assister->GetPlayerController() || Assister->GetPlayerController()->IsLocalController()) return false;
		KillerId = Killer->GetPlayerId(); AssisterId = Assister->GetPlayerId(); VictimId = Victim->GetPlayerId();
		UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Roles stage=%d killer=%d assister=%d victim=%d humans=4 bots=4 world=%s"),
			Stage, KillerId, AssisterId, VictimId, *GS->GetWorld()->GetPathName());
		return true;
	}
	void Capture(ABwayGameState* AuthorityGS)
	{
		++Sample;
		int32 ResultPeers = 0;
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			UWorld* W = Context.World();
			auto* GS = W && W->IsGameWorld() ? W->GetGameState<ABwayGameState>() : nullptr;
			auto* Round = GS ? GS->GetRoundManagement() : nullptr;
			if (!Round) continue;
			int32 ActiveResults = 0, HUDCount = 0;
			for (TObjectIterator<UBwayRespawnHUDWidget> HUD; HUD; ++HUD)
				if (HUD->GetWorld() == W && HUD->IsInViewport()) ++HUDCount;
			UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: World n=%d stage=%d world=%s net=%d phase=%d round=%d roster=%d"),
				Sample, Stage, *W->GetPathName(), int32(W->GetNetMode()), int32(Round->GetCurrentMatchPhase()), Round->GetCurrentRoundNumber(), GS->PlayerArray.Num());
			for (APlayerState* State : GS->PlayerArray)
			{
				auto* Player = Cast<ABwayPlayerState>(State);
				if (!Player) continue;
				const auto Stats = Player->GetMatchStatsSnapshot(), Last = Player->GetLastRoundStats();
				const auto* ASC = Player->GetLyraAbilitySystemComponent();
				const auto* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
				UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Player n=%d stage=%d world=%s net=%d id=%d team=%d bot=%d k=%d d=%d a=%d rk=%d rd=%d ra=%d gold=%.3f alive=%d pawn=%s locked=%d hero=%s"),
					Sample, Stage, *W->GetPathName(), int32(W->GetNetMode()), Player->GetPlayerId(), GS->GetPlayerTeam(Player), Player->IsABot(),
					Stats.Kills, Stats.Deaths, Stats.Assists, Last.Kills, Last.Deaths, Last.Assists, Gold ? Gold->GetCurrentGold() : -1, Alive(Player), *GetNameSafe(Player->GetPawn()), Player->IsHeroLocked(), *Player->GetSelectedHeroId().ToString());
			}
			if (Round->HasActivePostRoundSummary())
			{
				const auto Summary = Round->GetActivePostRoundSummary();
				UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: RoundSummary n=%d stage=%d world=%s net=%d round=%d k0=%d d0=%d a0=%d k1=%d d1=%d a1=%d"),
					Sample, Stage, *W->GetPathName(), int32(W->GetNetMode()), Summary.CompletedRoundNumber,
					Summary.Team0Stats.Stats.Kills, Summary.Team0Stats.Stats.Deaths, Summary.Team0Stats.Stats.Assists,
					Summary.Team1Stats.Stats.Kills, Summary.Team1Stats.Stats.Deaths, Summary.Team1Stats.Stats.Assists);
			}
			// The results orchestrator stays collapsed; its active breakdown
			// child owns the display payload and the rematch controls.
			for (TObjectIterator<UBwayMatchBreakdownWidget> It; It; ++It)
			{
				if (It->GetWorld() != W || !It->IsActivated()) continue;
				++ActiveResults;
				const auto* Property = FindFProperty<FStructProperty>(It->GetClass(), TEXT("CachedSummary"));
				const auto* Summary = Property && Property->Struct == FBwayPostMatchSummaryData::StaticStruct()
					? Property->ContainerPtrToValuePtr<FBwayPostMatchSummaryData>(*It) : nullptr;
				if (!Summary) continue;
				if (Summary->PlayerColumns.Num() == 8) ++ResultPeers;
				for (const auto& Column : Summary->PlayerColumns)
					UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Result n=%d stage=%d world=%s net=%d id=%d team=%d k=%d d=%d a=%d columns=%d rounds=%d"),
						Sample, Stage, *W->GetPathName(), int32(W->GetNetMode()), Column.PlayerId, Column.GameTeamIndex,
						Column.Stats.Kills, Column.Stats.Deaths, Column.Stats.Assists, Summary->PlayerColumns.Num(), Summary->TotalRounds);
			}
			UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Presentation n=%d stage=%d world=%s net=%d phase=%d activeResults=%d hud=%d"),
				Sample, Stage, *W->GetPathName(), int32(W->GetNetMode()), int32(Round->GetCurrentMatchPhase()), ActiveResults, HUDCount);
		}
		if (Stage == 8) ReadyResultSamples = ResultPeers == (Packaged ? 1 : 4) ? ReadyResultSamples + 1 : 0;
	}
	bool Damage(ABwayPlayerState* Source, float Amount, const TCHAR* Label)
	{
		if (!Alive(Source) || !Alive(Victim.Get())) return false;
		auto* Target = Victim->GetPawn();
		auto* Health = ULyraHealthComponent::FindHealthComponent(Target);
		const float Before = Health->GetHealth();
		UBwayDamageLibrary::ApplyDamageFromSource(Source->GetLyraAbilitySystemComponent(), Target, Amount, Source->GetPawn(), Source->GetPawn());
		if (auto* Character = Cast<ABwayCharacterWithAbilities>(Target))
			UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Ledger label=%s victim=%d pending=%d consumedAssisters=%d"),
				Label, VictimId, Character->GetCombatContributionCountForAudit(), Character->GetConsumedAssisterCountForAudit());
		UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Damage stage=%d label=%s source=%d target=%d before=%.3f after=%.3f k=%d a=%d d=%d"),
			Stage, Label, Source->GetPlayerId(), VictimId, Before, Health->GetHealth(), Source->GetKills(), Source->GetAssists(), Victim->GetDeaths());
		return Health->GetHealth() < Before;
	}
	bool Tick(float)
	{
		const double Now = FPlatformTime::Seconds();
		if ((!ActiveStarted && Now - Started > 180) || (ActiveStarted && Now - ActiveStarted > 240)) return Stop(TEXT("timeout"));
		auto* GS = Authority();
		auto* Round = GS ? GS->GetRoundManagement() : nullptr;
		if (!Round) return true;
		HoldAll(GS);
		const auto Phase = Round->GetCurrentMatchPhase();
		const bool Playing = Phase == EBwayMatchPhase::Playing;
		if (Stage > 0 && Now >= NextSample) { Capture(GS); NextSample = Now + 0.5; }
		if (Stage == 0 && Playing && Choose(GS))
		{
			ActiveStarted = Now;
			Original = GS; ConfigOwner = Round; OriginalPoints = Round->PointsToWin; OriginalWindow = Round->AssistCreditWindowSeconds;
			Round->SetPointsToWin(2); Round->AssistCreditWindowSeconds = 120;
			UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Policy originalWindow=%.3f fixtureWindow=120 originalPoints=%d fixturePoints=2"), OriginalWindow, OriginalPoints);
			Advance(1);
		}
		else if (Stage == 1 && Now - Since > 1)
		{
			if (!Damage(Assister.Get(), 25, TEXT("round1-assist"))) return Stop(TEXT("seed-failed"));
			Advance(2);
		}
		else if (Stage == 2 && Now - Since > 0.5)
		{
			if (!Damage(Killer.Get(), 100000, TEXT("round1-kill"))) return Stop(TEXT("kill-failed"));
			Advance(3);
		}
		else if (Stage == 3 && Now - Since > 4 && Alive(Victim.Get()))
		{
			if (!Damage(Assister.Get(), 25, TEXT("carry-across-round"))) return Stop(TEXT("round-seed-failed"));
			Round1Victim = Victim->GetPawn();
			Round->EndRound(0, EBwayWinCondition::GoalScored);
			Advance(4);
		}
		else if (Stage == 4 && Playing && Round->GetCurrentRoundNumber() == 2 && Alive(Victim.Get()))
		{
			UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: RoundReplacement changed=%d"), Round1Victim != Victim->GetPawn());
			Advance(5);
		}
		else if (Stage == 5 && Now - Since > 2)
		{
			if (!Damage(Killer.Get(), 100000, TEXT("after-round-reset"))) return Stop(TEXT("round2-kill-failed"));
			Advance(6);
		}
		else if (Stage == 6 && Now - Since > 4 && Alive(Victim.Get()))
		{
			if (!Damage(Assister.Get(), 25, TEXT("carry-across-match"))) return Stop(TEXT("match-seed-failed"));
			BeforeRematchVictim = Victim->GetPawn();
			Round->EndRound(0, EBwayWinCondition::GoalScored);
			Advance(7);
		}
		else if (Stage == 7 && Phase == EBwayMatchPhase::PostMatch) Advance(8);
		else if (Stage == 8 && Now - Since > 5 && ReadyResultSamples >= 3)
		{
			GS->RestartMatchFromResults();
			UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Rematch requested=1 source=production"));
			Advance(9);
		}
		else if (Stage == 9 && GS != Original.Get())
		{
			int32 Humans = 0, Ready = 0, Nonzero = 0;
			for (APlayerState* State : GS->PlayerArray)
			{
				auto* Player = Cast<ABwayPlayerState>(State);
				if (!Player || Player->IsABot()) continue;
				++Humans;
				const auto* ASC = Player->GetLyraAbilitySystemComponent();
				const auto* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
				Ready += Gold != nullptr;
				Nonzero += Player->GetKills() || Player->GetDeaths() || Player->GetAssists() || (Gold && Gold->GetCurrentGold() != 0);
			}
			auto* Selection = GS->FindComponentByClass<UBwayHeroSelectionManager>();
			if (Humans == 4 && Ready == 4 && Selection && Selection->IsSelectionActive())
			{
				UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Reset humans=%d nonzero=%d freshWorld=1 victimExpired=%d"), Humans, Nonzero, !BeforeRematchVictim.IsValid());
				if (Nonzero) return Stop(TEXT("rematch-not-reset"));
				Staging = GS;
				if (!Packaged) GEngine->Exec(GS->GetWorld(), TEXT("bway.Test.AutoLockAllHeroes"));
				Advance(10);
			}
		}
		else if (Stage == 10 && GS != Staging.Get() && Playing && Choose(GS))
		{
			if (Packaged)
			{
				RematchConfigOwner = Round; RematchOriginalWindow = Round->AssistCreditWindowSeconds;
				Round->AssistCreditWindowSeconds = 120;
				UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: RematchPolicy originalWindow=%.3f activeWindow=%.3f"), RematchOriginalWindow, Round->AssistCreditWindowSeconds);
			}
			Advance(11);
		}
		else if (Stage == 11 && Now - Since > 2)
		{
			const auto* NewVictim = Cast<ABwayCharacterWithAbilities>(Victim->GetPawn());
			UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: FreshLedger pending=%d originalPawnExpired=%d activeWindow=%.3f"),
				NewVictim ? NewVictim->GetCombatContributionCountForAudit() : -1, !BeforeRematchVictim.IsValid(), Round->AssistCreditWindowSeconds);
			if (!Damage(Killer.Get(), 100000, TEXT("after-rematch"))) return Stop(TEXT("rematch-kill-failed"));
			Advance(12);
		}
		else if (Stage == 12 && Now - Since > 4 && Alive(Victim.Get()))
		{
			if (!Damage(Assister.Get(), 25, TEXT("before-disconnect"))) return Stop(TEXT("disconnect-seed-failed"));
			DisconnectDamageTime = GS->GetWorld()->GetTimeSeconds();
			if (Packaged)
			{
				auto* RemotePC = Cast<ABwayPlayerController>(Assister->GetPlayerController());
				if (!RemotePC || RemotePC->IsLocalController() || !RemotePC->GetNetConnection()) return Stop(TEXT("disconnect-owner-missing"));
				UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Disconnect player=%d world=%s source=owned-client-rpc"), AssisterId, *GS->GetWorld()->GetPathName());
				Advance(13); RemotePC->Client_RunCreditDisconnectProbe(); return true;
			}
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				UWorld* W = Context.World();
				if (!W || W->GetNetMode() != NM_Client) continue;
				auto* PC = W->GetFirstPlayerController();
				if (PC && PC->PlayerState && PC->PlayerState->GetPlayerId() == AssisterId)
				{
					UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Disconnect player=%d world=%s"), AssisterId, *W->GetPathName());
					Advance(13); W->GetGameInstance()->ReturnToMainMenu(); return true;
				}
			}
			return Stop(TEXT("remote-connection-not-found"));
		}
		else if (Stage == 13 && Now - Since > 1 && !GS->PlayerArray.Contains(Assister.Get()))
		{
			const float Age = GS->GetWorld()->GetTimeSeconds() - DisconnectDamageTime;
			UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: Departed present=0 age=%.3f window=%.3f oldStateValid=%d"), Age, Round->AssistCreditWindowSeconds, Assister.IsValid());
			if (Age >= Round->AssistCreditWindowSeconds) return Stop(TEXT("disconnect-expired-before-probe"));
			if (!Damage(Killer.Get(), 100000, TEXT("after-disconnect"))) return Stop(TEXT("departed-kill-failed"));
			UE_LOG(LogTemp, Display, TEXT("BwayCreditLife: DepartedAward oldStateValid=%d assists=%d"), Assister.IsValid(), Assister.IsValid() ? Assister->GetAssists() : 0);
			Advance(14);
		}
		else if (Stage == 14 && Now - Since > 5) return Stop(TEXT("observed"));
		return true;
	}
};
FAutoConsoleCommand Command(TEXT("bway.Test.CombatCreditLifecycleAudit"),
	TEXT("Development: actual combat credit through two rounds, results, rematch and one remote disconnect; fixture-owned match and cleanup."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (Running || !GEngine) return;
		Running = true;
		TSharedRef<FRun> Run = MakeShared<FRun>();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.1f);
	}));

FAutoConsoleCommand PackagedCommand(TEXT("bway.Test.CombatCreditPackagedLifecycleAudit"),
	TEXT("Development: production credit/results/rematch/disconnect with independent packaged client journals and active-window controls."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (Running || !GEngine) return;
		Running = true;
		TSharedRef<FRun> Run = MakeShared<FRun>(); Run->Packaged = true;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.1f);
	}));

struct FPeerRun
{
	FRun Journal;
	double Started = FPlatformTime::Seconds(), ActiveStarted = 0, RematchPlayingStarted = 0;
	bool SawResults = false, SawStaging = false;
	TWeakObjectPtr<ABwayGameState> FirstMatch;
	bool Tick(float)
	{
		const double Now = FPlatformTime::Seconds();
		if ((!ActiveStarted && Now - Started > 180) || (ActiveStarted && Now - ActiveStarted > 260))
		{
			UE_LOG(LogTemp, Display, TEXT("BwayCreditLifePeer: COMPLETE reason=timeout samples=%d results=%d staging=%d"), Journal.Sample, SawResults, SawStaging);
			return false;
		}
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (RematchPlayingStarted && World && World->IsGameWorld() && World->GetNetMode() == NM_Standalone
				&& World->GetMapName().Contains(TEXT("L_LyraFrontEnd")))
			{
				UE_LOG(LogTemp, Display, TEXT("BwayCreditLifePeer: COMPLETE reason=departed samples=%d results=%d staging=%d oldWorldExpired=%d frontend=1"),
					Journal.Sample, SawResults, SawStaging, !FirstMatch.IsValid());
				return false;
			}
			if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Client) continue;
			auto* State = World->GetGameState<ABwayGameState>();
			auto* Round = State ? State->GetRoundManagement() : nullptr;
			if (!Round) continue;
			const auto Phase = Round->GetCurrentMatchPhase();
			if (!ActiveStarted && Phase == EBwayMatchPhase::Playing)
			{
				ActiveStarted = Now; FirstMatch = State;
			}
			if (!ActiveStarted) return true;
			Journal.Stage = 100; // Independent observer; host phase alignment happens offline.
			Journal.Capture(State);
			SawResults |= Phase == EBwayMatchPhase::PostMatch;
			if (SawResults && State != FirstMatch.Get() && Phase != EBwayMatchPhase::Playing) SawStaging = true;
			if (SawStaging && Phase == EBwayMatchPhase::Playing && !RematchPlayingStarted) RematchPlayingStarted = Now;
			if (RematchPlayingStarted && Now - RematchPlayingStarted > 25)
			{
				UE_LOG(LogTemp, Display, TEXT("BwayCreditLifePeer: COMPLETE reason=observed samples=%d results=%d staging=%d oldWorldExpired=%d"),
					Journal.Sample, SawResults, SawStaging, !FirstMatch.IsValid());
				return false;
			}
			break;
		}
		return true;
	}
};
FAutoConsoleCommand PeerCommand(TEXT("bway.Test.CombatCreditLifecyclePeerAudit"),
	TEXT("Development: read-only local packaged results, roster, wallet and rematch journal."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (!GEngine) return;
		TSharedRef<FPeerRun> Run = MakeShared<FPeerRun>();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.25f);
	}));
}
#endif
