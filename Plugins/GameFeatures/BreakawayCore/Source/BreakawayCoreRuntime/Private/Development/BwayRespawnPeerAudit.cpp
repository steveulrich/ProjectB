// Development-only read-only journal for the packaged respawn transport gate.
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "BwayPlayerController.h"
#include "AbilitySystem/LyraAbilitySystemComponent.h"
#include "Attributes/BwayHeroAttributeSet.h"
#include "Economy/BwayGoldAttributeSet.h"
#include "Economy/BwayUpgradeCatalog.h"
#include "Economy/BwayUpgradeComponent.h"
#include "Character/LyraHealthComponent.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameState/BwayRoundManagementComponent.h"
#include "HAL/IConsoleManager.h"
#include "UI/BwayRespawnHUDWidget.h"
#include "UObject/UObjectIterator.h"

namespace BwayRespawnPeerAudit
{
bool Running = false;
struct FRun
{
    double Started = FPlatformTime::Seconds(), SettledAt = 0, FinishedAt = 0;
    int32 Sample = 0, InitialRound = 0, LocalId = INDEX_NONE;
    bool SawPostRound = false, SawSecondRoundDeath = false;
    bool AuditUpgrade = false, SawResults = false, SawRematch = false;
    TWeakObjectPtr<ABwayGameState> OriginalState;
    FString StableKey;
    TMap<int32, int32> Round2BaselineDeaths;
    TWeakObjectPtr<UWorld> ObservedWorld;
    bool AuditHUD = false, ReattachedHUD = false, OpenedShop = false, ClosedShop = false;
    int32 HUDCycle = 0;
    double LastHUDDeadline = 0, HUDCycleStarted = 0;
    TWeakObjectPtr<ABwayPlayerController> ShopOwner;

    void ObserveHUD(UWorld* World, APlayerController* PC, ABwayGameState* GS, UBwayRoundManagementComponent* Round, double Now)
    {
        auto* PS = PC->GetPlayerState<ABwayPlayerState>();
        if (!PS) return;
        const auto Schedule = PS->GetRespawnSchedule();
        if (Round->GetCurrentRoundNumber() == InitialRound && Schedule.ServerEndTime > 0 && Schedule.ServerEndTime != LastHUDDeadline)
        {
            LastHUDDeadline = Schedule.ServerEndTime;
            HUDCycleStarted = Now;
            ++HUDCycle;
        }
        const int32 Stage = Round->GetCurrentRoundNumber() > InitialRound ? 4
            : Round->GetCurrentMatchPhase() == EBwayMatchPhase::PostRound ? 3 : HUDCycle > 0 ? HUDCycle : -1;
        UBwayRespawnHUDWidget* HUD = nullptr;
        int32 Count = 0;
        for (TObjectIterator<UBwayRespawnHUDWidget> Widget; Widget; ++Widget)
            if (Widget->GetOwningPlayer() == PC && Widget->IsInViewport()) { HUD = *Widget; ++Count; }
        UE_LOG(LogTemp, Display, TEXT("BwayRespawnHUD: Sample n=%d stage=%d world=%s net=%d id=%d round=%d phase=%d count=%d deadline=%.3f scheduleRound=%d serverTime=%.3f seconds=%d shown=%d focusable=%d menu=%d text=[%s]"),
            Sample, Stage, *World->GetPathName(), int32(World->GetNetMode()), PS->GetPlayerId(), Round->GetCurrentRoundNumber(), int32(Round->GetCurrentMatchPhase()),
            Count, Schedule.ServerEndTime, Schedule.RoundNumber, GS->GetServerWorldTimeSeconds(), HUD ? HUD->GetDisplayedSeconds() : -99,
            HUD && HUD->IsVisible(), HUD && HUD->IsFocusable(), HUD && HUD->IsSuppressedByMenu(), HUD ? *HUD->GetDisplayedStatus().ToString() : TEXT("missing"));
        if (Stage == 1 && HUD && HUD->IsVisible() && Now - HUDCycleStarted > 0.65 && !ReattachedHUD)
        {
            ReattachedHUD = true;
            HUD->RemoveFromParent();
            HUD->AddToPlayerScreen(20);
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnHUD: Reattached id=%d world=%s"), LocalId, *World->GetPathName());
        }
        auto* BwayPC = Cast<ABwayPlayerController>(PC);
        if (Stage == 2 && Schedule.ServerEndTime > 0 && HUD && BwayPC && Now - HUDCycleStarted > 0.35 && !OpenedShop && !HUD->IsSuppressedByMenu())
        {
            BwayPC->ToggleUpgradeShop();
            OpenedShop = true; ShopOwner = BwayPC;
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnHUD: ShopOpen id=%d world=%s"), LocalId, *World->GetPathName());
        }
        if (OpenedShop && !ClosedShop && ShopOwner.IsValid() && Now - HUDCycleStarted > 1.25)
        {
            ShopOwner->ToggleUpgradeShop();
            ClosedShop = true;
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnHUD: ShopClose id=%d world=%s"), LocalId, *World->GetPathName());
        }
    }

