#include "BwayPlayerController.h"

#if !UE_BUILD_SHIPPING
#include "Abilities/BwayGameplayAbility_PlaceBuildable.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "ActorPlacementTargeting/BwayTargetActor_ActorPlacementFace.h"
#include "ActorPlacementTargeting/BwayWorldReticle_ActorVisualization.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGameplayTags.h"
#include "BwayPlayerState.h"
#include "Buildable/BuildableBase.h"
#include "Buildable/BwayBuildablePlacementLibrary.h"
#include "Buildable/BwaySiegeEngineBuildable.h"
#include "HeroSystems/BwayHeroDataAsset.h"
#include "HeroSystems/BwayHeroSelectionManager.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameState/BwayBuildableRegistryComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Combat/BwayDamageLibrary.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayPrediction.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"

namespace BwayBuildableAudit
{
bool Running = false;
TMap<TWeakObjectPtr<ABwayPlayerController>, FTSTicker::FDelegateHandle> ActionTickers;
TMap<TWeakObjectPtr<ABwayPlayerController>, FTSTicker::FDelegateHandle> ObserverTickers;

FGameplayAbilitySpec* PlacementSpec(ULyraAbilitySystemComponent* ASC)
{
    if (ASC) for (FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
        if (Spec.Ability && Spec.Ability->IsA<UBwayGameplayAbility_PlaceBuildable>()) return &Spec;
    return nullptr;
}

TArray<ABuildableActor*> Buildables(UWorld* World)
{
    TArray<ABuildableActor*> Result;
    if (World) for (TActorIterator<ABuildableActor> It(World); It; ++It)
        if (!It->IsActorBeingDestroyed()) Result.Add(*It);
    return Result;
}

uint64 NetworkId(const AActor* Actor)
{
    const auto* Driver = Actor && Actor->GetWorld() ? Actor->GetWorld()->GetNetDriver() : nullptr;
    return Driver && Driver->GuidCache ? Driver->GuidCache->GetNetGUID(Actor).ObjectId : 0;
}

bool OwnerReadyForPlacement(ABwayPlayerController* PC)
{
    auto* Player = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
    auto* ASC = Player ? Player->GetLyraAbilitySystemComponent() : nullptr;
    auto* Spec = PlacementSpec(ASC);
    auto* GS = PC && PC->GetWorld() ? PC->GetWorld()->GetGameState<ABwayGameState>() : nullptr;
    auto* Round = GS ? GS->GetRoundManagement() : nullptr;
    return Round && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && PC->GetPawn()
        && ASC && ASC->AbilityActorInfo.IsValid() && ASC->AbilityActorInfo->AvatarActor.Get() == PC->GetPawn()
        && Spec && Spec->Ability && !Spec->IsActive()
        && Spec->Ability->CanActivateAbility(Spec->Handle, ASC->AbilityActorInfo.Get());
}

bool OwnerReadyForCapProbe(ABwayPlayerController* PC)
{
    auto* P = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
    auto* ASC = P ? P->GetLyraAbilitySystemComponent() : nullptr;
    auto* GS = PC && PC->GetWorld() ? PC->GetWorld()->GetGameState<ABwayGameState>() : nullptr;
    auto* Round = GS ? GS->GetRoundManagement() : nullptr;
    return P && !P->HasPlacedBuildableThisRound() && Round && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing
        && PC->GetPawn() && ASC && ASC->AbilityActorInfo.IsValid() && ASC->AbilityActorInfo->AvatarActor.Get() == PC->GetPawn() && PlacementSpec(ASC);
}

void Sample(UWorld* World, ABwayPlayerState* Player, int32 Sequence, const TCHAR* Side)
{
    auto* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
    auto* Rounds = GS ? GS->GetRoundManagement() : nullptr;
    if (!Player || !Rounds) return;
    ULyraAbilitySystemComponent* ASC = Player->GetLyraAbilitySystemComponent();
    FGameplayAbilitySpec* Spec = PlacementSpec(ASC);
    const auto* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
    const auto Actors = Buildables(World);
    int32 Targets = 0, Reticles = 0;
    for (TActorIterator<ABwayTargetActor_ActorPlacementFace> It(World); It; ++It) if (!It->IsActorBeingDestroyed()) ++Targets;
    for (TActorIterator<ABwayWorldReticle_ActorVisualization> It(World); It; ++It) if (!It->IsActorBeingDestroyed()) ++Reticles;
    const auto* PC = Cast<APlayerController>(Player->GetOwner());
    UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Sample side=%s sequence=%d world=%s net=%d id=%d owner=%d round=%d phase=%d count=%d quota=%d spec=%d active=%d tag=%d confirm=%d cancel=%d targets=%d reticles=%d gold=%.3f serverTime=%.3f"),
        Side, Sequence, *World->GetPathName(), int32(World->GetNetMode()), Player->GetPlayerId(), PC && PC->IsLocalController(),
        Rounds->GetCurrentRoundNumber(), int32(Rounds->GetCurrentMatchPhase()), Actors.Num(), Player->HasPlacedBuildableThisRound(),
        Spec != nullptr, Spec && Spec->IsActive(), ASC && ASC->HasMatchingGameplayTag(BwayGameplayTags::State_BuildablePlacement),
        ASC && ASC->GenericLocalConfirmCallbacks.IsBound(), ASC && ASC->GenericLocalCancelCallbacks.IsBound(), Targets, Reticles,
        Gold ? Gold->GetCurrentGold() : -1, GS->GetServerWorldTimeSeconds());
    UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Registry side=%s sequence=%d id=%d count=%d"), Side, Sequence, Player->GetPlayerId(), GS->BuildableRegistryComponent ? GS->BuildableRegistryComponent->GetBuildableCountForPlayer(Player) : -1);
    TArray<FString> CapturedIds;
    for (APlayerState* P : GS->PlayerArray)
        if (auto* PS = Cast<ABwayPlayerState>(P); PS && PS->GetLyraAbilitySystemComponent()->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Jailed))
            CapturedIds.Add(FString::FromInt(PS->GetPlayerId()));
    CapturedIds.Sort();
    UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: JailState side=%s sequence=%d captured=%s"), Side, Sequence, CapturedIds.IsEmpty() ? TEXT("None") : *FString::Join(CapturedIds, TEXT(",")));
    for (const ABuildableActor* Actor : Actors)
    {
        const FBox Bounds = Actor->GetPlacementLocalBounds();
        const FVector Center = Bounds.GetCenter(), Extent = Bounds.GetExtent(), Position = Actor->GetActorLocation();
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Actor side=%s sequence=%d world=%s round=%d name=%s netId=%llu class=%s team=%d mesh=%s cx=%.1f cy=%.1f cz=%.1f ex=%.1f ey=%.1f ez=%.1f x=%.3f y=%.3f z=%.3f"),
            Side, Sequence, *World->GetPathName(), Rounds->GetCurrentRoundNumber(), *Actor->GetName(), NetworkId(Actor), *Actor->GetClass()->GetName(),
            int32(Actor->GetTeamId().GetId()), *GetNameSafe(Actor->GetMesh()->GetSkeletalMeshAsset()),
            Center.X, Center.Y, Center.Z, Extent.X, Extent.Y, Extent.Z, Position.X, Position.Y, Position.Z);
        const auto* Health = ULyraHealthComponent::FindHealthComponent(Actor);
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Lifetime side=%s sequence=%d netId=%llu placer=%d health=%.3f maxHealth=%.3f parent=%llu damageable=%d"),
            Side, Sequence, NetworkId(Actor), Actor->GetPlacingPlayerState() ? Actor->GetPlacingPlayerState()->GetPlayerId() : -1,
            Health ? Health->GetHealth() : -1.f, Health ? Health->GetMaxHealth() : -1.f, NetworkId(Actor->GetAttachParentActor()), Actor->CanBeDamaged());
    }
}

