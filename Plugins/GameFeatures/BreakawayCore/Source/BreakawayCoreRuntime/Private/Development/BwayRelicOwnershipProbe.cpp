#include "Development/BwayRelicOwnershipProbe.h"
#include "Net/UnrealNetwork.h"

#if !UE_BUILD_SHIPPING
#include "Abilities/BwayGameplayAbility_RelicPass.h"
#include "Abilities/BwayGameplayAbility_RelicRequest.h"
#include "AbilitySystem/LyraAbilitySet.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayGoalVolume.h"
#include "BwayPlayerState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayMidfieldRulesLibrary.h"
#include "GameState/BwayRelicManagerComponent.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "GameState/BwayScoringComponent.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Relic/RelicActor.h"
#include "Relic/RelicSettings.h"
#include "UObject/UnrealType.h"

namespace BwayRelicOwnershipTest
{
bool Running = false;
ULyraAbilitySystemComponent* ASC(ABwayPlayerState* PS) { return PS ? PS->GetLyraAbilitySystemComponent() : nullptr; }
TArray<ABwayPlayerState*> Players(ABwayGameState* GS, bool HumansOnly = false)
{
    TArray<ABwayPlayerState*> Result;
    if (GS) for (APlayerState* P : GS->PlayerArray)
        if (auto* PS = Cast<ABwayPlayerState>(P); PS && !PS->IsInactive() && (!HumansOnly || !PS->IsABot())) Result.Add(PS);
    Result.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
    return Result;
}
int32 Grants(ABwayPlayerState* PS, ARelicActor* Relic)
{
    int32 Count = 0;
    if (auto* Component = ASC(PS)) for (const FGameplayAbilitySpec& Spec : Component->GetActivatableAbilities())
        if (Spec.SourceObject.Get() == Relic) ++Count;
    return Count;
}
FGameplayAbilitySpec* ActionSpec(ABwayPlayerState* PS, int32 Action)
{
    if (auto* Component = ASC(PS)) for (FGameplayAbilitySpec& Spec : Component->GetActivatableAbilities())
        if (Spec.Ability && ((Action == 1 && Spec.Ability->IsA<UBwayGameplayAbility_RelicRequest>()) ||
            (Action == 2 && Spec.Ability->IsA<UBwayGameplayAbility_RelicPass>()) ||
            (Action == 3 && Spec.Ability->GetClass()->GetFName() == FName(TEXT("GA_BW_Relic_Throw_C"))))) return &Spec;
    return nullptr;
}
}
#endif

