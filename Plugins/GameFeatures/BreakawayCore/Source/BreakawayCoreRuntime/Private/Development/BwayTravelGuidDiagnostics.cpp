#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/PackageMapClient.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/PlatformTime.h"
#include "Logging/LogMacros.h"
#include "Misc/CoreDelegates.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace BwayTravelGuidDiagnostics
{
struct FObserver
{
	FTSTicker::FDelegateHandle InitTickHandle;
	FTSTicker::FDelegateHandle TickHandle;
	FDelegateHandle SeamlessStartHandle;
	FDelegateHandle SeamlessTransitionHandle;
	FDelegateHandle CleanupHandle;
	FDelegateHandle PostWorldInitHandle;
	FDelegateHandle ActorsInitHandle;
	FDelegateHandle GarbageHandle;
	TArray<TWeakObjectPtr<UNetDriver>> Drivers;
	TArray<TStrongObjectPtr<UObject>> PinnedStaticObjects;
	TMap<FString, FString> Previous;
	TSet<FString> SelectedKeys;
	double Deadline = 0.0;
	double PinReleaseTime = 0.0;
	bool bActive = false;
	bool bHooksInstalled = false;
	bool bScopedLogNetPackageMapElevated = false;
	int32 ChangeCount = 0;
	int32 SanitizedCount = 0;
	int32 PinnedCount = 0;

	void SetScopedLogNetPackageMap(bool bElevate)
	{
#if !UE_BUILD_SHIPPING
		if (!GEngine || bScopedLogNetPackageMapElevated == bElevate)
		{
			return;
		}
		bScopedLogNetPackageMapElevated = bElevate;
		GEngine->Exec(nullptr, bElevate ? TEXT("Log LogNetPackageMap Log") : TEXT("Log LogNetPackageMap Warning"));
		UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: ScopedLogNetPackageMap verbosity=%s"),
			bElevate ? TEXT("Log") : TEXT("Warning"));
#else
		UE_UNUSED(bElevate);
#endif
	}

	void GatherDrivers()
	{
		if (!GEngine)
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
	}

	void PinStaticCacheObjectsForDriver(UNetDriver* Driver, const TCHAR* Reason)
	{
		if (!Driver || !Driver->GetNetGuidCache().IsValid())
		{
			return;
		}
		FNetGUIDCache& Cache = *Driver->GetNetGuidCache();
		int32 NewlyPinned = 0;
		for (const auto& Pair : Cache.ObjectLookup)
		{
			if (!Pair.Key.IsStatic())
			{
				continue;
			}
			UObject* Object = Pair.Value.Object.Get();
			if (!IsValid(Object))
			{
				continue;
			}
			UPackage* Outermost = Object->GetOutermost();
			if (!Outermost || Outermost->ContainsMap())
			{
				continue;
			}
			PinnedStaticObjects.AddUnique(TStrongObjectPtr<UObject>(Object));
			PinnedStaticObjects.AddUnique(TStrongObjectPtr<UObject>(Outermost));
			++NewlyPinned;
		}
		if (NewlyPinned > 0)
		{
			PinnedCount += NewlyPinned;
			PinReleaseTime = FPlatformTime::Seconds() + 30.0;
#if !UE_BUILD_SHIPPING
			UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: event=pin-static reason=%s driver=%s world=%s pinned=%d totalPinned=%d"),
				Reason, *Driver->GetName(), *GetPathNameSafe(Driver->GetWorld()), NewlyPinned, PinnedStaticObjects.Num());
#endif
		}
	}

	void SanitizePostTravelGuidCache(UWorld* WorldContext, const TCHAR* Event)
	{
		GatherDrivers();
		if (WorldContext && WorldContext->GetNetDriver())
		{
			Drivers.AddUnique(WorldContext->GetNetDriver());
		}

		for (const TWeakObjectPtr<UNetDriver>& WeakDriver : Drivers)
		{
			UNetDriver* Driver = WeakDriver.Get();
			if (!Driver || !Driver->GetNetGuidCache().IsValid())
			{
				continue;
			}
			FNetGUIDCache& Cache = *Driver->GetNetGuidCache();
			for (auto& Pair : Cache.ObjectLookup)
			{
				FNetGuidCacheObject& Entry = Pair.Value;
				if (Entry.ReadOnlyTimestamp <= 0.0)
				{
					continue;
				}
				const bool bValidReverse = !Entry.Object.IsValid()
					|| Cache.NetGUIDLookup.FindRef(Entry.Object) == Pair.Key;
				if (!bValidReverse)
				{
					continue;
				}
				const double PreviousReadOnly = Entry.ReadOnlyTimestamp;
				Entry.ReadOnlyTimestamp = 0.0;
				++SanitizedCount;
#if !UE_BUILD_SHIPPING
				const FString GuidPath = Cache.FullNetGUIDPath(Pair.Key);
				UE_LOG(LogTemp, Log,
					TEXT("BwayTravelGuid: event=%s sanitized=1 driver=%s world=%s guid=%s static=%d prevReadonly=%.6f path=%s"),
					Event, *Driver->GetName(), *GetPathNameSafe(Driver->GetWorld()),
					*Pair.Key.ToString(), Pair.Key.IsStatic(), PreviousReadOnly, *GuidPath);
#endif
			}
		}
	}

	void QuiesceOutgoingPlayerStates(UWorld* World, const TCHAR* Reason)
	{
		if (!World || !World->IsGameWorld() || World->GetNetMode() >= NM_Client)
		{
			return;
		}
		AGameStateBase* GS = World->GetGameState();
		if (!GS)
		{
			return;
		}
		int32 Quiesced = 0;
		for (APlayerState* PS : GS->PlayerArray)
		{
			if (IsValid(PS))
			{
				PS->SetReplicates(false);
				World->RemoveNetworkActor(PS);
				++Quiesced;
			}
		}
#if !UE_BUILD_SHIPPING
		if (Quiesced > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: event=quiesce-outgoing-playerstates reason=%s world=%s count=%d"),
				Reason, *GetPathNameSafe(World), Quiesced);
		}
