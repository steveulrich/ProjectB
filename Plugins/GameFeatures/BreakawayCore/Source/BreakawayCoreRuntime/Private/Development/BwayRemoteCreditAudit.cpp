// Development-only credit proof through the existing remote-owner attack RPC.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerController.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "Containers/Ticker.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"

namespace BwayRemoteCredit
{
bool Running = false;
struct FHold
{
	TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
	FTransform Transform;
	EMovementMode Mode;
	uint8 Custom;
	TWeakObjectPtr<UBrainComponent> Brain;
	bool WasRunning;
};
struct FRun
{
	double Started = FPlatformTime::Seconds(), Since = Started, NextSample = 0, ActiveStarted = 0;
	int32 Stage = 0, Sample = 0, RemoteId = INDEX_NONE, PartnerId = INDEX_NONE, VictimId = INDEX_NONE, RoundNumber = 0;
	float BeforeAttackHealth = 0;
	TWeakObjectPtr<ABwayGameState> State;
	TWeakObjectPtr<ABwayPlayerState> Remote, Partner, Victim;
	TArray<FHold> Holds;

	ULyraHealthComponent* Health(ABwayPlayerState* PS) const
	{
		return PS && PS->GetPawn() ? ULyraHealthComponent::FindHealthComponent(PS->GetPawn()) : nullptr;
	}
	bool Alive(ABwayPlayerState* PS) const
	{
		const auto* H = Health(PS);
		return H && H->GetHealth() > 0 && !H->IsDeadOrDying();
	}
	void HoldAll(ABwayGameState* GS)
	{
		for (APlayerState* PS : GS->PlayerArray)
		{
			auto* Pawn = PS ? Cast<ABwayCharacterWithAbilities>(PS->GetPawn()) : nullptr;
			if (!Pawn || Holds.ContainsByPredicate([Pawn](const FHold& H) { return H.Pawn == Pawn; })) continue;
			auto* Move = Pawn->GetCharacterMovement();
			auto* AI = Cast<AAIController>(Pawn->GetController());
			auto* Brain = AI ? AI->GetBrainComponent() : nullptr;
			Holds.Add({Pawn, Pawn->GetActorTransform(), Move->MovementMode, Move->CustomMovementMode, Brain, Brain && Brain->IsRunning()});
			Move->StopMovementImmediately(); Move->DisableMovement();
			if (Brain) Brain->StopLogic(TEXT("Owned remote combat-credit audit"));
		}
	}
	bool Stop(const TCHAR* Reason)
	{
		int32 Restored = 0, Expired = 0;
		for (const auto& H : Holds)
		{
			if (H.Pawn.IsValid())
			{
				H.Pawn->SetActorTransform(H.Transform, false, nullptr, ETeleportType::TeleportPhysics);
				H.Pawn->GetCharacterMovement()->SetMovementMode(H.Mode, H.Custom); ++Restored;
			}
			else ++Expired;
			if (H.WasRunning && H.Brain.IsValid()) H.Brain->RestartLogic();
		}
		UE_LOG(LogTemp, Display, TEXT("BwayRemoteCredit: COMPLETE reason=%s stage=%d samples=%d restored=%d expired=%d held=%d"), Reason, Stage, Sample, Restored, Expired, Holds.Num());
		Running = false;
		return false;
	}
	void Advance(int32 Value)
	{
		Stage = Value; Since = FPlatformTime::Seconds();
		UE_LOG(LogTemp, Display, TEXT("BwayRemoteCredit: Stage value=%d"), Stage);
	}
	bool Choose(ABwayGameState* GS)
	{
		if (GS->PlayerArray.Num() != 8) return false;
		for (APlayerState* PS : GS->PlayerArray)
		{
			auto* P = Cast<ABwayPlayerState>(PS);
			if (!Alive(P) || !P->IsHeroLocked()) return false;
			if (P->GetPlayerController() && P->GetPlayerController()->IsLocalController()) Partner = P;
		}
		if (!Partner.IsValid()) return false;
		const int32 Team = GS->GetPlayerTeam(Partner.Get());
		for (APlayerState* PS : GS->PlayerArray)
		{
			auto* P = Cast<ABwayPlayerState>(PS);
			auto* PC = P ? Cast<ABwayPlayerController>(P->GetPlayerController()) : nullptr;
			if (!PC || PC->IsLocalController() || !PC->GetNetConnection()) continue;
			if (GS->GetPlayerTeam(P) == Team) Remote = P;
			else Victim = P;
		}
		if (!Remote.IsValid() || !Victim.IsValid()) return false;
		State = GS; RemoteId = Remote->GetPlayerId(); PartnerId = Partner->GetPlayerId(); VictimId = Victim->GetPlayerId();
		RoundNumber = GS->GetRoundManagement()->GetCurrentRoundNumber();
		UE_LOG(LogTemp, Display, TEXT("BwayRemoteCredit: Roles remote=%d partner=%d victim=%d team=%d enemyTeam=%d remoteHero=%s controllerLocal=0 connection=1 killGold=%.3f assistGold=%.3f"),
			RemoteId, PartnerId, VictimId, Team, GS->GetPlayerTeam(Victim.Get()), *Remote->GetSelectedHeroId().ToString(), GS->GetRoundManagement()->GoldAwardForKill, GS->GetRoundManagement()->GoldAwardForAssist);
		return true;
	}
	void Position()
	{
		auto* GS = State.Get(); auto* Source = Remote->GetPawn();
		const FVector Origin = Source->GetActorLocation();
		FVector Forward = Source->GetActorForwardVector().GetSafeNormal2D();
		if (Forward.IsNearlyZero()) Forward = FVector::ForwardVector;
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (PS && PS->GetPawn() && PS != Remote.Get() && PS != Victim.Get())
				PS->GetPawn()->SetActorLocation(Origin + FVector(0, 10000, 2000), false, nullptr, ETeleportType::TeleportPhysics);
		}
		Victim->GetPawn()->SetActorLocation(Origin + Forward * 120, false, nullptr, ETeleportType::TeleportPhysics);
		Source->SetActorRotation(Forward.Rotation());
	}
	void Capture(const TCHAR* Label)
	{
		++Sample;
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			auto* W = Context.World();
			auto* GS = W && W->IsGameWorld() ? W->GetGameState<ABwayGameState>() : nullptr;
			if (!GS || (W->GetNetMode() != NM_Client && W->GetNetMode() != NM_ListenServer)) continue;
			for (APlayerState* PS : GS->PlayerArray)
			{
				auto* P = Cast<ABwayPlayerState>(PS); if (!P) continue;
				const auto* ASC = P->GetLyraAbilitySystemComponent();
				const auto* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
				const auto* H = Health(P);
				UE_LOG(LogTemp, Display, TEXT("BwayRemoteCredit: Player n=%d stage=%d label=%s world=%s net=%d id=%d team=%d k=%d d=%d a=%d gold=%.3f income=%.3f health=%.3f alive=%d gameTime=%.3f"),
					Sample, Stage, Label, *W->GetPathName(), int32(W->GetNetMode()), P->GetPlayerId(), GS->GetPlayerTeam(P), P->GetKills(), P->GetDeaths(), P->GetAssists(),
					Gold ? Gold->GetCurrentGold() : -1, Gold ? Gold->GetGoldPerSecond() : -1, H ? H->GetHealth() : -1, Alive(P), W->GetTimeSeconds());
			}
		}
	}
	void Attack()
	{
		BeforeAttackHealth = Health(Victim.Get())->GetHealth();
		UE_LOG(LogTemp, Display, TEXT("BwayRemoteCredit: Dispatch stage=%d remote=%d victim=%d before=%.3f"), Stage, RemoteId, VictimId, BeforeAttackHealth);
		CastChecked<ABwayPlayerController>(Remote->GetPlayerController())->Client_RunTeamRemoteAction(PartnerId, VictimId, false);
	}
	bool Tick(float)
	{
		const double Now = FPlatformTime::Seconds();
		// The packaged frontend and normal selection timer precede active combat.
		// Bound startup separately so it cannot consume the attack observation window.
		if (Stage == 0 && Now - Started > 180) return Stop(TEXT("startup-timeout"));
		if (Stage > 0 && Now - ActiveStarted > 90) return Stop(TEXT("active-timeout"));
		if (!State.IsValid())
		{
			if (Stage) return Stop(TEXT("world-lost"));
			for (const auto& Context : GEngine->GetWorldContexts())
			{
				auto* W = Context.World();
				auto* GS = W && W->IsGameWorld() && W->GetNetMode() == NM_ListenServer ? W->GetGameState<ABwayGameState>() : nullptr;
				if (!GS || !GS->GetRoundManagement()) continue;
				HoldAll(GS);
				if (GS->GetRoundManagement()->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && Choose(GS))
				{
					ActiveStarted = Now; Position(); Advance(1); Capture(TEXT("baseline")); break;
				}
			}
			return true;
		}
		auto* GS = State.Get(); auto* Round = GS->GetRoundManagement();
		if (!Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || Round->GetCurrentRoundNumber() != RoundNumber) return Stop(TEXT("phase-changed"));
		HoldAll(GS);
		if (Now >= NextSample) { Capture(TEXT("poll")); NextSample = Now + 0.5; }
		if (Stage == 1 && Now - Since > 2) { Attack(); Advance(2); }
		else if (Stage == 2 && Alive(Victim.Get()) && Health(Victim.Get())->GetHealth() < BeforeAttackHealth)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayRemoteCredit: DamageObserved stage=2 before=%.3f after=%.3f"), BeforeAttackHealth, Health(Victim.Get())->GetHealth());
			Advance(3);
		}
		else if (Stage == 3 && Now - Since > 1.5)
		{
			Capture(TEXT("before-finish"));
			UBwayDamageLibrary::ApplyDamageFromSource(Partner->GetLyraAbilitySystemComponent(), Victim->GetPawn(), 100000, Partner->GetPawn(), Partner->GetPawn());
			Capture(TEXT("after-finish")); Advance(4);
		}
		else if (Stage == 4 && Now - Since > 4 && Alive(Victim.Get()))
		{
			Position();
			UBwayDamageLibrary::ApplyDamageFromSource(Partner->GetLyraAbilitySystemComponent(), Victim->GetPawn(), 25, Partner->GetPawn(), Partner->GetPawn());
			// Low-health fixture setup is not the lethal hit; the remote ability must kill.
			Victim->GetLyraAbilitySystemComponent()->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 1);
			UE_LOG(LogTemp, Display, TEXT("BwayRemoteCredit: KillSetup partner=%d victim=%d health=%.3f"), PartnerId, VictimId, Health(Victim.Get())->GetHealth());
			Advance(5);
		}
		else if (Stage == 5 && Now - Since > 2) { Attack(); Advance(6); }
		else if (Stage == 6 && Victim->GetDeaths() == 2)
		{
			UE_LOG(LogTemp, Display, TEXT("BwayRemoteCredit: RemoteKill remote=%d k=%d partner=%d a=%d victim=%d d=%d"), RemoteId, Remote->GetKills(), PartnerId, Partner->GetAssists(), VictimId, Victim->GetDeaths());
			Advance(7);
		}
		else if (Stage == 7 && Now - Since > 4 && Alive(Victim.Get())) return Stop(TEXT("observed"));
		return true;
	}
};
FAutoConsoleCommand Command(TEXT("bway.Test.RemoteCreditAudit"), TEXT("Development: remote granted primary attack earns assist and kill; restores fixture transforms and movement."), FConsoleCommandDelegate::CreateLambda([]
{
	if (Running || !GEngine) return;
	Running = true;
	TSharedRef<FRun> Run = MakeShared<FRun>();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.1f);
}));

