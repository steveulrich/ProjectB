// Read-only observation of authority-resolved rules on each multiplayer peer.
#if !UE_BUILD_SHIPPING
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "String/LexFromString.h"

namespace BwayMatchRulesTest
{
bool Running = false;

struct FProbe
{
	int32 ExpectedPoints = 3;
	float ExpectedDuration = 240.0f;
	double Started = FPlatformTime::Seconds();
	double RoundSince = 0.0;
	int32 RoundNumber = 0;
	int32 Samples = 0;
	int32 FirstRemaining = 0;
	int32 LastRemaining = 0;
	TWeakObjectPtr<ABwayGameState> Observed;

	bool Stop(const TCHAR* Error = nullptr)
	{
		if (Error) UE_LOG(LogTemp, Error, TEXT("BwayMatchRules: FAIL reason=%s samples=%d"), Error, Samples);
		Running = false;
		return false;
	}

	bool Tick(float)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 180.0) return Stop(TEXT("ready-timeout"));
		if (!GEngine) return true;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (!World || World->WorldType != EWorldType::Game ||
				(World->GetNetMode() != NM_ListenServer && World->GetNetMode() != NM_Client)) continue;
			auto* GS = World->GetGameState<ABwayGameState>();
			auto* Round = GS ? GS->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
			if (!Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing ||
				Round->GetCurrentRoundState() != ERoundState::RoundActive) continue;
			int32 Humans = 0;
			int32 Bots = 0;
			for (APlayerState* Player : GS->PlayerArray)
				if (auto* PS = Cast<ABwayPlayerState>(Player); PS && !PS->IsInactive())
				{
					if (PS->IsABot()) ++Bots; else ++Humans;
				}
			if (Humans != 4) continue;
			if (Observed.Get() != GS || RoundNumber != Round->GetCurrentRoundNumber())
			{
				Observed = GS;
				RoundNumber = Round->GetCurrentRoundNumber();
				RoundSince = Now;
				Samples = 0;
			}
			if (Now - RoundSince < 2.0) return true;
			const int32 Remaining = Round->GetRoundTimeRemaining();
			UE_LOG(LogTemp, Display, TEXT("BwayMatchRules: Sample net=%d round=%d sample=%d humans=%d bots=%d points=%d duration=%.1f remaining=%d"),
				int32(World->GetNetMode()), RoundNumber, Samples + 1, Humans, Bots, Round->PointsToWin, Round->RoundDuration, Remaining);
			if (Round->PointsToWin != ExpectedPoints || !FMath::IsNearlyEqual(Round->RoundDuration, ExpectedDuration))
				return Stop(TEXT("resolved-rule-mismatch"));
			if (Remaining <= 0 || Remaining > FMath::CeilToInt(ExpectedDuration) || (Samples && Remaining > LastRemaining))
				return Stop(TEXT("replicated-countdown-invalid"));
			if (!Samples) FirstRemaining = Remaining;
			LastRemaining = Remaining;
			if (++Samples == 4)
			{
				if (FirstRemaining <= Remaining) return Stop(TEXT("countdown-did-not-advance"));
				UE_LOG(LogTemp, Display, TEXT("BwayMatchRules: PASS net=%d round=%d samples=%d humans=%d bots=%d points=%d duration=%.1f elapsedTicks=%d"),
					int32(World->GetNetMode()), RoundNumber, Samples, Humans, Bots, Round->PointsToWin, Round->RoundDuration, FirstRemaining - Remaining);
				return Stop();
			}
			return true;
		}
		return true;
	}
};

FAutoConsoleCommand Command(TEXT("bway.Test.MatchRules"), TEXT("Development-only read-only four-human rule/countdown probe. Args: expected points, duration."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Running) return;
		TSharedRef<FProbe> Probe = MakeShared<FProbe>();
		if (Args.Num() != 2 || !LexTryParseString(Probe->ExpectedPoints, *Args[0]) ||
			!LexTryParseString(Probe->ExpectedDuration, *Args[1]) || Probe->ExpectedPoints < 1 || Probe->ExpectedDuration < 1.0f)
		{
			UE_LOG(LogTemp, Error, TEXT("BwayMatchRules: FAIL reason=invalid-arguments"));
			return;
		}
		Running = true;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Probe](float Delta) { return Probe->Tick(Delta); }), 1.0f);
	}));
}
#endif
