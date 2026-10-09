#include "BwayPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Buildable/BuildableBase.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "Buildable/BwaySiegeEngineBuildable.h"
#include "Combat/BwayDamageLibrary.h"
#include "Components/BoxComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HeroSystems/BwayHeroSelectionManager.h"

namespace BwaySiegeCombatAudit
{
bool Running = false;
TMap<TWeakObjectPtr<ABwayPlayerController>, FTSTicker::FDelegateHandle> Observers;

uint64 NetId(AActor* Actor)
{
    const auto* Driver = Actor && Actor->GetWorld() ? Actor->GetWorld()->GetNetDriver() : nullptr;
    return Driver && Driver->GuidCache ? Driver->GuidCache->GetNetGUID(Actor).ObjectId : 0;
}
float Health(const ABuildableActor* Actor)
{
    const auto* ASC = Actor ? Actor->GetAbilitySystemComponent() : nullptr;
    return ASC && ASC->GetSet<ULyraHealthSet>() ? ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : -1.f;
}
struct FSample
{
    TWeakObjectPtr<ABwaySiegeEngineBuildable> Siege;
    TWeakObjectPtr<ABuildableActor> Enemy, Friendly;
    uint64 SiegeId = 0, EnemyId = 0, FriendlyId = 0;
    float LastX = 0;
    bool Log(ABwayPlayerController* PC, int32 Sequence, const TCHAR* Side)
    {
        if (!PC || !Enemy.IsValid() || !Friendly.IsValid()) return false;
        const bool Alive = Siege.IsValid() && !Siege->IsActorBeingDestroyed();
        if (Alive) { SiegeId = NetId(Siege.Get()); LastX = Siege->GetActorLocation().X; }
        EnemyId = NetId(Enemy.Get()); FriendlyId = NetId(Friendly.Get());
        const auto* GS = PC->GetWorld()->GetGameState<ABwayGameState>();
        int32 Registered = 0;
        if (GS && GS->BuildableRegistryComponent) for (auto* Actor : GS->BuildableRegistryComponent->GetAllBuildables())
            if (SiegeId > 1 && NetId(Actor) == SiegeId) ++Registered;
        UE_LOG(LogTemp, Display, TEXT("BwaySiegeCombat: Sample side=%s peer=%d sequence=%d siegeId=%llu enemyId=%llu friendlyId=%llu alive=%d rolling=%d x=%.3f enemyHealth=%.3f friendlyHealth=%.3f siegeHealth=%.3f registered=%d"),
            Side, PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1, Sequence, SiegeId, EnemyId, FriendlyId,
            Alive, Alive && Siege->IsRollingForDevelopment(), LastX, Health(Enemy.Get()), Health(Friendly.Get()), Alive ? Health(Siege.Get()) : -1, Registered);
        return SiegeId > 1 && EnemyId > 1 && FriendlyId > 1;
    }
};
void Observe(ABwayPlayerController* PC, ABuildableActor* Siege, ABuildableActor* Enemy, ABuildableActor* Friendly, int32 Sequence)
{
    if (!PC || !PC->IsLocalController()) return;
    if (auto* Old = Observers.Find(PC)) { FTSTicker::GetCoreTicker().RemoveTicker(*Old); Observers.Remove(PC); }
    if (Sequence < 0)
    {
        UE_LOG(LogTemp, Display, TEXT("BwaySiegeCombat: PeerReleased peer=%d"), PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1);
        return;
    }
    struct FPeer { TWeakObjectPtr<ABwayPlayerController> Owner; FSample State; double Started = FPlatformTime::Seconds(); int32 Samples = 0; };
    auto Peer = MakeShared<FPeer>(); Peer->Owner = PC; Peer->State.Siege = Cast<ABwaySiegeEngineBuildable>(Siege); Peer->State.Enemy = Enemy; Peer->State.Friendly = Friendly;
    auto Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Peer, Sequence](float)
    {
        auto* Owner = Peer->Owner.Get();
        if (!Peer->State.Log(Owner, Sequence, TEXT("peer")))
        {
            UE_LOG(LogTemp, Error, TEXT("BwaySiegeCombat: PeerFailure sequence=%d reason=identity-or-target"), Sequence);
            return false;
        }
        ++Peer->Samples;
        if (FPlatformTime::Seconds() - Peer->Started >= 13)
        {
            UE_LOG(LogTemp, Display, TEXT("BwaySiegeCombat: PeerComplete sequence=%d peer=%d samples=%d siegeId=%llu alive=%d enemyHealth=%.3f friendlyHealth=%.3f"),
                Sequence, Owner->PlayerState->GetPlayerId(), Peer->Samples, Peer->State.SiegeId, Peer->State.Siege.IsValid(), Health(Peer->State.Enemy.Get()), Health(Peer->State.Friendly.Get()));
            return false;
        }
        return true;
    }), .2f);
    Observers.Add(PC, Handle);
}
struct FHeld
{
    TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
    FTransform Transform;
    FVector Velocity;
    EMovementMode Mode;
    uint8 Custom;
    TWeakObjectPtr<UBrainComponent> Brain;
    bool WasRunning;
};
struct FRun
{
    TWeakObjectPtr<ABwayGameState> GS;
    TArray<TWeakObjectPtr<ABwayPlayerController>> Humans;
    TArray<FHeld> Held;
    TArray<TWeakObjectPtr<AActor>> Owned;
    FSample State;
    double Started = FPlatformTime::Seconds(), CaseStarted = 0, NextSample = 0;
    int32 Sequence = 1, RoundNumber = -1, Created = 0, FixtureDestroyed = 0, NativeDestroyed = 0, WallsCreated = 0, WallsDestroyed = 0;
    bool AutoLocked = false, Dispatched = false, LethalSent = false;
    float ArenaZ = 10000, EnemyBefore = -1, FriendlyBefore = -1;
    IConsoleVariable* AuditCvar = nullptr;
    int32 OldAuditCvar = 0;
    void CleanupCase()
    {
        for (auto Weak : Owned) if (auto* Actor = Weak.Get())
        {
            if (Cast<ABuildableActor>(Actor)) ++FixtureDestroyed; else ++WallsDestroyed;
            Actor->Destroy();
        }
        Owned.Reset(); State = FSample();
    }
    bool Stop(const TCHAR* Reason)
    {
        CleanupCase();
        int32 Restored = 0, Expired = 0;
        for (const auto& H : Held)
        {
            if (!H.Pawn.IsValid()) { ++Expired; continue; }
            H.Pawn->SetActorTransform(H.Transform, false, nullptr, ETeleportType::TeleportPhysics);
            auto* Move = H.Pawn->GetCharacterMovement(); Move->SetMovementMode(H.Mode, H.Custom); Move->Velocity = H.Velocity;
            if (H.WasRunning && H.Brain.IsValid()) H.Brain->RestartLogic();
            ++Restored;
        }
        if (AuditCvar) AuditCvar->Set(OldAuditCvar, ECVF_SetByCode);
        for (auto Weak : Humans) if (auto* PC = Weak.Get()) PC->Client_RunSiegeCombatProbe(nullptr, nullptr, nullptr, -1);
        UE_LOG(LogTemp, Display, TEXT("BwaySiegeCombat: Complete reason=%s sequence=%d created=%d nativeDestroyed=%d fixtureDestroyed=%d wallsCreated=%d wallsDestroyed=%d restored=%d expired=%d held=%d cvarRestored=%d"),
            Reason, Sequence, Created, NativeDestroyed, FixtureDestroyed, WallsCreated, WallsDestroyed, Restored, Expired, Held.Num(), !AuditCvar || AuditCvar->GetInt() == OldAuditCvar);
        Running = false; return false;
    }
    ABuildableActor* Spawn(const TCHAR* Path, const FVector& Location, int32 Team)
    {
        const auto* Data = LoadObject<UBwayBuildableDataAsset>(nullptr, Path);
        if (!Data || !Data->BuildableActorClass) return nullptr;
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor = GS->GetWorld()->SpawnActor<ABuildableActor>(Data->BuildableActorClass, Location, FRotator::ZeroRotator, Params);
        if (!Actor) return nullptr;
        Owned.Add(Actor); ++Created;
        Actor->ConfigurePlacementBounds(Data->PlacementBoundsCenter, Data->PlacementBoundsExtent);
        UBwayBuildablePlacementLibrary::ApplyResolvedBuildableMesh(Actor, Data, 0);
        Actor->InitializeBuildable(nullptr, Team - 1); Actor->ForceNetUpdate();
        return Actor;
    }
    bool Prepare()
    {
        CleanupCase(); Dispatched = false; LethalSent = false; CaseStarted = FPlatformTime::Seconds();
        const TCHAR* Shrine = TEXT("/Hero_Alona/Characters/Heroes/Alona/Buildables/DA_BW_BuildableData_SunShrine.DA_BW_BuildableData_SunShrine");
        State.Enemy = Spawn(Shrine, Sequence == 1 ? FVector(2000,-60,ArenaZ) : FVector(0,3000,ArenaZ), 2);
        State.Friendly = Spawn(Shrine, Sequence == 1 ? FVector(2000,60,ArenaZ) : FVector(0,3300,ArenaZ), 1);
        State.Siege = Cast<ABwaySiegeEngineBuildable>(Spawn(TEXT("/Hero_Spartacus/Characters/Heroes/Argus/Buildables/DA_BW_BuildableData_SiegeEngine.DA_BW_BuildableData_SiegeEngine"), FVector(0,0,ArenaZ), 1));
        if (!State.Enemy.IsValid() || !State.Friendly.IsValid() || !State.Siege.IsValid()) return false;
        EnemyBefore = Health(State.Enemy.Get()); FriendlyBefore = Health(State.Friendly.Get());
        if (Sequence == 2)
        {
            auto* Wall = GS->GetWorld()->SpawnActor<AActor>(); if (!Wall) return false;
            auto* Box = NewObject<UBoxComponent>(Wall); Wall->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(40,200,200)); Box->SetCollisionObjectType(ECC_WorldStatic);
            Box->SetCollisionResponseToAllChannels(ECR_Block); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Box->RegisterComponent(); Wall->SetActorLocation(FVector(2500,0,ArenaZ)); Owned.Add(Wall); ++WallsCreated;
            UE_LOG(LogTemp, Display, TEXT("BwaySiegeCombat: Wall sequence=2 name=%s x=2500 halfWidth=40 siegeExtentX=%.3f siegeCenterX=%.3f"),
                *Wall->GetName(), State.Siege->GetAttackCollision()->GetScaledBoxExtent().X, State.Siege->GetAttackCollision()->GetRelativeLocation().X);
        }
        UE_LOG(LogTemp, Display, TEXT("BwaySiegeCombat: Case sequence=%d siege=%s enemy=%s friendly=%s beforeEnemy=%.3f beforeFriendly=%.3f siegeHealth=%.3f speed=%.3f movementReplicated=%d"),
            Sequence, *State.Siege->GetName(), *State.Enemy->GetName(), *State.Friendly->GetName(), EnemyBefore, FriendlyBefore,
            Health(State.Siege.Get()), State.Siege->GetRollSpeedForDevelopment(), State.Siege->IsReplicatingMovement());
        return true;
    }
    bool Tick(float)
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > 240) return Stop(TEXT("timeout"));
        if (!GS.IsValid())
        {
            if (!Held.IsEmpty()) return Stop(TEXT("lost-world"));
            for (const auto& Context : GEngine->GetWorldContexts())
            {
                auto* World = Context.World(); auto* StateGS = World ? World->GetGameState<ABwayGameState>() : nullptr;
                auto* Round = StateGS ? StateGS->GetRoundManagement() : nullptr;
                if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client || !Round) continue;
                TArray<ABwayPlayerController*> PCs; int32 Remotes = 0;
                for (auto It = World->GetPlayerControllerIterator(); It; ++It) if (auto* PC = Cast<ABwayPlayerController>(It->Get()))
                    if (PC->GetPlayerState<ABwayPlayerState>()) { PCs.Add(PC); if (!PC->IsLocalController() && PC->GetNetConnection()) ++Remotes; }
                if (PCs.Num() != 4 || Remotes != 3) continue;
                if (!AutoLocked && StateGS->HeroSelectionManager && StateGS->HeroSelectionManager->IsSelectionActive())
                { GEngine->Exec(World, TEXT("bway.Test.AutoLockAllHeroes")); AutoLocked = true; }
                if (Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) continue;
                int32 Pawns = 0; for (const auto& P : StateGS->PlayerArray) if (P && P->GetPawn()) ++Pawns;
                if (Pawns != 8) continue;
                GS = StateGS; RoundNumber = Round->GetCurrentRoundNumber();
                for (auto* PC : PCs) Humans.Add(PC);
                for (const auto& P : StateGS->PlayerArray) if (auto* Pawn = P ? Cast<ABwayCharacterWithAbilities>(P->GetPawn()) : nullptr)
                {
                    auto* Move = Pawn->GetCharacterMovement(); auto* AI = Cast<AAIController>(Pawn->GetController()); auto* Brain = AI ? AI->GetBrainComponent() : nullptr;
                    Held.Add({Pawn, Pawn->GetActorTransform(), Move->Velocity, Move->MovementMode, Move->CustomMovementMode, Brain, Brain && Brain->IsRunning()});
                    ArenaZ = FMath::Max(ArenaZ, float(Pawn->GetActorLocation().Z + 10000));
                    Move->StopMovementImmediately(); Move->DisableMovement(); if (Brain) Brain->StopLogic(TEXT("Owned Siege combat audit"));
                }
                int32 Index = 0; for (const auto& H : Held) H.Pawn->SetActorLocation(FVector(0,-1000 - 300 * Index++,ArenaZ), false, nullptr, ETeleportType::TeleportPhysics);
                AuditCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("bway.Debug.SiegeDamage"));
                if (!AuditCvar) return Stop(TEXT("missing-audit-cvar"));
                OldAuditCvar = AuditCvar->GetInt(); AuditCvar->Set(1, ECVF_SetByCode);
                UE_LOG(LogTemp, Display, TEXT("BwaySiegeCombat: Setup humans=4 remotes=3 held=%d round=%d"), Held.Num(), RoundNumber);
                return Prepare() ? true : Stop(TEXT("prepare"));
            }
            return true;
        }
        auto* Round = GS->GetRoundManagement();
        if (Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || Round->GetCurrentRoundNumber() != RoundNumber) return Stop(TEXT("phase-changed"));
        if (!State.Enemy.IsValid() || !State.Friendly.IsValid()) return Stop(TEXT("lost-target"));
        const double Age = Now - CaseStarted;
        if (Now >= NextSample) { NextSample = Now + .1; State.Log(Humans[0].Get(), Sequence, TEXT("authority")); }
        if (!Dispatched && Age >= .6)
        {
            if (State.SiegeId <= 1 || State.EnemyId <= 1 || State.FriendlyId <= 1) return Stop(TEXT("unassigned-identity"));
            for (auto Weak : Humans) if (auto* PC = Weak.Get()) PC->Client_RunSiegeCombatProbe(State.Siege.Get(), State.Enemy.Get(), State.Friendly.Get(), Sequence);
            Dispatched = true;
        }
        if (Sequence == 3 && !LethalSent && Age >= 3)
        {
            auto* Siege = State.Siege.Get(); const float Before = Health(Siege);
            const bool Applied = Siege && UBwayDamageLibrary::ApplyDamageFromSource(State.Enemy->GetAbilitySystemComponent(), Siege, 300, State.Enemy.Get(), State.Enemy.Get());
            UE_LOG(LogTemp, Display, TEXT("BwaySiegeCombat: Lethal sequence=3 siegeId=%llu before=%.3f raw=300 applied=%d destroyed=%d"),
                State.SiegeId, Before, Applied, !State.Siege.IsValid() || State.Siege->IsActorBeingDestroyed());
            LethalSent = true;
        }
        if (Age >= 14.5)
        {
            const float EnemyAfter = Health(State.Enemy.Get()), FriendlyAfter = Health(State.Friendly.Get());
            const bool Passed = !State.Siege.IsValid() && FMath::IsNearlyEqual(FriendlyAfter, FriendlyBefore)
                && (Sequence == 1 ? EnemyAfter > 0 && EnemyAfter < EnemyBefore : FMath::IsNearlyEqual(EnemyAfter, EnemyBefore));
            UE_LOG(LogTemp, Display, TEXT("BwaySiegeCombat: Verify sequence=%d siegeId=%llu enemyId=%llu friendlyId=%llu beforeEnemy=%.3f afterEnemy=%.3f beforeFriendly=%.3f afterFriendly=%.3f alive=%d pass=%d"),
                Sequence, State.SiegeId, State.EnemyId, State.FriendlyId, EnemyBefore, EnemyAfter, FriendlyBefore, FriendlyAfter, State.Siege.IsValid(), Passed);
            if (!Passed) return Stop(TEXT("case-verdict"));
            ++NativeDestroyed; ++Sequence;
            if (Sequence == 4) return Stop(TEXT("observed"));
            return Prepare() ? true : Stop(TEXT("prepare"));
        }
        return true;
    }
};
FAutoConsoleCommand Command(TEXT("bway.Test.SiegeCombatAudit"), TEXT("Development four-process Siege damage/wall/health-destruction fixture."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (Running) return;
        Running = true; auto Run = MakeShared<FRun>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), .1f);
    }));
}
#endif

void ABwayPlayerController::Client_RunSiegeCombatProbe_Implementation(ABuildableActor* Siege, ABuildableActor* Enemy, ABuildableActor* Friendly, int32 Sequence)
{
#if !UE_BUILD_SHIPPING
    BwaySiegeCombatAudit::Observe(this, Siege, Enemy, Friendly, Sequence);
#endif
}