ABwayRelicOwnershipProbe::ABwayRelicOwnershipProbe()
{
#if !UE_BUILD_SHIPPING
    bReplicates = true;
    bOnlyRelevantToOwner = true;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
#endif
}
void ABwayRelicOwnershipProbe::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ABwayRelicOwnershipProbe, Frame);
}
void ABwayRelicOwnershipProbe::SetFrame(const FBwayRelicOwnershipFrame& Value)
{
    Frame = Value;
    ReportedStage = -1;
    ForceNetUpdate();
}
void ABwayRelicOwnershipProbe::ServerReport_Implementation(int32 Stage, bool Passed)
{
    if (Stage != Frame.Stage) return;
    ReportedStage = Stage;
    ReportFailed |= !Passed;
}
void ABwayRelicOwnershipProbe::ClientFinish_Implementation()
{
#if !UE_BUILD_SHIPPING
    auto* PC = GetOwner<APlayerController>();
    if (PC && SavedControl) PC->SetControlRotation(SavedControlRotation);
    UE_LOG(LogTemp, Display, TEXT("BwayRelicOwnership: PASS viewer=%d net=%d observations=%d actions=%d cleanup=1"),
        PC && PC->PlayerState ? PC->PlayerState->GetPlayerId() : -1, int32(GetNetMode()), Observations, Actions);
    SetActorTickEnabled(false);
    if (GetNetMode() == NM_Client) FPlatformMisc::RequestExit(false);
#endif
}
void ABwayRelicOwnershipProbe::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
#if !UE_BUILD_SHIPPING
    using namespace BwayRelicOwnershipTest;
    auto* PC = GetOwner<APlayerController>();
    auto* Viewer = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
    auto* GS = GetWorld()->GetGameState<ABwayGameState>();
    if (!PC || !PC->IsLocalController() || !Viewer || !GS) return;
    const double Now = FPlatformTime::Seconds();
    if (ReleaseAt > 0 && Now >= ReleaseAt)
    {
        BwayRelicOwnershipTest::ASC(Viewer)->AbilityInputTagReleased(ReleaseTag);
        BwayRelicOwnershipTest::ASC(Viewer)->ProcessAbilityInput(DeltaSeconds, false);
        ReleaseAt = 0;
    }
    if (Frame.Stage < 0 || SubmittedStage == Frame.Stage) return;
    if (ObservedStage != Frame.Stage) { ObservedStage = Frame.Stage; StageStarted = Now; StableStarted = 0; }
    if (Frame.Action)
    {
        bool Activated = true;
        FString AbilityName = TEXT("Idle");
        if (Viewer->GetPlayerId() == Frame.ActionPlayer)
        {
            if (!SavedControl) { SavedControlRotation = PC->GetControlRotation(); SavedControl = true; }
            PC->SetControlRotation(FRotator::ZeroRotator);
            FGameplayAbilitySpec* Spec = ActionSpec(Viewer, Frame.Action);
            if (!Spec && Now - StageStarted < 8.0) return;
            Activated = false;
            if (Spec)
            {
                const FGameplayAbilitySpecHandle Handle = Spec->Handle;
                AbilityName = Spec->Ability->GetClass()->GetName();
                FGameplayTag InputTag;
                for (const FGameplayTag& Tag : Spec->GetDynamicSpecSourceTags())
                    if (Tag.ToString().StartsWith(TEXT("InputTag."))) { InputTag = Tag; break; }
                Activated = BwayRelicOwnershipTest::ASC(Viewer)->TryActivateAbility(Handle);
                if (Activated && InputTag.IsValid()) { ReleaseTag = InputTag; ReleaseAt = Now + 0.4; }
            }
            ++Actions;
            UE_LOG(LogTemp, Display, TEXT("BwayRelicOwnership: Activate stage=%d viewer=%d net=%d action=%d ability=%s accepted=%d"),
                Frame.Stage, Viewer->GetPlayerId(), int32(GetNetMode()), Frame.Action, *AbilityName, Activated);
        }
        SubmittedStage = Frame.Stage;
        ServerReport(Frame.Stage, Activated);
        return;
    }
    ARelicActor* Relic = Frame.Relic;
    const auto Peers = Players(GS);
    int32 Carrier = -1;
    if (Relic && Relic->CurrentCarrier && Relic->CurrentCarrier->GetPlayerState()) Carrier = Relic->CurrentCarrier->GetPlayerState()->GetPlayerId();
    const int32 State = Relic ? int32(Relic->GetCurrentState()) : -1;
    const int32 LocalCount = Relic ? Grants(Viewer, Relic) : -1;
    const int32 Expected = Viewer->GetPlayerId() == Frame.Carrier ? Frame.AbilityCount : 0;
    const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("Gameplay.State.RelicCarrier"));
    bool Flags = true, TagsAgree = true, Counts = LocalCount == Expected, Score = true;
    for (ABwayPlayerState* Peer : Peers)
    {
        const bool Owns = Peer->GetPlayerId() == Frame.Carrier;
        Flags &= Peer->bHasRelic == Owns;
        TagsAgree &= BwayRelicOwnershipTest::ASC(Peer) && BwayRelicOwnershipTest::ASC(Peer)->GetTagCount(Tag) == (Owns ? 1 : 0);
        if (HasAuthority()) Counts &= Grants(Peer, Relic) == (Owns ? Frame.AbilityCount : 0);
        if (Peer->GetPlayerId() == Frame.ScorePlayer) Score &= Peer->GetObjectiveScore() == Frame.ObjectiveScore;
    }
    if (Frame.ScoreTeam >= 0)
    {
        auto* Scoring = GS->FindComponentByClass<UBwayScoringComponent>();
        Score &= Scoring && Scoring->GetTeamScore(Frame.ScoreTeam) == Frame.TeamScore;
    }
    bool Roster = Peers.Num() == Frame.Players.Num();
    for (ABwayPlayerState* Peer : Peers) Roster &= Frame.Players.Contains(Peer->GetPlayerId());
    // Check the cooked attachment on every peer, not just the authoritative carrier.
    const bool CheckCarry = Frame.Carrier >= 0 && Frame.State == int32(ERelicState::Carried);
    auto* CarrierPawn = Relic ? Cast<ACharacter>(Relic->CurrentCarrier) : nullptr;
    auto* CarrierMesh = CarrierPawn ? CarrierPawn->GetMesh() : nullptr;
    auto* Root = Relic ? Relic->GetRootComponent() : nullptr;
    const FName Socket = Relic && Relic->GetRelicSettings() ? FName(*Relic->GetRelicSettings()->RelicSocket) : NAME_None;
    const bool SocketExists = CarrierMesh && !Socket.IsNone() && CarrierMesh->DoesSocketExist(Socket);
    const bool ParentMatches = Root && CarrierMesh && Root->GetAttachParent() == CarrierMesh && Root->GetAttachSocketName() == Socket;
    const float SocketError = SocketExists && Root ? FVector::Distance(Root->GetComponentLocation(), CarrierMesh->GetSocketLocation(Socket)) : -1.0f;
    const bool CarryMatches = !CheckCarry || (SocketExists && ParentMatches && SocketError >= 0 && SocketError <= 2.0f);
    const bool Passed = Relic && Roster && Carrier == Frame.Carrier && (Frame.State < 0 || State == Frame.State) && Flags && TagsAgree && Counts && Score && CarryMatches;
    if (!Passed) StableStarted = 0;
    else if (StableStarted == 0) StableStarted = Now;
    if ((StableStarted == 0 || Now - StableStarted < 0.7) && Now - StageStarted < 12.0) return;
    ++Observations;
    if (CheckCarry)
    {
        auto* CarrierPS = CarrierPawn ? CarrierPawn->GetPlayerState<ABwayPlayerState>() : nullptr;
        UE_LOG(LogTemp, Display, TEXT("BwayRelicCarry: Observe stage=%d viewer=%d net=%d carrier=%d hero=%s mesh=%s socket=%s bone=%s exists=%d parent=%d errorCm=%.4f passed=%d"),
            Frame.Stage, Viewer->GetPlayerId(), int32(GetNetMode()), Carrier,
            CarrierPS ? *CarrierPS->GetSelectedHeroId().ToString() : TEXT("None"), *GetNameSafe(CarrierMesh ? CarrierMesh->GetSkeletalMeshAsset() : nullptr),
            *Socket.ToString(), CarrierMesh ? *CarrierMesh->GetSocketBoneName(Socket).ToString() : TEXT("None"), SocketExists, ParentMatches, SocketError, CarryMatches);
    }
    UE_LOG(LogTemp, Display, TEXT("BwayRelicOwnership: Observe stage=%d viewer=%d net=%d carrier=%d expectedCarrier=%d state=%d expectedState=%d localAbilities=%d expectedAbilities=%d players=%d flags=%d tags=%d counts=%d score=%d stableSeconds=%.2f passed=%d"),
        Frame.Stage, Viewer->GetPlayerId(), int32(GetNetMode()), Carrier, Frame.Carrier, State, Frame.State, LocalCount, Expected, Peers.Num(), Flags, TagsAgree, Counts, Score, StableStarted > 0 ? Now - StableStarted : 0, Passed);
    SubmittedStage = Frame.Stage;
    ServerReport(Frame.Stage, Passed);