void RunAction(ABwayPlayerController* PC, int32 Action, int32 Sequence)
{
    if (const auto* Existing = ActionTickers.Find(PC)) { FTSTicker::GetCoreTicker().RemoveTicker(*Existing); ActionTickers.Remove(PC); }
    auto* Player = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
    auto* ASC = Player ? Player->GetLyraAbilitySystemComponent() : nullptr;
    FGameplayAbilitySpec* Spec = PlacementSpec(ASC);
    if (Action == -1) { if (ASC && Spec && Spec->IsActive()) ASC->LocalInputCancel(); return; }
    if (!PC || !PC->IsLocalController() || PC->GetNetMode() != NM_Client)
    {
        UE_LOG(LogTemp, Error, TEXT("BwayBuildableAudit: ClientSetupFail sequence=%d action=%d"), Sequence, Action);
        return;
    }
    // The RPC can precede pawn/phase/ability or cleared-quota replication. Wait for the
    // real owner prerequisites without retrying activation or relaxing a rejection probe.
    if ((Action >= 0 && Action <= 5 && !OwnerReadyForPlacement(PC)) || (Action == 8 && !OwnerReadyForCapProbe(PC)))
    {
        const TWeakObjectPtr<ABwayPlayerController> WeakController(PC);
        const double Deadline = FPlatformTime::Seconds() + 10.0;
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: AwaitOwnerReady sequence=%d"), Sequence);
        const auto Wait = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakController, Action, Sequence, Deadline](float)
        {
            auto* Controller = WeakController.Get();
            if (Action == 8 ? OwnerReadyForCapProbe(Controller) : OwnerReadyForPlacement(Controller))
            {
                ActionTickers.Remove(WeakController);
                RunAction(Controller, Action, Sequence);
                return false;
            }
            if (!Controller || FPlatformTime::Seconds() >= Deadline)
            {
                ActionTickers.Remove(WeakController);
                UE_LOG(LogTemp, Error, TEXT("BwayBuildableAudit: ClientReadyWaitFail sequence=%d"), Sequence);
                return false;
            }
            return true;
        }), .05f);
        ActionTickers.Add(PC, Wait);
        return;
    }
    if (!Spec) { UE_LOG(LogTemp, Error, TEXT("BwayBuildableAudit: ClientSetupFail sequence=%d action=%d"), Sequence, Action); return; }
    const FGameplayAbilitySpecHandle Handle = Spec->Handle;
    const bool Activated = Action == 6 ? Spec->IsActive() : ASC->TryActivateAbility(Handle, false);
    UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: ClientActivate sequence=%d action=%d id=%d activated=%d quota=%d"),
        Sequence, Action, Player->GetPlayerId(), Activated, Player->HasPlacedBuildableThisRound());
    if (Action == 7 || Action == 8 || !Activated || Action == 5) return;
    const TWeakObjectPtr<ULyraAbilitySystemComponent> WeakASC(ASC);
    const TWeakObjectPtr<ABwayPlayerController> WeakPC(PC);
    const double Due = FPlatformTime::Seconds() + 1.0;
    const auto Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakASC, WeakPC, Handle, Action, Sequence, Due](float)
    {
        if (FPlatformTime::Seconds() < Due) return true;
        ActionTickers.Remove(WeakPC);
        ULyraAbilitySystemComponent* Current = WeakASC.Get();
        FGameplayAbilitySpec* CurrentSpec = Current ? Current->FindAbilitySpecFromHandle(Handle) : nullptr;
        auto* Ability = CurrentSpec ? Cast<UBwayGameplayAbility_PlaceBuildable>(CurrentSpec->GetPrimaryInstance()) : nullptr;
        if (!Current || !CurrentSpec || !CurrentSpec->IsActive() || !Ability)
        {
            UE_LOG(LogTemp, Error, TEXT("BwayBuildableAudit: ClientLostAbility sequence=%d"), Sequence);
            return false;
        }
        if (Action == 0)
        {
            Current->LocalInputCancel();
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: ClientCancel sequence=%d"), Sequence);
            return false;
        }
        FHitResult Hit;
        if (!Ability->FindDevelopmentGroundTarget(Hit))
        {
            Current->LocalInputCancel();
            UE_LOG(LogTemp, Error, TEXT("BwayBuildableAudit: ClientNoGround sequence=%d"), Sequence);
            return false;
        }
        if (Action == 1) { Hit.Location.X += 10000; Hit.ImpactPoint = Hit.Location; }
        if (Action == 2) { Hit.Location.Z += 500; Hit.ImpactPoint = Hit.Location; }
        if (Action == 3) Hit.ImpactNormal = FVector::ForwardVector;
        const FGameplayAbilityTargetDataHandle Data(new FGameplayAbilityTargetData_SingleTargetHit(Hit));
        FScopedPredictionWindow Prediction(Current, true);
        Current->CallServerSetReplicatedTargetData(Handle, Ability->GetCurrentActivationInfo().GetActivationPredictionKey(), Data, FGameplayTag(), Current->ScopedPredictionKey);
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: ClientSubmit sequence=%d action=%d blocking=%d x=%.3f y=%.3f z=%.3f"),
            Sequence, Action, Hit.bBlockingHit, Hit.Location.X, Hit.Location.Y, Hit.Location.Z);
        return false;
    }), 0.05f);
    ActionTickers.Add(PC, Ticker);
}

void RunObserver(ABwayPlayerController* PC, int32 Id)
{
    if (!PC || !PC->IsLocalController() || PC->GetNetMode() != NM_Client) return;
    if (const auto* Existing = ObserverTickers.Find(PC)) { FTSTicker::GetCoreTicker().RemoveTicker(*Existing); ObserverTickers.Remove(PC); }
    if (Id < 0)
    {
        // Packaged clients exit only after authority has finished its cleanup, not when
        // the faster local observer completes while the host still needs this player.
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: PeerReleased local=%d"), PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1);
        return;
    }
    struct FState { TWeakObjectPtr<ABwayPlayerController> PC; double Started = FPlatformTime::Seconds(), ClearAt = 0; int32 Sample = 0, InitialRound = 0, MaxCount = 0; bool RoundReset = false; };
    auto State = MakeShared<FState>(); State->PC = PC;
    const auto Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State, Id](float)
    {
        ABwayPlayerController* Controller = State->PC.Get();
        UWorld* World = Controller ? Controller->GetWorld() : nullptr;
        auto* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
        auto* Round = GS ? GS->GetRoundManagement() : nullptr;
        if (!Controller || FPlatformTime::Seconds() - State->Started > 300)
        {
            UE_LOG(LogTemp, Error, TEXT("BwayBuildableAudit: PeerFail reason=timeout-or-world id=%d"), Id);
            ObserverTickers.Remove(State->PC);
            return false;
        }
        ABwayPlayerState* Player = nullptr;
        if (GS) for (APlayerState* P : GS->PlayerArray) if (P && P->GetPlayerId() == Id) { Player = Cast<ABwayPlayerState>(P); break; }
        if (!Player || !Round) return true;
        if (State->InitialRound == 0) State->InitialRound = Round->GetCurrentRoundNumber();
        ++State->Sample;
        Sample(World, Player, State->Sample, TEXT("peer"));
        const int32 Count = Buildables(World).Num();
        State->MaxCount = FMath::Max(State->MaxCount, Count);
        State->RoundReset |= Round->GetCurrentRoundNumber() > State->InitialRound && Count == 1 && !Player->HasPlacedBuildableThisRound();
        if (State->MaxCount >= 2 && State->RoundReset && Count == 0 && !Player->HasPlacedBuildableThisRound())
        {
            if (State->ClearAt == 0) State->ClearAt = FPlatformTime::Seconds();
            if (FPlatformTime::Seconds() - State->ClearAt >= 2)
            {
                UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: PeerComplete reason=observed local=%d id=%d samples=%d maxCount=%d roundReset=%d"),
                    Controller->PlayerState ? Controller->PlayerState->GetPlayerId() : -1, Id, State->Sample, State->MaxCount, State->RoundReset);
                ObserverTickers.Remove(State->PC);
                return false;
            }
        }
        else State->ClearAt = 0;
        return true;
    }), 0.2f);
    ObserverTickers.Add(PC, Ticker);
}

