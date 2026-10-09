// Controlled production actions in an owned match; never calls EndRound or the timeout handler.
#if !UE_BUILD_SHIPPING
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGoalVolume.h"
#include "BwayPlayerState.h"
#include "Combat/BwayDamageLibrary.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameState/BwayMidfieldDividerComponent.h"
#include "GameState/BwayMidfieldRulesLibrary.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameState/BwayScoringComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Match/BwayMidfieldDividerActor.h"
#include "Relic/RelicActor.h"
#include "UI/BwayResultsScreenWidget.h"
#include "UObject/UnrealType.h"

namespace BwayMatchOutcomeTest
{
bool Running = false;
constexpr int32 Winners[] = {0, 1, 0, 1, 0};
constexpr int32 Score0[] = {1, 1, 2, 2, 3};
constexpr int32 Score1[] = {0, 1, 1, 2, 2};

struct FHeldPawn
{
	TWeakObjectPtr<ACharacter> Pawn;
	FTransform Transform;
	EMovementMode Mode = MOVE_None;
	uint8 CustomMode = 0;
	TWeakObjectPtr<UBrainComponent> Brain;
	bool BrainWasRunning = false;
};

struct FProbe
{
	double Started = FPlatformTime::Seconds();
	double ReadySince = 0.0;
	double SetupWaitStarted = 0.0;
	FString LastWaitReason;
	double SummarySince = 0.0;
	double TerritoryStart = 0.0;
	int32 TerritoryRemaining = 0;
	int32 PreparedRound = 0;
	int32 StartedActionRound = 0;
	int32 SeenSummary = 0;
	int32 PendingSummary = 0;
	int32 GoalBefore = 0;
	TWeakObjectPtr<ABwayPlayerState> Scorer;
	TWeakObjectPtr<ABwayGameState> GameState;
	TArray<FHeldPawn> Held;
	bool OwnsRelic = false;
	bool GeometryProven = false;

	void Restore()
	{
		int32 Restored = 0, Expired = 0;
		if (OwnsRelic)
			if (ABwayGameState* GS = GameState.Get())
				if (auto* Manager = GS->FindComponentByClass<UBwayRelicManagerComponent>()) Manager->ResetRelic();
		OwnsRelic = false;
		for (const FHeldPawn& State : Held)
		{
			if (auto* Pawn = State.Pawn.Get())
			{
				Pawn->SetActorTransform(State.Transform, false, nullptr, ETeleportType::TeleportPhysics);
				if (auto* Movement = Pawn->GetCharacterMovement()) Movement->SetMovementMode(State.Mode, State.CustomMode);
				++Restored;
			}
			else ++Expired;
			if (State.BrainWasRunning)
				if (UBrainComponent* Brain = State.Brain.Get()) Brain->RestartLogic();
		}
		if (!Held.IsEmpty()) UE_LOG(LogTemp, Display, TEXT("BwayMatchOutcomes: Cleanup round=%d restored=%d expired=%d owned=%d"), PreparedRound, Restored, Expired, Held.Num());
		Held.Reset();
	}

	bool Stop(const TCHAR* Error = nullptr)
	{
		Restore();
		if (Error) UE_LOG(LogTemp, Error, TEXT("BwayMatchOutcomes: FAIL reason=%s actionRound=%d summaries=%d"), Error, StartedActionRound, SeenSummary);
		Running = false;
		return false;
	}

