// Development-only observation of production death scheduling and pawn lifetimes.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "BwayPlayerController.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "UI/BwayRespawnHUDWidget.h"
#include "UObject/UObjectIterator.h"

namespace BwayRespawnAudit
{
bool Running = false;
struct FHeld
{
    TWeakObjectPtr<ABwayCharacterWithAbilities> Pawn;
    EMovementMode Mode;
    uint8 Custom;
    TWeakObjectPtr<UBrainComponent> Brain;
    bool BrainRunning;
};
struct FSubject
{
    TWeakObjectPtr<ABwayPlayerState> Player;
    TWeakObjectPtr<AController> Controller;
    TWeakObjectPtr<APawn> Last;
    int32 Replacements = 0;
    double Killed = 0;
};
struct FRun
{
    double Started = FPlatformTime::Seconds(), StageStarted = Started;
    int32 Stage = 0, InitialRound = 0;
    TWeakObjectPtr<ABwayGameState> GS;
    TWeakObjectPtr<UBwayRoundManagementComponent> Rounds;
    TArray<FSubject> Subjects;
    TArray<FHeld> Held;
    TWeakObjectPtr<ABwayPlayerState> Human, Bot;
    int32 MissingRespawns = 0, DuplicateRespawns = 0, PostRoundRespawns = 0;
    bool AuditHUD = false;
    bool AuditUpgrade = false;
    bool PackagedUpgrade = false;
    float OriginalGold = 0;
    bool UpgradeSeeded = false;
    int32 UpgradeRequest = 0;
    double NextHUDSample = 0;
    int32 HUDSample = 0;
    int32 PreviousTimerTrace = 0;
    TSet<TWeakObjectPtr<UBwayRespawnHUDWidget>> ReattachedHUDs;
    TSet<TWeakObjectPtr<ABwayPlayerController>> OpenedShops;
    TSet<TWeakObjectPtr<ABwayPlayerController>> ClosedShops;

    void SampleUpgrade()
    {
        if (!AuditUpgrade || !Human.IsValid() || !GEngine) return;
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            auto* State = World && World->IsGameWorld() ? World->GetGameState<ABwayGameState>() : nullptr;
            if (!State) continue;
            for (APlayerState* P : State->PlayerArray)
            {
                auto* PS = Cast<ABwayPlayerState>(P);
                if (!PS || PS->GetPlayerId() != Human->GetPlayerId()) continue;
                const auto* ASC = PS->GetLyraAbilitySystemComponent();
                const auto* Hero = ASC ? ASC->GetSet<UBwayHeroAttributeSet>() : nullptr;
                const auto* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
                const auto* PC = Cast<APlayerController>(PS->GetOwner());
                const auto Schedule = PS->GetRespawnSchedule();
                UE_LOG(LogTemp, Display, TEXT("BwayRespawnUpgrade: Sample stage=%d world=%s net=%d id=%d owner=%d rank=%d reduction=%.6f gold=%.6f deadline=%.6f serverTime=%.6f scheduleRound=%d round=%d phase=%d alive=%d deaths=%d pawn=%s"),
                    Stage, *World->GetPathName(), int32(World->GetNetMode()), PS->GetPlayerId(), PC && PC->IsLocalController(),
                    PS->UpgradeComponent->GetOwnedRank(TEXT("RespawnTimeReduction")), Hero ? Hero->GetRespawnTimeReduction() : -1,
                    Gold ? Gold->GetCurrentGold() : -1, Schedule.ServerEndTime, State->GetServerWorldTimeSeconds(), Schedule.RoundNumber,
                    State->GetRoundManagement()->GetCurrentRoundNumber(), int32(State->GetRoundManagement()->GetCurrentMatchPhase()),
                    Alive(PS), PS->GetDeaths(), *GetNameSafe(PS->GetPawn()));
            }
        }
    }

