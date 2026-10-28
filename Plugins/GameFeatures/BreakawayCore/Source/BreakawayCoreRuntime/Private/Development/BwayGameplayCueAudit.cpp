#if !UE_BUILD_SHIPPING
#include "AbilitySystem/BwayGameplayCueTags.h"
#include "AbilitySystemGlobals.h"
#include "BwayGameState.h"
#include "BwayPlayerState.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "GameFramework/Pawn.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "GameplayTagsManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "GameState/BwayRoundManagementComponent.h"

namespace BwayGameplayCueAudit
{
static TAutoConsoleVariable<float> CVarCueAuditTimeoutSeconds(
    TEXT("bway.CueAudit.TimeoutSeconds"),
    180.0f,
    TEXT("Maximum lifetime of the development gameplay-cue observer. "
         "Raise this only for long-running fixtures whose later production actions need observation."),
    ECVF_Default);

TArray<FGameplayTag> Tags()
{
    FGameplayTagContainer Dictionary;
    UGameplayTagsManager::Get().RequestAllGameplayTags(Dictionary, true);
    const FGameplayTag Root = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Breakaway"));
    TArray<FGameplayTag> Result;
    for (const FGameplayTag Tag : Dictionary)
    {
        if (Tag.MatchesTag(Root)) Result.Add(Tag);
    }
    const FGameplayTag LegacyPistol = FGameplayTag::RequestGameplayTag(TEXT("GameplayCue.Weapon.Pistol.Fire"), false);
    if (LegacyPistol.IsValid()) Result.AddUnique(LegacyPistol);
    return Result;
}
struct FObserver : TSharedFromThis<FObserver>
{
    TWeakObjectPtr<UGameplayCueManager> Manager;
    FDelegateHandle RouteHandle, ExitHandle;
    double Started = FPlatformTime::Seconds();
    bool Stopped = false, Mapped = false;
    int32 Routed = 0;
    TArray<FGameplayTag> ObservedTags;