#endif
	}

	void Sample(const TCHAR* Event)
	{
#if !UE_BUILD_SHIPPING
		if (!bActive || !GEngine)
		{
			return;
		}
		GatherDrivers();
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
				if (!SelectedKeys.Contains(Key)
					&& !GuidPath.Contains(TEXT("BwayPlayerState"))
					&& !GuidPath.Contains(TEXT("BP_BW_GameState")))
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
#else
		UE_UNUSED(Event);
#endif
	}

	void Stop(const TCHAR* Reason)
	{
		if (!bActive)
		{
			return;
		}
		Sample(TEXT("stop"));
		SetScopedLogNetPackageMap(false);
		bActive = false;
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
		FCoreUObjectDelegates::GetPostGarbageCollect().Remove(GarbageHandle);
		GarbageHandle.Reset();
		Drivers.Reset();
		Previous.Reset();
		SelectedKeys.Reset();
		PinnedStaticObjects.Reset();
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: STOP reason=%s changes=%d sanitized=%d pinned=%d callbacksRemoved=1"),
			Reason, ChangeCount, SanitizedCount, PinnedCount);
#else
		UE_UNUSED(Reason);
#endif
	}

	void Start(double DurationSeconds = 120.0, const TCHAR* TriggerReason = TEXT("manual"))
	{
		InstallTravelHooks();
		if (bActive)
		{
			Deadline = FMath::Max(Deadline, FPlatformTime::Seconds() + DurationSeconds);
			return;
		}
		bActive = true;
		ChangeCount = 0;
		SanitizedCount = 0;
		PinnedCount = 0;
		Deadline = FPlatformTime::Seconds() + DurationSeconds;
		SetScopedLogNetPackageMap(true);
		GarbageHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddLambda([this]()
		{
			SanitizePostTravelGuidCache(nullptr, TEXT("post-gc"));
			Sample(TEXT("post-gc"));
		});
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this](float)
		{
			const double Now = FPlatformTime::Seconds();
			if (PinReleaseTime > 0.0 && Now >= PinReleaseTime)
			{
				PinnedStaticObjects.Reset();
				PinReleaseTime = 0.0;
			}
			if (Now >= Deadline)
			{
				Stop(TEXT("timeout"));
				return false;
			}
			Sample(TEXT("tick"));
			return true;
		}));
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: START reason=%s timeoutSeconds=%.0f readOnly=0"),
			TriggerReason, DurationSeconds);