struct FHeld
{
    TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
    EMovementMode Mode;
    uint8 Custom;
    TWeakObjectPtr<UBrainComponent> Brain;
    bool WasRunning;
};

struct FRun
{
    TWeakObjectPtr<ABwayGameState> GS;
    TWeakObjectPtr<ABwayPlayerController> PC;
    TWeakObjectPtr<ABwayPlayerState> Player;
    TArray<FHeld> Held;
    TArray<TWeakObjectPtr<ABuildableActor>> Owned;
    double Started = FPlatformTime::Seconds(), StageStarted = Started, NextSample = 0;
    FString ExpectedBuildableClass;
    int32 ExpectedActiveCap = 0;
    int32 Stage = 0, InitialRound = 0;
    bool InjectedQuota = false;
    bool SawCancelActive = false;
    double QuotaInjectedAt = 0;
    bool StartedFixture = false;
    float GoldBefore = 0;
    bool Accumulation = false;
    FString RequiredClass;
    int32 SelectedOwnerId = -1;
    TWeakObjectPtr<ABuildableActor> ActiveCage;
    TArray<TWeakObjectPtr<ABuildableActor>> CapturedCages;
    TWeakObjectPtr<ABwayCharacterWithAbilities> MovedVictim;
    FTransform VictimOriginalTransform;
    uint64 ConsumedTrapId = 0;
    int32 RestoredVictimMoves = 0;

    bool IsJail() const { return Accumulation && RequiredClass == TEXT("Gunslinger"); }

    ABwayPlayerState* EnemyPlayer() const
    {
        for (APlayerState* P : GS->PlayerArray)
            if (auto* Candidate = Cast<ABwayPlayerState>(P); Candidate && Candidate->GetPawn()
                && Candidate->GetGenericTeamId() != Player->GetGenericTeamId()) return Candidate;
        return nullptr;
    }

