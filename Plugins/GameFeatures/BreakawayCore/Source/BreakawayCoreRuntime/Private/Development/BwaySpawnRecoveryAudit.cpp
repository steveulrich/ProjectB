// Development-only live reproduction of temporary spawn obstruction.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "AI/BwayRelicBotController.h"
#include "BrainComponent.h"
#include "BreakawayGameMode.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Character/LyraHealthComponent.h"
#include "Components/BoxComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerStart.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameState/BwayBotCreationComponent.h"
#include "HAL/IConsoleManager.h"
#include "BwaySpawnTestGeometry.h"

namespace BwaySpawnRecoveryAudit
{
bool Running = false;
struct FHeld
{
    TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
    FTransform Transform;
    EMovementMode Mode;
    uint8 Custom;
    TWeakObjectPtr<UBrainComponent> Brain;
    bool BrainRunning;
};
struct FStart
{
    TWeakObjectPtr<APlayerStart> Start;
    FTransform Transform;
    TWeakObjectPtr<AActor> Blocker;
    EComponentMobility::Type Mobility;
};
struct FRun
{
    FString Scenario = TEXT("death");
    double Started = FPlatformTime::Seconds(), StageStarted = Started, Deadline = 0, PrimedAt = 0;
    int32 Stage = 0, Team = -1, Deaths = 0, Round = 0, Samples = 0;
    TWeakObjectPtr<ABwayGameState> GS;
    TWeakObjectPtr<ABreakawayGameMode> GM;
    TWeakObjectPtr<ABwayRelicBotController> Bot;
    TWeakObjectPtr<ABwayPlayerState> Subject;
    TWeakObjectPtr<APawn> OriginalPawn, Replacement;
    TArray<FHeld> Held;
    TArray<FStart> Starts;

