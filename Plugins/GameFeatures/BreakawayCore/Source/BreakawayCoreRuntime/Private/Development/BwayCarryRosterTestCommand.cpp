#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HeroSystems/BwayHeroRegistry.h"
#include "HeroSystems/BwayHeroSelectionManager.h"

namespace BwayCarryRosterTest
{
bool Running = false;
FAutoConsoleCommand Command(TEXT("bway.Test.SelectDistinctCarryRoster"),
    TEXT("Development carry fixture: [remote-argus] selects four distinct human heroes, then fills unique team bot slots and locks through production selection."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        if (Args.Num() > 1 || (Args.Num() == 1 && Args[0] != TEXT("remote-argus")))
        { UE_LOG(LogTemp, Error, TEXT("BwayCarryRoster: STOP reason=invalid-roster-option")); return; }
        const bool bRemoteArgus = Args.Num() == 1;
        if (Running) return;
        Running = true;
        const double Started = FPlatformTime::Seconds();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Started, bRemoteArgus](float)
        {
            auto Stop = [](const TCHAR* Reason)
            {
                UE_LOG(LogTemp, Display, TEXT("BwayCarryRoster: STOP reason=%s"), Reason);
                Running = false;
                return false;
            };
            if (FPlatformTime::Seconds() - Started > 150.0) return Stop(TEXT("timeout"));
            if (!GEngine) return true;
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                UWorld* World = Context.World();
                if (!World || !World->IsGameWorld() || World->GetNetMode() != NM_ListenServer || !World->GetMapName().Contains(TEXT("HeroSelect_Staging"))) continue;
                auto* GS = World->GetGameState<ABwayGameState>();
                auto* Manager = GS ? GS->HeroSelectionManager.Get() : nullptr;
                if (!Manager || !Manager->IsSelectionActive()) continue;
                TArray<ABwayPlayerState*> Humans, Bots;
                for (APlayerState* Player : GS->PlayerArray)
                    if (auto* PS = Cast<ABwayPlayerState>(Player); PS && !PS->IsInactive())
                        (PS->IsABot() ? Bots : Humans).Add(PS);
                // The LAN staging map has humans only; its bots spawn in Dorado.
                if (Humans.Num() != 4) continue;
                Humans.Sort([](const ABwayPlayerState& A, const ABwayPlayerState& B) { return A.GetPlayerId() < B.GetPlayerId(); });
                FName Names[] = {TEXT("DA_BW_HeroData_Argus"), TEXT("DA_BW_HeroData_Gunslinger"), TEXT("DA_BW_HeroData_Hexweaver"), TEXT("DA_BW_HeroData_Alona")};
                if (bRemoteArgus) Swap(Names[0], Names[1]);
                for (FName Name : Names)
                    if (!UBwayHeroRegistry::GetHeroDataById(FPrimaryAssetId(TEXT("HeroDataAsset"), Name))) return true;
                TMap<int32, TSet<FName>> Used;
                TArray<ABwayPlayerState*> All = Humans;
                All.Append(Bots);
                for (auto* PS : All) if (GS->GetPlayerTeam(PS) < 0) return true;
                // Temporarily permit reassignment while replacing the fixture roster.
                // Final teams remain unique, and the original rule is restored before locking.
                {
                    TGuardValue<bool> AllowReassignment(Manager->bAllowDuplicateHeroes, true);
                    for (auto* PS : All) PS->UnlockHeroSelection();
                    for (int32 Index = 0; Index < All.Num(); ++Index)
                    {
                        auto* PS = All[Index];
                        const int32 Team = GS->GetPlayerTeam(PS);
                        FName Name = NAME_None;
                        if (Index < 4) Name = Names[Index];
                        else for (FName Candidate : Names) if (!Used.FindOrAdd(Team).Contains(Candidate)) { Name = Candidate; break; }
                        if (Name.IsNone() || !PS->TrySetSelectedHeroId(FPrimaryAssetId(TEXT("HeroDataAsset"), Name))) return Stop(TEXT("selection-rejected"));
                        Used.FindOrAdd(Team).Add(Name);
                        UE_LOG(LogTemp, Display, TEXT("BwayCarryRoster: Selected id=%d human=%d team=%d hero=%s"), PS->GetPlayerId(), !PS->IsABot(), Team, *Name.ToString());
                    }
                }
                for (auto* PS : All) PS->ServerLockHeroSelection();
                return Stop(TEXT("selected-roster-locked"));
            }
            return true;
        }), 0.25f);
    }));
}
#endif