	bool Hold(ABwayGameState* GS)
	{
		auto Wait = [this](const FString& Reason)
		{
			if (LastWaitReason != Reason) { LastWaitReason = Reason; UE_LOG(LogTemp, Display, TEXT("BwayMatchOutcomes: Readiness %s"), *Reason); }
			return false;
		};
		int32 Humans = 0;
		int32 Teams[2] = {0, 0};
		for (APlayerState* Player : GS->PlayerArray)
		{
			auto* PS = Cast<ABwayPlayerState>(Player);
			if (!PS || PS->IsInactive()) continue;
			auto* ASC = PS->GetLyraAbilitySystemComponent();
			auto* Pawn = ASC ? Cast<ACharacter>(ASC->GetAvatarActor()) : nullptr;
			const int32 Team = GS->GetPlayerTeam(PS);
			const float Health = ASC ? ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : -1.0f;
			if (!Pawn || !ASC || Team < 0 || Team > 1 || Health <= 0.0f)
				return Wait(FString::Printf(TEXT("player=%d bot=%d pawn=%s avatar=%s team=%d health=%.1f"), PS->GetPlayerId(), PS->IsABot(), *GetNameSafe(PS->GetPawn()), *GetNameSafe(ASC ? ASC->GetAvatarActor() : nullptr), Team, Health));
			++Teams[Team];
			if (!PS->IsABot()) ++Humans;
		}
		if (Humans != 4 || Teams[0] != 4 || Teams[1] != 4) return Wait(FString::Printf(TEXT("humans=%d team0=%d team1=%d"), Humans, Teams[0], Teams[1]));
		for (APlayerState* Player : GS->PlayerArray)
		{
			auto* PS = Cast<ABwayPlayerState>(Player);
			if (!PS || PS->IsInactive()) continue;
			auto* Pawn = Cast<ACharacter>(PS->GetLyraAbilitySystemComponent()->GetAvatarActor());
			FHeldPawn State;
			State.Pawn = Pawn;
			State.Transform = Pawn->GetActorTransform();
			if (auto* Movement = Pawn->GetCharacterMovement())
			{
				State.Mode = Movement->MovementMode;
				State.CustomMode = Movement->CustomMovementMode;
				Movement->StopMovementImmediately();
				Movement->DisableMovement();
			}
			if (auto* AI = Cast<AAIController>(Pawn->GetController()))
				if (UBrainComponent* Brain = AI->GetBrainComponent())
				{
					State.Brain = Brain;
					State.BrainWasRunning = Brain->IsRunning();
					Brain->StopLogic(TEXT("Owned match-outcome fixture"));
				}
			Held.Add(State);
		}
		// Attachment updates overlap state synchronously. Separate every participant
		// before invoking pickup so unrelated pawns cannot intercept this fixture.
		FVector Center, Direction;
		if (!UBwayMidfieldRulesLibrary::TryGetMidfieldFrame(GS, Center, Direction))
		{
			Restore();
			return Wait(TEXT("missing-goal-frame-for-isolation"));
		}
		const FVector Across = FVector::CrossProduct(Direction, FVector::UpVector);
		for (int32 Index = 0; Index < Held.Num(); ++Index)
			if (ACharacter* Pawn = Held[Index].Pawn.Get())
			{
				const int32 Team = GS->GetPlayerTeam(Pawn->GetPlayerState());
				const FVector Position = Center + Direction * (Team == 0 ? -1000.0f : 1000.0f)
					+ Across * ((Index - 3.5f) * 500.0f) + FVector(0.0f, 0.0f, 150.0f);
				Pawn->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);
			}
		return true;
	}

	bool Act(ABwayGameState* GS, UBwayRoundManagementComponent* Round)
	{
		const int32 Number = Round->GetCurrentRoundNumber();
		const int32 Winner = Winners[Number - 1];
		auto* Manager = GS->FindComponentByClass<UBwayRelicManagerComponent>();
		ARelicActor* Relic = Manager ? Manager->GetRelicActor() : nullptr;
		ABwayPlayerState* Source = nullptr;
		for (APlayerState* Player : GS->PlayerArray)
			if (auto* PS = Cast<ABwayPlayerState>(Player); PS && !PS->IsABot() && GS->GetPlayerTeam(PS) == Winner) { Source = PS; break; }
		auto* Carrier = Source ? Cast<ABwayCharacterWithAbilities>(Source->GetPawn()) : nullptr;
		if (!Relic || !Carrier) return Stop(TEXT("missing-relic-or-human-source"));
		StartedActionRound = Number;
		if (Number == 2 || Number == 5)
		{
			int32 Killed = 0;
			for (APlayerState* Player : GS->PlayerArray)
				if (auto* Target = Cast<ABwayPlayerState>(Player); Target && GS->GetPlayerTeam(Target) == 1 - Winner)
				{
					auto* ASC = Target->GetLyraAbilitySystemComponent();
					const float Before = ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
					const bool Applied = UBwayDamageLibrary::ApplyDamageFromSource(Source->GetLyraAbilitySystemComponent(), Target->GetPawn(), 1000000.0f, Carrier, Carrier);
					const float After = ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute());
					UE_LOG(LogTemp, Display, TEXT("BwayMatchOutcomes: Damage round=%d target=%d before=%.1f after=%.1f applied=%d"), Number, Target->GetPlayerId(), Before, After, Applied);
					if (!Applied || Before <= 0.0f || After > 0.0f) return Stop(TEXT("lethal-production-damage-failed"));
					++Killed;
				}
			if (Killed != 4) return Stop(TEXT("elimination-roster-not-four"));
			UE_LOG(LogTemp, Display, TEXT("BwayMatchOutcomes: Action round=%d kind=elimination winner=%d victims=%d"), Number, Winner, Killed);
			return true;
		}
		Relic->OnDropped();
		Relic->SetActorLocation(Carrier->GetActorLocation() + FVector(0.0f, 0.0f, 1000.0f), false, nullptr, ETeleportType::TeleportPhysics);
		Relic->OnPickedUp(Carrier);
		OwnsRelic = true;
		if (Relic->CurrentCarrier != Carrier) return Stop(TEXT("production-pickup-failed"));
		if (Number == 3)
		{
			FVector Center, Direction;
			if (!UBwayMidfieldRulesLibrary::TryGetMidfieldFrame(GS, Center, Direction)) return Stop(TEXT("missing-arena-frame"));
			Carrier->SetActorLocation(Center + Direction * 1500.0f - FVector::CrossProduct(Direction, FVector::UpVector) * 600.0f, false, nullptr, ETeleportType::TeleportPhysics);
			const int32 Half = UBwayMidfieldRulesLibrary::GetLosingTeamAtMidfieldFromRelic(Relic);
			if (Half != 1) return Stop(TEXT("territory-setup-wrong-half"));
			TerritoryStart = GS->GetWorld()->GetTimeSeconds();
			TerritoryRemaining = Round->GetRoundTimeRemaining();
			if (TerritoryRemaining < 230) return Stop(TEXT("territory-round-was-shortened"));
			UE_LOG(LogTemp, Display, TEXT("BwayMatchOutcomes: Action round=3 kind=territory winner=0 losingHalf=%d remaining=%d relic=%s axis=%s"), Half, TerritoryRemaining, *Relic->GetActorLocation().ToString(), *Direction.ToString());
			return true;
		}
		ABwayGoalVolume* Goal = nullptr;
		for (TActorIterator<ABwayGoalVolume> It(GS->GetWorld()); It; ++It)
			if (It->OwningTeam == 1 - Winner) { Goal = *It; break; }
		if (!Goal) return Stop(TEXT("missing-opposing-goal"));
		Scorer = Source;
		GoalBefore = Source->GetObjectiveScore();
		Carrier->SetActorLocation(Goal->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
		for (UActorComponent* Component : Goal->GetComponents())
			if (auto* Primitive = Cast<UPrimitiveComponent>(Component)) Primitive->UpdateOverlaps();
		UE_LOG(LogTemp, Display, TEXT("BwayMatchOutcomes: Action round=%d kind=goal winner=%d scorer=%d before=%d"), Number, Winner, Source->GetPlayerId(), GoalBefore);
		return true;
	}