    static bool Alive(APawn* Pawn)
    {
        const auto* H = Pawn ? ULyraHealthComponent::FindHealthComponent(Pawn) : nullptr;
        return H && !H->IsDeadOrDying() && H->GetHealth() > 0;
    }
    void Hold(APawn* Pawn)
    {
        auto* C = Cast<ABwayCharacterWithAbilities>(Pawn);
        if (!C || Held.ContainsByPredicate([C](const FHeld& H) { return H.Pawn == C; })) return;
        auto* M = C->GetCharacterMovement();
        auto* AI = Cast<AAIController>(C->GetController());
        auto* Brain = AI ? AI->GetBrainComponent() : nullptr;
        FTransform RestoreTransform = C->GetActorTransform();
        for (const auto& S : Starts) if (S.Start.IsValid() && FVector::Dist(C->GetActorLocation(), S.Start->GetActorLocation()) < 500)
        { RestoreTransform = Starts[Held.Num() % Starts.Num()].Transform; break; }
        Held.Add({C, RestoreTransform, M->MovementMode, M->CustomMovementMode, Brain, Brain && Brain->IsRunning()});
        M->StopMovementImmediately(); M->DisableMovement();
        if (Brain) Brain->StopLogic(TEXT("Owned spawn recovery audit"));
        // Keep observers and replacements near the one released start. Moving
        // new pawns between distant relevance regions obscures restart evidence.
        C->SetActorLocation(FVector(50000, Held.Num() * 300, 20000), false, nullptr, ETeleportType::TeleportPhysics);
        C->ForceNetUpdate();
    }
    bool Stop(const TCHAR* Reason)
    {
        int32 Restored = 0;
        for (const auto& S : Starts)
        {
            if (S.Blocker.IsValid()) S.Blocker->Destroy();
            if (S.Start.IsValid())
            {
                S.Start->SetActorTransform(S.Transform);
                S.Start->GetRootComponent()->SetMobility(S.Mobility);
            }
        }
        for (const auto& H : Held)
        {
            if (H.Pawn.IsValid())
            {
                H.Pawn->SetActorTransform(H.Transform, false, nullptr, ETeleportType::TeleportPhysics);
                H.Pawn->GetCharacterMovement()->SetMovementMode(H.Mode, H.Custom); ++Restored;
            }
            if (H.BrainRunning && H.Brain.IsValid()) H.Brain->RestartLogic();
        }
        UE_LOG(LogTemp, Display, TEXT("BwaySpawnRecovery: COMPLETE reason=%s stage=%d samples=%d startsRestored=%d restored=%d held=%d scenario=%s"),
            Reason, Stage, Samples, Starts.Num(), Restored, Held.Num(), *Scenario);
        Running = false; return false;
    }
    bool Setup(UWorld* W, ABwayGameState* G, ABreakawayGameMode* Mode)
    {
        GS = G; GM = Mode; Round = G->GetRoundManagement()->GetCurrentRoundNumber();
        for (APlayerState* P : G->PlayerArray)
        {
            if (auto* C = Cast<ABwayRelicBotController>(P->GetOwner()))
                if (!Subject.IsValid() || P->GetPlayerId() > Subject->GetPlayerId())
                { Bot = C; Subject = Cast<ABwayPlayerState>(P); }
            Hold(P->GetPawn());
        }
        if (!Bot.IsValid() || !Subject.IsValid()) return false;
        Team = G->GetPlayerTeam(Subject.Get()); Deaths = Subject->GetDeaths(); OriginalPawn = Bot->GetPawn();
        const FName Tag = Team == 0 ? Mode->Team1SpawnPointTag : Mode->Team2SpawnPointTag;
        for (TActorIterator<APlayerStart> It(W); It; ++It) if (It->PlayerStartTag == Tag)
        {
            // Isolate collision geometry while retaining real tagged starts, mode and controller.
            const FTransform Original = It->GetActorTransform();
            const auto Mobility = It->GetRootComponent()->Mobility;
            Starts.Add({*It, Original, nullptr, Mobility});
            It->GetRootComponent()->SetMobility(EComponentMobility::Movable);
            const FVector FixtureLocation(50000, (Starts.Num() - 1) * 8000, 20000);
            if (!It->SetActorLocation(FixtureLocation) || !It->GetActorLocation().Equals(FixtureLocation, 1))
            { UE_LOG(LogTemp, Display, TEXT("BwaySpawnRecovery: SetupRejected reason=start-move")); return false; }
            auto* A = BwaySpawnTestGeometry::SpawnObstruction(W, It->GetActorLocation());
            if (!A) return false;
            Starts.Last().Blocker = A;
        }
        AActor* SelectedStart = Mode->ChoosePlayerStart_Implementation(Bot.Get());
        if (Starts.IsEmpty() || SelectedStart)
        { UE_LOG(LogTemp, Display, TEXT("BwaySpawnRecovery: SetupRejected reason=geometry starts=%d selected=%s pawnClass=%s"), Starts.Num(), *GetNameSafe(SelectedStart), *GetNameSafe(Mode->GetDefaultPawnClassForController(Bot.Get()))); return false; }
        UE_LOG(LogTemp, Display, TEXT("BwaySpawnRecovery: Setup starts=%d team=%d id=%d pawn=%s round=%d mode=%s botClass=%s geometry=isolated-real-blockers"),
            Starts.Num(), Team, Subject->GetPlayerId(), *GetNameSafe(OriginalPawn.Get()), Round, *Mode->GetClass()->GetName(), *Bot->GetClass()->GetName());
        Stage = 1; StageStarted = FPlatformTime::Seconds();
        if (Scenario == TEXT("initial"))
        {
            auto* Creation = G->FindComponentByClass<UBwayBotCreationComponent>();
            if (!Creation || Creation->SpawnedBotList.IsEmpty() || Creation->SpawnedBotList.Last() != Bot.Get()) return false;
            const int32 OldId = Subject->GetPlayerId();
            Creation->TrimExcessBots(Creation->SpawnedBotList.Num() - 1);
            Creation->SpawnMissingBots();
            Bot = nullptr; Subject = nullptr; OriginalPawn = nullptr;
            for (APlayerState* P : G->PlayerArray) if (P->GetPlayerId() > OldId)
                if (auto* C = Cast<ABwayRelicBotController>(P->GetOwner())) { Bot = C; Subject = Cast<ABwayPlayerState>(P); }
            if (!Bot.IsValid() || !Subject.IsValid() || Bot->GetPawn() || Bot->StartSpot.IsValid() || G->GetPlayerTeam(Subject.Get()) != Team) return false;
            Deaths = Subject->GetDeaths(); Deadline = G->GetServerWorldTimeSeconds();
            UE_LOG(LogTemp, Display, TEXT("BwaySpawnRecovery: InitialSpawn oldId=%d newId=%d team=%d pawn=None start=None"), OldId, Subject->GetPlayerId(), Team);
        }
        else if (Scenario == TEXT("reset"))
        {
            G->GetRoundManagement()->ResetRoundState(); Deadline = G->GetServerWorldTimeSeconds();
            UE_LOG(LogTemp, Display, TEXT("BwaySpawnRecovery: RoundReset round=%d phase=%d"), Round, int32(G->GetRoundManagement()->GetCurrentMatchPhase()));
        }
        else ULyraHealthComponent::FindHealthComponent(OriginalPawn.Get())->DamageSelfDestruct(false);
        return true;
    }
    bool Tick(float)
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > 120) return Stop(TEXT("overall-timeout"));
        if (Stage == 0)
        {
            if (GEngine) for (const auto& Context : GEngine->GetWorldContexts())
            {
                auto* W = Context.World(); auto* G = W ? W->GetGameState<ABwayGameState>() : nullptr;
                auto* Mode = W ? W->GetAuthGameMode<ABreakawayGameMode>() : nullptr;
                auto* R = G ? G->GetRoundManagement() : nullptr;
                if (!W || !W->IsGameWorld() || !Mode || !R || R->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || G->PlayerArray.Num() != 8) continue;
                bool Ready = true; for (APlayerState* P : G->PlayerArray) Ready &= P && Alive(P->GetPawn());
                if (!Ready) continue;
                if (PrimedAt <= 0)
                {
                    GS = G; PrimedAt = Now;
                    for (APlayerState* P : G->PlayerArray) Hold(P->GetPawn());
                    UE_LOG(LogTemp, Display, TEXT("BwaySpawnRecovery: BaselineHold players=%d delay=3"), G->PlayerArray.Num());
                    return true;
                }
                if (GS.Get() != G) return Stop(TEXT("baseline-world-changed"));
                if (Now - PrimedAt < 3) return true;
                UE_LOG(LogTemp, Display, TEXT("BwaySpawnRecovery: BaselineReady players=%d"), G->PlayerArray.Num());
                return Setup(W, G, Mode) ? true : Stop(TEXT("setup-blocked"));
            }
            return true;
        }
        if (!GS.IsValid() || !GM.IsValid() || !Bot.IsValid() || !Subject.IsValid()) return Stop(TEXT("world-lost"));
        auto* R = GS->GetRoundManagement();
        const auto ExpectedPhase = Stage == 4 ? EBwayMatchPhase::PostRound : EBwayMatchPhase::Playing;
        if (R->GetCurrentRoundNumber() != Round || R->GetCurrentMatchPhase() != ExpectedPhase) return Stop(TEXT("phase-changed"));
        const double ServerTime = GS->GetServerWorldTimeSeconds();
        Deadline = FMath::Max(Deadline, Subject->GetRespawnSchedule().ServerEndTime);
        APawn* Pawn = Bot->GetPawn();
        int32 OwnedLiving = 0, WorldLiving = 0;
        TMap<APlayerState*, int32> PawnsPerPlayer;
        for (TActorIterator<APawn> It(GS->GetWorld()); It; ++It)
            if (Alive(*It))
            {
                ++WorldLiving;
                if (It->GetPlayerState() == Subject.Get()) ++OwnedLiving;
                if (++PawnsPerPlayer.FindOrAdd(It->GetPlayerState()) > 1) return Stop(TEXT("duplicate-player-pawns"));
            }
        const int32 TeamAlive = GS->GetTeamInfo(Team).AlivePlayerCount;
        int32 TeamActual = 0, TotalAlive = 0;
        for (APlayerState* P : GS->PlayerArray) if (Alive(P->GetPawn()))
        {
            ++TotalAlive; if (GS->GetPlayerTeam(P) == Team) ++TeamActual;
            if (Scenario == TEXT("reset") || Scenario == TEXT("initial")) Hold(P->GetPawn());
        }
        if (WorldLiving != TotalAlive || GS->PlayerArray.Num() != 8) return Stop(TEXT("pawn-roster-mismatch"));
        ++Samples;
        UE_LOG(LogTemp, Display, TEXT("BwaySpawnRecovery: Sample stage=%d time=%.3f id=%d pawn=%s alive=%d ownedLiving=%d team=%d teamAlive=%d deaths=%d deadline=%.3f phase=%d actual=%d total=%d scenario=%s"),
            Stage, ServerTime, Subject->GetPlayerId(), *GetNameSafe(Pawn), Alive(Pawn), OwnedLiving, GS->GetPlayerTeam(Subject.Get()), TeamAlive,
            Subject->GetDeaths(), Deadline, int32(R->GetCurrentMatchPhase()), TeamActual, TotalAlive, *Scenario);
        if (OwnedLiving > 1 || GS->GetPlayerTeam(Subject.Get()) != Team) return Stop(TEXT("duplicate-or-wrong-team"));
        if (Stage == 1)
        {
            if (Alive(Pawn)) return Stop(TEXT("spawned-through-obstruction"));
            if (Deadline <= 0 || ServerTime < Deadline + 2) return true;
            const bool DeathScenario = Scenario == TEXT("death") || Scenario == TEXT("cancel");
            if (Subject->GetDeaths() != Deaths + (DeathScenario ? 1 : 0) || OwnedLiving != 0
                || TeamAlive != (Scenario == TEXT("reset") ? 0 : 3) || TeamAlive != TeamActual) return Stop(TEXT("blocked-state-mismatch"));
            // Repeated completion notifications for a living teammate must not
            // count the still-blocked subject as another life.
            for (APlayerState* P : GS->PlayerArray) if (GS->GetPlayerTeam(P) == Team && Alive(P->GetPawn()))
            { GS->OnPlayerRespawned(P); GS->OnPlayerRespawned(P); break; }
            if (GS->GetTeamInfo(Team).AlivePlayerCount != TeamActual) return Stop(TEXT("duplicate-alive-credit"));
            if (Scenario == TEXT("cancel")) R->EndRound(1 - Team, EBwayWinCondition::GoalScored);
            Starts[0].Blocker->Destroy();
            if (GM->ChoosePlayerStart_Implementation(Bot.Get()) != Starts[0].Start.Get()) return Stop(TEXT("clear-start-unavailable"));
            Stage = Scenario == TEXT("cancel") ? 4 : 2; StageStarted = Now;
            UE_LOG(LogTemp, Display, TEXT("BwaySpawnRecovery: Unblocked start=%s time=%.3f"), *Starts[0].Start->GetName(), ServerTime);
        }
        else if (Stage == 2)
        {
            if (Alive(Pawn) && TeamActual == 4)
            {
                if (Pawn == OriginalPawn || OwnedLiving != 1 || TeamAlive != 4) return Stop(TEXT("recovery-state-mismatch"));
                Replacement = Pawn; Hold(Pawn); Stage = 3; StageStarted = Now;
                UE_LOG(LogTemp, Display, TEXT("BwaySpawnRecovery: Recovered pawn=%s delay=%.3f"), *Pawn->GetName(), Now - Started);
            }
            else if (Now - StageStarted > 10) return Stop(TEXT("missing-recovery"));
        }
        else if (Stage == 3)
        {
            if (!Alive(Pawn) || Pawn != Replacement || OwnedLiving != 1 || TeamAlive != 4 || TeamActual != 4 || TotalAlive != 8) return Stop(TEXT("unstable-recovery"));
            if (Now - StageStarted > 3) return Stop(TEXT("passed"));
        }
        else if (Stage == 4)
        {
            if (Alive(Pawn) || OwnedLiving || TeamActual != 3 || TeamAlive != 3) return Stop(TEXT("postround-restart"));
            if (Now - StageStarted > 3) return Stop(TEXT("passed"));
        }
        return true;
    }
};
FAutoConsoleCommand Command(TEXT("bway.Test.SpawnRecoveryAudit"), TEXT("Development: obstruction recovery; optional death, initial, reset or cancel scenario. Owns fixture cleanup."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Running) return; Running = true; auto Run = MakeShared<FRun>();
        if (!Args.IsEmpty()) Run->Scenario = Args[0];
        if (Run->Scenario != TEXT("death") && Run->Scenario != TEXT("initial") && Run->Scenario != TEXT("reset") && Run->Scenario != TEXT("cancel"))
        { Run->Stop(TEXT("invalid-scenario")); return; }
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.2f);
    }));
}
#endif
