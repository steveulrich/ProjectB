#include "BwayPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "Abilities/GameplayAbility.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Buildable/BuildableBase.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Physics/LyraCollisionChannels.h"

namespace BwayBuildableCombatAudit
{
const TCHAR* Heroes[] = {
    TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus.DA_BW_HeroData_Argus"),
    TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger.DA_BW_HeroData_Gunslinger"),
    TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona.DA_BW_HeroData_Alona"),
    TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver.DA_BW_HeroData_Hexweaver")};
const TCHAR* Primaries[] = {
    TEXT("/Script/BreakawayCoreRuntime.BwayGameplayAbility_MeleePrimary"),
    TEXT("/Script/HeroRawlinsRuntime.BwayGameplayAbility_RawlinsPrimary"),
    TEXT("/Script/HeroAlonaRuntime.BwayGameplayAbility_AlonaPrimary"),
    TEXT("/Script/HeroMorganRuntime.BwayGameplayAbility_KorrynPrimary")};
bool Running = false;
TMap<TWeakObjectPtr<ABwayPlayerController>, FTSTicker::FDelegateHandle> Probes;

uint64 NetId(AActor* Actor)
{
    const auto* Driver = Actor && Actor->GetWorld() ? Actor->GetWorld()->GetNetDriver() : nullptr;
    return Driver && Driver->GuidCache ? Driver->GuidCache->GetNetGUID(Actor).ObjectId : 0;
}
float Health(ABuildableActor* Target)
{
    auto* ASC = Target ? Target->GetAbilitySystemComponent() : nullptr;
    return ASC && ASC->GetSet<ULyraHealthSet>() ? ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : -1.f;
}
FGameplayAbilitySpec* Primary(ULyraAbilitySystemComponent* ASC, int32 Hero)
{
    UClass* Family = LoadClass<UGameplayAbility>(nullptr, Primaries[Hero]);
    if (ASC && Family) for (auto& Spec : ASC->GetActivatableAbilities())
        if (Spec.Ability && Spec.Ability->IsA(Family)) return &Spec;
    return nullptr;
}
bool Snapshot(ABwayPlayerController* PC, ABuildableActor* Target, int32 Sequence, const TCHAR* Side)
{
    if (!PC || !Target || Target->IsActorBeingDestroyed()) return false;
    auto* Box = Target->GetAttackCollision();
    if (!Box) return false;
    const FVector Center = Box->GetRelativeLocation(), Extent = Box->GetUnscaledBoxExtent();
    const FVector Origin = Box->GetComponentLocation();
    FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(BuildableCombatAudit), false);
    auto* GS = PC->GetWorld()->GetGameState<ABwayGameState>();
    if (GS) for (const auto& Player : GS->PlayerArray) if (Player && Player->GetPawn()) Query.AddIgnoredActor(Player->GetPawn());
    PC->GetWorld()->LineTraceSingleByChannel(Hit, Origin - FVector(75,0,0), Origin + FVector(75,0,0), Lyra_TraceChannel_Weapon, Query);
    const bool bHit = Hit.GetActor() == Target && Hit.GetComponent() == Box && Hit.bBlockingHit;
    UE_LOG(LogTemp, Display, TEXT("BwayBuildableCombat: Sample side=%s peer=%d sequence=%d netId=%llu health=%.3f team=%d centerX=%.3f centerY=%.3f centerZ=%.3f extentX=%.3f extentY=%.3f extentZ=%.3f hit=%d mesh=%s class=%s"),
        Side, PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1, Sequence, NetId(Target), Health(Target), Target->GetTeamId().GetId(),
        Center.X, Center.Y, Center.Z, Extent.X, Extent.Y, Extent.Z, bHit,
        *GetNameSafe(Target->GetMesh()->GetSkeletalMeshAsset()), *Target->GetClass()->GetName());
    return NetId(Target) > 1 && Health(Target) > 0 && bHit;
}
void Probe(ABwayPlayerController* PC, ABuildableActor* Target, int32 Sequence, int32 FiringId)
{
    if (!PC || !PC->IsLocalController()) return;
    if (auto* Previous = Probes.Find(PC)) { FTSTicker::GetCoreTicker().RemoveTicker(*Previous); Probes.Remove(PC); }
    if (Sequence < 0)
    {
        IConsoleManager::Get().ProcessUserConsoleInput(TEXT("bway.Test.CueAudit.Stop"), *GLog, PC->GetWorld());
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableCombat: PeerReleased peer=%d"), PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1);
        return;
    }
    struct FProbe
    {
        TWeakObjectPtr<ABwayPlayerController> PC;
        TWeakObjectPtr<ABuildableActor> Target;
        double Started = FPlatformTime::Seconds();
        int32 Sequence = 0, FiringId = -1, Samples = 0;
        bool Fired = false;
    };
    auto State = MakeShared<FProbe>(); State->PC = PC; State->Target = Target; State->Sequence = Sequence; State->FiringId = FiringId;
    auto Handle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State](float)
    {
        auto* Owner = State->PC.Get(); auto* Actor = State->Target.Get();
        const double Age = FPlatformTime::Seconds() - State->Started;
        if (!Owner || !Actor) { UE_LOG(LogTemp, Error, TEXT("BwayBuildableCombat: ProbeFailure reason=lost-actor sequence=%d"), State->Sequence); return false; }
        auto* Player = Owner->GetPlayerState<ABwayPlayerState>();
        if (Snapshot(Owner, Actor, State->Sequence, TEXT("peer"))) ++State->Samples;
        if (Player && Player->GetPlayerId() == State->FiringId && !State->Fired && Age >= 1)
        {
            auto* ASC = Player->GetLyraAbilitySystemComponent();
            auto* Spec = Primary(ASC, (State->Sequence - 1) / 2);
            if (ASC && Owner->GetPawn() && ASC->GetAvatarActor() == Owner->GetPawn() && Spec && !Spec->IsActive()
                && Spec->Ability->CanActivateAbility(Spec->Handle, ASC->AbilityActorInfo.Get()))
            {
                Owner->SetControlRotation(FRotator::ZeroRotator);
                const bool Activated = ASC->TryActivateAbility(Spec->Handle);
                State->Fired = true;
                UE_LOG(LogTemp, Display, TEXT("BwayBuildableCombat: OwnerFire sequence=%d peer=%d activated=%d spec=%s"), State->Sequence, Player->GetPlayerId(), Activated, *Spec->Ability->GetClass()->GetName());
                if (!Activated) { UE_LOG(LogTemp, Error, TEXT("BwayBuildableCombat: ProbeFailure reason=activation sequence=%d"), State->Sequence); return false; }
            }
            else if (Age > 2.5) { UE_LOG(LogTemp, Error, TEXT("BwayBuildableCombat: ProbeFailure reason=owner-not-ready sequence=%d"), State->Sequence); return false; }
        }
        if (Age >= 4)
        {
            const bool bSource = Player && Player->GetPlayerId() == State->FiringId;
            auto* Spec = bSource ? Primary(Player->GetLyraAbilitySystemComponent(), (State->Sequence - 1) / 2) : nullptr;
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableCombat: PeerComplete sequence=%d peer=%d samples=%d health=%.3f fired=%d ended=%d"),
                State->Sequence, Player ? Player->GetPlayerId() : -1, State->Samples, Health(Actor), State->Fired, !bSource || (Spec && !Spec->IsActive()));
            return false;
        }
        return true;
    }), .2f);
    Probes.Add(PC, Handle);
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
    TArray<FRotator> ControlRotations;
    TArray<FHeld> Held;
    TWeakObjectPtr<ABuildableActor> Target;
    double Started = FPlatformTime::Seconds(), StageStarted = Started, NextSample = 0;
    int32 Stage = 0, Sequence = 1, RoundNumber = -1, Created = 0, Destroyed = 0;
    bool Selected = false;
    float Before = -1, ArenaZ = 10000;

    bool Stop(const TCHAR* Reason)
    {
        if (Target.IsValid()) { Target->Destroy(); ++Destroyed; }
        int32 Restored = 0, Expired = 0;
        for (const auto& H : Held)
        {
            if (!H.Pawn.IsValid()) { ++Expired; continue; }
            H.Pawn->SetActorTransform(H.Transform, false, nullptr, ETeleportType::TeleportPhysics);
            auto* Move = H.Pawn->GetCharacterMovement(); Move->SetMovementMode(H.Mode, H.Custom); Move->Velocity = H.Velocity;
            if (H.WasRunning && H.Brain.IsValid()) H.Brain->RestartLogic();
            ++Restored;
        }
        for (int32 Index = 0; Index < Humans.Num(); ++Index) if (auto* PC = Humans[Index].Get())
        {
            if (ControlRotations.IsValidIndex(Index)) { PC->SetControlRotation(ControlRotations[Index]); PC->ClientSetRotation(ControlRotations[Index], false); }
            PC->Client_RunBuildableCombatProbe(nullptr, -1, -1);
        }
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableCombat: Complete reason=%s sequence=%d created=%d destroyed=%d restored=%d expired=%d held=%d"), Reason, Sequence, Created, Destroyed, Restored, Expired, Held.Num());
        Running = false; return false;
    }
    bool Prepare()
    {
        auto* PC = Humans[(Sequence - 1) / 2].Get();
        auto* Player = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
        if (!Player || !PC->GetPawn()) return false;
        int32 Index = 0;
        for (const auto& H : Held) if (H.Pawn.IsValid())
            H.Pawn->SetActorLocationAndRotation(FVector(5000 + 3000 * Index++, 0, ArenaZ), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
        PC->GetPawn()->SetActorLocationAndRotation(FVector(0,0,ArenaZ), FRotator::ZeroRotator, false, nullptr, ETeleportType::TeleportPhysics);
        PC->SetControlRotation(FRotator::ZeroRotator); PC->ClientSetRotation(FRotator::ZeroRotator, false);
        const auto* Data = LoadObject<UBwayBuildableDataAsset>(nullptr, TEXT("/Hero_Alona/Characters/Heroes/Alona/Buildables/DA_BW_BuildableData_SunShrine.DA_BW_BuildableData_SunShrine"));
        if (!Data || !Data->BuildableActorClass) return false;
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor = GS->GetWorld()->SpawnActor<ABuildableActor>(Data->BuildableActorClass, FVector(150,0,ArenaZ), FRotator::ZeroRotator, Params);
        if (!Actor) return false;
        Target = Actor; ++Created;
        const bool Enemy = Sequence % 2 == 0;
        // A bounded geometry variant proves RepNotify updates, without touching the asset.
        Actor->ConfigurePlacementBounds(Enemy ? FVector(0,15,0) : Data->PlacementBoundsCenter,
            Enemy ? FVector(45,35,50) : Data->PlacementBoundsExtent);
        UBwayBuildablePlacementLibrary::ApplyResolvedBuildableMesh(Actor, Data, 0);
        const int32 Team = Player->GetTeamId();
        if (Team != 1 && Team != 2) return false;
        Actor->InitializeBuildable(nullptr, Enemy ? (Team == 1 ? 1 : 0) : Team - 1);
        Actor->ForceNetUpdate(); Before = Health(Actor);
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableCombat: Case sequence=%d hero=%d source=%d enemy=%d sourceTeam=%d targetTeam=%d before=%.3f"),
            Sequence, (Sequence - 1) / 2, Player->GetPlayerId(), Enemy, Team, Actor->GetTeamId().GetId(), Before);
        Stage = 1; StageStarted = FPlatformTime::Seconds(); return true;
    }
    bool Tick(float)
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > 240) return Stop(TEXT("timeout"));
        if (Stage == 0)
        {
            for (const auto& Context : GEngine->GetWorldContexts())
            {
                auto* World = Context.World(); auto* State = World ? World->GetGameState<ABwayGameState>() : nullptr;
                auto* Round = State ? State->GetRoundManagement() : nullptr;
                if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client || !Round) continue;
                TArray<ABwayPlayerController*> PCs; int32 Remotes = 0;
                for (auto It = World->GetPlayerControllerIterator(); It; ++It) if (auto* PC = Cast<ABwayPlayerController>(It->Get()))
                { if (PC->GetPlayerState<ABwayPlayerState>()) { PCs.Add(PC); if (!PC->IsLocalController() && PC->GetNetConnection()) ++Remotes; } }
                if (PCs.Num() != 4 || Remotes != 3) continue;
                PCs.Sort([](const auto& A, const auto& B) { return A.PlayerState->GetPlayerId() < B.PlayerState->GetPlayerId(); });
                if (!Selected && State->HeroSelectionManager && State->HeroSelectionManager->IsSelectionActive())
                {
                    for (int32 Index = 0; Index < 4; ++Index)
                    {
                        const auto* Hero = LoadObject<UBwayHeroDataAsset>(nullptr, Heroes[Index]);
                        auto* P = PCs[Index]->GetPlayerState<ABwayPlayerState>();
                        if (!Hero || P->IsHeroLocked()) return Stop(TEXT("selection-setup"));
                        PCs[Index]->Server_SubmitPreSelectedHero(Hero->GetPrimaryAssetId(), true);
                        if (!P->IsHeroLocked() || P->GetSelectedHeroId() != Hero->GetPrimaryAssetId()) return Stop(TEXT("selection-rejected"));
                        UE_LOG(LogTemp, Display, TEXT("BwayBuildableCombat: Selection hero=%d player=%d remote=%d asset=%s"), Index, P->GetPlayerId(), !PCs[Index]->IsLocalController(), *Hero->GetPrimaryAssetId().ToString());
                    }
                    Selected = true;
                }
                if (!Selected || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) continue;
                bool Ready = true;
                for (int32 Index = 0; Index < 4; ++Index)
                {
                    auto* P = PCs[Index]->GetPlayerState<ABwayPlayerState>(); auto* ASC = P->GetLyraAbilitySystemComponent();
                    Ready &= PCs[Index]->GetPawn() && ASC && ASC->GetAvatarActor() == PCs[Index]->GetPawn() && Primary(ASC, Index);
                }
                int32 Pawns = 0; for (const auto& P : State->PlayerArray) if (P && P->GetPawn()) ++Pawns;
                if (!Ready || Pawns != 8) continue;
                GS = State; RoundNumber = Round->GetCurrentRoundNumber();
                for (auto* PC : PCs) { Humans.Add(PC); ControlRotations.Add(PC->GetControlRotation()); }
                for (const auto& P : State->PlayerArray) if (auto* Pawn = P ? Cast<ABwayCharacterWithAbilities>(P->GetPawn()) : nullptr)
                {
                    auto* Move = Pawn->GetCharacterMovement(); auto* AI = Cast<AAIController>(Pawn->GetController()); auto* Brain = AI ? AI->GetBrainComponent() : nullptr;
                    Held.Add({Pawn, Pawn->GetActorTransform(), Move->Velocity, Move->MovementMode, Move->CustomMovementMode, Brain, Brain && Brain->IsRunning()});
                    ArenaZ = FMath::Max(ArenaZ, float(Pawn->GetActorLocation().Z + 10000));
                    Move->StopMovementImmediately(); Move->DisableMovement(); if (Brain) Brain->StopLogic(TEXT("Owned buildable combat audit"));
                }
                UE_LOG(LogTemp, Display, TEXT("BwayBuildableCombat: Setup humans=4 remotes=3 held=%d round=%d"), Held.Num(), RoundNumber);
                return Prepare() ? true : Stop(TEXT("prepare"));
            }
            return true;
        }
        if (!GS.IsValid() || !Target.IsValid()) return Stop(TEXT("lost-world-or-target"));
        auto* Round = GS->GetRoundManagement();
        if (Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || Round->GetCurrentRoundNumber() != RoundNumber) return Stop(TEXT("phase-changed"));
        if (Now >= NextSample) { NextSample = Now + .2; Snapshot(Humans[0].Get(), Target.Get(), Sequence, TEXT("authority")); }
        const double Age = Now - StageStarted;
        if (Stage == 1 && Age >= 1)
        {
            if (NetId(Target.Get()) <= 1 || Before <= 0) return Stop(TEXT("unassigned-baseline"));
            auto* Source = Humans[(Sequence - 1) / 2].Get();
            for (auto Weak : Humans) if (auto* PC = Weak.Get()) PC->Client_RunBuildableCombatProbe(Target.Get(), Sequence, Source->PlayerState->GetPlayerId());
            Stage = 2; StageStarted = Now;
        }
        else if (Stage == 2 && Age >= 5)
        {
            const float After = Health(Target.Get()); const bool Enemy = Sequence % 2 == 0;
            const bool Passed = Enemy ? After > 0 && After < Before : FMath::IsNearlyEqual(After, Before);
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableCombat: Verify sequence=%d netId=%llu before=%.3f after=%.3f enemy=%d pass=%d"), Sequence, NetId(Target.Get()), Before, After, Enemy, Passed);
            if (!Passed) return Stop(TEXT("damage-verdict"));
            Target->Destroy(); Target.Reset(); ++Destroyed; ++Sequence;
            if (Sequence == 9) return Stop(TEXT("observed"));
            if (!Prepare()) return Stop(TEXT("prepare"));
        }
        return true;
    }
};
FAutoConsoleCommand Command(TEXT("bway.Test.BuildableCombatAudit"), TEXT("Development four-player primary/buildable combat and collision replication audit."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (Running) return;
        Running = true; auto Run = MakeShared<FRun>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), .1f);
    }));
}
#endif

void ABwayPlayerController::Client_RunBuildableCombatProbe_Implementation(ABuildableActor* Target, int32 Sequence, int32 FiringPlayerId)
{
#if !UE_BUILD_SHIPPING
    BwayBuildableCombatAudit::Probe(this, Target, Sequence, FiringPlayerId);
#endif
}
