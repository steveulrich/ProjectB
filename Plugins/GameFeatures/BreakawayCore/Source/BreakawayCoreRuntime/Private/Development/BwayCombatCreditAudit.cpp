// Development-only production damage/credit observation. Never awards stats itself.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
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
#include "UObject/UnrealType.h"

namespace BwayCombatCreditAudit
{
bool Running = false;
struct FHeld
{
	TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
	EMovementMode Mode;
	uint8 Custom;
	TWeakObjectPtr<UBrainComponent> Brain;
	bool BrainRunning;
};
struct FSubject
{
	const TCHAR* Role;
	TWeakObjectPtr<ABwayPlayerState> Player;
	int32 Id;
};
struct FRun
{
	static constexpr const TCHAR* Cases[] = {TEXT("normal"), TEXT("respawn"), TEXT("expired"), TEXT("self"), TEXT("friendly"), TEXT("source-respawn")};
	double Started = FPlatformTime::Seconds(), Since = Started, NextSample = 0;
	int32 Stage = 0, InitialRound = 0, Sample = 0, Case = 0;
	TWeakObjectPtr<ABwayGameState> State;
	TArray<FSubject> Subjects;
	TArray<FHeld> Held;

	void Hold(APawn* Pawn)
	{
		auto* Character = Cast<ABwayCharacterWithAbilities>(Pawn);
		if (!Character || Held.ContainsByPredicate([Character](const FHeld& H) { return H.Pawn == Character; })) return;
		auto* Move = Character->GetCharacterMovement();
		auto* AI = Cast<AAIController>(Character->GetController());
		auto* Brain = AI ? AI->GetBrainComponent() : nullptr;
		Held.Add({Character, Move->MovementMode, Move->CustomMovementMode, Brain, Brain && Brain->IsRunning()});
		Move->StopMovementImmediately(); Move->DisableMovement();
		if (Brain) Brain->StopLogic(TEXT("Owned combat credit audit"));
	}
	bool Stop(const TCHAR* Reason)
	{
		int32 Restored = 0, Expired = 0;
		for (const auto& H : Held)
		{
			if (H.Pawn.IsValid()) { H.Pawn->GetCharacterMovement()->SetMovementMode(H.Mode, H.Custom); ++Restored; }
			else ++Expired;
			if (H.BrainRunning && H.Brain.IsValid()) H.Brain->RestartLogic();
		}
		UE_LOG(LogTemp, Display, TEXT("BwayCombatCredit: COMPLETE reason=%s stage=%d samples=%d restored=%d expired=%d held=%d"), Reason, Stage, Sample, Restored, Expired, Held.Num());
		Running = false;
		return false;
	}
	bool Alive(ABwayPlayerState* PS) const
	{
		auto* Pawn = PS ? PS->GetPawn() : nullptr;
		auto* Health = Pawn ? ULyraHealthComponent::FindHealthComponent(Pawn) : nullptr;
		return Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0;
	}
	void Capture(const TCHAR* Label)
	{
		++Sample;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* W = Context.World();
			auto* GS = W && W->IsGameWorld() ? W->GetGameState<ABwayGameState>() : nullptr;
			auto* Round = GS ? GS->GetRoundManagement() : nullptr;
			if (!Round) continue;
			for (const FSubject& Subject : Subjects)
			{
				ABwayPlayerState* Player = nullptr;
				for (APlayerState* Peer : GS->PlayerArray)
					if (Peer && Peer->GetPlayerId() == Subject.Id) Player = Cast<ABwayPlayerState>(Peer);
				if (!Player) continue;
				auto* ASC = Player->GetLyraAbilitySystemComponent();
				const auto* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
				const auto* Pawn = Player->GetPawn();
				UE_LOG(LogTemp, Display, TEXT("BwayCombatCredit: Sample case=%s n=%d label=%s world=%s net=%d role=%s id=%d team=%d bot=%d round=%d phase=%d kills=%d deaths=%d assists=%d gold=%.3f income=%.3f alive=%d pawn=%s"),
					Cases[Case], Sample, Label, *W->GetPathName(), int32(W->GetNetMode()), Subject.Role, Subject.Id, GS->GetPlayerTeam(Player), Player->IsABot(), Round->GetCurrentRoundNumber(), int32(Round->GetCurrentMatchPhase()),
					Player->GetKills(), Player->GetDeaths(), Player->GetAssists(), Gold ? Gold->GetCurrentGold() : -1, Gold ? Gold->GetGoldPerSecond() : -1, Alive(Player), *GetNameSafe(Pawn));
			}
		}
	}
	bool Damage(int32 SourceIndex, float Amount, const TCHAR* Label, int32 TargetIndex = 2)
	{
		auto* Source = Subjects[SourceIndex].Player.Get();
		auto* Victim = Subjects[TargetIndex].Player.Get();
		if (!Alive(Source) || !Alive(Victim)) return false;
		APawn* SourcePawn = Source->GetPawn();
		APawn* VictimPawn = Victim->GetPawn();
		TWeakObjectPtr<ULyraHealthComponent> Health = ULyraHealthComponent::FindHealthComponent(VictimPawn);
		const float Before = Health->GetHealth();
		for (const FGameplayAbilitySpec& Ability : Victim->GetLyraAbilitySystemComponent()->GetActivatableAbilities())
		{
			if (!Ability.Ability || !Ability.Ability->GetClass()->GetName().Contains(TEXT("Death"))) continue;
			const auto* Duration = FindFProperty<FNumericProperty>(Ability.Ability->GetClass(), TEXT("Duration"));
			UE_LOG(LogTemp, Display, TEXT("BwayCombatCredit: DeathAbility case=%s label=%s class=%s active=%d state=%d duration=%.3f"),
				Cases[Case], Label, *Ability.Ability->GetClass()->GetName(), Ability.IsActive(), int32(Health->GetDeathState()),
				Duration ? Duration->GetFloatingPointPropertyValue(Duration->ContainerPtrToValuePtr<void>(Ability.Ability)) : -1.0);
		}
		UBwayDamageLibrary::ApplyDamageFromSource(Source->GetLyraAbilitySystemComponent(), VictimPawn, Amount, SourcePawn, SourcePawn);
		UE_LOG(LogTemp, Display, TEXT("BwayCombatCredit: Damage label=%s source=%d victim=%d requested=%.1f before=%.3f after=%.3f deaths=%d"),
			Label, Source->GetPlayerId(), Victim->GetPlayerId(), Amount, Before, Health.IsValid() ? Health->GetHealth() : 0, Victim->GetDeaths());
		return Health.IsValid() ? Health->GetHealth() < Before : Victim->GetDeaths() > 0;
	}
	bool Tick(float)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 160) return Stop(TEXT("timeout"));
		if (!State.IsValid())
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				UWorld* W = Context.World();
				auto* GS = W && W->IsGameWorld() && W->GetNetMode() != NM_Client ? W->GetGameState<ABwayGameState>() : nullptr;
				auto* Round = GS ? GS->GetRoundManagement() : nullptr;
				if (!Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) continue;
				TArray<ABwayPlayerState*> Team[2];
				int32 Humans = 0;
				for (APlayerState* Peer : GS->PlayerArray)
				{
					auto* PS = Cast<ABwayPlayerState>(Peer);
					const int32 T = PS ? GS->GetPlayerTeam(PS) : INDEX_NONE;
					if (PS && T >= 0 && T <= 1 && PS->IsHeroLocked() && Alive(PS))
					{ Team[T].Add(PS); Humans += !PS->IsABot(); }
				}
				if (Humans != 4 || Team[0].Num() != 4 || Team[1].Num() != 4) continue;
				for (auto& Players : Team) Players.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B)
					{ return A.IsABot() != B.IsABot() ? !A.IsABot() : A.GetPlayerId() < B.GetPlayerId(); });
				if (Team[0][1]->IsABot() || Team[1][0]->IsABot()) continue;
				State = GS; InitialRound = Round->GetCurrentRoundNumber();
				Subjects = {{TEXT("assister"), Team[0][0], Team[0][0]->GetPlayerId()},
					{TEXT("killer"), Team[0][1], Team[0][1]->GetPlayerId()},
					{TEXT("victim"), Team[1][0], Team[1][0]->GetPlayerId()},
					{TEXT("noncontributor"), Team[0][2], Team[0][2]->GetPlayerId()},
					{TEXT("friendly"), Team[1][1], Team[1][1]->GetPlayerId()}};
				for (const auto& Players : Team) for (auto* PS : Players) Hold(PS->GetPawn());
				const auto* Property = FindFProperty<FFloatProperty>(Round->GetClass(), TEXT("GoldAwardForKill"));
				UE_LOG(LogTemp, Display, TEXT("BwayCombatCredit: Setup humans=4 bots=4 round=%d killGold=%.3f assister=%d killer=%d victim=%d noncontributor=%d"),
					InitialRound, Property ? Property->GetPropertyValue_InContainer(Round) : -1, Subjects[0].Id, Subjects[1].Id, Subjects[2].Id, Subjects[3].Id);
				UE_LOG(LogTemp, Display, TEXT("BwayCombatCredit: Policy assistGold=%.3f window=%.3f cases=6 provisional=1"), Round->GoldAwardForAssist, Round->AssistCreditWindowSeconds);
				Stage = 1; Since = Now;
				break;
			}
			return true;
		}
		auto* Round = State->GetRoundManagement();
		if (!Round || Round->GetCurrentRoundNumber() != InitialRound || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) return Stop(TEXT("unexpected-round-transition"));
		for (APlayerState* Peer : State->PlayerArray) if (Peer) Hold(Peer->GetPawn());
		if (Stage == 1 && Now - Since >= 1.0)
		{
			for (const auto& Subject : Subjects) if (!Alive(Subject.Player.Get())) return true;
			Capture(TEXT("before"));
			// The normal case deliberately uses damage below the integer HUD rounding threshold.
			if (Case != 1 && !Damage(0, Case == 0 ? 0.25f : 25.0f, TEXT("contribution"))) return Stop(TEXT("first-damage-failed"));
			if (Case == 5)
			{
				if (!Damage(2, 100000.0f, TEXT("source-death"), 0)) return Stop(TEXT("source-death-failed"));
				Capture(TEXT("source-death"));
			}
			Stage = 2; Since = Now;
		}
		else if (Stage == 2 && Now - Since >= (Case == 2 ? Round->AssistCreditWindowSeconds + 0.5 : 0.5))
		{
			if (Case == 5 && !Alive(Subjects[0].Player.Get())) return true;
			Capture(TEXT("prekill"));
			auto* VictimPawn = Subjects[2].Player->GetPawn();
			if (Case == 3)
			{
				auto* Health = ULyraHealthComponent::FindHealthComponent(VictimPawn);
				if (!Health) return Stop(TEXT("missing-self-health"));
				Health->DamageSelfDestruct();
			}
			else if (!Damage(Case == 4 ? 4 : 1, 100000.0f, TEXT("lethal"))) return Stop(TEXT("lethal-damage-failed"));
			// Reentrant/duplicate callbacks must not award a second death, kill, assist or gold.
			Round->OnPlayerDied(VictimPawn->GetController(), Subjects[1].Player->GetPawn()->GetController());
			Round->OnPlayerDied(VictimPawn->GetController(), Subjects[1].Player->GetPawn()->GetController());
			Capture(TEXT("immediate"));
			UE_LOG(LogTemp, Display, TEXT("BwayCombatCredit: Duplicates case=%s calls=2"), Cases[Case]);
			Stage = 3; Since = Now; NextSample = Now;
		}
		else if (Stage == 3)
		{
			if (Now >= NextSample) { Capture(Now - Since < 2.0 ? TEXT("settling") : TEXT("settled")); NextSample = Now + 0.5; }
			if (Now - Since > 2.0)
			{
				auto* Pawn = Subjects[2].Player->GetPawn();
				auto* Health = Pawn ? ULyraHealthComponent::FindHealthComponent(Pawn) : nullptr;
				if (Health && Health->GetHealth() <= 0 && !Health->IsDeadOrDying()) return Stop(TEXT("zero-health-without-death"));
			}
			if (Now - Since >= 6.0)
			{
				if (Case == 5) return Stop(TEXT("observed"));
				++Case; Stage = 1; Since = Now;
			}
		}
		return true;
	}
};

FAutoConsoleCommand Command(TEXT("bway.Test.CombatCreditAudit"), TEXT("Development: observe two-enemy-contributor lethal damage, peer KDA/gold and respawn; restores owned movement and bot holds."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (Running || !GEngine) return;
		Running = true;
		TSharedRef<FRun> Run = MakeShared<FRun>();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.1f);
	}));
}
#endif