bool PeerRunning = false;
FAutoConsoleCommand PeerCommand(TEXT("bway.Test.RemoteCreditPeerAudit"), TEXT("Development: read-only packaged client credit journal; bounded wait and normal test-exit marker."), FConsoleCommandDelegate::CreateLambda([]
{
	if (PeerRunning || !GEngine) return;
	PeerRunning = true;
	TSharedRef<FRun> Observer = MakeShared<FRun>();
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Observer](float)
	{
		const double Now = FPlatformTime::Seconds();
		bool Ready = false;
		for (const auto& Context : GEngine->GetWorldContexts())
		{
			auto* W = Context.World();
			auto* GS = W && W->GetNetMode() == NM_Client ? W->GetGameState<ABwayGameState>() : nullptr;
			auto* Round = GS ? GS->GetRoundManagement() : nullptr;
			Ready |= Round && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && GS->PlayerArray.Num() == 8;
		}
		if (Ready && Observer->Stage == 0)
		{
			Observer->Stage = 8; Observer->Since = Now;
			UE_LOG(LogTemp, Display, TEXT("BwayRemoteCreditPeer: READY"));
		}
		if (Observer->Stage == 8) Observer->Capture(TEXT("peer"));
		if ((Observer->Stage == 8 && Now - Observer->Since >= 45) || (Observer->Stage == 0 && Now - Observer->Started >= 180))
		{
			UE_LOG(LogTemp, Display, TEXT("BwayRemoteCreditPeer: COMPLETE reason=%s samples=%d"), Observer->Stage == 8 ? TEXT("observed") : TEXT("timeout"), Observer->Sample);
			PeerRunning = false;
			return false;
		}
		return true;
	}), 0.5f);
}));
}
#endif