    bool RestoreVictim()
    {
        auto* Victim = MovedVictim.Get();
        if (!Victim) return false;
        auto* PS = Victim->GetPlayerState<ABwayPlayerState>();
        if (!PS || PS->GetLyraAbilitySystemComponent()->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Jailed)) return false;
        Victim->SetActorTransform(VictimOriginalTransform, false, nullptr, ETeleportType::TeleportPhysics);
        Victim->ForceNetUpdate();
        if (!Victim->GetActorTransform().Equals(VictimOriginalTransform, .1f)) return false;
        MovedVictim.Reset(); ++RestoredVictimMoves;
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: VictimRestored sequence=%d id=%d moves=%d"), Stage, PS->GetPlayerId(), RestoredVictimMoves);
        return true;
    }

    bool ArmedImmunity(ABuildableActor* Trap)
    {
        auto* Enemy = EnemyPlayer();
        const auto* Health = ULyraHealthComponent::FindHealthComponent(Trap);
        if (!Trap || !Enemy || !Health) return false;
        const float Before = Health->GetHealth();
        const bool Applied = UBwayDamageLibrary::ApplyDamageFromSource(Enemy->GetLyraAbilitySystemComponent(), Trap, 30.f, Enemy->GetPawn(), Enemy->GetPawn());
        const bool Safe = !Applied && !Trap->CanBeDamaged() && !Trap->IsActorBeingDestroyed() && Health->GetHealth() == Before && Before > 0;
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: ArmedImmunity sequence=%d netId=%llu applied=%d before=%.3f after=%.3f pass=%d"), Stage, NetworkId(Trap), Applied, Before, Health->GetHealth(), Safe);
        return Safe;
    }

    bool TriggerTrap(ABuildableActor* Trap)
    {
        auto* Enemy = EnemyPlayer();
        auto* Victim = Enemy ? Cast<ABwayCharacterWithAbilities>(Enemy->GetPawn()) : nullptr;
        const auto* Sphere = Trap ? Trap->FindComponentByClass<USphereComponent>() : nullptr;
        const auto* CaptureProperty = Trap ? FindFProperty<FFloatProperty>(Trap->GetClass(), TEXT("CaptureRadius")) : nullptr;
        if (!Trap || !Victim || !Sphere || !CaptureProperty || MovedVictim.IsValid()) return false;
        const float Radius = Sphere->GetScaledSphereRadius();
        const float CaptureRadius = CaptureProperty->GetPropertyValue_InContainer(Trap);
        const float CapsuleRadius = Victim->GetCapsuleComponent()->GetScaledCapsuleRadius();
        const float HalfHeight = Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        // Prove one isolated real overlap before moving a pawn. Never shrink trigger/capture
        // volumes, manufacture cages, or make the armed trap damageable for this fixture.
        for (APlayerState* P : GS->PlayerArray)
            if (auto* Other = Cast<ABwayPlayerState>(P); Other && Other != Enemy && Other->GetPawn()
                && Other->GetGenericTeamId() != Player->GetGenericTeamId()
                && FVector::Dist(Other->GetPawn()->GetActorLocation(), Trap->GetActorLocation()) < CaptureRadius + 200.f) return false;
        FVector Target; bool Found = false;
        for (int32 Index = 0; Index < 72 && !Found; ++Index)
        {
            const float Angle = 2.f * PI * float(Index) / 72.f;
            Target = Sphere->GetComponentLocation() + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * FMath::Max(10.f, Radius - CapsuleRadius - 12.f);
            Target.Z = Trap->GetActorLocation().Z + HalfHeight + 2.f;
            Found = true;
            for (ABuildableActor* Other : Buildables(GS->GetWorld()))
                if (Other != Trap && Other->GetClass() == Trap->GetClass())
                    if (const auto* OtherSphere = Other->FindComponentByClass<USphereComponent>(); OtherSphere
                        && FVector::Dist2D(Target, OtherSphere->GetComponentLocation()) <= OtherSphere->GetScaledSphereRadius() + CapsuleRadius + 12.f) { Found = false; break; }
        }
        if (!Found) { UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: TriggerSetupBlocked reason=no-isolated-point")); return false; }
        ConsumedTrapId = NetworkId(Trap);
        const TWeakObjectPtr<ABuildableActor> WeakTrap(Trap);
        MovedVictim = Victim; VictimOriginalTransform = Victim->GetActorTransform();
        Victim->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics); Victim->ForceNetUpdate();
        TArray<ABuildableActor*> Cages;
        for (ABuildableActor* Actor : Buildables(GS->GetWorld()))
            if (Actor->GetAttachParentActor() == Victim) { Cages.Add(Actor); CapturedCages.AddUnique(Actor); }
        if ((WeakTrap.IsValid() && !WeakTrap->IsActorBeingDestroyed()) || Cages.Num() != 1) return false;
        ActiveCage = Cages[0];
        if (ActiveCage->GetPlacingPlayerState() || !Enemy->GetLyraAbilitySystemComponent()->HasMatchingGameplayTag(BwayGameplayTags::State_Status_Jailed)) return false;
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Trigger sequence=%d trap=%llu victim=%d worldCount=%d placerCount=%d quota=%d capAllows=%d"),
            Stage, ConsumedTrapId, Enemy->GetPlayerId(), Buildables(GS->GetWorld()).Num(), GS->BuildableRegistryComponent->GetBuildableCountForPlayer(Player.Get()), Player->HasPlacedBuildableThisRound(), PlacementAllowed());
        return true;
    }

    bool DamageOwned(ABuildableActor* Actor, float Amount)
    {
        ABwayPlayerState* Enemy = EnemyPlayer();
        auto* Health = ULyraHealthComponent::FindHealthComponent(Actor);
        if (!Enemy || !Actor || !Health) return false;
        const uint64 Id = NetworkId(Actor);
        const float Before = Health->GetHealth();
        const TWeakObjectPtr<ABuildableActor> WeakActor(Actor);
        const bool Applied = UBwayDamageLibrary::ApplyDamageFromSource(Enemy->GetLyraAbilitySystemComponent(), Actor, Amount, Enemy->GetPawn(), Enemy->GetPawn());
        const bool Gone = !WeakActor.IsValid() || WeakActor->IsActorBeingDestroyed();
        const float After = Gone ? 0.f : Health->GetHealth();
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Damage sequence=%d netId=%llu applied=%d before=%.3f after=%.3f gone=%d"), Stage, Id, Applied, Before, After, Gone);
        return Applied && (Amount >= Before ? Gone : !Gone && After < Before && After > 0.f);
    }

    bool PlacementAllowed() const
    {
        const auto* Definition = UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(PC->GetPawn(), PC.Get());
        FText Failure;
        return UBwayBuildablePlacementLibrary::CanPlayerPlaceBuildable(PC->GetPawn(), PC.Get(), Definition, Failure);
    }

    void Hold(APawn* Pawn)
    {
        auto* Character = Cast<ABwayCharacterWithAbilities>(Pawn);
        if (!Character || Held.ContainsByPredicate([Character](const FHeld& H) { return H.Pawn == Character; })) return;
        auto* Move = Character->GetCharacterMovement();
        auto* AI = Cast<AAIController>(Character->GetController());
        UBrainComponent* Brain = AI ? AI->GetBrainComponent() : nullptr;
        Held.Add({Character, Move->MovementMode, Move->CustomMovementMode, Brain, Brain && Brain->IsRunning()});
        Move->StopMovementImmediately(); Move->DisableMovement();
        if (Brain) Brain->StopLogic(TEXT("Owned buildable audit"));
    }

    bool Stop(const TCHAR* Reason)
    {
        int32 Destroyed = 0, Restored = 0, Expired = 0;
        for (auto& Cage : CapturedCages) if (Cage.IsValid()) Cage->Destroy();
        if (MovedVictim.IsValid()) RestoreVictim();
        for (auto& Actor : Owned) if (Actor.IsValid()) { Actor->Destroy(); ++Destroyed; }
        if (StartedFixture && Player.IsValid())
        {
            auto* ASC = Player->GetLyraAbilitySystemComponent();
            if (auto* Spec = PlacementSpec(ASC); Spec && Spec->IsActive()) ASC->CancelAbilityHandle(Spec->Handle);
            Player->BeginRoundStatTracking();
        }
        if (PC.IsValid()) PC->Client_RunBuildableAuditAction(-1, Stage);
        if (GS.IsValid()) for (auto It = GS->GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (auto* Remote = Cast<ABwayPlayerController>(It->Get()); Remote && !Remote->IsLocalController()) Remote->Client_RunBuildableAuditObserver(-1);
        for (const FHeld& H : Held)
        {
            if (!H.Pawn.IsValid()) { ++Expired; continue; }
            H.Pawn->GetCharacterMovement()->SetMovementMode(H.Mode, H.Custom); ++Restored;
            if (H.WasRunning && H.Brain.IsValid()) H.Brain->RestartLogic();
        }
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Complete reason=%s stage=%d destroyed=%d restored=%d expired=%d held=%d restoredVictimMoves=%d pendingMoved=%d cages=%d"), Reason, Stage, Destroyed, Restored, Expired, Held.Num(), RestoredVictimMoves, MovedVictim.IsValid(), CapturedCages.Num());
        Running = false;
        return false;
    }

    void Dispatch(int32 Action, int32 NewStage)
    {
        Stage = NewStage; StageStarted = FPlatformTime::Seconds();
        const auto* Gold = Player->GetLyraAbilitySystemComponent()->GetSet<UBwayGoldAttributeSet>();
        GoldBefore = Gold ? Gold->GetCurrentGold() : -1;
        UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Dispatch sequence=%d action=%d id=%d gold=%.3f"), Stage, Action, Player->GetPlayerId(), GoldBefore);
        PC->Client_RunBuildableAuditAction(Action, Stage);
    }

    bool Clean(int32 ExpectedCount, bool ExpectedQuota)
    {
        auto* ASC = Player->GetLyraAbilitySystemComponent();
        auto* Spec = PlacementSpec(ASC);
        const auto* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
        return Spec && !Spec->IsActive() && !ASC->HasMatchingGameplayTag(BwayGameplayTags::State_BuildablePlacement)
            && !ASC->GenericLocalConfirmCallbacks.IsBound() && !ASC->GenericLocalCancelCallbacks.IsBound()
            && Buildables(GS->GetWorld()).Num() == ExpectedCount && Player->HasPlacedBuildableThisRound() == ExpectedQuota
            && Gold && Gold->GetCurrentGold() >= GoldBefore;
    }

    bool Tick(float)
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > (Accumulation ? 360 : 240)) return Stop(TEXT("timeout"));
        if (Stage == 0)
        {
            if (GEngine) for (const auto& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client) continue;
                auto* State = World->GetGameState<ABwayGameState>();
                auto* Round = State ? State->GetRoundManagement() : nullptr;
                if (Accumulation && !RequiredClass.IsEmpty() && Round && SelectedOwnerId < 0
                    && State->HeroSelectionManager && State->HeroSelectionManager->IsSelectionActive())
                {
                    TArray<ABwayPlayerController*> Remotes;
                    for (auto It = World->GetPlayerControllerIterator(); It; ++It)
                        if (auto* Remote = Cast<ABwayPlayerController>(It->Get()); Remote && !Remote->IsLocalController() && Remote->GetNetConnection() && Remote->PlayerState)
                            Remotes.Add(Remote);
                    if (Remotes.Num() != 3) continue;
                    Remotes.Sort([](const auto& A, const auto& B) { return A.PlayerState->GetPlayerId() < B.PlayerState->GetPlayerId(); });
                    const TCHAR* HeroPath = RequiredClass == TEXT("SunShrine") ? TEXT("/Hero_Alona/Characters/Heroes/Alona/DA_BW_HeroData_Alona.DA_BW_HeroData_Alona") :
                        RequiredClass == TEXT("Gunslinger") ? TEXT("/Hero_Rawlins/Characters/Heroes/Gunslinger/DA_BW_HeroData_Gunslinger.DA_BW_HeroData_Gunslinger") :
                        RequiredClass == TEXT("Hexweaver") ? TEXT("/Hero_Morgan/Characters/Heroes/Hexweaver/DA_BW_HeroData_Hexweaver.DA_BW_HeroData_Hexweaver") : nullptr;
                    const auto* Hero = HeroPath ? LoadObject<UBwayHeroDataAsset>(nullptr, HeroPath) : nullptr;
                    auto* Candidate = Remotes[0]; auto* P = Candidate->GetPlayerState<ABwayPlayerState>();
                    if (!Hero || !P || P->IsHeroLocked()) return Stop(TEXT("accumulation-selection-setup"));
                    Candidate->Server_SubmitPreSelectedHero(Hero->GetPrimaryAssetId(), true);
                    if (!P->IsHeroLocked() || P->GetSelectedHeroId() != Hero->GetPrimaryAssetId()) return Stop(TEXT("accumulation-selection-rejected"));
                    SelectedOwnerId = P->GetPlayerId();
                    UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Selection id=%d hero=%s source=production-selection-request"), SelectedOwnerId, *Hero->GetPrimaryAssetId().ToString());
                }
                if (!Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || State->PlayerArray.Num() != 8) continue;
                if (Accumulation && !RequiredClass.IsEmpty() && SelectedOwnerId < 0) return Stop(TEXT("accumulation-selection-window-missed"));
                if (State->PlayerArray.ContainsByPredicate([](const APlayerState* P) { return !P || !P->GetPawn(); })) continue;
                int32 Humans = 0, RemoteHumans = 0;
                for (auto It = World->GetPlayerControllerIterator(); It; ++It)
                    if (auto* Human = Cast<ABwayPlayerController>(It->Get()))
                    { ++Humans; if (!Human->IsLocalController() && Human->GetNetConnection()) ++RemoteHumans; }
                if (Humans != 4 || RemoteHumans != 3) continue;
                PC.Reset(); Player.Reset();
                for (APlayerState* P : State->PlayerArray)
                {
                    auto* Candidate = Cast<ABwayPlayerController>(P ? P->GetOwner() : nullptr);
                    if (Candidate && !Candidate->IsLocalController() && Candidate->GetNetConnection() && P->GetPawn())
                    {
                        if (SelectedOwnerId >= 0 && P->GetPlayerId() != SelectedOwnerId) continue;
                        const auto* Definition = UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(P->GetPawn(), Candidate);
                        const auto* Defaults = Definition && Definition->BuildableActorClass ? Definition->BuildableActorClass.GetDefaultObject() : nullptr;
                        // This gate tests two surviving structures. One-use/cap-one definitions
                        // require their own lifetime/cap gate, without modifying their behavior.
                        if (!Defaults || !Defaults->ShouldPersistBetweenRounds() || Definition->MaxActiveBuildablesPerPlayer == 1) continue;
                        if (Accumulation && (Definition->MaxActiveBuildablesPerPlayer != 3 || !Defaults->GetClass()->GetName().Contains(RequiredClass))) continue;
                        if (Player.IsValid() && Player->GetPlayerId() <= P->GetPlayerId()) continue;
                        PC = Candidate; Player = Cast<ABwayPlayerState>(P);
                        ExpectedBuildableClass = Definition->BuildableActorClass->GetName();
                        ExpectedActiveCap = Definition->MaxActiveBuildablesPerPlayer;
                    }
                }
                if (!PC.IsValid() || !Player.IsValid() || !PlacementSpec(Player->GetLyraAbilitySystemComponent())) continue;
                if (!Buildables(World).IsEmpty() || Player->HasPlacedBuildableThisRound()) return Stop(TEXT("preexisting-placement"));
                GS = State; InitialRound = Round->GetCurrentRoundNumber(); StartedFixture = true;
                for (APlayerState* P : State->PlayerArray) if (P) Hold(P->GetPawn());
                for (auto It = World->GetPlayerControllerIterator(); It; ++It)
                    if (auto* Remote = Cast<ABwayPlayerController>(It->Get()); Remote && !Remote->IsLocalController())
                        Remote->Client_RunBuildableAuditObserver(Player->GetPlayerId());
                UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Setup id=%d round=%d humans=%d remoteHumans=%d held=%d class=%s persists=1 maxActive=%d"), Player->GetPlayerId(), InitialRound, Humans, RemoteHumans, Held.Num(), *ExpectedBuildableClass, ExpectedActiveCap);
                if (Accumulation) UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: AccumulationSetup class=%s cap=%d"), *ExpectedBuildableClass, ExpectedActiveCap);
                Dispatch(0, 1);
                return true;
            }
            return true;
        }
        if (!GS.IsValid() || !PC.IsValid() || !Player.IsValid()) return Stop(TEXT("lost-world"));
        UWorld* World = GS->GetWorld();
        auto* Round = GS->GetRoundManagement();
        for (ABuildableActor* Actor : Buildables(World)) if (Actor->GetOwner() == PC.Get())
        {
            Owned.AddUnique(Actor);
        }
        for (APlayerState* P : GS->PlayerArray) if (P) Hold(P->GetPawn());
        if (Now >= NextSample) { NextSample = Now + .2; Sample(World, Player.Get(), Stage, TEXT("authority")); }
        const double Age = Now - StageStarted;
        if (Stage == 1)
        {
            const auto* Spec = PlacementSpec(Player->GetLyraAbilitySystemComponent());
            if (!SawCancelActive && Spec && Spec->IsActive())
            {
                SawCancelActive = true;
                UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: CancelActiveObserved sequence=1"));
            }
            if (Age >= 14) return Stop(TEXT("cancel-readiness-timeout"));
            if (Age < 4 || !SawCancelActive || !Clean(0, false)) return true;
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Verify sequence=1 clean=1 count=0 quota=0"));
            Dispatch(1, 2);
        }
        else if (Stage <= 4 && Age >= 4)
        {
            if (!Clean(0, false)) return Stop(TEXT("reject-or-cancel-cleanup"));
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Verify sequence=%d clean=1 count=0 quota=0"), Stage);
            Dispatch(Stage == 4 ? 5 : Stage, Stage + 1);
        }
        else if (Stage == 5)
        {
            if (!InjectedQuota && Age >= 1.5)
            {
                auto* Spec = PlacementSpec(Player->GetLyraAbilitySystemComponent());
                if (!Spec || !Spec->IsActive())
                {
                    if (Age >= 10) return Stop(TEXT("quota-race-no-active-ability"));
                    return true;
                }
                Player->MarkBuildablePlacedThisRound(); InjectedQuota = true;
                QuotaInjectedAt = Now;
                UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: QuotaInjected sequence=5 active=1"));
                PC->Client_RunBuildableAuditAction(6, Stage);
            }
            if (InjectedQuota && Now - QuotaInjectedAt >= 3.5)
            {
                if (!Clean(0, true)) return Stop(TEXT("quota-reject-cleanup"));
                Player->BeginRoundStatTracking();
                UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Verify sequence=5 clean=1 count=0 quota=1"));
                Dispatch(4, 6);
            }
        }
        else if (Stage == 6 && Age >= 6)
        {
            if (!Clean(1, true)) return Stop(TEXT("valid-placement"));
            ABuildableActor* Actor = Buildables(World)[0]; Owned.AddUnique(Actor);
            Actor->ConfigurePlacementBounds(FVector(7,3,5), FVector(45,55,40));
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: NondefaultBounds name=%s netId=%llu"), *Actor->GetName(), NetworkId(Actor));
            Dispatch(7, 7);
        }
        else if (Stage == 7 && Age >= 4)
        {
            if (!Clean(1, true)) return Stop(TEXT("repeat-quota"));
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: RoundEnd source=production count=1"));
            Round->EndRound(0, EBwayWinCondition::GoalScored);
            Stage = 8; StageStarted = Now;
        }
        else if (Stage == 8 && Round->GetCurrentRoundNumber() == InitialRound + 1 && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && Age >= 9)
        {
            if (!Owned[0].IsValid() || !Clean(1, false)) return Stop(TEXT("round-persistence"));
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: RoundReset source=production count=1 quota=0 first=%s"), *Owned[0]->GetName());
            Dispatch(4, 9);
        }
        else if (Stage == 9 && Age >= 4)
        {
            if (!Clean(2, true) || !Owned[0].IsValid()) return Stop(TEXT("second-placement"));
            for (ABuildableActor* Actor : Buildables(World)) Owned.AddUnique(Actor);
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Verify sequence=9 clean=1 count=2 quota=1"));
            Stage = 10; StageStarted = Now;
        }
        else if (Stage == 10 && Age >= 5)
        {
            if (Accumulation)
            {
                if (!Clean(2, true) || !Owned[0].IsValid() || !(IsJail() ? ArmedImmunity(Owned[0].Get()) : DamageOwned(Owned[0].Get(), 30.f))) return Stop(TEXT("accumulation-damage"));
                Round->EndRound(1, EBwayWinCondition::GoalScored);
                Stage = 12; StageStarted = Now;
                return true;
            }
            for (auto& Actor : Owned) if (Actor.IsValid()) Actor->Destroy();
            Player->BeginRoundStatTracking();
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Cleanup actors=%d"), Owned.Num());
            Stage = 11; StageStarted = Now;
        }
        else if (Stage == 11 && Age >= 5)
        {
            if (!Clean(0, false)) return Stop(TEXT("final-cleanup"));
            return Stop(TEXT("observed"));
        }
        else if (Stage == 12 && Round->GetCurrentRoundNumber() == InitialRound + 2 && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && Age >= 6)
        {
            if (!Clean(2, false) || !Owned[0].IsValid() || !Owned[1].IsValid()) return Stop(TEXT("third-round-persistence"));
            Dispatch(4, 13);
        }
        else if (Stage == 13 && Age >= 6)
        {
            if (!Clean(3, true)) return Stop(TEXT("third-placement"));
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Accumulated round=%d count=3 quota=1"), Round->GetCurrentRoundNumber());
            Stage = 14; StageStarted = Now;
        }
        else if (Stage == 14 && Age >= 4)
        {
            Round->EndRound(0, EBwayWinCondition::GoalScored);
            Stage = 15; StageStarted = Now;
        }
        else if (Stage == 15 && Round->GetCurrentRoundNumber() == InitialRound + 3 && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing && Age >= 6)
        {
            if (!Clean(3, false) || PlacementAllowed()) return Stop(TEXT("active-cap-not-enforced"));
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: CapRejected round=%d count=3 quota=0"), Round->GetCurrentRoundNumber());
            Dispatch(8, 16);
        }
        else if (Stage == 16 && Age >= 4)
        {
            if (IsJail())
            {
                if (!Clean(3, false) || !Owned[0].IsValid() || !TriggerTrap(Owned[0].Get())) return Stop(TEXT("jail-first-trigger"));
                Stage = 22; StageStarted = Now;
                return true;
            }
            if (!Clean(3, false) || !Owned[0].IsValid() || !DamageOwned(Owned[0].Get(), 2000.f)) return Stop(TEXT("cap-destruction"));
            Stage = 17; StageStarted = Now;
        }
        else if (Stage == 17 && Age >= 4)
        {
            if (!Clean(2, false) || !PlacementAllowed()) return Stop(TEXT("cap-not-released"));
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: CapReleased count=2 quota=0"));
            Dispatch(4, 18);
        }
        else if (Stage == 18 && Age >= 6)
        {
            if (!Clean(3, true) || Owned.Num() != 4 || !Owned.Last().IsValid()) return Stop(TEXT("replacement-placement"));
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Replacement netId=%llu count=3 quota=1"), NetworkId(Owned.Last().Get()));
            if (!DamageOwned(Owned.Last().Get(), 2000.f)) return Stop(TEXT("replacement-destruction"));
            Stage = 19; StageStarted = Now;
        }
        else if (Stage == 19 && Age >= 4)
        {
            if (!Clean(2, true) || PlacementAllowed()) return Stop(TEXT("destruction-reset-quota"));
            Dispatch(7, 20);
        }
        else if (Stage == 20 && Age >= 4)
        {
            if (!Clean(2, true)) return Stop(TEXT("repeat-after-destruction"));
            for (auto& Actor : Owned) if (Actor.IsValid()) Actor->Destroy();
            Player->BeginRoundStatTracking();
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Cleanup actors=%d"), Owned.Num());
            Stage = 21; StageStarted = Now;
        }
        else if (Stage == 21 && Age >= 5)
        {
            if (!Clean(0, false)) return Stop(TEXT("final-cleanup"));
            return Stop(TEXT("observed"));
        }
        else if (Stage == 22 && Age >= 4)
        {
            if (!Clean(3, false) || !ActiveCage.IsValid() || GS->BuildableRegistryComponent->GetBuildableCountForPlayer(Player.Get()) != 2 || !PlacementAllowed()) return Stop(TEXT("jail-cage-used-capacity"));
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: CageCapacity sequence=%d cage=%llu trap=%llu worldCount=3 placerCount=2 quota=0 capAllows=1"), Stage, NetworkId(ActiveCage.Get()), ConsumedTrapId);
            Dispatch(4, 23);
        }
        else if (Stage == 23 && Age >= 6)
        {
            if (!Clean(4, true) || Owned.Num() != 4 || !Owned.Last().IsValid() || !ActiveCage.IsValid()) return Stop(TEXT("jail-replacement-with-cage"));
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Replacement netId=%llu count=4 quota=1"), NetworkId(Owned.Last().Get()));
            if (!DamageOwned(ActiveCage.Get(), 2000.f) || !RestoreVictim()) return Stop(TEXT("jail-first-cage-release"));
            Stage = 24; StageStarted = Now;
        }
        else if (Stage == 24 && Age >= 4)
        {
            if (!Clean(3, true) || !Owned.Last().IsValid() || !TriggerTrap(Owned.Last().Get())) return Stop(TEXT("jail-second-trigger"));
            Stage = 25; StageStarted = Now;
        }
        else if (Stage == 25 && Age >= 4)
        {
            if (!Clean(3, true) || !ActiveCage.IsValid() || GS->BuildableRegistryComponent->GetBuildableCountForPlayer(Player.Get()) != 2 || PlacementAllowed()) return Stop(TEXT("jail-trigger-reset-quota"));
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: CageCapacity sequence=%d cage=%llu trap=%llu worldCount=3 placerCount=2 quota=1 capAllows=0"), Stage, NetworkId(ActiveCage.Get()), ConsumedTrapId);
            Dispatch(7, 26);
        }
        else if (Stage == 26 && Age >= 4)
        {
            if (!Clean(3, true) || !ActiveCage.IsValid() || !DamageOwned(ActiveCage.Get(), 2000.f) || !RestoreVictim()) return Stop(TEXT("jail-second-cage-release"));
            Stage = 27; StageStarted = Now;
        }
        else if (Stage == 27 && Age >= 4)
        {
            if (!Clean(2, true) || PlacementAllowed() || MovedVictim.IsValid()) return Stop(TEXT("jail-final-quota"));
            for (auto& Actor : Owned) if (Actor.IsValid()) Actor->Destroy();
            Player->BeginRoundStatTracking();
            UE_LOG(LogTemp, Display, TEXT("BwayBuildableAudit: Cleanup actors=%d cages=%d restoredVictimMoves=%d"), Owned.Num(), CapturedCages.Num(), RestoredVictimMoves);
            Stage = 21; StageStarted = Now;
        }
        return true;
    }
};