    void Stop(const TCHAR* Reason)
    {
        if (Stopped) return;
        Stopped = true;
        if (Manager.IsValid()) Manager->OnGameplayCueRouted().Remove(RouteHandle);
        FCoreDelegates::OnPreExit.Remove(ExitHandle);
        UE_LOG(LogTemp, Display, TEXT("BwayCueAudit: Complete reason=%s mapped=%d routed=%d delegateRemoved=1"), Reason, Mapped, Routed);
    }
    void Start()
    {
        Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
        if (!Manager.IsValid()) { Stop(TEXT("missing-manager")); return; }
        ObservedTags = Tags();
        const TWeakPtr<FObserver> Weak = AsShared();
        ExitHandle = FCoreDelegates::OnPreExit.AddLambda([Weak]() { if (auto Run = Weak.Pin()) Run->Stop(TEXT("process-exit")); });
        RouteHandle = Manager->OnGameplayCueRouted().AddLambda([Weak](AActor* Target, FGameplayTag Tag, EGameplayCueEvent::Type Event, const FGameplayCueParameters& Params, EGameplayCueExecutionOptions)
        {
            auto Run = Weak.Pin();
            if (!Run || Run->Stopped || !Target || !Run->ObservedTags.Contains(Tag)) return;
            auto* Pawn = Cast<APawn>(Target);
            auto* PS = Pawn ? Pawn->GetPlayerState<ABwayPlayerState>() : nullptr;
            const auto* Driver = Target->GetWorld()->GetNetDriver();
            const uint64 Id = Driver && Driver->GuidCache ? Driver->GuidCache->GetNetGUID(Target).ObjectId : 0;
            bool Ready = false;
            if (const UGameplayCueSet* Set = Run->Manager->GetRuntimeCueSet())
            {
                for (const auto& Entry : Set->GameplayCueData) if (Entry.GameplayCueTag == Tag)
                {
                    // The engine fills LoadedGameplayCueClass lazily. ResolveObject only
                    // observes resident objects; it must not load the class for this probe.
                    Ready = (Entry.LoadedGameplayCueClass != nullptr || Cast<UClass>(Entry.GameplayCueNotifyObj.ResolveObject()) != nullptr)
                        && !Run->Manager->IsAsyncLoadingGameplayCueNotifyClass(Entry.GameplayCueNotifyObj);
                }
            }
            ++Run->Routed;
            UE_LOG(LogTemp, Display, TEXT("BwayCueAudit: Routed tag=%s event=%d player=%d target=%llu net=%d local=%d instigatorMatches=%d ready=%d count=%d world=%s"),
                *Tag.ToString(), int32(Event), PS ? PS->GetPlayerId() : -1, Id, int32(Target->GetNetMode()), Pawn && Pawn->IsLocallyControlled(), Params.Instigator.Get() == Target, Ready, Run->Routed, *Target->GetWorld()->GetPathName());
        });
        UE_LOG(LogTemp, Display, TEXT("BwayCueAudit: Started timeoutSeconds=%.1f executedEvent=%d"),
            FMath::Max(1.0f, CVarCueAuditTimeoutSeconds.GetValueOnGameThread()), int32(EGameplayCueEvent::Executed));
    }
    bool Tick(float)
    {
        if (Stopped) return false;
        const double TimeoutSeconds = FMath::Max(1.0f, CVarCueAuditTimeoutSeconds.GetValueOnGameThread());
        if (FPlatformTime::Seconds() - Started > TimeoutSeconds) { Stop(TEXT("timeout")); return false; }
        if (!Mapped && Manager.IsValid()) for (const auto& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            auto* GS = World && World->IsGameWorld() ? World->GetGameState<ABwayGameState>() : nullptr;
            if (!GS || !GS->GetRoundManagement() || GS->GetRoundManagement()->GetCurrentMatchPhase() != EBwayMatchPhase::Playing) continue;
            const UGameplayCueSet* Set = Manager->GetRuntimeCueSet();
            if (!Set) continue;
            for (FGameplayTag Tag : ObservedTags)
            {
                int32 Count = 0; FString Path = TEXT("None"); bool Loaded = false, Cached = false, Pending = false;
                for (const auto& Entry : Set->GameplayCueData) if (Entry.GameplayCueTag == Tag)
                {
                    ++Count; Path = Entry.GameplayCueNotifyObj.ToString();
                    Cached = Entry.LoadedGameplayCueClass != nullptr;
                    Loaded = Cached || Cast<UClass>(Entry.GameplayCueNotifyObj.ResolveObject()) != nullptr;
                    Pending = Manager->IsAsyncLoadingGameplayCueNotifyClass(Entry.GameplayCueNotifyObj);
                }
                UE_LOG(LogTemp, Display, TEXT("BwayCueAudit: Map tag=%s valid=%d count=%d cue=%s loaded=%d cached=%d pending=%d net=%d"), *Tag.ToString(), Tag.IsValid(), Count, *Path, Loaded, Cached, Pending, int32(World->GetNetMode()));
            }
            Mapped = true;
            break;
        }
        return true;
    }
};
TSharedPtr<FObserver> Active;
FAutoConsoleCommand StopObserve(TEXT("bway.Test.CueAudit.Stop"), TEXT("Remove the owned cue observer after a fixture."), FConsoleCommandDelegate::CreateLambda([]()
{
    if (Active) Active->Stop(TEXT("fixture-cleanup"));
}));
FAutoConsoleCommand Observe(TEXT("bway.Test.CueAudit"), TEXT("Observe actual cue routes and production cue registry for 180s; does not load cue classes or dispatch synthetic cues."), FConsoleCommandDelegate::CreateLambda([]()
{
    if (Active && !Active->Stopped) return;
    Active = MakeShared<FObserver>(); Active->Start();
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run = Active](float Delta) { return Run->Tick(Delta); }), .2f);
}));
}
#endif