#endif
		Sample(TEXT("start"));
	}

	void InstallTravelHooks()
	{
		if (bHooksInstalled)
		{
			return;
		}
		bHooksInstalled = true;

		SeamlessStartHandle = FWorldDelegates::OnSeamlessTravelStart.AddLambda([this](UWorld* CurrentWorld, const FString&)
		{
			Start(45.0, TEXT("seamless-travel-start"));
			QuiesceOutgoingPlayerStates(CurrentWorld, TEXT("OnSeamlessTravelStart"));
			if (CurrentWorld && CurrentWorld->GetNetDriver())
			{
				PinStaticCacheObjectsForDriver(CurrentWorld->GetNetDriver(), TEXT("OnSeamlessTravelStart"));
			}
			Sample(TEXT("seamless-start"));
		});

		SeamlessTransitionHandle = FWorldDelegates::OnSeamlessTravelTransition.AddLambda([this](UWorld* CurrentWorld)
		{
			if (CurrentWorld && CurrentWorld->GetNetDriver())
			{
				PinStaticCacheObjectsForDriver(CurrentWorld->GetNetDriver(), TEXT("OnSeamlessTravelTransition"));
			}
			SanitizePostTravelGuidCache(CurrentWorld, TEXT("seamless-transition"));
			Sample(TEXT("seamless-transition"));
		});

		CleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda([this](UWorld* World, bool, bool)
		{
			if (!World || !World->IsGameWorld())
			{
				return;
			}
			if (World->GetNetDriver())
			{
				if (!bActive && World->GetMapName().Contains(TEXT("HeroSelect")))
				{
					Start(45.0, TEXT("client-staging-world-cleanup"));
				}
				PinStaticCacheObjectsForDriver(World->GetNetDriver(), TEXT("OnWorldCleanup"));
			}
			Sample(TEXT("world-cleanup"));
		});

		PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddLambda([this](UWorld* World, const UWorld::InitializationValues)
		{
			if (!World || !World->IsGameWorld())
			{
				return;
			}
			SanitizePostTravelGuidCache(World, TEXT("post-world-init"));
			Sample(TEXT("post-world-init"));
		});

		ActorsInitHandle = FWorldDelegates::OnWorldInitializedActors.AddLambda([this](const UWorld::FActorsInitializedParams& Params)
		{
			UWorld* World = Params.World;
			if (!World || !World->IsGameWorld())
			{
				return;
			}
			SanitizePostTravelGuidCache(World, TEXT("world-actors-init"));
			Sample(TEXT("world-actors-init"));
			if (World->GetMapName().Contains(TEXT("Dorado")))
			{
				SetScopedLogNetPackageMap(false);
				PinReleaseTime = FPlatformTime::Seconds() + 5.0;
				if (bActive)
				{
					Deadline = FMath::Min(Deadline, FPlatformTime::Seconds() + 8.0);
				}
			}
		});
	}
};

FObserver Observer;

struct FAutoHookInstaller
{
	FAutoHookInstaller()
	{
		if (GEngine)
		{
			Observer.InstallTravelHooks();
		}
		FCoreDelegates::OnPostEngineInit.AddLambda([]()
		{
			Observer.InstallTravelHooks();
		});
	}
} AutoHookInstaller;

#if !UE_BUILD_SHIPPING
FAutoConsoleCommand Command(
	TEXT("bway.Test.TravelGuidWatch"),
	TEXT("Observe PlayerState and GameState NetGUID lifetime for 120 seconds; pass 0 to stop."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() > 0 && Args[0] == TEXT("0"))
		{
			Observer.Stop(TEXT("requested"));
		}
		else
		{
			Observer.Start(120.0, TEXT("console"));
		}
	}));
#endif
}