void SampleSiege(UWorld* World, ABwayPlayerState* Player, int32 Sequence, const TCHAR* Side)
{
    Sample(World, Player, Sequence, Side);
    for (ABuildableActor* Actor : Buildables(World)) if (const auto* Siege = Cast<ABwaySiegeEngineBuildable>(Actor))
    {
        UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: Roll side=%s sequence=%d world=%s netId=%llu name=%s rolling=%d elapsed=%.3f maximum=%.3f speed=%.3f persists=%d tick=%d time=%.3f"),
            Side, Sequence, *World->GetPathName(), NetworkId(Siege), *Siege->GetName(), Siege->IsRollingForDevelopment(),
            Siege->GetRollElapsedForDevelopment(), Siege->GetMaxRollDurationForDevelopment(), Siege->GetRollSpeedForDevelopment(),
            Siege->ShouldPersistBetweenRounds(), Siege->IsActorTickEnabled(), World->GetGameState()->GetServerWorldTimeSeconds());
    }
}

TMap<TWeakObjectPtr<ABwayPlayerController>, FTSTicker::FDelegateHandle> SiegeObservers;
void ObserveSiege(ABwayPlayerController* PC, int32 Id)
{
    if (!PC || !PC->IsLocalController() || PC->GetNetMode() != NM_Client) return;
    if (const auto* Existing = SiegeObservers.Find(PC)) { FTSTicker::GetCoreTicker().RemoveTicker(*Existing); SiegeObservers.Remove(PC); }
    if (Id < 0) { UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: PeerReleased local=%d"), PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1); return; }
    struct FObserver
    {
        TWeakObjectPtr<ABwayPlayerController> PC;
        double Started = FPlatformTime::Seconds(), ClearAt = 0;
        int32 InitialRound = 0, Sequence = 0, MaxCount = 0;
        TSet<uint64> Ids;
        uint64 FirstId = 0;
        bool Survived = false;
    };
    auto State = MakeShared<FObserver>(); State->PC = PC;
    const auto Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([State, Id](float)
    {
        auto* Controller = State->PC.Get();
        auto* World = Controller ? Controller->GetWorld() : nullptr;
        auto* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
        auto* Round = GS ? GS->GetRoundManagement() : nullptr;
        if (!Controller || FPlatformTime::Seconds() - State->Started > 120)
        {
            UE_LOG(LogTemp, Error, TEXT("BwaySiegeAudit: PeerFail reason=timeout-or-world id=%d"), Id);
            SiegeObservers.Remove(State->PC); return false;
        }
        ABwayPlayerState* Player = nullptr;
        if (GS) for (APlayerState* P : GS->PlayerArray) if (P && P->GetPlayerId() == Id) { Player = Cast<ABwayPlayerState>(P); break; }
        if (!Player || !Round) return true;
        if (State->InitialRound == 0) State->InitialRound = Round->GetCurrentRoundNumber();
        SampleSiege(World, Player, ++State->Sequence, TEXT("peer"));
        const auto Actors = Buildables(World); State->MaxCount = FMath::Max(State->MaxCount, Actors.Num());
        for (ABuildableActor* Actor : Actors)
        {
            const uint64 NetId = NetworkId(Actor);
            if (NetId > 1)
            {
                State->Ids.Add(NetId); if (!State->FirstId) State->FirstId = NetId;
                State->Survived |= NetId == State->FirstId && Round->GetCurrentRoundNumber() > State->InitialRound && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing;
            }
        }
        if (State->Ids.Num() == 2 && State->Survived && Actors.IsEmpty() && !Player->HasPlacedBuildableThisRound())
        {
            if (!State->ClearAt) State->ClearAt = FPlatformTime::Seconds();
            if (FPlatformTime::Seconds() - State->ClearAt >= 2)
            {
                UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: PeerComplete local=%d id=%d identities=%d maxCount=%d survived=%d samples=%d"), Controller->PlayerState->GetPlayerId(), Id, State->Ids.Num(), State->MaxCount, State->Survived, State->Sequence);
                SiegeObservers.Remove(State->PC); return false;
            }
        }
        else State->ClearAt = 0;
        return true;
    }), .1f);
    SiegeObservers.Add(PC, Ticker);
}

