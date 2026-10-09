#include "BwayPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Buildable/BuildableBase.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "Character/LyraCharacterMovementComponent.h"

// An owned development fixture, never a shipping gameplay path. Server mutations
// use production trap overlaps, damage, death and reset; clients journal local state.
namespace BwayJailCaptureAudit
{
bool Running = false;
TMap<TWeakObjectPtr<ABwayPlayerController>, FTSTicker::FDelegateHandle> Observers;
uint64 NetId(AActor* Actor)
{
    const auto* Driver = Actor && Actor->GetWorld() ? Actor->GetWorld()->GetNetDriver() : nullptr;
    return Driver && Driver->GuidCache ? Driver->GuidCache->GetNetGUID(Actor).ObjectId : 0;
}
ABwayCharacterWithAbilities* Pawn(ABwayPlayerState* PS)
{ auto* P = PS ? Cast<ABwayCharacterWithAbilities>(PS->GetPawn()) : nullptr; return P && P->GetPlayerState() == PS ? P : nullptr; }
bool Alive(ABwayPlayerState* PS)
{ const auto* H = ULyraHealthComponent::FindHealthComponent(Pawn(PS)); return H && !H->IsDeadOrDying() && H->GetHealth() > 0; }
bool Tag(ABwayPlayerState* PS, FGameplayTag Value)
{ auto* ASC = PS ? PS->GetLyraAbilitySystemComponent() : nullptr; return ASC && ASC->HasMatchingGameplayTag(Value); }
ABuildableActor* Cage(UWorld* World, ABwayPlayerState* PS)
{
    APawn* Victim = Pawn(PS); if (!Victim) return nullptr;
    for (TActorIterator<ABuildableActor> It(World); It; ++It)
        if (IsValid(*It) && !It->IsActorBeingDestroyed() && It->GetAttachParentActor() == Victim) return *It;
    return nullptr;
}
float Health(ABuildableActor* Actor)
{ return Actor ? Actor->GetAbilitySystemComponent()->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : -1; }
FGameplayAbilitySpec* Primary(ULyraAbilitySystemComponent* ASC)
{
    auto* Family = LoadClass<UGameplayAbility>(nullptr, TEXT("/Script/BreakawayCoreRuntime.BwayGameplayAbility_MeleePrimary"));
    if (ASC && Family) for (auto& Spec : ASC->GetActivatableAbilities())
        if (Spec.Ability && Spec.Ability->IsA(Family)) return &Spec;
    return nullptr;
}
struct FState
{
    TWeakObjectPtr<ABwayPlayerState> Placer, Ally, Victim, Second;
    bool Log(ABwayPlayerController* PC, int32 Phase, const TCHAR* Side) const
    {
        auto* W = PC ? PC->GetWorld() : nullptr; auto* GS = W ? W->GetGameState<ABwayGameState>() : nullptr;
        if (!GS || !GS->BuildableRegistryComponent || !Placer.IsValid() || !Ally.IsValid() || !Victim.IsValid() || !Second.IsValid()) return false;
        auto* A = Cage(W, Victim.Get()); auto* B = Cage(W, Second.Get());
        TArray<uint64> Ids; for (auto* Actor : GS->BuildableRegistryComponent->GetAllBuildables()) Ids.Add(NetId(Actor)); Ids.Sort();
        auto* Spec = PC->PlayerState == Victim.Get() ? Primary(Victim->GetLyraAbilitySystemComponent()) : nullptr;
        UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: Sample side=%s phase=%d peer=%d time=%.3f placer=%d ownerPawn=%llu ownerTeam=%d allyJailed=%d victim=%d victimPawn=%llu victimTeam=%d jailedA=%d stoppedA=%d cageA=%llu healthA=%.3f attachA=%.3f secondPawn=%llu jailedB=%d stoppedB=%d cageB=%llu healthB=%.3f attachB=%.3f registered=%d first=%llu second=%llu quota=%d primaryActive=%d"),
            Side, Phase, PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1, GS->GetServerWorldTimeSeconds(),
            Placer->GetPlayerId(), NetId(Pawn(Placer.Get())), int32(Placer->GetGenericTeamId().GetId()), Tag(Ally.Get(), BwayGameplayTags::State_Status_Jailed),
            Victim->GetPlayerId(), NetId(Pawn(Victim.Get())), int32(Victim->GetGenericTeamId().GetId()), Tag(Victim.Get(), BwayGameplayTags::State_Status_Jailed), Tag(Victim.Get(), TAG_Gameplay_MovementStopped),
            NetId(A), Health(A), A ? FVector::Dist(A->GetActorLocation(), Pawn(Victim.Get())->GetActorLocation()) : -1.f,
            NetId(Pawn(Second.Get())), Tag(Second.Get(), BwayGameplayTags::State_Status_Jailed), Tag(Second.Get(), TAG_Gameplay_MovementStopped),
            NetId(B), Health(B), B ? FVector::Dist(B->GetActorLocation(), Pawn(Second.Get())->GetActorLocation()) : -1.f,
            Ids.Num(), Ids.Num() > 0 ? Ids[0] : 0, Ids.Num() > 1 ? Ids[1] : 0,
            GS->BuildableRegistryComponent->GetBuildableCountForPlayer(Placer.Get()), Spec ? int32(Spec->IsActive()) : -1);
        return true;
    }
};
void Observe(ABwayPlayerController* PC, ABwayPlayerState* Placer, ABwayPlayerState* Ally, ABwayPlayerState* Victim, ABwayPlayerState* Second, int32 Phase)
{
    if (!PC || !PC->IsLocalController()) return;
    if (auto* Old = Observers.Find(PC)) { FTSTicker::GetCoreTicker().RemoveTicker(*Old); Observers.Remove(PC); }
    if (Phase < 0) { UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: PeerReleased peer=%d"), PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1); return; }
    FState State; State.Placer = Placer; State.Ally = Ally; State.Victim = Victim; State.Second = Second;
    TWeakObjectPtr<ABwayPlayerController> WeakPC = PC; const double Started = FPlatformTime::Seconds();
    Observers.Add(PC, FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC, State, Phase, Started, Requested = false](float) mutable
    {
        if (!WeakPC.IsValid()) return false;
        if (FPlatformTime::Seconds() - Started > 40) { UE_LOG(LogTemp, Error, TEXT("BwayJailCapture: PeerFailure phase=%d reason=timeout"), Phase); return false; }
        if (!State.Log(WeakPC.Get(), Phase, TEXT("peer"))) return true;
        if (Phase == 2 && !Requested && WeakPC->PlayerState == State.Victim.Get())
        {
            auto* ASC = State.Victim->GetLyraAbilitySystemComponent(); auto* Spec = Primary(ASC);
            if (!Spec || !Pawn(State.Victim.Get()) || !Tag(State.Victim.Get(), BwayGameplayTags::State_Status_Jailed)) return true;
            Requested = true; const bool Activated = ASC->TryActivateAbility(Spec->Handle, false);
            UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: Primary peer=%d authority=%d local=%d jailed=%d activated=%d"), State.Victim->GetPlayerId(), WeakPC->HasAuthority(), WeakPC->IsLocalController(), Tag(State.Victim.Get(), BwayGameplayTags::State_Status_Jailed), Activated);
            if (!Activated) { UE_LOG(LogTemp, Error, TEXT("BwayJailCapture: PeerFailure phase=2 reason=primary")); return false; }
        }
        return true;
    }), .2f));
}
struct FHeld
{
    TWeakObjectPtr<ABwayCharacterWithAbilities> Character; FTransform Transform; FVector Velocity;
    EMovementMode Mode; uint8 Custom; TWeakObjectPtr<UBrainComponent> Brain; bool WasRunning;
};
struct FRun
{
    TWeakObjectPtr<ABwayGameState> GS;
    TArray<TWeakObjectPtr<ABwayPlayerController>> Humans;
    TArray<FHeld> Held;
    TArray<TWeakObjectPtr<ABuildableActor>> Spawned;
    TWeakObjectPtr<ABuildableActor> Trap, Survivor;
    TWeakObjectPtr<APawn> Unpossessed, DeadPawn, PreResetVictim;
    FState State;
    double Started = FPlatformTime::Seconds(), PhaseStarted = 0;
    int32 Phase = 0, Round = -1, PlacerId = -1, VictimId = -1, DeathsBefore = 0;
    float ArenaZ = 10000;
    bool Selected = false, DeathReplaced = false;
    void Move(ABwayPlayerState* PS, FVector P)
    { if (auto* Actor = Pawn(PS)) { Actor->SetActorLocation(P, false, nullptr, ETeleportType::TeleportPhysics); Actor->ForceNetUpdate(); } }
    void Hold(APawn* P)
    {
        auto* C = Cast<ABwayCharacterWithAbilities>(P); if (!C || Held.ContainsByPredicate([C](const FHeld& H) { return H.Character == C; })) return;
        auto* M = C->GetCharacterMovement(); auto* AI = Cast<AAIController>(C->GetController()); auto* B = AI ? AI->GetBrainComponent() : nullptr;
        Held.Add({C, C->GetActorTransform(), M->Velocity, M->MovementMode, M->CustomMovementMode, B, B && B->IsRunning()});
        M->StopMovementImmediately(); M->DisableMovement(); if (B) B->StopLogic(TEXT("Owned Jail capture audit"));
        C->SetActorLocation(FVector(0,-3000-400*Held.Num(),ArenaZ), false, nullptr, ETeleportType::TeleportPhysics);
    }
    void Repossess()
    {
        if (!Unpossessed.IsValid() || !State.Placer.IsValid()) return;
        if (auto* PC = Cast<AController>(State.Placer->GetOwner()))
        {
            PC->Possess(Unpossessed.Get());
            if (auto* C = Cast<ABwayCharacterWithAbilities>(PC->GetPawn())) { C->GetCharacterMovement()->StopMovementImmediately(); C->GetCharacterMovement()->DisableMovement(); }
        }
        Unpossessed.Reset();
    }
    bool Stop(const TCHAR* Reason)
    {
        Repossess(); int32 Removed = 0, Restored = 0, Expired = 0;
        if (GS.IsValid()) for (auto* Actor : GS->BuildableRegistryComponent->GetAllBuildables()) { Actor->Destroy(); ++Removed; }
        for (const auto& H : Held)
        {
            if (!H.Character.IsValid()) { ++Expired; continue; }
            H.Character->SetActorTransform(H.Transform, false, nullptr, ETeleportType::TeleportPhysics);
            auto* M = H.Character->GetCharacterMovement(); M->SetMovementMode(H.Mode, H.Custom); M->Velocity = H.Velocity;
            if (H.WasRunning && H.Brain.IsValid()) H.Brain->RestartLogic(); ++Restored;
        }
        for (auto Weak : Humans) if (Weak.IsValid()) Weak->Client_RunJailCaptureProbe(nullptr,nullptr,nullptr,nullptr,-1);
        UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: Complete reason=%s phase=%d spawned=%d removed=%d restored=%d expired=%d held=%d replacement=%d"), Reason, Phase, Spawned.Num(), Removed, Restored, Expired, Held.Num(), DeathReplaced);
        Running = false; return false;
    }
    ABuildableActor* Spawn(float X)
    {
        const auto* Data = LoadObject<UBwayBuildableDataAsset>(nullptr, TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_BuildableData_Gunslinger.DA_BW_BuildableData_Gunslinger"));
        auto* PC = Cast<AController>(State.Placer->GetOwner()); if (!Data || !PC) return nullptr;
        FActorSpawnParameters Params; Params.Owner = PC; Params.Instigator = PC->GetPawn(); Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor = GS->GetWorld()->SpawnActor<ABuildableActor>(Data->BuildableActorClass, FVector(X,0,ArenaZ), FRotator::ZeroRotator, Params);
        if (!Actor) return nullptr; Spawned.Add(Actor);
        Actor->ConfigurePlacementBounds(Data->PlacementBoundsCenter, Data->PlacementBoundsExtent); UBwayBuildablePlacementLibrary::ApplyResolvedBuildableMesh(Actor,Data,0);
        Actor->InitializeBuildable(PC,int32(State.Placer->GetGenericTeamId().GetId())-1); Actor->ForceNetUpdate(); return Actor;
    }
    bool BeginPhase()
    {
        PhaseStarted = GS->GetWorld()->GetTimeSeconds();
        if (Phase == 0) { Trap = Spawn(0); if (!Trap.IsValid()) return false; Move(State.Ally.Get(),FVector(0,0,ArenaZ)); }
        if (Phase == 1)
        {
            auto* PC = Cast<AController>(State.Placer->GetOwner()); if (!PC || !PC->GetPawn()) return false;
            Unpossessed = PC->GetPawn(); PC->UnPossess();
            Move(State.Ally.Get(),FVector(700,0,ArenaZ)); Move(State.Ally.Get(),FVector(0,0,ArenaZ));
            const bool Safe = Trap.IsValid() && !Trap->IsActorBeingDestroyed() && !Tag(State.Ally.Get(),BwayGameplayTags::State_Status_Jailed);
            UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: Friendly ownerAbsent=%d safe=%d"), !Pawn(State.Placer.Get()), Safe);
            if (!Safe) return false;
            Move(State.Ally.Get(),FVector(400,100,ArenaZ)); Move(State.Second.Get(),FVector(400,0,ArenaZ)); Move(State.Victim.Get(),FVector(0,0,ArenaZ));
        }
        if (Phase == 2)
        {
            auto* A = Cage(GS->GetWorld(),State.Victim.Get()); if (!A) return false;
            const float Before = Health(A); const bool Applied = UBwayDamageLibrary::ApplyDamageToEnemyBuildable(Pawn(State.Ally.Get()),A,1000,Pawn(State.Ally.Get()));
            UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: FriendlyDamage applied=%d before=%.3f after=%.3f"), Applied,Before,Health(A));
            if (Applied || Health(A) != Before) return false;
        }
        if (Phase == 3)
        {
            auto* A = Cage(GS->GetWorld(),State.Victim.Get()); if (!A) return false; const uint64 Id = NetId(A);
            const bool Applied = UBwayDamageLibrary::ApplyDamageToEnemyBuildable(Pawn(State.Victim.Get()),A,2000,Pawn(State.Victim.Get()));
            const bool Released = Cage(GS->GetWorld(),State.Victim.Get()) == nullptr && !Tag(State.Victim.Get(),BwayGameplayTags::State_Status_Jailed);
            UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: Destroy id=%llu applied=%d released=%d"),Id,Applied,Released); if (!Applied || !Released) return false;
        }
        if (Phase == 4)
        {
            DeadPawn = Pawn(State.Second.Get()); DeathsBefore = State.Second->GetDeaths(); auto* H = ULyraHealthComponent::FindHealthComponent(DeadPawn.Get()); if (!H) return false;
            H->DamageSelfDestruct(false);
            UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: VictimDeath pawn=%llu deathsBefore=%d cageGone=%d"),NetId(DeadPawn.Get()),DeathsBefore,Cage(GS->GetWorld(),State.Second.Get()) == nullptr);
        }
        if (Phase == 5)
        {
            Repossess(); Move(State.Ally.Get(),FVector(1500,0,ArenaZ)); Move(State.Second.Get(),FVector(1000,0,ArenaZ)); Move(State.Victim.Get(),FVector(1600,1000,ArenaZ));
            Trap = Spawn(2000); Survivor = Spawn(4000); if (!Trap.IsValid() || !Survivor.IsValid()) return false;
            Move(State.Victim.Get(),FVector(2000,0,ArenaZ));
        }
        if (Phase == 6)
        {
            PreResetVictim = Pawn(State.Victim.Get()); const uint64 Id = NetId(PreResetVictim.Get()); GS->GetRoundManagement()->ResetRoundState();
            UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: RoundReset oldPawn=%llu survivor=%llu"),Id,NetId(Survivor.Get()));
        }
        UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: Begin phase=%d time=%.3f"), Phase, PhaseStarted);
        for (auto Weak : Humans) Weak->Client_RunJailCaptureProbe(State.Placer.Get(),State.Ally.Get(),State.Victim.Get(),State.Second.Get(),Phase);
        return true;
    }
    bool Tick(float)
    {
        if (FPlatformTime::Seconds() - Started > 240) return Stop(TEXT("timeout"));
        if (!GS.IsValid())
        {
            if (!Held.IsEmpty()) return Stop(TEXT("lost-world"));
            for (const auto& Context : GEngine->GetWorldContexts())
            {
                auto* W = Context.World(); auto* G = W ? W->GetGameState<ABwayGameState>() : nullptr; auto* R = G ? G->GetRoundManagement() : nullptr;
                if (!W || !W->IsGameWorld() || W->GetNetMode() == NM_Client || !R) continue;
                TArray<ABwayPlayerController*> PCs; int32 Remotes = 0;
                for (auto It=W->GetPlayerControllerIterator(); It; ++It) if (auto* PC=Cast<ABwayPlayerController>(It->Get()))
                    if (PC->GetPlayerState<ABwayPlayerState>()) { PCs.Add(PC); if (!PC->IsLocalController() && PC->GetNetConnection()) ++Remotes; }
                if (PCs.Num()!=4 || Remotes!=3) continue;
                if (!Selected && G->HeroSelectionManager && G->HeroSelectionManager->IsSelectionActive())
                {
                    ABwayPlayerState *Placer=nullptr,*Victim=nullptr;
                    for (auto* PC : PCs) if (!PC->IsLocalController()) { Placer=PC->GetPlayerState<ABwayPlayerState>(); break; }
                    for (auto* PC : PCs) if (!PC->IsLocalController() && PC->GetPlayerState<ABwayPlayerState>()->GetGenericTeamId()!=Placer->GetGenericTeamId()) { Victim=PC->GetPlayerState<ABwayPlayerState>(); break; }
                    if (!Victim) return Stop(TEXT("remote-teams")); PlacerId=Placer->GetPlayerId(); VictimId=Victim->GetPlayerId(); int32 Others=0;
                    for (auto* PC : PCs)
                    {
                        auto* PS=PC->GetPlayerState<ABwayPlayerState>();
                        const TCHAR* Path = PS==Placer ? TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger.DA_BW_HeroData_Gunslinger") :
                            PS==Victim ? TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus.DA_BW_HeroData_Argus") :
                            Others++==0 ? TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona.DA_BW_HeroData_Alona") : TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver.DA_BW_HeroData_Hexweaver");
                        const auto* Hero=LoadObject<UBwayHeroDataAsset>(nullptr,Path); if (!Hero) return Stop(TEXT("selection"));
                        PC->Server_SubmitPreSelectedHero(Hero->GetPrimaryAssetId(),true); if (!PS->IsHeroLocked() || PS->GetSelectedHeroId()!=Hero->GetPrimaryAssetId()) return Stop(TEXT("selection-rejected"));
                    }
                    Selected=true;
                }
                if (!Selected || R->GetCurrentMatchPhase()!=EBwayMatchPhase::Playing || G->PlayerArray.Num()!=8) continue;
                bool Ready=true; for (const auto& P : G->PlayerArray) { auto* PS=Cast<ABwayPlayerState>(P); Ready &= PS && Alive(PS) && PS->GetLyraAbilitySystemComponent()->GetAvatarActor()==Pawn(PS); }
                if (!Ready) continue;
                GS=G; Round=R->GetCurrentRoundNumber(); if (!G->BuildableRegistryComponent->GetAllBuildables().IsEmpty()) return Stop(TEXT("preexisting-buildable"));
                for (auto* PC : PCs) { Humans.Add(PC); auto* PS=PC->GetPlayerState<ABwayPlayerState>(); if(PS->GetPlayerId()==PlacerId) State.Placer=PS; if(PS->GetPlayerId()==VictimId) State.Victim=PS; }
                if (!State.Placer.IsValid() || !State.Victim.IsValid()) return Stop(TEXT("identities"));
                for (const auto& P : G->PlayerArray) if (auto* PS=Cast<ABwayPlayerState>(P))
                {
                    if (PS==State.Placer || PS==State.Victim) continue;
                    if (PS->GetGenericTeamId()==State.Placer->GetGenericTeamId() && !State.Ally.IsValid()) State.Ally=PS;
                    else if (PS->GetGenericTeamId()==State.Victim->GetGenericTeamId() && !State.Second.IsValid()) State.Second=PS;
                }
                if (!State.Ally.IsValid() || !State.Second.IsValid()) return Stop(TEXT("roles"));
                for (const auto& P : G->PlayerArray) ArenaZ=FMath::Max(ArenaZ,float(P->GetPawn()->GetActorLocation().Z+10000));
                for (const auto& P : G->PlayerArray) Hold(P->GetPawn());
                UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: Setup humans=4 remotes=3 held=%d round=%d placer=%d victim=%d second=%d"),Held.Num(),Round,PlacerId,VictimId,State.Second->GetPlayerId());
                return BeginPhase() ? true : Stop(TEXT("begin"));
            }
            return true;
        }
        auto* R=GS->GetRoundManagement(); if (R->GetCurrentMatchPhase()!=EBwayMatchPhase::Playing || R->GetCurrentRoundNumber()!=Round) return Stop(TEXT("phase-changed"));
        for (const auto& P : GS->PlayerArray) if (auto* PS=Cast<ABwayPlayerState>(P)) if (Alive(PS)) Hold(Pawn(PS));
        if (Phase==4 && !DeathReplaced && Alive(State.Second.Get()) && Pawn(State.Second.Get())!=DeadPawn.Get())
        { DeathReplaced=true; PhaseStarted=GS->GetWorld()->GetTimeSeconds(); UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: Respawn pawn=%llu deaths=%d"),NetId(Pawn(State.Second.Get())),State.Second->GetDeaths()); }
        State.Log(Humans[0].Get(),Phase,TEXT("authority"));
        if (GS->GetWorld()->GetTimeSeconds()-PhaseStarted < 5.2 || (Phase==4 && !DeathReplaced)) return true;
        auto* A=Cage(GS->GetWorld(),State.Victim.Get()); auto* B=Cage(GS->GetWorld(),State.Second.Get());
        const bool ExpectedA=Phase==1 || Phase==2 || Phase==5, ExpectedB=Phase>=1 && Phase<=3;
        const int32 ExpectedCount=Phase==0 || Phase==3 || Phase==6 ? 1 : Phase==4 ? 0 : 2;
        bool Passed=(bool(A)==ExpectedA) && (bool(B)==ExpectedB) && Tag(State.Victim.Get(),BwayGameplayTags::State_Status_Jailed)==ExpectedA
            && Tag(State.Second.Get(),BwayGameplayTags::State_Status_Jailed)==ExpectedB && !Tag(State.Ally.Get(),BwayGameplayTags::State_Status_Jailed)
            && GS->BuildableRegistryComponent->GetAllBuildables().Num()==ExpectedCount;
        Passed &= !A || (Phase==2 ? Health(A)>0 && Health(A)<450 : Health(A)==450); Passed &= !B || Health(B)==450;
        if (Phase==4) Passed &= State.Second->GetDeaths()==DeathsBefore+1;
        if (Phase==6) Passed &= Survivor.IsValid() && Pawn(State.Victim.Get()) && Pawn(State.Victim.Get())!=PreResetVictim.Get();
        UE_LOG(LogTemp, Display, TEXT("BwayJailCapture: Verify phase=%d healthA=%.3f healthB=%.3f registered=%d pass=%d"),Phase,Health(A),Health(B),GS->BuildableRegistryComponent->GetAllBuildables().Num(),Passed);
        if (!Passed) return Stop(TEXT("verdict")); if (++Phase==7) return Stop(TEXT("observed")); return BeginPhase() ? true : Stop(TEXT("begin"));
    }
};
FAutoConsoleCommand Command(TEXT("bway.Test.JailCaptureAudit"),TEXT("Development four-process Jail capture, remote primary, destruction and reset fixture."),
    FConsoleCommandDelegate::CreateLambda([]() { if (Running) return; Running=true; auto Run=MakeShared<FRun>(); FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta){return Run->Tick(Delta);}),.1f); }));
}
#endif

void ABwayPlayerController::Client_RunJailCaptureProbe_Implementation(ABwayPlayerState* Placer, ABwayPlayerState* Ally, ABwayPlayerState* Victim, ABwayPlayerState* SecondVictim, int32 Phase)
{
#if !UE_BUILD_SHIPPING
    BwayJailCaptureAudit::Observe(this,Placer,Ally,Victim,SecondVictim,Phase);
#endif
}