    bool Stop(const TCHAR* Reason)
    {
        if (OpenedShop && !ClosedShop && ShopOwner.IsValid()) ShopOwner->ToggleUpgradeShop();
        if (FCString::Strcmp(Reason, TEXT("observed")) == 0)
        {
            UE_LOG(LogTemp, Display, TEXT("%s: COMPLETE reason=observed local=%d samples=%d sawPostRound=%d sawResults=%d sawRematch=%d"),
                AuditUpgrade ? TEXT("BwayRespawnUpgradePeer") : TEXT("BwayRespawnPeer"), LocalId, Sample, SawPostRound, SawResults, SawRematch);
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("BwayRespawnPeer: FAIL reason=%s local=%d samples=%d"), Reason, LocalId, Sample);
        }
        Running = false;
        return false;
    }

    bool Tick(float)
    {
        const double Now = FPlatformTime::Seconds();
        if (Now - Started > (AuditUpgrade ? 400 : 220)) return Stop(TEXT("timeout"));
        // Keep the connection alive until after the server's bounded observer cleanup.
        if (FinishedAt > 0) return Now - FinishedAt >= 20 ? Stop(TEXT("observed")) : true;
        UWorld* World = AuditUpgrade ? nullptr : ObservedWorld.Get();
        if (!World && GEngine) for (const auto& Context : GEngine->GetWorldContexts())
        {
            UWorld* Candidate = Context.World();
            if (Candidate && Candidate->IsGameWorld() && Candidate->GetNetMode() == NM_Client
                && (AuditUpgrade || Candidate->GetMapName().Contains(TEXT("L_BW_Dorado"))))
            {
                World = Candidate; break;
            }
        }
        auto* GS = World ? World->GetGameState<ABwayGameState>() : nullptr;
        auto* Round = GS ? GS->FindComponentByClass<UBwayRoundManagementComponent>() : nullptr;
        APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
        if (!GS || !Round || !PC || !PC->PlayerState || (!AuditUpgrade && GS->PlayerArray.Num() != 8)) return true;
        const int32 Number = Round->GetCurrentRoundNumber();
        const auto Phase = Round->GetCurrentMatchPhase();
        if ((!AuditUpgrade || InitialRound == 0) && (Number < 1 || (Phase != EBwayMatchPhase::Playing && Phase != EBwayMatchPhase::PostRound))) return true;
        if (InitialRound == 0)
        {
            ObservedWorld = World; InitialRound = Number; LocalId = PC->PlayerState->GetPlayerId();
            OriginalState = GS;
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnPeer: Setup local=%d world=%s initialRound=%d"), LocalId, *World->GetPathName(), InitialRound);
        }
        ++Sample;
        if (AuditUpgrade)
        {
            SawResults |= Phase == EBwayMatchPhase::PostMatch;
            SawRematch |= SawResults && GS != OriginalState.Get();
        }
        if (AuditHUD) ObserveHUD(World, PC, GS, Round, Now);
        bool AllAlive = true;
        int32 Humans = 0, ResetHumans = 0;
        FString Key = FString::Printf(TEXT("%d:%d"), Number, int32(Phase));
        for (APlayerState* Player : GS->PlayerArray)
        {
            auto* PS = Cast<ABwayPlayerState>(Player);
            APawn* Pawn = PS ? PS->GetPawn() : nullptr;
            auto* Health = ULyraHealthComponent::FindHealthComponent(Pawn);
            const bool Alive = Health && !Health->IsDeadOrDying() && Health->GetHealth() > 0;
            AllAlive &= Alive;
            if (PS && Number == InitialRound + 1)
            {
                if (const int32* Before = Round2BaselineDeaths.Find(PS->GetPlayerId()))
                    SawSecondRoundDeath |= PS->GetDeaths() > *Before;
            }
            const int32 Team = PS ? GS->GetPlayerTeam(PS) : INDEX_NONE;
            if (AuditUpgrade && PS)
            {
                const auto* ASC = PS->GetLyraAbilitySystemComponent();
                const auto* Hero = ASC ? ASC->GetSet<UBwayHeroAttributeSet>() : nullptr;
                const auto* Gold = ASC ? ASC->GetSet<UBwayGoldAttributeSet>() : nullptr;
                const UBwayUpgradeComponent* Shop = PS->UpgradeComponent;
                const auto* Catalog = Shop ? Shop->GetCatalog() : nullptr;
                const auto Schedule = PS->GetRespawnSchedule();
                const bool Owner = PS == PC->PlayerState;
                UBwayRespawnHUDWidget* HUD = nullptr;
                int32 HUDCount = 0;
                if (Owner) for (TObjectIterator<UBwayRespawnHUDWidget> Widget; Widget; ++Widget)
                    if (Widget->GetOwningPlayer() == PC && Widget->IsInViewport()) { HUD = *Widget; ++HUDCount; }
                if (!PS->IsABot())
                {
                    ++Humans;
                    ResetHumans += Hero && Shop && Hero->GetRespawnTimeReduction() == 0 && Shop->GetOwnedUpgrades().IsEmpty()
                        && Schedule.ServerEndTime == 0 && PS->GetDeaths() == 0;
                }
                UE_LOG(LogTemp, Display, TEXT("BwayRespawnUpgradePeer: Row sample=%d group=%d world=%s local=%d id=%d owner=%d bot=%d round=%d phase=%d rank=%d owned=%d hasHero=%d reduction=%.6f gold=%.6f deadline=%.6f serverTime=%.6f alive=%d deaths=%d pawn=%s catalog=%d hudCount=%d hudSeconds=%d hudShown=%d"),
                    Sample, SawRematch, *World->GetPathName(), PC->PlayerState->GetPlayerId(), PS->GetPlayerId(), Owner, PS->IsABot(), Number, int32(Phase),
                    Shop ? Shop->GetOwnedRank(TEXT("RespawnTimeReduction")) : -1, Shop ? Shop->GetOwnedUpgrades().Num() : -1, Hero != nullptr,
                    Hero ? Hero->GetRespawnTimeReduction() : -1, Gold ? Gold->GetCurrentGold() : -1,
                    Schedule.ServerEndTime, GS->GetServerWorldTimeSeconds(), Alive, PS->GetDeaths(), *GetNameSafe(Pawn),
                    Catalog ? Catalog->Upgrades.Num() : -1, HUDCount, HUD ? HUD->GetDisplayedSeconds() : -99, HUD && HUD->IsVisible());
            }
            Key += FString::Printf(TEXT("|%d:%s:%d"), PS ? PS->GetPlayerId() : -1, *GetNameSafe(Pawn), Alive);
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnPeer: Row sample=%d local=%d round=%d phase=%d id=%d bot=%d team=%d alive=%d deaths=%d pawn=%s health=%.2f teamAlive=%d"),
                Sample, LocalId, Number, int32(Phase), PS ? PS->GetPlayerId() : -1, PS && PS->IsABot(), Team,
                Alive, PS ? PS->GetDeaths() : -1, *GetNameSafe(Pawn), Health ? Health->GetHealth() : -1,
                Team >= 0 ? GS->GetTeamInfo(Team).AlivePlayerCount : -1);
        }
        SawPostRound |= Phase == EBwayMatchPhase::PostRound;
        if (SawPostRound && Number == InitialRound + 1 && AllAlive && Round2BaselineDeaths.IsEmpty())
        {
            for (APlayerState* Player : GS->PlayerArray)
                if (auto* PS = Cast<ABwayPlayerState>(Player)) Round2BaselineDeaths.Add(PS->GetPlayerId(), PS->GetDeaths());
        }
        const bool Ready = AuditUpgrade
            ? SawResults && SawRematch && Number == 1 && Phase == EBwayMatchPhase::Playing && Humans == 4 && ResetHumans == 4 && AllAlive
            : SawPostRound && SawSecondRoundDeath && Number == InitialRound + 1 && Phase == EBwayMatchPhase::Playing && AllAlive;
        if (!Ready || StableKey != Key || SettledAt == 0)
        {
            StableKey = Key; SettledAt = Ready ? Now : 0;
        }
        if (SettledAt > 0 && Now - SettledAt >= 2)
        {
            FinishedAt = Now;
            UE_LOG(LogTemp, Display, TEXT("BwayRespawnPeer: RoundRecovered local=%d round=%d samples=%d"), LocalId, Number, Sample);
        }
        return true;
    }
};

FAutoConsoleCommand Command(TEXT("bway.Test.RespawnPeerAudit"), TEXT("Development: journal replicated respawn state through the next round; read-only client observer."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (Running) return;
        Running = true;
        TSharedRef<FRun> Run = MakeShared<FRun>();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.25f);
    }));
FAutoConsoleCommand HUDCommand(TEXT("bway.Test.RespawnHUDPeerAudit"), TEXT("Development: observe local countdown and reconstruct/open/close owned UI during the packaged respawn fixture."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (Running) return;
        Running = true;
        TSharedRef<FRun> Run = MakeShared<FRun>();
        Run->AuditHUD = true;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.25f);
    }));
FAutoConsoleCommand UpgradeCommand(TEXT("bway.Test.RespawnUpgradePeerAudit"), TEXT("Development: journal upgrades and countdowns through production rematch in an independent packaged client."),
    FConsoleCommandDelegate::CreateLambda([]
    {
        if (Running) return;
        Running = true;
        TSharedRef<FRun> Run = MakeShared<FRun>();
        Run->AuditUpgrade = true;
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float Delta) { return Run->Tick(Delta); }), 0.1f);
    }));
}
#endif