struct FSiegeRun
{
    FRun Fixture; // Reuse pawn isolation only; barrel state is never overridden.
    TWeakObjectPtr<ABwayGameState> GS;
    TWeakObjectPtr<ABwayPlayerController> PC;
    TWeakObjectPtr<ABwayPlayerState> Player;
    TWeakObjectPtr<ABwaySiegeEngineBuildable> Current;
    TArray<TWeakObjectPtr<ABuildableActor>> Owned;
    double Started = FPlatformTime::Seconds(), StageStarted = Started, NextSample = 0, ResetAt = 0;
    int32 SelectedId = -1, Stage = 0, InitialRound = 0;
    uint64 FirstId = 0;

    bool Stop(const TCHAR* Reason)
    {
        int32 Destroyed = 0, Restored = 0, Expired = 0;
        for (auto& Actor : Owned) if (Actor.IsValid()) { Actor->Destroy(); ++Destroyed; }
        if (Player.IsValid())
        {
            auto* ASC = Player->GetLyraAbilitySystemComponent();
            if (auto* Spec = PlacementSpec(ASC); Spec && Spec->IsActive()) ASC->CancelAbilityHandle(Spec->Handle);
            Player->BeginRoundStatTracking();
        }
        if (PC.IsValid()) PC->Client_RunBuildableAuditAction(-1, 106);
        for (const auto& H : Fixture.Held)
        {
            if (!H.Pawn.IsValid()) { ++Expired; continue; }
            H.Pawn->GetCharacterMovement()->SetMovementMode(H.Mode, H.Custom); ++Restored;
            if (H.WasRunning && H.Brain.IsValid()) H.Brain->RestartLogic();
        }
        UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: Complete reason=%s stage=%d destroyed=%d restored=%d expired=%d held=%d"), Reason, Stage, Destroyed, Restored, Expired, Fixture.Held.Num());
        if (GS.IsValid()) for (auto It = GS->GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (auto* Remote = Cast<ABwayPlayerController>(It->Get()); Remote && !Remote->IsLocalController()) Remote->Client_RunSiegeAuditObserver(-1);
        Running = false; return false;
    }

    void Dispatch(int32 Action, int32 Sequence)
    {
        UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: Dispatch sequence=%d action=%d id=%d"), Sequence, Action, Player->GetPlayerId());
        PC->Client_RunBuildableAuditAction(Action, Sequence);
    }

    bool Tick(float)
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > 240) return Stop(TEXT("timeout"));
        if (Stage == 0)
        {
            if (GEngine) for (const auto& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World(); auto* State = World ? World->GetGameState<ABwayGameState>() : nullptr;
                auto* Round = State ? State->GetRoundManagement() : nullptr;
                if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client || !Round) continue;
                TArray<ABwayPlayerController*> Remotes; int32 Humans = 0;
                for (auto It = World->GetPlayerControllerIterator(); It; ++It) if (auto* Human = Cast<ABwayPlayerController>(It->Get()))
                { ++Humans; if (!Human->IsLocalController() && Human->GetNetConnection()) Remotes.Add(Human); }
                if (Humans != 4 || Remotes.Num() != 3) continue;
                if (SelectedId < 0 && State->HeroSelectionManager && State->HeroSelectionManager->IsSelectionActive())
                {
                    Remotes.Sort([](const auto& A, const auto& B) { return A.PlayerState->GetPlayerId() < B.PlayerState->GetPlayerId(); });
                    const auto* Hero = LoadObject<UBwayHeroDataAsset>(nullptr, TEXT("/Hero_Spartacus/Characters/Heroes/Argus/DA_BW_HeroData_Argus.DA_BW_HeroData_Argus"));
                    auto* Candidate = Remotes[0]; auto* P = Candidate->GetPlayerState<ABwayPlayerState>();
                    if (!Hero || !P || P->IsHeroLocked()) return Stop(TEXT("selection-setup"));
                    Candidate->Server_SubmitPreSelectedHero(Hero->GetPrimaryAssetId(), true);
                    if (!P->IsHeroLocked() || P->GetSelectedHeroId() != Hero->GetPrimaryAssetId()) return Stop(TEXT("selection-rejected"));
                    SelectedId = P->GetPlayerId();
                    UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: Selection id=%d hero=%s source=production-selection-request"), SelectedId, *Hero->GetPrimaryAssetId().ToString());
                }
                if (Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) continue;
                for (auto* Remote : Remotes) if (Remote->PlayerState->GetPlayerId() == SelectedId) { PC = Remote; Player = Remote->GetPlayerState<ABwayPlayerState>(); }
                if (!PC.IsValid() || !Player.IsValid() || !PC->GetPawn() || !PlacementSpec(Player->GetLyraAbilitySystemComponent())) continue;
                const auto* Data = UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(PC->GetPawn(), PC.Get());
                const auto* Defaults = Data && Data->BuildableActorClass ? Cast<ABwaySiegeEngineBuildable>(Data->BuildableActorClass.GetDefaultObject()) : nullptr;
                if (!Defaults || !Defaults->ShouldPersistBetweenRounds() || Data->MaxActiveBuildablesPerPlayer != 1 || !Buildables(World).IsEmpty() || Player->HasPlacedBuildableThisRound()) return Stop(TEXT("invalid-baseline"));
                GS = State; InitialRound = Round->GetCurrentRoundNumber();
                for (APlayerState* P : State->PlayerArray) if (P) Fixture.Hold(P->GetPawn());
                for (auto* Remote : Remotes) Remote->Client_RunSiegeAuditObserver(SelectedId);
                UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: Setup id=%d round=%d humans=%d remoteHumans=%d maxActive=%d maximum=%.3f speed=%.3f persists=1"), SelectedId, InitialRound, Humans, Remotes.Num(), Data->MaxActiveBuildablesPerPlayer, Defaults->GetMaxRollDurationForDevelopment(), Defaults->GetRollSpeedForDevelopment());
                Stage = 1; StageStarted = Now; Dispatch(4, 101); return true;
            }
            return true;
        }
        if (!GS.IsValid() || !PC.IsValid() || !Player.IsValid()) return Stop(TEXT("lost-world"));
        auto* World = GS->GetWorld(); auto* Round = GS->GetRoundManagement();
        for (APlayerState* P : GS->PlayerArray) if (P) Fixture.Hold(P->GetPawn());
        const auto Actors = Buildables(World);
        for (ABuildableActor* Actor : Actors) if (Actor->GetOwner() == PC.Get()) Owned.AddUnique(Actor);
        if (Actors.Num() > 1) return Stop(TEXT("cap-exceeded"));
        if (Now >= NextSample) { NextSample = Now + .1; SampleSiege(World, Player.Get(), Stage, TEXT("authority")); }
        const double Age = Now - StageStarted;
        if (Stage == 1 && Actors.Num() == 1 && NetworkId(Actors[0]) > 1)
        {
            Current = Cast<ABwaySiegeEngineBuildable>(Actors[0]); if (!Current.IsValid()) return Stop(TEXT("wrong-class"));
            FirstId = NetworkId(Current.Get());
            UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: Spawn ordinal=1 netId=%llu name=%s time=%.3f"), FirstId, *Current->GetName(), World->GetTimeSeconds());
            Round->EndRound(0, EBwayWinCondition::GoalScored);
            UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: RoundEnd source=production netId=%llu time=%.3f"), FirstId, World->GetTimeSeconds());
            Stage = 2; StageStarted = Now;
        }
        else if (Stage == 2 && Round->GetCurrentRoundNumber() == InitialRound + 1 && Round->GetCurrentMatchPhase() == EBwayMatchPhase::Playing)
        {
            if (!Current.IsValid() || NetworkId(Current.Get()) != FirstId || Player->HasPlacedBuildableThisRound()) return Stop(TEXT("round-survival"));
            const auto* Data = UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(PC->GetPawn(), PC.Get()); FText Failure;
            const bool Allowed = UBwayBuildablePlacementLibrary::CanPlayerPlaceBuildable(PC->GetPawn(), PC.Get(), Data, Failure);
            UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: RoundRetained netId=%llu round=%d quota=%d capAllows=%d time=%.3f"), FirstId, Round->GetCurrentRoundNumber(), Player->HasPlacedBuildableThisRound(), Allowed, World->GetTimeSeconds());
            if (Allowed) return Stop(TEXT("cap-not-enforced"));
            Dispatch(8, 102); Stage = 3; StageStarted = Now;
        }
        else if (Stage == 3 && Actors.IsEmpty())
        {
            const auto* Data = UBwayBuildablePlacementLibrary::ResolveBuildableDataForPlayer(PC->GetPawn(), PC.Get()); FText Failure;
            const bool Allowed = UBwayBuildablePlacementLibrary::CanPlayerPlaceBuildable(PC->GetPawn(), PC.Get(), Data, Failure);
            UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: Expired ordinal=1 netId=%llu quota=%d capAllows=%d time=%.3f"), FirstId, Player->HasPlacedBuildableThisRound(), Allowed, World->GetTimeSeconds());
            if (!Allowed || Player->HasPlacedBuildableThisRound()) return Stop(TEXT("cap-not-released"));
            Dispatch(4, 103); Stage = 4; StageStarted = Now;
        }
        else if (Stage == 4 && Actors.Num() == 1 && NetworkId(Actors[0]) > 1)
        {
            Current = Cast<ABwaySiegeEngineBuildable>(Actors[0]);
            if (!Current.IsValid() || NetworkId(Current.Get()) == FirstId || !Player->HasPlacedBuildableThisRound()) return Stop(TEXT("second-placement"));
            UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: Spawn ordinal=2 netId=%llu name=%s time=%.3f"), NetworkId(Current.Get()), *Current->GetName(), World->GetTimeSeconds());
            Stage = 5; StageStarted = Now;
        }
        else if (Stage == 5 && Actors.IsEmpty())
        {
            UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: Expired ordinal=2 quota=%d time=%.3f"), Player->HasPlacedBuildableThisRound(), World->GetTimeSeconds());
            if (!Player->HasPlacedBuildableThisRound()) return Stop(TEXT("expiry-cleared-quota"));
            Dispatch(7, 104); Stage = 6; StageStarted = Now;
        }
        else if (Stage == 6)
        {
            if (!ResetAt && Age >= 2) { Player->BeginRoundStatTracking(); ResetAt = Now; UE_LOG(LogTemp, Display, TEXT("BwaySiegeAudit: CleanupQuota count=0")); }
            if (ResetAt && Now - ResetAt >= 5)
            {
                const auto* Spec = PlacementSpec(Player->GetLyraAbilitySystemComponent());
                if (!Actors.IsEmpty() || Player->HasPlacedBuildableThisRound() || !Spec || Spec->IsActive()) return Stop(TEXT("final-cleanup"));
                return Stop(TEXT("observed"));
            }
        }
        if (Age > 30) return Stop(TEXT("stage-timeout"));
        return true;
    }
};

