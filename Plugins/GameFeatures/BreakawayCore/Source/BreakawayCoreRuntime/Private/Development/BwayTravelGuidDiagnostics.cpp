#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "UObject/UObjectGlobals.h"

// Read-only, opt-in diagnostics. Keep strings and weak drivers only: observing travel
// must not retain outgoing PlayerStates, subobjects, or worlds through collection.
namespace BwayTravelGuidDiagnostics
{
struct FObserver
{
	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle CleanupHandle;
	FDelegateHandle GarbageHandle;
	TArray<TWeakObjectPtr<UNetDriver>> Drivers;
	TMap<FString, FString> Previous;
	TSet<FString> SelectedKeys;
	double Deadline = 0.0;
	bool bActive = false;
	int32 ChangeCount = 0;

	void Sample(const TCHAR* Event)
	{
		if (!bActive || !GEngine)
		{
			return;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && World->IsGameWorld() && World->GetNetDriver())
			{
				Drivers.AddUnique(World->GetNetDriver());
			}
		}
		TMap<FString, FString> Current;
		for (const TWeakObjectPtr<UNetDriver>& WeakDriver : Drivers)
		{
			UNetDriver* Driver = WeakDriver.Get();
			if (!Driver || !Driver->GetNetGuidCache().IsValid())
			{
				continue;
			}
			const FNetGUIDCache& Cache = *Driver->GetNetGuidCache();
			for (const auto& Pair : Cache.ObjectLookup)
			{
				const FString Key = FString::Printf(TEXT("%s:%s"), *Driver->GetName(), *Pair.Key.ToString());
				const FString GuidPath = Cache.FullNetGUIDPath(Pair.Key);
				if (!SelectedKeys.Contains(Key) && !GuidPath.Contains(TEXT("BwayPlayerState")))
				{
					continue;
				}
				SelectedKeys.Add(Key);
				const FNetGuidCacheObject& Entry = Pair.Value;
				UObject* Object = Entry.Object.Get();
				const FString Value = FString::Printf(
					TEXT("driver=%s world=%s guid=%s static=%d readonly=%.6f objectId=%u reverse=%s object=%s path=%s"),
					*Driver->GetName(), *GetPathNameSafe(Driver->GetWorld()), *Pair.Key.ToString(), Pair.Key.IsStatic(),
					Entry.ReadOnlyTimestamp, Object ? Object->GetUniqueID() : 0,
					*Cache.NetGUIDLookup.FindRef(Object).ToString(), *GetPathNameSafe(Object), *GuidPath);
				Current.Add(Key, Value);
				const FString* Old = Previous.Find(Key);
				if (!Old || *Old != Value)
				{
					++ChangeCount;
					UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: event=%s %s"), Event, *Value);
				}
			}
		}
		for (const auto& Pair : Previous)
		{
			if (!Current.Contains(Pair.Key))
			{
				++ChangeCount;
				UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: event=%s removed=%s previous={%s}"), Event, *Pair.Key, *Pair.Value);
			}
		}
		Previous = MoveTemp(Current);
	}

	void Stop(const TCHAR* Reason)
	{
		if (!bActive)
		{
			return;
		}
		Sample(TEXT("stop"));
		bActive = false;
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
		FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
		FCoreUObjectDelegates::GetPostGarbageCollect().Remove(GarbageHandle);
		CleanupHandle.Reset();
		GarbageHandle.Reset();
		Drivers.Reset();
		Previous.Reset();
		SelectedKeys.Reset();
		UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: STOP reason=%s changes=%d callbacksRemoved=1"), Reason, ChangeCount);
	}

	void Start()
	{
		Stop(TEXT("replaced"));
		bActive = true;
		ChangeCount = 0;
		Deadline = FPlatformTime::Seconds() + 120.0;
		CleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda([this](UWorld*, bool, bool) { Sample(TEXT("world-cleanup")); });
		GarbageHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddLambda([this]() { Sample(TEXT("post-gc")); });
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float)
		{
			if (FPlatformTime::Seconds() >= Deadline)
			{
				Stop(TEXT("timeout"));
				return false;
			}
			Sample(TEXT("tick"));
			return true;
		}));
		UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: START timeoutSeconds=120 readOnly=1"));
		Sample(TEXT("start"));
	}
};

FObserver Observer;
FAutoConsoleCommand Command(
	TEXT("bway.Test.TravelGuidWatch"),
	TEXT("Observe PlayerState NetGUID lifetime for 120 seconds; pass 0 to stop. Does not modify replication."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() > 0 && Args[0] == TEXT("0"))
		{
			Observer.Stop(TEXT("requested"));
		}
		else
		{
			Observer.Start();
		}
	}));
}
#endif
