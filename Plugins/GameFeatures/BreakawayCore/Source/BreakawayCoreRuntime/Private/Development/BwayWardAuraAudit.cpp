#include "BwayPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "BwayGameplayTags.h"
#include "BwayGameState.h"
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
#include "GameFramework/CharacterMovementComponent.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroSelectionManager.h"

namespace BwayWardAuraAudit
{
bool Running = false;
TMap<TWeakObjectPtr<ABwayPlayerController>, FTSTicker::FDelegateHandle> Observers;
uint64 NetId(const AActor* Actor)
{
    const auto* Driver = Actor && Actor->GetWorld() ? Actor->GetWorld()->GetNetDriver() : nullptr;
    return Driver && Driver->GuidCache ? Driver->GuidCache->GetNetGUID(Actor).ObjectId : 0;
}
float Health(ABwayPlayerState* PS)
{
    auto* ASC = PS ? PS->GetLyraAbilitySystemComponent() : nullptr;
    return ASC && ASC->GetSet<ULyraHealthSet>() ? ASC->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()) : -1.f;
}
APawn* BoundPawn(ABwayPlayerState* PS)
{
    // PlayerState caches its pawn locally. The pawn's replicated PlayerState is
    // the binding that actually clears during unpossession, before destruction.
    APawn* Pawn = PS ? PS->GetPawn() : nullptr;
    return Pawn && Pawn->GetPlayerState() == PS ? Pawn : nullptr;
}
bool Alive(ABwayPlayerState* PS)
{
    const auto* H = ULyraHealthComponent::FindHealthComponent(BoundPawn(PS));
    return H && !H->IsDeadOrDying() && H->GetHealth() > 0;
}