FAutoConsoleCommand SiegeCommand(TEXT("bway.Test.SiegeLifecycleAudit"), TEXT("Development remote Siege lifetime/cap/production-round audit; normal barrel ticks and collision."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (Running) { UE_LOG(LogTemp, Warning, TEXT("BwaySiegeAudit: already running")); return; }
        Running = true; auto Run = MakeShared<FSiegeRun>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), .1f);
    }));

FAutoConsoleCommand AccumulationCommand(TEXT("bway.Test.BuildableAccumulationAudit"), TEXT("Development three-round surviving structures/cap/replacement audit. Optional class-name filter: SunShrine, Gunslinger, Hexweaver."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Running) { UE_LOG(LogTemp, Warning, TEXT("BwayBuildableAudit: already running")); return; }
        Running = true;
        auto Run = MakeShared<FRun>(); Run->Accumulation = true;
        Run->RequiredClass = Args.IsEmpty() ? TEXT("") : Args[0];
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), .1f);
    }));

FAutoConsoleCommand Command(TEXT("bway.Test.BuildableAuthorityAudit"), TEXT("Development remote GAS geometry/quota/cancel/round audit with independent peer journals."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (Running) { UE_LOG(LogTemp, Warning, TEXT("BwayBuildableAudit: already running")); return; }
        Running = true;
        auto Run = MakeShared<FRun>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), .1f);
    }));
}
#endif

void ABwayPlayerController::Client_RunBuildableAuditAction_Implementation(int32 Action, int32 Sequence)
{
#if !UE_BUILD_SHIPPING
    BwayBuildableAudit::RunAction(this, Action, Sequence);
#endif
}

void ABwayPlayerController::Client_RunBuildableAuditObserver_Implementation(int32 ObservedPlayerId)
{
#if !UE_BUILD_SHIPPING
    BwayBuildableAudit::RunObserver(this, ObservedPlayerId);
#endif
}

void ABwayPlayerController::Client_RunSiegeAuditObserver_Implementation(int32 ObservedPlayerId)
{
#if !UE_BUILD_SHIPPING
    BwayBuildableAudit::ObserveSiege(this, ObservedPlayerId);
#endif
}