    bool RequestUpgrade(int32 ExpectedRank)
    {
        if (!Human.IsValid() || !GEngine) return false;
        if (PackagedUpgrade)
        {
            auto* PC = Cast<ABwayPlayerController>(Human->GetOwner());
            if (!PC || PC->IsLocalController() || !PC->GetNetConnection()) return false;
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnUpgrade: Dispatch stage=%d expected=%d id=%d"), Stage, ExpectedRank, Human->GetPlayerId());
            PC->Client_RunRespawnUpgradePurchase(ExpectedRank, Stage);
            return true;
        }
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_Client) continue;
            for (auto It = World->GetPlayerControllerIterator(); It; ++It)
            {
                auto* PC = It->Get();
                auto* PS = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
                if (!PS || !PC->IsLocalController() || PS->GetPlayerId() != Human->GetPlayerId()) continue;
                const auto Before = PS->GetRespawnSchedule();
                UE_LOG(LogTemp, Display, TEXT("BwayRespawnUpgrade: Request stage=%d expected=%d id=%d world=%s alive=%d deadline=%.6f serverTime=%.6f"),
                    Stage, ExpectedRank, PS->GetPlayerId(), *World->GetPathName(), Alive(PS), Before.ServerEndTime, World->GetGameState()->GetServerWorldTimeSeconds());
                PS->UpgradeComponent->ServerPurchaseUpgrade(TEXT("RespawnTimeReduction"), ExpectedRank);
                return true;
            }
        }
        return false;
    }

    bool TickRematch(double Now)
    {
        if (!GEngine) return Stop(TEXT("engine-lost"));
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client) continue;
            auto* State = World->GetGameState<ABwayGameState>();
            auto* Round = State ? State->GetRoundManagement() : nullptr;
            if (!State || !Round || State == GS.Get()) continue;
            const auto Phase = Round->GetCurrentMatchPhase();
            const bool Selection = Phase == EBwayMatchPhase::Prematch;
            const bool Playing = Phase == EBwayMatchPhase::Playing && World->GetMapName().Contains(TEXT("Dorado"));
            if ((Stage == 6 && !Selection) || (Stage >= 7 && !Playing)) continue;
            int32 Humans = 0, Ready = 0, Invalid = 0;
            for (APlayerState* P : State->PlayerArray)
            {
                auto* PS = Cast<ABwayPlayerState>(P);
                if (!PS || PS->IsABot()) continue;
                ++Humans;
                const auto* ASC = PS->GetLyraAbilitySystemComponent();
                const auto* Hero = ASC ? ASC->GetSet<UBwayHeroAttributeSet>() : nullptr;
                const auto* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
                if (!Gold || (Playing && (!Hero || !Alive(PS)))) continue;
                ++Ready;
                const int32 Owned = PS->UpgradeComponent->GetOwnedUpgrades().Num();
                const float Reduction = Hero ? Hero->GetRespawnTimeReduction() : 0;
                const double Deadline = PS->GetRespawnSchedule().ServerEndTime;
                Invalid += Owned != 0 || Reduction != 0 || Deadline != 0 || PS->GetDeaths() != 0 || (Selection && Gold->GetCurrentGold() != 0);
                UE_LOG(LogTemp, Display, TEXT("BwayRespawnUpgrade: Reset stage=%d id=%d phase=%d round=%d hasHero=%d owned=%d reduction=%.6f gold=%.6f deadline=%.6f deaths=%d alive=%d world=%s"),
                    Stage, PS->GetPlayerId(), int32(Phase), Round->GetCurrentRoundNumber(), Hero != nullptr, Owned, Reduction,
                    Gold->GetCurrentGold(), Deadline, PS->GetDeaths(), Alive(PS), *World->GetPathName());
            }
            if (Humans != 4 || Ready != 4) return true;
            if (Invalid) return Stop(TEXT("production-rematch-not-reset"));
            if (Stage == 6)
            {
                Stage = 7; StageStarted = Now;
                UE_LOG(LogTemp, Display, TEXT("BwayRespawnUpgrade: SelectionReset humans=4 source=production freshWorld=1"));
            }
            else
            {
                for (APlayerState* P : State->PlayerArray) if (auto* PS = Cast<ABwayPlayerState>(P)) Hold(PS->GetPawn());
                if (Stage == 7) { Stage = 8; StageStarted = Now; }
                if (Now - StageStarted >= 5) return Stop(TEXT("observed"));
            }
            return true;
        }
        return true;
    }

    void SampleHUD(double Now)
    {
        if (!AuditHUD || Now < NextHUDSample || !GEngine) return;
        NextHUDSample = Now + 0.2;
        ++HUDSample;
        for (const auto& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            auto* State = World && World->IsGameWorld() ? World->GetGameState<ABwayGameState>() : nullptr;
            auto* Round = State ? State->GetRoundManagement() : nullptr;
            if (!Round) continue;
            for (auto It = World->GetPlayerControllerIterator(); It; ++It)
            {
                APlayerController* PC = It->Get();
                auto* PS = PC ? PC->GetPlayerState<ABwayPlayerState>() : nullptr;
                if (!PS || !PC->IsLocalPlayerController() || !PC->GetLocalPlayer()) continue;
                UBwayRespawnHUDWidget* HUD = nullptr;
                int32 Count = 0;
                for (TObjectIterator<UBwayRespawnHUDWidget> Widget; Widget; ++Widget)
                    if (Widget->GetOwningPlayer() == PC && Widget->IsInViewport()) { HUD = *Widget; ++Count; }
                const auto Schedule = PS->GetRespawnSchedule();
                UE_LOG(LogTemp, Display, TEXT("BwayRespawnHUD: Sample n=%d stage=%d world=%s net=%d id=%d round=%d phase=%d count=%d deadline=%.3f scheduleRound=%d serverTime=%.3f seconds=%d shown=%d focusable=%d menu=%d text=[%s]"),
                    HUDSample, Stage, *World->GetPathName(), int32(World->GetNetMode()), PS->GetPlayerId(), Round->GetCurrentRoundNumber(), int32(Round->GetCurrentMatchPhase()),
                    Count, Schedule.ServerEndTime, Schedule.RoundNumber, State->GetServerWorldTimeSeconds(), HUD ? HUD->GetDisplayedSeconds() : -99,
                    HUD && HUD->IsVisible(), HUD && HUD->IsFocusable(), HUD && HUD->IsSuppressedByMenu(), HUD ? *HUD->GetDisplayedStatus().ToString() : TEXT("missing"));
                // Reconstruct once while a countdown is active; verify the next samples read
                // the same deadline, with one widget and no client-owned gameplay timer.
                if (Stage == 1 && HUD && HUD->IsVisible() && Now - StageStarted > 0.8 && !ReattachedHUDs.Contains(HUD))
                {
                    ReattachedHUDs.Add(HUD);
                    HUD->RemoveFromParent();
                    HUD->AddToPlayerScreen(20);
                    UE_LOG(LogTemp, Display, TEXT("BwayRespawnHUD: Reattached id=%d world=%s"), PS->GetPlayerId(), *World->GetPathName());
                }
                auto* BwayPC = Cast<ABwayPlayerController>(PC);
                if (Stage == 2 && HUD && BwayPC && Now - StageStarted > 0.65 && !OpenedShops.Contains(BwayPC))
                {
                    if (!HUD->IsSuppressedByMenu())
                    {
                        BwayPC->ToggleUpgradeShop();
                        OpenedShops.Add(BwayPC);
                        UE_LOG(LogTemp, Display, TEXT("BwayRespawnHUD: ShopOpen id=%d world=%s"), PS->GetPlayerId(), *World->GetPathName());
                    }
                }
                if (BwayPC && OpenedShops.Contains(BwayPC) && !ClosedShops.Contains(BwayPC) && Now - StageStarted > 1.55)
                {
                    BwayPC->ToggleUpgradeShop();
                    ClosedShops.Add(BwayPC);
                    UE_LOG(LogTemp, Display, TEXT("BwayRespawnHUD: ShopClose id=%d world=%s"), PS->GetPlayerId(), *World->GetPathName());
                }
            }
        }
    }

    void Hold(APawn* Pawn)
    {
        auto* Character = Cast<ABwayCharacterWithAbilities>(Pawn);
        if (!Character || Held.ContainsByPredicate([Character](const FHeld& H) { return H.Pawn == Character; })) return;
        auto* Move = Character->GetCharacterMovement();
        auto* AI = Cast<AAIController>(Character->GetController());
        auto* Brain = AI ? AI->GetBrainComponent() : nullptr;
        Held.Add({Character, Move->MovementMode, Move->CustomMovementMode, Brain, Brain && Brain->IsRunning()});
        Move->StopMovementImmediately();
        Move->DisableMovement();
        if (Brain) Brain->StopLogic(TEXT("Owned respawn audit"));
    }
    bool Stop(const TCHAR* Reason)
    {
        if (AuditHUD || AuditUpgrade)
            if (auto* Trace = IConsoleManager::Get().FindConsoleVariable(TEXT("bway.Test.RespawnTimerTrace")))
                Trace->Set(PreviousTimerTrace, ECVF_SetByCode);
        if (UpgradeSeeded && Human.IsValid())
        {
            Human->UpgradeComponent->ResetUpgrades();
            Human->GetLyraAbilitySystemComponent()->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), OriginalGold);
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnUpgrade: Cleanup id=%d rank=%d reduction=%.6f restoredGold=%.6f"),
                Human->GetPlayerId(), Human->UpgradeComponent->GetOwnedRank(TEXT("RespawnTimeReduction")),
                Human->GetLyraAbilitySystemComponent()->GetSet<UBwayHeroAttributeSet>()->GetRespawnTimeReduction(), OriginalGold);
        }
        for (const auto& PC : OpenedShops)
            if (PC.IsValid() && !ClosedShops.Contains(PC)) PC->ToggleUpgradeShop();
        int32 Restored = 0, Expired = 0;
        for (const auto& H : Held)
        {
            if (H.Pawn.IsValid()) { H.Pawn->GetCharacterMovement()->SetMovementMode(H.Mode, H.Custom); ++Restored; }
            else ++Expired;
            if (H.BrainRunning && H.Brain.IsValid()) H.Brain->RestartLogic();
        }
        UE_LOG(LogTemp, Display, TEXT("BwayRespawnAudit: COMPLETE reason=%s stage=%d missing=%d duplicates=%d postRoundRespawns=%d restored=%d expired=%d held=%d"),
            Reason, Stage, MissingRespawns, DuplicateRespawns, PostRoundRespawns, Restored, Expired, Held.Num());
        Running = false;
        return false;
    }
    bool Alive(ABwayPlayerState* PS) const
    {
        auto* Pawn = PS ? PS->GetPawn() : nullptr;
        auto* Health = Pawn ? ULyraHealthComponent::FindHealthComponent(Pawn) : nullptr;
        return Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0;
    }
    void Kill(ABwayPlayerState* PS)
    {
        if (!PS || !Alive(PS))
        {
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnAudit: MissingTarget stage=%d id=%d bot=%d"), Stage, PS ? PS->GetPlayerId() : -1, PS && PS->IsABot());
            return;
        }
        auto* Pawn = PS->GetPawn();
        for (auto& S : Subjects) if (S.Player == PS) { S.Replacements = 0; S.Killed = FPlatformTime::Seconds(); S.Last = Pawn; }
        UE_LOG(LogTemp, Display, TEXT("BwayRespawnAudit: Kill stage=%d id=%d bot=%d pawn=%s deathsBefore=%d phase=%d round=%d"),
            Stage, PS->GetPlayerId(), PS->IsABot(), *Pawn->GetName(), PS->GetDeaths(), int32(Rounds->GetCurrentMatchPhase()), Rounds->GetCurrentRoundNumber());
        ULyraHealthComponent::FindHealthComponent(Pawn)->DamageSelfDestruct(false);
    }
    void StartDeaths(int32 Next)
    {
        Stage = Next; StageStarted = FPlatformTime::Seconds();
        UpgradeRequest = 0;
        if (AuditUpgrade && Next == 2 && Human.IsValid())
        {
            auto* ASC = Human->GetLyraAbilitySystemComponent();
            ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 500.f);
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnUpgrade: Refill stage=2 id=%d gold=%.6f"),
                Human->GetPlayerId(), ASC->GetSet<UBwayGoldAttributeSet>()->GetCurrentGold());
        }
        if (AuditHUD)
        {
            for (const auto& S : Subjects) if (S.Player.IsValid() && !S.Player->IsABot()) Kill(S.Player.Get());
        }
        else Kill(Human.Get());
        Kill(Bot.Get());
    }
    void Summarize()
    {
        for (const auto& S : Subjects) if (S.Player == Human || S.Player == Bot)
        {
            const bool IsAlive = Alive(S.Player.Get());
            if (!IsAlive || S.Replacements == 0) ++MissingRespawns;
            if (S.Replacements > 1) ++DuplicateRespawns;
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnAudit: Observe stage=%d id=%d bot=%d replacements=%d alive=%d deaths=%d"),
                Stage, S.Player->GetPlayerId(), S.Player->IsABot(), S.Replacements, IsAlive, S.Player->GetDeaths());
        }
        // Compare settled player state and pawn recovery in every PIE world, including authority.
        if (GEngine) for (const auto& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            auto* State = World && World->IsGameWorld() ? World->GetGameState<ABwayGameState>() : nullptr;
            if (!State) continue;
            for (const auto& S : Subjects) if (S.Player == Human || S.Player == Bot)
            {
                ABwayPlayerState* Peer = nullptr;
                for (APlayerState* P : State->PlayerArray)
                    if (P && P->GetPlayerId() == S.Player->GetPlayerId()) { Peer = Cast<ABwayPlayerState>(P); break; }
                const int32 Team = Peer ? State->GetPlayerTeam(Peer) : -1;
                const int32 AliveCount = Team >= 0 ? State->GetTeamInfo(Team).AlivePlayerCount : -1;
                UE_LOG(LogTemp, Display, TEXT("BwayRespawnAudit: Peer stage=%d world=%s id=%d bot=%d alive=%d deaths=%d team=%d teamAlive=%d"),
                    Stage, *World->GetPathName(), S.Player->GetPlayerId(), S.Player->IsABot(), Alive(Peer),
                    Peer ? Peer->GetDeaths() : -1, Team, AliveCount);
            }
        }
    }
    bool Tick(float)
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > (PackagedUpgrade ? 360 : 160)) return Stop(TEXT("timeout"));
        if (PackagedUpgrade && Stage >= 6) return TickRematch(Now);
        if (Stage == 0)
        {
            if (GEngine) for (const auto& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client) continue;
                auto* State = World->GetGameState<ABwayGameState>();
                auto* Round = State ? State->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
                if (!Round || Round->GetCurrentMatchPhase() != EBwayMatchPhase::Playing || State->PlayerArray.Num() != 8) continue;
                TArray<ABwayPlayerState*> Players;
                for (APlayerState* P : State->PlayerArray) if (auto* PS = Cast<ABwayPlayerState>(P); PS && Alive(PS)) Players.Add(PS);
                if (Players.Num() != 8) return true;
                GS = State; Rounds = Round; InitialRound = Round->GetCurrentRoundNumber();
                for (auto* PS : Players)
                {
                    APawn* Pawn = PS->GetPawn();
                    Subjects.Add({PS, Pawn->GetController(), Pawn});
                    Hold(Pawn);
                    if (!PS->IsABot() && (!Human.IsValid() || !CastChecked<APlayerController>(Pawn->GetController())->IsLocalController())) Human = PS;
                    if (PS->IsABot() && !Bot.IsValid()) Bot = PS;
                    int32 Auto = 0;
                    for (const auto& Spec : PS->GetLyraAbilitySystemComponent()->GetActivatableAbilities())
                        if (Spec.Ability && Spec.Ability->GetClass()->GetName().Contains(TEXT("AutoRespawn"))) ++Auto;
                    UE_LOG(LogTemp, Display, TEXT("BwayRespawnAudit: Setup id=%d bot=%d team=%d autoRespawnSpecs=%d nativeDelay=%.2f pawn=%s"),
                        PS->GetPlayerId(), PS->IsABot(), State->GetPlayerTeam(PS), Auto, Round->RespawnDelay, *Pawn->GetName());
                }
                if (!Human.IsValid() || !Bot.IsValid()) return Stop(TEXT("topology-missing"));
                if (AuditUpgrade)
                {
                    auto* ASC = Human->GetLyraAbilitySystemComponent();
                    const auto* Gold = ASC->GetSet<UBwayGoldAttributeSet>();
                    if (!Gold || !ASC->GetSet<UBwayHeroAttributeSet>() || !Human->UpgradeComponent->GetOwnedUpgrades().IsEmpty())
                        return Stop(TEXT("upgrade-setup-conflict"));
                    OriginalGold = Gold->GetCurrentGold();
                    ASC->SetNumericAttributeBase(UBwayGoldAttributeSet::GetCurrentGoldAttribute(), 500.f);
                    UpgradeSeeded = true;
                    UE_LOG(LogTemp, Display, TEXT("BwayRespawnUpgrade: Setup id=%d baseline=%.6f gold=%.6f round=%d"), Human->GetPlayerId(), Round->RespawnDelay,
                        ASC->GetSet<UBwayGoldAttributeSet>()->GetCurrentGold(), InitialRound);
                }
                // Give packaged clients time to receive the initial eight-pawn baseline.
                Stage = -1; StageStarted = Now;
                return true;
            }
            return true;
        }
        if (!GS.IsValid() || !Rounds.IsValid()) return Stop(TEXT("world-lost"));
        // Independent of widget lookup, reconstruction and menu state. The before/after
        // game-clock bracket remains evidence even when a game-thread hitch delays ticks.
        if (AuditHUD)
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnClock: stage=%d serverTime=%.6f wall=%.6f delta=%.6f round=%d"),
                Stage, GS->GetServerWorldTimeSeconds(), Now, GS->GetWorld()->GetDeltaSeconds(), Rounds->GetCurrentRoundNumber());
        SampleHUD(Now);
        SampleUpgrade();
        if (AuditUpgrade && (Stage == 1 || Stage == 2) && UpgradeRequest < 3
            && Now - StageStarted >= 0.35 + 0.4 * UpgradeRequest)
        {
            // Two sequential ranks then one deliberately stale request per death.
            const int32 ExpectedRank = UpgradeRequest == 2 ? 0 : (Stage - 1) * 2 + UpgradeRequest;
            if (!RequestUpgrade(ExpectedRank)) return Stop(TEXT("remote-upgrade-owner-missing"));
            ++UpgradeRequest;
        }
        for (auto& S : Subjects)
        {
            APawn* Pawn = S.Controller.IsValid() ? S.Controller->GetPawn() : nullptr;
            if (Pawn && Pawn != S.Last.Get())
            {
                S.Last = Pawn; ++S.Replacements; Hold(Pawn);
                const auto Phase = Rounds->GetCurrentMatchPhase();
                if (Stage == 3 && Phase == EBwayMatchPhase::PostRound) ++PostRoundRespawns;
                UE_LOG(LogTemp, Display, TEXT("BwayRespawnAudit: Replacement stage=%d id=%d bot=%d pawn=%s elapsed=%.3f phase=%d round=%d count=%d"),
                    Stage, S.Player->GetPlayerId(), S.Player->IsABot(), *Pawn->GetName(), S.Killed ? Now - S.Killed : -1,
                    int32(Phase), Rounds->GetCurrentRoundNumber(), S.Replacements);
            }
        }
        if (Stage == -1)
        {
            if (Now - StageStarted >= 5.0) StartDeaths(1);
            return true;
        }
        if (Stage < 3 && Now - StageStarted >= FMath::Max(12.0, double(Rounds->RespawnDelay) + 4))
        {
            Summarize();
            if (Stage == 1) StartDeaths(2);
            else
            {
                Stage = 3; StageStarted = Now;
                const int32 Team = GS->GetPlayerTeam(Human.Get());
                // Last death belongs to a human: catch a timer added after elimination cancels timers.
                for (auto& S : Subjects) if (S.Player != Human && GS->GetPlayerTeam(S.Player.Get()) == Team) Kill(S.Player.Get());
                Kill(Human.Get());
                UE_LOG(LogTemp, Display, TEXT("BwayRespawnAudit: Eliminated team=%d phase=%d round=%d"), Team, int32(Rounds->GetCurrentMatchPhase()), Rounds->GetCurrentRoundNumber());
            }
        }
        if (Stage == 3 && Now - StageStarted > 16)
        {
            if (Rounds->GetCurrentRoundNumber() != InitialRound + 1 || Rounds->GetCurrentMatchPhase() != EBwayMatchPhase::Playing)
                return Stop(TEXT("second-round-missing"));
            Stage = 4; StageStarted = Now;
            Kill(Human.Get());
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnAudit: Round2Death id=%d phase=%d round=%d teamAlive=%d"),
                Human->GetPlayerId(), int32(Rounds->GetCurrentMatchPhase()), Rounds->GetCurrentRoundNumber(), GS->GetTeamInfo(GS->GetPlayerTeam(Human.Get())).AlivePlayerCount);
        }
        if (Stage == 4 && Now - StageStarted >= 12)
        {
            for (const auto& S : Subjects) if (S.Player == Human)
            {
                const bool IsAlive = Alive(Human.Get());
                if (!IsAlive || S.Replacements == 0) ++MissingRespawns;
                if (S.Replacements > 1) ++DuplicateRespawns;
                UE_LOG(LogTemp, Display, TEXT("BwayRespawnAudit: Round2Observe id=%d alive=%d replacements=%d deaths=%d phase=%d round=%d teamAlive=%d"),
                    Human->GetPlayerId(), IsAlive, S.Replacements, Human->GetDeaths(), int32(Rounds->GetCurrentMatchPhase()),
                    Rounds->GetCurrentRoundNumber(), GS->GetTeamInfo(GS->GetPlayerTeam(Human.Get())).AlivePlayerCount);
            }
            if (!PackagedUpgrade) return Stop(TEXT("observed"));
            Stage = 5; StageStarted = Now;
        }
        if (PackagedUpgrade && Stage == 5 && Now - StageStarted >= 5)
        {
            const auto Phase = Rounds->GetCurrentMatchPhase();
            if (Phase == EBwayMatchPhase::Playing)
            {
                Rounds->EndRound(0, EBwayWinCondition::GoalScored);
                StageStarted = Now;
            }
            else if (Phase == EBwayMatchPhase::PostMatch)
            {
                GS->RestartMatchFromResults();
                Stage = 6; StageStarted = Now;
                UE_LOG(LogTemp, Display, TEXT("BwayRespawnUpgrade: Rematch requested=1 source=production"));
            }
        }
        return true;
    }
};
FAutoConsoleCommand Command(TEXT("bway.Test.RespawnAudit"), TEXT("Development: observe two human/bot deaths and elimination cleanup; modifies an owned test match."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (Running) return;
        Running = true;
        TSharedRef<FRun> Run = MakeShared<FRun>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.05f);
    }));