float Speed(ABwayPlayerState* PS)
{
    auto* ASC=PS ? PS->GetLyraAbilitySystemComponent() : nullptr;
    return ASC && ASC->GetSet<UBwayHeroAttributeSet>() ? ASC->GetNumericAttribute(UBwayHeroAttributeSet::GetMoveSpeedMultiplierAttribute()) : -1;
}
bool Slowed(ABwayPlayerState* PS)
{ return PS && PS->GetLyraAbilitySystemComponent()->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Slowed); }
struct FState
{
    TWeakObjectPtr<ABwayPlayerState> Owner, Ally, Enemy, Outside;
    bool Log(ABwayPlayerController* PC,int32 Phase,const TCHAR* Side) const
    {
        auto* GS=PC && PC->GetWorld() ? PC->GetWorld()->GetGameState<ABwayGameState>() : nullptr;
        if(!GS || !Owner.IsValid() || !Ally.IsValid() || !Enemy.IsValid() || !Outside.IsValid()) return false;
        TArray<ABuildableActor*> Actors;
        for(auto* A:GS->BuildableRegistryComponent->GetAllBuildables()) if(IsValid(A)) Actors.Add(A);
        Actors.Sort([](const ABuildableActor& A,const ABuildableActor& B){return NetId(&A)<NetId(&B);});
        UE_LOG(LogTemp,Display,TEXT("BwayWardAura: Sample side=%s phase=%d peer=%d time=%.3f owner=%d ownerPawn=%llu ownerAlive=%d ownerDeaths=%d enemy=%d enemyPawn=%llu enemyAlive=%d enemyDeaths=%d ownerTeam=%d enemyTeam=%d enemySpeed=%.3f allySpeed=%.3f outsideSpeed=%.3f enemySlow=%d allySlow=%d outsideSlow=%d registered=%d quota=%d first=%llu second=%llu firstHealth=%.3f secondHealth=%.3f"),
            Side,Phase,PC->PlayerState ? PC->PlayerState->GetPlayerId():-1,GS->GetServerWorldTimeSeconds(),Owner->GetPlayerId(),NetId(BoundPawn(Owner.Get())),Alive(Owner.Get()),Owner->GetDeaths(),
            Enemy->GetPlayerId(),NetId(BoundPawn(Enemy.Get())),Alive(Enemy.Get()),Enemy->GetDeaths(),int32(Owner->GetGenericTeamId().GetId()),int32(Enemy->GetGenericTeamId().GetId()),
            Speed(Enemy.Get()),Speed(Ally.Get()),Speed(Outside.Get()),Slowed(Enemy.Get()),Slowed(Ally.Get()),Slowed(Outside.Get()),Actors.Num(),GS->BuildableRegistryComponent->GetBuildableCountForPlayer(Owner.Get()),
            Actors.Num()>0 ? NetId(Actors[0]):0,Actors.Num()>1 ? NetId(Actors[1]):0,
            Actors.Num()>0 ? Actors[0]->GetAbilitySystemComponent()->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()):0,
            Actors.Num()>1 ? Actors[1]->GetAbilitySystemComponent()->GetNumericAttribute(ULyraHealthSet::GetHealthAttribute()):0);
        return true;
    }
};
void Observe(ABwayPlayerController* PC, ABwayPlayerState* Owner, ABwayPlayerState* Ally, ABwayPlayerState* Enemy, ABwayPlayerState* Outside, int32 Phase)
{
    if (!PC || !PC->IsLocalController()) return;
    if (auto* Old = Observers.Find(PC)) { FTSTicker::GetCoreTicker().RemoveTicker(*Old); Observers.Remove(PC); }
    if (Phase < 0) { UE_LOG(LogTemp, Display, TEXT("BwayWardAura: PeerReleased peer=%d"), PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1); return; }
    FState State; State.Owner = Owner; State.Ally = Ally; State.Enemy = Enemy; State.Outside = Outside;
    TWeakObjectPtr<ABwayPlayerController> WeakPC = PC;
    const double Started = FPlatformTime::Seconds();
    Observers.Add(PC, FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC, State, Phase, Started](float)
    {
        if (!WeakPC.IsValid() || !State.Log(WeakPC.Get(), Phase, TEXT("peer"))) return false;
        if (FPlatformTime::Seconds() - Started > 30)
        { UE_LOG(LogTemp, Error, TEXT("BwayWardAura: PeerFailure phase=%d reason=timeout"), Phase); return false; }
        return true;
    }), .2f));
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
    TWeakObjectPtr<ABuildableActor> First,Second;
    TWeakObjectPtr<APawn> TemporarilyUnpossessedPawn,OldOwner,OldEnemy,PreResetEnemy;
    FState State;
    double Started=FPlatformTime::Seconds(),PhaseStarted=0;
    int32 Phase=0,Round=-1,PlacerId=-1,VictimId=-1,OriginalEnemyTeam=0,OwnerDeaths=0,EnemyDeaths=0,Created=0,Destroyed=0;
    float ArenaZ=10000;
    bool Selected=false,OwnerReplaced=false,EnemyReplaced=false,ResetSeen=false;
    void Move(ABwayPlayerState* PS,FVector Position)
    { if(auto* P=BoundPawn(PS)) P->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics); }
    void RestorePossession()
    {
        if(!TemporarilyUnpossessedPawn.IsValid() || !State.Owner.IsValid()) return;
        if(auto* PC=Cast<AController>(State.Owner->GetOwner()))
        {
            PC->Possess(TemporarilyUnpossessedPawn.Get());
            // Lyra OnUnPossess clears the ASC avatar. This fixture deliberately
            // reuses a GameplayReady pawn, so restore the binding it interrupted.
            State.Owner->GetLyraAbilitySystemComponent()->InitAbilityActorInfo(State.Owner.Get(),PC->GetPawn());
            if(auto* P=Cast<ABwayCharacterWithAbilities>(PC->GetPawn())) { P->GetCharacterMovement()->StopMovementImmediately(); P->GetCharacterMovement()->DisableMovement(); }
        }
        TemporarilyUnpossessedPawn.Reset();
    }
    void SetEnemyTeam(int32 Team)
    {
        if(!GS.IsValid() || !State.Enemy.IsValid() || Team<=0) return;
        GS->AddPlayerToTeam(State.Enemy.Get(),Team-1);
        State.Enemy->SetGenericTeamId(FGenericTeamId(Team)); State.Enemy->ForceNetUpdate();
        // The possessed pawn follows the PlayerState/controller team delegate.
        if(auto* P=BoundPawn(State.Enemy.Get())) P->ForceNetUpdate();
    }
    bool Stop(const TCHAR* Reason)
    {
        RestorePossession(); SetEnemyTeam(OriginalEnemyTeam);
        int32 FixtureDestroyed=0,Restored=0,Expired=0;
        for(auto W:{First,Second}) if(auto* A=W.Get()) { A->Destroy(); ++FixtureDestroyed; }
        for(const auto& H:Held)
        {
            if(!H.Pawn.IsValid()) {++Expired;continue;}
            H.Pawn->SetActorTransform(H.Transform,false,nullptr,ETeleportType::TeleportPhysics);
            auto* M=H.Pawn->GetCharacterMovement(); M->SetMovementMode(H.Mode,H.Custom); M->Velocity=H.Velocity;
            if(H.WasRunning && H.Brain.IsValid()) H.Brain->RestartLogic(); ++Restored;
        }
        for(auto W:Humans) if(auto* PC=W.Get()) PC->Client_RunWardAuraProbe(nullptr,nullptr,nullptr,nullptr,-1);
        UE_LOG(LogTemp,Display,TEXT("BwayWardAura: Complete reason=%s phase=%d created=%d nativeDestroyed=%d fixtureDestroyed=%d restored=%d expired=%d held=%d ownerReplaced=%d enemyReplaced=%d reset=%d"),
            Reason,Phase,Created,Destroyed,FixtureDestroyed,Restored,Expired,Held.Num(),OwnerReplaced,EnemyReplaced,ResetSeen);
        Running=false;return false;
    }
    void Hold(APawn* Pawn)
    {
        auto* Character = Cast<ABwayCharacterWithAbilities>(Pawn);
        if (!Character || Held.ContainsByPredicate([Character](const FHeld& H) { return H.Pawn == Character; })) return;
        auto* Movement = Character->GetCharacterMovement();
        auto* AI = Cast<AAIController>(Character->GetController()); auto* Brain = AI ? AI->GetBrainComponent() : nullptr;
        Held.Add({Character, Character->GetActorTransform(), Movement->Velocity, Movement->MovementMode, Movement->CustomMovementMode, Brain, Brain && Brain->IsRunning()});
        Movement->StopMovementImmediately(); Movement->DisableMovement(); if (Brain) Brain->StopLogic(TEXT("Owned Ward aura audit"));
        Character->SetActorLocation(FVector(0, -3000 - 300 * Held.Num(), ArenaZ), false, nullptr, ETeleportType::TeleportPhysics);
    }
    ABuildableActor* Spawn(FVector Location)
    {
        const auto* Data = LoadObject<UBwayBuildableDataAsset>(nullptr, TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_BuildableData_Hexweaver.DA_BW_BuildableData_Hexweaver"));
        if (!Data || !Data->BuildableActorClass || !State.Owner.IsValid()) return nullptr;
        auto* PC = Cast<AController>(State.Owner->GetOwner());
        FActorSpawnParameters Params; Params.Owner = PC; Params.Instigator = PC ? PC->GetPawn() : nullptr;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Actor = GS->GetWorld()->SpawnActor<ABuildableActor>(Data->BuildableActorClass, Location, FRotator::ZeroRotator, Params);
        if (!Actor) return nullptr;
        ++Created; Actor->ConfigurePlacementBounds(Data->PlacementBoundsCenter, Data->PlacementBoundsExtent);
        UBwayBuildablePlacementLibrary::ApplyResolvedBuildableMesh(Actor, Data, 0);
        Actor->InitializeBuildable(PC, int32(State.Owner->GetGenericTeamId().GetId()) - 1); Actor->ForceNetUpdate();
        return Actor;
    }
    bool DestroySource(TWeakObjectPtr<ABuildableActor>& Weak)
    {
        auto* Actor = Weak.Get(); if (!Actor) return false;
        const uint64 Id = NetId(Actor);
        const bool Applied = UBwayDamageLibrary::ApplyDamageFromSource(State.Enemy->GetLyraAbilitySystemComponent(), Actor, 2000, State.Enemy->GetPawn(), State.Enemy->GetPawn());
        const bool Gone = !Weak.IsValid() || Weak->IsActorBeingDestroyed();
        UE_LOG(LogTemp, Display, TEXT("BwayWardAura: Destroy phase=%d id=%llu applied=%d gone=%d"), Phase, Id, Applied, Gone);
        if (Gone) ++Destroyed;
        return Applied && Gone;
    }

    bool BeginPhase()
    {
        PhaseStarted=GS->GetWorld()->GetTimeSeconds();
        if(Phase==1) { First=Spawn(FVector(0,0,ArenaZ)); if(!First.IsValid()) return false; }
        if(Phase==2)
        {
            auto* PC=Cast<AController>(State.Owner->GetOwner()); if(!PC || !PC->GetPawn()) return false;
            TemporarilyUnpossessedPawn=PC->GetPawn(); PC->UnPossess();
            Move(State.Ally.Get(),FVector(2000,-100,ArenaZ));
            UE_LOG(LogTemp,Display,TEXT("BwayWardAura: Unpossess pawn=%llu"),NetId(TemporarilyUnpossessedPawn.Get()));
        }
        if(Phase==3)
        {
            RestorePossession(); OldOwner=BoundPawn(State.Owner.Get());
            if(!OldOwner.IsValid() || State.Owner->GetLyraAbilitySystemComponent()->GetAvatarActor()!=OldOwner.Get()) return false;
            auto* H=ULyraHealthComponent::FindHealthComponent(OldOwner.Get()); if(!H)return false;
            H->DamageSelfDestruct(false);
            UE_LOG(LogTemp,Display,TEXT("BwayWardAura: Kill role=owner pawn=%llu before=%d"),NetId(OldOwner.Get()),OwnerDeaths);
        }
        if(Phase==6) { Second=Spawn(FVector(250,0,ArenaZ)); if(!Second.IsValid())return false; }
        if(Phase==7 && !DestroySource(First)) return false;
        if(Phase==8)
        {
            OldEnemy=BoundPawn(State.Enemy.Get());
            auto* H=ULyraHealthComponent::FindHealthComponent(OldEnemy.Get()); if(!H)return false;
            H->DamageSelfDestruct(false);
            UE_LOG(LogTemp,Display,TEXT("BwayWardAura: Kill role=enemy pawn=%llu before=%d"),NetId(OldEnemy.Get()),EnemyDeaths);
        }
        if(Phase==10) SetEnemyTeam(State.Owner->GetGenericTeamId().GetId());
        if(Phase==11) SetEnemyTeam(OriginalEnemyTeam);
        if(Phase==12)
        {
            PreResetEnemy=BoundPawn(State.Enemy.Get()); GS->GetRoundManagement()->ResetRoundState();
            UE_LOG(LogTemp,Display,TEXT("BwayWardAura: RoundReset oldPawn=%llu survivor=%llu"),NetId(PreResetEnemy.Get()),NetId(Second.Get()));
        }
        if(Phase==13 && !DestroySource(Second))return false;
        UE_LOG(LogTemp,Display,TEXT("BwayWardAura: Begin phase=%d time=%.3f"),Phase,PhaseStarted);
        for(auto W:Humans) W->Client_RunWardAuraProbe(State.Owner.Get(),State.Ally.Get(),State.Enemy.Get(),State.Outside.Get(),Phase);
        return true;
    }
    bool Tick(float)
    {
        if(FPlatformTime::Seconds()-Started>300) return Stop(TEXT("timeout"));
        if(!GS.IsValid())
        {
            if(!Held.IsEmpty()) return Stop(TEXT("lost-world"));
            for(const auto& Context:GEngine->GetWorldContexts())
            {
                auto* W=Context.World(); auto* G=W?W->GetGameState<ABwayGameState>():nullptr; auto* R=G?G->GetRoundManagement():nullptr;
                if(!W || !W->IsGameWorld() || W->GetNetMode()==NM_Client || !R)continue;
                TArray<ABwayPlayerController*> PCs;int32 Remotes=0;
                for(auto It=W->GetPlayerControllerIterator();It;++It) if(auto* PC=Cast<ABwayPlayerController>(It->Get()))
                    if(PC->GetPlayerState<ABwayPlayerState>()) {PCs.Add(PC);if(!PC->IsLocalController() && PC->GetNetConnection())++Remotes;}
                if(PCs.Num()!=4 || Remotes!=3)continue;
                if (!Selected && G->HeroSelectionManager && G->HeroSelectionManager->IsSelectionActive())
                {
                    ABwayPlayerState *Placer=nullptr,*Victim=nullptr;
                    for (auto* PC : PCs) if (!PC->IsLocalController()) { Placer=PC->GetPlayerState<ABwayPlayerState>(); break; }
                    for (auto* PC : PCs) if (!PC->IsLocalController() && PC->GetPlayerState<ABwayPlayerState>()->GetGenericTeamId()!=Placer->GetGenericTeamId()) { Victim=PC->GetPlayerState<ABwayPlayerState>(); break; }
                    if (!Victim) return Stop(TEXT("remote-teams")); PlacerId=Placer->GetPlayerId(); VictimId=Victim->GetPlayerId(); int32 Others=0;
                    for (auto* PC : PCs)
                    {
                        auto* PS=PC->GetPlayerState<ABwayPlayerState>();
                        const TCHAR* Path = PS==Placer ? TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver.DA_BW_HeroData_Hexweaver") :
                            PS==Victim ? TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus.DA_BW_HeroData_Argus") :
                            Others++==0 ? TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona.DA_BW_HeroData_Alona") : TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger.DA_BW_HeroData_Gunslinger");
                        const auto* Hero=LoadObject<UBwayHeroDataAsset>(nullptr,Path); if (!Hero) return Stop(TEXT("selection"));
                        PC->Server_SubmitPreSelectedHero(Hero->GetPrimaryAssetId(),true); if (!PS->IsHeroLocked() || PS->GetSelectedHeroId()!=Hero->GetPrimaryAssetId()) return Stop(TEXT("selection-rejected"));
                    }
                    Selected=true;
                }
                if (!Selected || R->GetCurrentMatchPhase()!=EBwayMatchPhase::Playing || G->PlayerArray.Num()!=8) continue;
                bool Ready=true; for (const auto& P : G->PlayerArray) { auto* PS=Cast<ABwayPlayerState>(P); Ready &= PS && Alive(PS) && PS->GetLyraAbilitySystemComponent()->GetAvatarActor()==BoundPawn(PS); }
                if (!Ready) continue;
                GS=G; Round=R->GetCurrentRoundNumber(); if (!G->BuildableRegistryComponent->GetAllBuildables().IsEmpty()) return Stop(TEXT("preexisting-buildable"));
                for (auto* PC : PCs) { Humans.Add(PC); auto* PS=PC->GetPlayerState<ABwayPlayerState>(); if(PS->GetPlayerId()==PlacerId) State.Owner=PS; if(PS->GetPlayerId()==VictimId) State.Enemy=PS; }
                if (!State.Owner.IsValid() || !State.Enemy.IsValid()) return Stop(TEXT("identities"));
                for (const auto& P : G->PlayerArray) if (auto* PS=Cast<ABwayPlayerState>(P))
                {
                    if (PS==State.Owner || PS==State.Enemy) continue;
                    if (PS->GetGenericTeamId()==State.Owner->GetGenericTeamId() && !State.Ally.IsValid()) State.Ally=PS;
                    else if (PS->GetGenericTeamId()==State.Enemy->GetGenericTeamId() && !State.Outside.IsValid()) State.Outside=PS;
                }
                if (!State.Ally.IsValid() || !State.Outside.IsValid()) return Stop(TEXT("roles"));
                for (const auto& P : G->PlayerArray) ArenaZ=FMath::Max(ArenaZ,float(P->GetPawn()->GetActorLocation().Z+10000));
                for (const auto& P : G->PlayerArray) Hold(P->GetPawn());
                UE_LOG(LogTemp, Display, TEXT("BwayWardAura: Setup humans=4 remotes=3 held=%d round=%d placer=%d victim=%d second=%d"),Held.Num(),Round,PlacerId,VictimId,State.Outside->GetPlayerId());
                OriginalEnemyTeam=State.Enemy->GetGenericTeamId().GetId();
                OwnerDeaths=State.Owner->GetDeaths(); EnemyDeaths=State.Enemy->GetDeaths();

                return BeginPhase()?true:Stop(TEXT("begin"));
            }
            return true;
        }
        auto* R=GS->GetRoundManagement();
        if(R->GetCurrentMatchPhase()!=EBwayMatchPhase::Playing || R->GetCurrentRoundNumber()!=Round)return Stop(TEXT("phase-changed"));
        for(const auto& P:GS->PlayerArray) Hold(P->GetPawn());
        if(Phase==3 && !OwnerReplaced && BoundPawn(State.Owner.Get())!=OldOwner.Get() && Alive(State.Owner.Get()))
        {OwnerReplaced=true;PhaseStarted=GS->GetWorld()->GetTimeSeconds();UE_LOG(LogTemp,Display,TEXT("BwayWardAura: Replacement role=owner old=%llu current=%llu"),NetId(OldOwner.Get()),NetId(BoundPawn(State.Owner.Get())));}
        if(Phase==8 && !EnemyReplaced && BoundPawn(State.Enemy.Get())!=OldEnemy.Get() && Alive(State.Enemy.Get()))
        {EnemyReplaced=true;PhaseStarted=GS->GetWorld()->GetTimeSeconds();UE_LOG(LogTemp,Display,TEXT("BwayWardAura: Replacement role=enemy old=%llu current=%llu"),NetId(OldEnemy.Get()),NetId(BoundPawn(State.Enemy.Get())));}
        if(Phase==12 && !ResetSeen && BoundPawn(State.Enemy.Get())!=PreResetEnemy.Get() && Alive(State.Enemy.Get()))
        {ResetSeen=true;PhaseStarted=GS->GetWorld()->GetTimeSeconds();}
        Move(State.Ally.Get(),FVector(100,-100,ArenaZ)); Move(State.Outside.Get(),FVector(1500,500,ArenaZ));
        if(Alive(State.Enemy.Get())) Move(State.Enemy.Get(),FVector(Phase==4 || Phase==8 ? 1800:200,0,ArenaZ));
        State.Log(Humans[0].Get(),Phase,TEXT("authority"));
        const double Age=GS->GetWorld()->GetTimeSeconds()-PhaseStarted;
        if(Age>25)return Stop(TEXT("phase-timeout"));
        if(Age<5.2 || (Phase==3&&!OwnerReplaced) || (Phase==8&&!EnemyReplaced) || (Phase==12&&!ResetSeen))return true;
        const float Expected=Phase==0 || Phase==4 || Phase==8 || Phase==10 || Phase==13 ? 1.f : Phase==6 ? .25f:.5f;
        const int32 ExpectedCount=Phase==0 || Phase==13 ? 0 : Phase==6 ? 2:1;
        const bool Passed=FMath::IsNearlyEqual(Speed(State.Enemy.Get()),Expected,.001f) && Slowed(State.Enemy.Get())==(Expected<1)
            && FMath::IsNearlyEqual(Speed(State.Ally.Get()),1.f) && !Slowed(State.Ally.Get()) && FMath::IsNearlyEqual(Speed(State.Outside.Get()),1.f) && !Slowed(State.Outside.Get())
            && GS->BuildableRegistryComponent->GetAllBuildables().Num()==ExpectedCount
            && State.Owner->GetDeaths()==OwnerDeaths+(Phase>=3?1:0) && State.Enemy->GetDeaths()==EnemyDeaths+(Phase>=8?1:0);
        UE_LOG(LogTemp,Display,TEXT("BwayWardAura: Verify phase=%d time=%.3f expected=%.3f speed=%.3f count=%d pass=%d"),Phase,GS->GetWorld()->GetTimeSeconds(),Expected,Speed(State.Enemy.Get()),ExpectedCount,Passed);
        if(!Passed)return Stop(TEXT("verdict"));
        if(++Phase==14)return Stop(TEXT("observed"));
        return BeginPhase()?true:Stop(TEXT("begin"));
    }
};
FAutoConsoleCommand Command(TEXT("bway.Test.WardAuraAudit"),TEXT("Development four-process Ward lifecycle and independent replicated slow observations."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if(Running)return;Running=true;auto Run=MakeShared<FRun>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta){return Run->Tick(Delta);}),.1f);
    }));
}
#endif
void ABwayPlayerController::Client_RunWardAuraProbe_Implementation(ABwayPlayerState* OwnerPlayer,ABwayPlayerState* Ally,ABwayPlayerState* Enemy,ABwayPlayerState* Outside,int32 Phase)
{
#if !UE_BUILD_SHIPPING
    BwayWardAuraAudit::Observe(this,OwnerPlayer,Ally,Enemy,Outside,Phase);
#endif
}