	bool Tick(float)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 600.0) return Stop(TEXT("full-match-timeout"));
		if (!GEngine) return true;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || World->WorldType != EWorldType::Game || !World->GetMapName().Contains(TEXT("L_BW_Dorado")) ||
				(World->GetNetMode() != NM_ListenServer && World->GetNetMode() != NM_Client)) continue;
			auto* GS = World->GetGameState<ABwayGameState>();
			auto* Round = GS ? GS->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
			auto* Scoring = GS ? GS->FindComponentByClass<UBwayScoringComponent>() : nullptr;
			if (!Round || !Scoring) continue;
			GameState = GS;
			const bool Authority = GS->HasAuthority();
			const int32 Number = Round->GetCurrentRoundNumber();
			if (Round->HasActivePostRoundSummary())
			{
				const auto Summary = Round->GetActivePostRoundSummary();
				const int32 Completed = Summary.CompletedRoundNumber;
				if (Completed > SeenSummary)
				{
					if (Completed != PendingSummary) { PendingSummary = Completed; SummarySince = Now; }
					if (Now - SummarySince < 0.75) return true;
					if (Completed != SeenSummary + 1 || Completed < 1 || Completed > 5 || Summary.RoundWinningTeam != Winners[Completed - 1] ||
						Summary.PointsToWin != 3 || Summary.Team0Score != Score0[Completed - 1] || Summary.Team1Score != Score1[Completed - 1] ||
						Scoring->GetTeamScore(0) != Summary.Team0Score || Scoring->GetTeamScore(1) != Summary.Team1Score)
						return Stop(TEXT("round-summary-or-score-mismatch"));
					if (Authority)
					{
						if (StartedActionRound != Completed) return Stop(TEXT("round-ended-before-fixture-action"));
						if ((Completed == 1 || Completed == 4) && (!Scorer.IsValid() || Scorer->GetObjectiveScore() != GoalBefore + 1)) return Stop(TEXT("goal-credit-mismatch"));
						if (Completed == 3)
						{
							const double Elapsed = World->GetTimeSeconds() - TerritoryStart;
							if (Elapsed < TerritoryRemaining - 1.0) return Stop(TEXT("territory-ended-before-timer"));
							UE_LOG(LogTemp, Display, TEXT("BwayMatchOutcomes: TerritoryElapsed seconds=%.2f setupRemaining=%d"), Elapsed, TerritoryRemaining);
						}
						Restore();
					}
					if (!GeometryProven)
					{
						FVector Center, Direction;
						if (!UBwayMidfieldRulesLibrary::TryGetMidfieldFrame(GS, Center, Direction)) return Stop(TEXT("peer-goal-frame-missing"));
						for (TActorIterator<ABwayMidfieldDividerActor> Divider(World); Divider; ++Divider)
							for (UActorComponent* Component : Divider->GetComponents())
								if (auto* Mesh = Cast<UStaticMeshComponent>(Component); Mesh && Mesh->GetFName() == TEXT("DividerMesh"))
									GeometryProven = FMath::Abs(FVector::DotProduct(Mesh->GetForwardVector(), Direction)) > 0.99f && Mesh->GetUpVector().Z > 0.99f &&
										FMath::Abs(FVector::DotProduct(Divider->GetActorLocation() - Center, Direction)) < 1.0f;
						if (!GeometryProven) return Stop(TEXT("peer-divider-stripe-misaligned"));
						UE_LOG(LogTemp, Display, TEXT("BwayMatchOutcomes: Geometry net=%d stripeAligned=1 center=%s axis=%s"), int32(World->GetNetMode()), *Center.ToString(), *Direction.ToString());
					}
					SeenSummary = Completed;
					UE_LOG(LogTemp, Display, TEXT("BwayMatchOutcomes: Summary net=%d round=%d winner=%d score=%d-%d points=%d"), int32(World->GetNetMode()), Completed, Summary.RoundWinningTeam, Summary.Team0Score, Summary.Team1Score, Summary.PointsToWin);
				}
			}
			if (Round->GetCurrentMatchPhase() == EBwayMatchPhase::PostMatch)
			{
				if (SeenSummary != 5) return Stop(TEXT("match-ended-before-five-controlled-rounds"));
				for (TObjectIterator<UBwayResultsScreenWidget> It; It; ++It)
				{
					if (It->IsTemplate() || It->GetWorld() != World) continue;
					const FStructProperty* Property = FindFProperty<FStructProperty>(It->GetClass(), TEXT("CachedSummary"));
					if (!Property) return Stop(TEXT("results-schema-missing"));
					const auto* Result = Property->ContainerPtrToValuePtr<FBwayPostMatchSummaryData>(*It);
					if (Result->TotalRounds == 0) continue;
					if (Result->WinningTeam != 0 || Result->Team0Score != 3 || Result->Team1Score != 2 || Result->TotalRounds != 5 || Result->PointsToWin != 3 || Result->PlayerColumns.Num() != 8)
						return Stop(TEXT("final-results-mismatch"));
					UE_LOG(LogTemp, Display, TEXT("BwayMatchOutcomes: PASS net=%d rounds=5 score=3-2 winner=0 points=3 columns=%d summaries=%d"), int32(World->GetNetMode()), Result->PlayerColumns.Num(), SeenSummary);
					return Stop();
				}
				return true;
			}
			if (!Authority || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || Round->GetCurrentRoundState() != ERoundState::RoundActive) return true;
			if (Number < 1 || Number > 5 || Round->PointsToWin != 3 || !FMath::IsNearlyEqual(Round->RoundDuration, 240.0f)) return Stop(TEXT("unexpected-demo-round-rules"));
			if (PreparedRound != Number)
			{
				if (Number != SeenSummary + 1) return Stop(TEXT("round-start-skipped-summary"));
				if (SetupWaitStarted == 0.0) SetupWaitStarted = Now;
				if (!Hold(GS)) return Now - SetupWaitStarted > 15.0 ? Stop(TEXT("participant-readiness-timeout")) : true;
				SetupWaitStarted = 0.0;
				PreparedRound = Number;
				ReadySince = Now;
				FVector Center, Direction;
				ABwayMidfieldDividerActor* Divider = GS->MidfieldDividerComponent ? GS->MidfieldDividerComponent->GetMidfieldDivider() : nullptr;
				if (!UBwayMidfieldRulesLibrary::TryGetMidfieldFrame(GS, Center, Direction) || !Divider || FVector::DotProduct(Divider->GetActorForwardVector(), Direction) < 0.99f)
					return Stop(TEXT("divider-not-aligned-with-goals"));
				UE_LOG(LogTemp, Display, TEXT("BwayMatchOutcomes: Ready round=%d humans=4 bots=4 duration=240 points=3 dividerAligned=1"), Number);
			}
			if (StartedActionRound != Number && Now - ReadySince >= 2.0) return Act(GS, Round);
			return true;
		}
		return true;
	}
};

FAutoConsoleCommand Command(TEXT("bway.Test.MatchOutcomes"), TEXT("Development-only controlled five-round match: goal, elimination, full-time territory, goal, elimination; read-only peers verify summaries/results."),
	FConsoleCommandDelegate::CreateLambda([]
	{
		if (Running) return;
		Running = true;
		TSharedRef<FProbe> Probe = MakeShared<FProbe>();
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Delta) { return Probe->Tick(Delta); }), 0.25f);
	}));
}
#endif
