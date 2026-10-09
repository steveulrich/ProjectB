#include "BwayPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/LyraHealthSet.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BwayCharacterWithAbilities.h"
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

namespace BwayShrineAuraAudit
{
bool Running = false;
TMap<TWeakObjectPtr<ABwayPlayerController>, FTSTicker::FDelegateHandle> Observers;
uint64 NetId(AActor* Actor)
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
struct FState
{
    TWeakObjectPtr<ABwayPlayerState> Owner, Ally, Enemy, Outside;
    bool Log(ABwayPlayerController* PC, int32 Phase, const TCHAR* Side) const
    {
        auto* GS = PC && PC->GetWorld() ? PC->GetWorld()->GetGameState<ABwayGameState>() : nullptr;
        if (!GS || !GS->BuildableRegistryComponent || !Owner.IsValid() || !Ally.IsValid() || !Enemy.IsValid() || !Outside.IsValid()) return false;
        TArray<uint64> Ids;
        for (auto* Actor : GS->BuildableRegistryComponent->GetAllBuildables()) if (IsValid(Actor)) Ids.Add(NetId(Actor));
        Ids.Sort();
        UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Sample side=%s phase=%d peer=%d time=%.3f owner=%d ownerPawn=%llu ownerAlive=%d deaths=%d ally=%d allyPawn=%llu allyTeam=%d enemyTeam=%d allyHealth=%.3f enemyHealth=%.3f outsideHealth=%.3f registered=%d first=%llu second=%llu"),
            Side, Phase, PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1, GS->GetServerWorldTimeSeconds(),
            Owner->GetPlayerId(), NetId(BoundPawn(Owner.Get())), Alive(Owner.Get()), Owner->GetDeaths(),
            Ally->GetPlayerId(), NetId(Ally->GetPawn()), int32(Ally->GetGenericTeamId().GetId()), int32(Enemy->GetGenericTeamId().GetId()),
            Health(Ally.Get()), Health(Enemy.Get()), Health(Outside.Get()), Ids.Num(), Ids.Num() > 0 ? Ids[0] : 0, Ids.Num() > 1 ? Ids[1] : 0);
        return true;
    }
};
void Observe(ABwayPlayerController* PC, ABwayPlayerState* Owner, ABwayPlayerState* Ally, ABwayPlayerState* Enemy, ABwayPlayerState* Outside, int32 Phase)
{
    if (!PC || !PC->IsLocalController()) return;
    if (auto* Old = Observers.Find(PC)) { FTSTicker::GetCoreTicker().RemoveTicker(*Old); Observers.Remove(PC); }
    if (Phase < 0) { UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: PeerReleased peer=%d"), PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1); return; }
    FState State; State.Owner = Owner; State.Ally = Ally; State.Enemy = Enemy; State.Outside = Outside;
    TWeakObjectPtr<ABwayPlayerController> WeakPC = PC;
    const double Started = FPlatformTime::Seconds();
    Observers.Add(PC, FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakPC, State, Phase, Started](float)
    {
        if (!WeakPC.IsValid() || !State.Log(WeakPC.Get(), Phase, TEXT("peer"))) return false;
        if (FPlatformTime::Seconds() - Started > 30)
        { UE_LOG(LogTemp, Error, TEXT("BwayShrineAura: PeerFailure phase=%d reason=timeout"), Phase); return false; }
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
    TWeakObjectPtr<ABuildableActor> First, Second;
    TWeakObjectPtr<APawn> OriginalOwnerPawn;
    TWeakObjectPtr<APawn> TemporarilyUnpossessedPawn;
    FState State;
    double Started = FPlatformTime::Seconds(), PhaseStarted = 0;
    int32 Phase = 0, RoundNumber = -1, DeathsBefore = 0, Created = 0, Destroyed = 0;
    float ArenaZ = 10000, FinalHealth = 100;
    float OriginalHealth[3] = {};
    bool Selected = false, Settling = false, ReplacementSeen = false;
    IConsoleVariable* AuditCvar = nullptr;
    int32 OldAuditCvar = 0;
    void Move(ABwayPlayerState* PS, FVector Position)
    { if (PS && PS->GetPawn()) PS->GetPawn()->SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics); }
    void Hold(APawn* Pawn)
    {
        auto* Character = Cast<ABwayCharacterWithAbilities>(Pawn);
        if (!Character || Held.ContainsByPredicate([Character](const FHeld& H) { return H.Pawn == Character; })) return;
        auto* Movement = Character->GetCharacterMovement();
        auto* AI = Cast<AAIController>(Character->GetController()); auto* Brain = AI ? AI->GetBrainComponent() : nullptr;
        Held.Add({Character, Character->GetActorTransform(), Movement->Velocity, Movement->MovementMode, Movement->CustomMovementMode, Brain, Brain && Brain->IsRunning()});
        Movement->StopMovementImmediately(); Movement->DisableMovement(); if (Brain) Brain->StopLogic(TEXT("Owned Shrine aura audit"));
        Character->SetActorLocation(FVector(0, -3000 - 300 * Held.Num(), ArenaZ), false, nullptr, ETeleportType::TeleportPhysics);
    }
    bool Stop(const TCHAR* Reason)
    {
        if (TemporarilyUnpossessedPawn.IsValid() && State.Owner.IsValid())
            if (auto* PC = Cast<AController>(State.Owner->GetOwner())) PC->Possess(TemporarilyUnpossessedPawn.Get());
        int32 FixtureDestroyed = 0, Restored = 0, Expired = 0;
        for (auto Weak : {First, Second}) if (auto* Actor = Weak.Get()) { Actor->Destroy(); ++FixtureDestroyed; }
        for (const auto& H : Held)
        {
            if (!H.Pawn.IsValid()) { ++Expired; continue; }
            H.Pawn->SetActorTransform(H.Transform, false, nullptr, ETeleportType::TeleportPhysics);
            auto* M = H.Pawn->GetCharacterMovement(); M->SetMovementMode(H.Mode, H.Custom); M->Velocity = H.Velocity;
            if (H.WasRunning && H.Brain.IsValid()) H.Brain->RestartLogic();
            ++Restored;
        }
        int32 Index = 0;
        for (auto Weak : {State.Ally, State.Enemy, State.Outside})
        { if (Weak.IsValid() && OriginalHealth[Index] > 0) Weak->GetLyraAbilitySystemComponent()->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), OriginalHealth[Index]); ++Index; }
        if (AuditCvar) AuditCvar->Set(OldAuditCvar, ECVF_SetByCode);
        for (auto Weak : Humans) if (auto* PC = Weak.Get()) PC->Client_RunShrineAuraProbe(nullptr, nullptr, nullptr, nullptr, -1);
        UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Complete reason=%s phase=%d created=%d nativeDestroyed=%d fixtureDestroyed=%d restored=%d expired=%d held=%d replacement=%d cvarRestored=%d"),
            Reason, Phase, Created, Destroyed, FixtureDestroyed, Restored, Expired, Held.Num(), ReplacementSeen, !AuditCvar || AuditCvar->GetInt() == OldAuditCvar);
        Running = false; return false;
    }
    ABuildableActor* Spawn(FVector Location)
    {
        const auto* Data = LoadObject<UBwayBuildableDataAsset>(nullptr, TEXT("/Hero_Alona/Characters/Heroes/Alona/Buildables/DA_BW_BuildableData_SunShrine.DA_BW_BuildableData_SunShrine"));
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
        UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Destroy phase=%d id=%llu applied=%d gone=%d"), Phase, Id, Applied, Gone);
        if (Gone) ++Destroyed;
        return Applied && Gone;
    }
    bool BeginPhase()
    {
        PhaseStarted = GS->GetWorld()->GetTimeSeconds(); Settling = false;
        for (auto Weak : {State.Ally, State.Enemy, State.Outside}) Weak->GetLyraAbilitySystemComponent()->SetNumericAttributeBase(ULyraHealthSet::GetHealthAttribute(), 100);
        Move(State.Ally.Get(), FVector(Phase == 7 ? 1200 : 100, 0, ArenaZ));
        Move(State.Enemy.Get(), FVector(200, 0, ArenaZ)); Move(State.Outside.Get(), FVector(1200, 200, ArenaZ));
        if (Phase == 1) { First = Spawn(FVector(0,0,ArenaZ)); if (!First.IsValid()) return false; }
        if (Phase == 2) { Second = Spawn(FVector(250,0,ArenaZ)); if (!Second.IsValid()) return false; }
        if (Phase == 3)
        {
            OriginalOwnerPawn = State.Owner->GetPawn(); DeathsBefore = State.Owner->GetDeaths();
            auto* H = ULyraHealthComponent::FindHealthComponent(OriginalOwnerPawn.Get()); if (!H) return false;
            H->DamageSelfDestruct(false);
            UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Kill pawn=%llu deathsBefore=%d alive=%d"), NetId(OriginalOwnerPawn.Get()), DeathsBefore, Alive(State.Owner.Get()));
        }
        if (Phase == 5)
        {
            auto* PC = Cast<AController>(State.Owner->GetOwner()); if (!PC || !PC->GetPawn()) return false;
            TemporarilyUnpossessedPawn = PC->GetPawn(); PC->UnPossess();
            UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Unpossess pawn=%llu"), NetId(TemporarilyUnpossessedPawn.Get()));
        }
        if (Phase == 6)
        {
            auto* PC = Cast<AController>(State.Owner->GetOwner()); if (!PC || !TemporarilyUnpossessedPawn.IsValid()) return false;
            PC->Possess(TemporarilyUnpossessedPawn.Get()); TemporarilyUnpossessedPawn.Reset();
            auto* Pawn = Cast<ABwayCharacterWithAbilities>(PC->GetPawn()); if (!Pawn) return false;
            auto* Movement = Pawn->GetCharacterMovement(); const int32 ResumedMode = Movement->MovementMode;
            // ACharacter::Restart restores the default movement mode on possession.
            // Reapply this fixture's hold so the elevated pawn cannot fall out of world.
            Movement->StopMovementImmediately(); Movement->DisableMovement();
            UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Repossess pawn=%llu resumedMode=%d heldMode=%d"), NetId(PC->GetPawn()), ResumedMode, int32(Movement->MovementMode));
            if (!DestroySource(First)) return false;
        }
        if (Phase == 9 && !DestroySource(Second)) return false;
        UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Begin phase=%d time=%.3f ownerPawn=%llu"), Phase, PhaseStarted, NetId(State.Owner->GetPawn()));
        for (auto Weak : Humans) Weak->Client_RunShrineAuraProbe(State.Owner.Get(), State.Ally.Get(), State.Enemy.Get(), State.Outside.Get(), Phase);
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
                for (auto It = W->GetPlayerControllerIterator(); It; ++It) if (auto* PC = Cast<ABwayPlayerController>(It->Get()))
                    if (PC->GetPlayerState<ABwayPlayerState>()) { PCs.Add(PC); if (!PC->IsLocalController() && PC->GetNetConnection()) ++Remotes; }
                if (PCs.Num() != 4 || Remotes != 3) continue;
                if (!Selected && G->HeroSelectionManager && G->HeroSelectionManager->IsSelectionActive())
                {
                    const TCHAR* Paths[] = {
                        TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus.DA_BW_HeroData_Argus"),
                        TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger.DA_BW_HeroData_Gunslinger"),
                        TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona.DA_BW_HeroData_Alona"),
                        TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver.DA_BW_HeroData_Hexweaver")};
                    for (int32 I = 0; I < 4; ++I)
                    {
                        const auto* Hero = LoadObject<UBwayHeroDataAsset>(nullptr, Paths[I]); if (!Hero) return Stop(TEXT("selection"));
                        PCs[I]->Server_SubmitPreSelectedHero(Hero->GetPrimaryAssetId(), true);
                        auto* PS = PCs[I]->GetPlayerState<ABwayPlayerState>();
                        if (!PS->IsHeroLocked() || PS->GetSelectedHeroId() != Hero->GetPrimaryAssetId()) return Stop(TEXT("selection-rejected"));
                    }
                    Selected = true;
                }
                if (!Selected || R->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || G->PlayerArray.Num() != 8) continue;
                bool Ready = true;
                for (const auto& P : G->PlayerArray)
                { auto* PS = Cast<ABwayPlayerState>(P); auto* ASC = PS ? PS->GetLyraAbilitySystemComponent() : nullptr; Ready &= PS && Alive(PS) && ASC && ASC->GetAvatarActor() == PS->GetPawn(); }
                if (!Ready) continue;
                GS = G; RoundNumber = R->GetCurrentRoundNumber();
                if (G->BuildableRegistryComponent->GetAllBuildables().Num() != 0) return Stop(TEXT("preexisting-buildable"));
                State.Owner = PCs[2]->GetPlayerState<ABwayPlayerState>();
                for (auto* PC : PCs) Humans.Add(PC);
                // Choose actual current teams rather than depending on join-order team assignment.
                for (const auto& P : G->PlayerArray) if (auto* PS = Cast<ABwayPlayerState>(P))
                {
                    if (PS == State.Owner.Get()) continue;
                    if (PS->GetGenericTeamId() == State.Owner->GetGenericTeamId())
                    { if (!State.Ally.IsValid()) State.Ally = PS; else if (!State.Outside.IsValid()) State.Outside = PS; }
                    else if (!State.Enemy.IsValid()) State.Enemy = PS;
                }
                if (!State.Ally.IsValid() || !State.Enemy.IsValid() || !State.Outside.IsValid()) return Stop(TEXT("teams"));
                for (const auto& P : G->PlayerArray) ArenaZ = FMath::Max(ArenaZ, float(P->GetPawn()->GetActorLocation().Z + 10000));
                for (const auto& P : G->PlayerArray) Hold(P->GetPawn());
                int32 I = 0; for (auto Weak : {State.Ally, State.Enemy, State.Outside}) OriginalHealth[I++] = Health(Weak.Get());
                AuditCvar = IConsoleManager::Get().FindConsoleVariable(TEXT("bway.Debug.ShrineHealing")); if (!AuditCvar) return Stop(TEXT("audit-cvar"));
                OldAuditCvar = AuditCvar->GetInt(); AuditCvar->Set(1, ECVF_SetByCode);
                UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Setup humans=4 remotes=3 held=%d round=%d owner=%d ally=%d enemy=%d outside=%d"), Held.Num(), RoundNumber, State.Owner->GetPlayerId(), State.Ally->GetPlayerId(), State.Enemy->GetPlayerId(), State.Outside->GetPlayerId());
                return BeginPhase() ? true : Stop(TEXT("begin"));
            }
            return true;
        }
        auto* R = GS->GetRoundManagement();
        if (R->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || R->GetCurrentRoundNumber() != RoundNumber) return Stop(TEXT("phase-changed"));
        if (Phase >= 3 && State.Owner->GetPawn() && State.Owner->GetPawn() != OriginalOwnerPawn.Get() && Alive(State.Owner.Get()))
        {
            Hold(State.Owner->GetPawn());
            if (!ReplacementSeen) UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Respawn pawn=%llu deaths=%d"), NetId(State.Owner->GetPawn()), State.Owner->GetDeaths());
            ReplacementSeen = true;
        }
        State.Log(Humans[0].Get(), Phase, TEXT("authority"));
        const double Age = GS->GetWorld()->GetTimeSeconds() - PhaseStarted;
        if (!Settling && Age >= 3.2)
        {
            FinalHealth = Health(State.Ally.Get()); Move(State.Ally.Get(), FVector(1500,0,ArenaZ)); Settling = true;
            UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: WindowEnd phase=%d time=%.3f health=%.3f"), Phase, GS->GetWorld()->GetTimeSeconds(), FinalHealth);
        }
        if (Age < 5.2 || (Phase == 3 && !ReplacementSeen)) return true;
        const bool Healing = (Phase >= 1 && Phase <= 6) || Phase == 8;
        const float Delta = FinalHealth - 100;
        const bool Passed = FMath::IsNearlyEqual(Health(State.Ally.Get()), FinalHealth) && FMath::IsNearlyEqual(Health(State.Enemy.Get()), 100)
            && FMath::IsNearlyEqual(Health(State.Outside.Get()), 100) && (Healing ? Delta >= 105 && Delta <= 140 : FMath::IsNearlyZero(Delta))
            && (Phase < 3 || State.Owner->GetDeaths() == DeathsBefore + 1);
        UE_LOG(LogTemp, Display, TEXT("BwayShrineAura: Verify phase=%d health=%.3f delta=%.3f healing=%d replacement=%d pass=%d"), Phase, FinalHealth, Delta, Healing, ReplacementSeen, Passed);
        if (!Passed) return Stop(TEXT("verdict"));
        if (++Phase == 10) return Stop(TEXT("observed"));
        return BeginPhase() ? true : Stop(TEXT("begin"));
    }
};
FAutoConsoleCommand Command(TEXT("bway.Test.ShrineAuraAudit"), TEXT("Development four-process native Shrine healing and owner respawn fixture."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (Running) return; Running = true; auto Run = MakeShared<FRun>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), .1f);
    }));
}
#endif

void ABwayPlayerController::Client_RunShrineAuraProbe_Implementation(ABwayPlayerState* OwnerPlayer, ABwayPlayerState* Ally, ABwayPlayerState* Enemy, ABwayPlayerState* Outside, int32 Phase)
{
#if !UE_BUILD_SHIPPING
    BwayShrineAuraAudit::Observe(this, OwnerPlayer, Ally, Enemy, Outside, Phase);
#endif
}