FAutoConsoleCommand HUDCommand(TEXT("bway.Test.RespawnHUDAudit"), TEXT("Development: kill all four humans twice and inspect each local countdown, reconstruction and round cleanup."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (Running) return;
        Running = true;
        TSharedRef<FRun> Run = MakeShared<FRun>();
        Run->AuditHUD = true;
        if (auto* Trace = IConsoleManager::Get().FindConsoleVariable(TEXT("bway.Test.RespawnTimerTrace")))
        {
            Run->PreviousTimerTrace = Trace->GetInt();
            Trace->Set(1, ECVF_SetByCode);
        }
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.05f);
    }));
FAutoConsoleCommand UpgradeCommand(TEXT("bway.Test.RespawnUpgradeAudit"), TEXT("Development: remote purchases during real deaths, peer deadlines and round persistence; resets fixture-owned upgrades and gold."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (Running) return;
        Running = true;
        TSharedRef<FRun> Run = MakeShared<FRun>();
        Run->AuditUpgrade = true;
        if (auto* Trace = IConsoleManager::Get().FindConsoleVariable(TEXT("bway.Test.RespawnTimerTrace")))
        {
            Run->PreviousTimerTrace = Trace->GetInt();
            Trace->Set(1, ECVF_SetByCode);
        }
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.05f);
    }));
FAutoConsoleCommand PackagedUpgradeCommand(TEXT("bway.Test.RespawnUpgradePackagedAudit"), TEXT("Development: remote RPC purchases during death, rounds and production rematch; packaged four-player fixture."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (Running) return;
        Running = true;
        TSharedRef<FRun> Run = MakeShared<FRun>();
        Run->AuditUpgrade = true;
        Run->PackagedUpgrade = true;
        if (auto* Trace = IConsoleManager::Get().FindConsoleVariable(TEXT("bway.Test.RespawnTimerTrace")))
        {
            Run->PreviousTimerTrace = Trace->GetInt();
            Trace->Set(1, ECVF_SetByCode);
        }
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.05f);
    }));
}
#endif