#endif
}

#if !UE_BUILD_SHIPPING
namespace BwayRelicOwnershipTest
{
struct FHeld
{
    TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
    FTransform Transform;
    EMovementMode Mode = MOVE_None;
    uint8 Custom = 0;
    TWeakObjectPtr<UBrainComponent> Brain;
    bool BrainRunning = false;
    TWeakObjectPtr<AController> Controller;
    FRotator ControlRotation;
};

struct FRun
{
    double Started = FPlatformTime::Seconds();
    double Changed = Started;
    int32 Phase = 0, Cycle = 0, Stage = 0, ExpectedAbilities = 0;
    int32 GoalBefore = 0, ScoreBefore = 0, ScoreTeam = -1;
    TWeakObjectPtr<ABwayGameState> GameState;
    TWeakObjectPtr<ARelicActor> Relic;
    TArray<TWeakObjectPtr<ABwayPlayerState>> Humans;
    TArray<TWeakObjectPtr<ABwayRelicOwnershipProbe>> Probes;
    TArray<FHeld> Held;
    TArray<int32> Roster;
    FVector Center;

    ABwayPlayerState* Source() const { return Humans.IsValidIndex(FMath::Min(Cycle, 3)) ? Humans[FMath::Min(Cycle, 3)].Get() : nullptr; }
    ABwayCharacterWithAbilities* Pawn(ABwayPlayerState* PS) const { return PS && ASC(PS) ? Cast<ABwayCharacterWithAbilities>(ASC(PS)->GetAvatarActor()) : nullptr; }
    void Cleanup()
    {
        int32 Restored = 0, Expired = 0;
        for (const auto& Player : Humans) if (auto* PS = Player.Get())
        {
            if (FGameplayAbilitySpec* Spec = ActionSpec(PS, 1); Spec && Spec->IsActive()) ASC(PS)->CancelAbilityHandle(Spec->Handle);
            if (Relic.IsValid() && Relic->GetRelicSettings())
                ASC(PS)->RemoveActiveGameplayEffectBySourceEffect(Relic->GetRelicSettings()->RequestingGameplayEffectClass, ASC(PS));
        }
        if (auto* GS = GameState.Get()) if (auto* Manager = GS->FindComponentByClass<UBwayRelicManagerComponent>()) Manager->ResetRelic();
        for (const FHeld& Item : Held)
        {
            if (auto* P = Item.Pawn.Get())
            {
                P->SetActorTransform(Item.Transform, false, nullptr, ETeleportType::TeleportPhysics);
                P->GetCharacterMovement()->SetMovementMode(Item.Mode, Item.Custom);
                ++Restored;
            }
            else ++Expired;
            if (Item.Controller.IsValid()) Item.Controller->SetControlRotation(Item.ControlRotation);
            if (Item.BrainRunning && Item.Brain.IsValid()) Item.Brain->RestartLogic();
        }
        UE_LOG(LogTemp, Display, TEXT("BwayRelicOwnership: Cleanup restored=%d expired=%d owned=%d"), Restored, Expired, Held.Num());
        Held.Reset();
    }
    bool Stop(const TCHAR* Error)
    {
        Cleanup();
        UE_LOG(LogTemp, Error, TEXT("BwayRelicOwnership: FAIL reason=%s phase=%d cycle=%d stage=%d"), Error, Phase, Cycle, Stage);
        for (const auto& Probe : Probes) if (Probe.IsValid()) Probe->SetLifeSpan(2.0f);
        Running = false;
        return false;
    }
    void Publish(int32 NextPhase, int32 Action = 0, int32 Carrier = -1, int32 State = -1, bool Score = false)
    {
        Phase = NextPhase;
        Changed = FPlatformTime::Seconds();
        FBwayRelicOwnershipFrame Frame;
        Frame.Stage = ++Stage;
        Frame.Action = Action;
        Frame.ActionPlayer = Source()->GetPlayerId();
        Frame.Carrier = Carrier;
        Frame.State = State;
        Frame.AbilityCount = ExpectedAbilities;
        Frame.Players = Roster;
        Frame.Relic = Relic.Get();
        if (Score)
        {
            Frame.ScorePlayer = Source()->GetPlayerId();
            Frame.ObjectiveScore = GoalBefore + 1;
            Frame.ScoreTeam = ScoreTeam;
            Frame.TeamScore = ScoreBefore + 1;
        }
        for (const auto& Probe : Probes) if (Probe.IsValid()) Probe->SetFrame(Frame);
        UE_LOG(LogTemp, Display, TEXT("BwayRelicOwnership: Stage stage=%d phase=%d cycle=%d action=%d source=%d carrier=%d state=%d"),
            Stage, Phase, Cycle, Action, Source()->GetPlayerId(), Carrier, State);
    }
    bool AllReported() const
    {
        if (Probes.Num() != 4) return false;
        for (const auto& Probe : Probes) if (!Probe.IsValid() || Probe->ReportedStage != Stage || Probe->ReportFailed) return false;
        return true;
    }
    bool Arrange()
    {
        auto* GS = GameState.Get();
        auto* Carrier = Pawn(Source());
        if (!GS || !Carrier || !Relic.IsValid()) return false;
        auto* Manager = GS->FindComponentByClass<UBwayRelicManagerComponent>();
        if (!Manager) return false;
        Manager->ResetRelic();
        ABwayCharacterWithAbilities* Ally = nullptr;
        for (int32 Index = 0; Index < Held.Num(); ++Index)
        {
            auto* P = Held[Index].Pawn.Get();
            if (!P) return false;
            P->SetActorLocation(Center + FVector((Index - 3.5f) * 500.0f, 2500.0f, 150.0f), false, nullptr, ETeleportType::TeleportPhysics);
            if (P != Carrier && GS->GetPlayerTeam(P->GetPlayerState()) == GS->GetPlayerTeam(Source())) Ally = P;
        }
        if (!Ally) return false;
        Carrier->SetActorLocation(Center + FVector(0, 0, 150), false, nullptr, ETeleportType::TeleportPhysics);
        Carrier->SetActorRotation(FRotator::ZeroRotator);
        Carrier->GetController()->SetControlRotation(FRotator::ZeroRotator);
        Ally->SetActorLocation(Center + FVector(1000, 0, 150), false, nullptr, ETeleportType::TeleportPhysics);
        // Keep the loose relic away from all capsules until the request arrives.
        Relic->SetActorLocation(Center + FVector(0, 0, 1500), false, nullptr, ETeleportType::TeleportPhysics);
        return true;
    }
    bool Pickup()
    {
        auto* Carrier = Pawn(Source());
        if (!Carrier || !Relic.IsValid() || !Relic->CanBePickedUpBy(Carrier)) return false;
        // Controlled overlap invokes the production request/event/ability path.
        Relic->OnInteractionSphereOverlap(nullptr, Carrier, nullptr, 0, false, FHitResult());
        const bool Accepted = Relic->CurrentCarrier == Carrier;
        UE_LOG(LogTemp, Display, TEXT("BwayRelicOwnership: Pickup cycle=%d source=%d accepted=%d grants=%d"), Cycle, Source()->GetPlayerId(), Accepted, Grants(Source(), Relic.Get()));
        return Accepted;
    }
    bool Setup(ABwayGameState* GS)
    {
        const auto People = Players(GS, true);
        const auto All = Players(GS);
        auto* Manager = GS->FindComponentByClass<UBwayRelicManagerComponent>();
        auto* Ball = Manager ? Manager->GetRelicActor() : nullptr;
        if (People.Num() != 4 || All.Num() != 8 || !Ball || !Ball->GetRelicSettings()) return false;
        for (ABwayPlayerState* PS : All) if (!Pawn(PS) || !Pawn(PS)->GetController()) return false;
        for (ABwayPlayerState* PS : People)
            if (!Cast<APlayerController>(PS->GetOwner()) ||
                (Ball->GetRelicSettings()->RequestingTag.IsValid() && ASC(PS)->HasMatchingGameplayTag(Ball->GetRelicSettings()->RequestingTag))) return false;
        auto* Set = Ball->GetRelicSettings()->RelicAbilitySet.LoadSynchronous();
        auto* Property = FindFProperty<FArrayProperty>(ULyraAbilitySet::StaticClass(), TEXT("GrantedGameplayAbilities"));
        if (!Set || !Property) return false;
        const auto* Abilities = Property->ContainerPtrToValuePtr<TArray<FLyraAbilitySet_GameplayAbility>>(Set);
        ExpectedAbilities = 1;
        for (const auto& Ability : *Abilities) if (Ability.Ability) ++ExpectedAbilities;
        if (ExpectedAbilities <= 1) return false;
        FVector Axis;
        if (!UBwayMidfieldRulesLibrary::TryGetMidfieldFrame(GS, Center, Axis)) return false;
        GameState = GS;
        Relic = Ball;
        for (ABwayPlayerState* PS : All)
        {
            Roster.Add(PS->GetPlayerId());
            auto* P = Pawn(PS);
            FHeld Item;
            Item.Pawn = P;
            Item.Transform = P->GetActorTransform();
            Item.Mode = P->GetCharacterMovement()->MovementMode;
            Item.Custom = P->GetCharacterMovement()->CustomMovementMode;
            Item.Controller = P->GetController();
            Item.ControlRotation = P->GetController()->GetControlRotation();
            P->GetCharacterMovement()->StopMovementImmediately();
            P->GetCharacterMovement()->DisableMovement();
            if (auto* AI = Cast<AAIController>(P->GetController())) if (auto* Brain = AI->GetBrainComponent())
            {
                Item.Brain = Brain;
                Item.BrainRunning = Brain->IsRunning();
                Brain->StopLogic(TEXT("Owned relic ownership fixture"));
            }
            Held.Add(Item);
        }
        for (ABwayPlayerState* PS : People)
        {
            Humans.Add(PS);
            FActorSpawnParameters Params;
            Params.Owner = Cast<APlayerController>(PS->GetOwner());
            auto* Probe = GS->GetWorld()->SpawnActor<ABwayRelicOwnershipProbe>(Params);
            Probes.Add(Probe);
        }
        UE_LOG(LogTemp, Display, TEXT("BwayRelicOwnership: Setup humans=4 bots=4 expectedAbilities=%d set=%s"), ExpectedAbilities, *Set->GetName());
        return true;
    }
    bool Tick(float)
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > 200.0) return Stop(TEXT("overall-timeout"));
        if (Phase == 0)
        {
            if (GEngine) for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || World->WorldType != EWorldType::Game || World->GetNetMode() != NM_ListenServer || !World->GetMapName().Contains(TEXT("L_BW_Dorado"))) continue;
                auto* GS = World->GetGameState<ABwayGameState>();
                auto* Round = GS ? GS->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
                if (!Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || !Setup(GS)) return true;
                if (!Arrange()) return Stop(TEXT("arrange-failed"));
                Publish(1, 1);
                return true;
            }
            return true;
        }
        if (!GameState.IsValid() || !Relic.IsValid() || !Source()) return Stop(TEXT("world-or-source-lost"));
        for (const auto& Probe : Probes) if (!Probe.IsValid() || Probe->ReportFailed) return Stop(TEXT("peer-reported-failure"));
        if (Now - Changed > 16.0) return Stop(TEXT("stage-timeout"));
        if (!AllReported()) return true;
        auto* GS = GameState.Get();
        if (Phase == 1 || Phase == 6)
        {
            if (!Pickup()) return true;
            Publish(Phase == 1 ? 2 : 7, 0, Source()->GetPlayerId(), int32(ERelicState::Carried));
        }
        else if (Phase == 2)
        {
            Relic->OnPickedUp(Pawn(Source()));
            Relic->OnPickedUp(Pawn(Humans[(Cycle + 1) % 4].Get()));
            UE_LOG(LogTemp, Display, TEXT("BwayRelicOwnership: Duplicate cycle=%d retained=%d grants=%d"), Cycle, Relic->CurrentCarrier == Pawn(Source()), Grants(Source(), Relic.Get()));
            Publish(3, 0, Source()->GetPlayerId(), int32(ERelicState::Carried));
        }
        else if (Phase == 3) Publish(4, Cycle % 2 == 0 ? 2 : 3);
        else if (Phase == 4)
        {
            const ERelicState Expected = Cycle % 2 == 0 ? ERelicState::BeingPassed : ERelicState::Thrown;
            if (Relic->CurrentCarrier || Relic->GetCurrentState() != Expected) return true;
            // Hold the released object away from other capsules while peers settle.
            // This verifies release transport, not ballistic flight or catch timing.
            if (auto* Mesh = Relic->FindComponentByClass<UStaticMeshComponent>()) Mesh->SetSimulatePhysics(false);
            Relic->SetActorLocation(Center + FVector(0, 0, 1500), false, nullptr, ETeleportType::TeleportPhysics);
            UE_LOG(LogTemp, Display, TEXT("BwayRelicOwnership: Release cycle=%d source=%d state=%d grants=%d"), Cycle, Source()->GetPlayerId(), int32(Expected), Grants(Source(), Relic.Get()));
            Publish(5, 0, -1, int32(Expected));
        }
        else if (Phase == 5)
        {
            ++Cycle;
            if (!Arrange()) return Stop(TEXT("next-carrier-arrange-failed"));
            Publish(Cycle < 4 ? 1 : 6, 1);
        }
        else if (Phase == 7)
        {
            ScoreTeam = GS->GetPlayerTeam(Source());
            auto* Scoring = GS->FindComponentByClass<UBwayScoringComponent>();
            if (!Scoring) return Stop(TEXT("scoring-component-missing"));
            GoalBefore = Source()->GetObjectiveScore();
            ScoreBefore = Scoring->GetTeamScore(ScoreTeam);
            ABwayGoalVolume* Goal = nullptr;
            for (TActorIterator<ABwayGoalVolume> It(GS->GetWorld()); It; ++It) if (It->OwningTeam == 1 - ScoreTeam) { Goal = *It; break; }
            if (!Goal) return Stop(TEXT("enemy-goal-missing"));
            Pawn(Source())->SetActorLocation(Goal->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
            Publish(8, 0, -1, -1, true);
        }
        else if (Phase == 8)
        {
            Cleanup();
            UE_LOG(LogTemp, Display, TEXT("BwayRelicOwnership: COMPLETE cycles=4 stages=%d expectedAbilities=%d scoreDelta=1"), Stage, ExpectedAbilities);
            for (const auto& Probe : Probes) if (Probe.IsValid()) { Probe->ClientFinish(); Probe->SetLifeSpan(2.0f); }
            Running = false;
            return false;
        }
        return true;
    }
};

FAutoConsoleCommand Command(TEXT("bway.Test.RelicOwnership"), TEXT("Development-only four-owner request/pickup/pass/throw/score and replicated ability cleanup gate."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (Running) return;
        Running = true;
        TSharedRef<FRun> Run = MakeShared<FRun>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.1f);
    }));
}
#endif
