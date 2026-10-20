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

#if WITH_DEV_AUTOMATION_TESTS
#include "GameModes/BwayMatchFlowConfig.h"
#include "Misc/AutomationTest.h"
#endif

namespace BwayTravelGuidDiagnostics
{
bool RestoreLiveGuidEntry(FNetGUIDCache& Cache, const FNetworkGUID& Guid, FNetGuidCacheObject& Entry)
{
	if (Entry.ReadOnlyTimestamp <= 0.0 || !Entry.Object.IsValid()
		|| Cache.NetGUIDLookup.FindRef(Entry.Object) != Guid)
	{
		return false;
	}
	Entry.ReadOnlyTimestamp = 0.0;
	return true;
}

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
	TArray<TWeakObjectPtr<UWorld>> ArrivedWorlds;
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

	/** Scope observation to the game net driver of a world that is actually traveling. The driver
	 * object moves with the seamless travel (staging -> transition -> destination), so a weak
	 * reference keeps the scope across world replacement without touching unrelated drivers
	 * (other PIE instances, beacons, demo drivers, or worlds that never traveled). */
	void TrackTravelingDriver(UWorld* World, const TCHAR* Reason)
	{
		UNetDriver* Driver = World && World->IsGameWorld() ? World->GetNetDriver() : nullptr;
		if (!Driver || Drivers.Contains(Driver))
		{
			return;
		}
		Drivers.Add(Driver);
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: event=track-driver reason=%s driver=%s world=%s netMode=%d tracked=%d"),
			Reason, *Driver->GetName(), *GetPathNameSafe(World), static_cast<int32>(World->GetNetMode()), Drivers.Num());
#else
		UE_UNUSED(Reason);
#endif
	}

	bool IsTrackedDriver(const UNetDriver* Driver) const
	{
		if (!Driver)
		{
			return false;
		}
		for (const TWeakObjectPtr<UNetDriver>& WeakDriver : Drivers)
		{
			if (WeakDriver.Get() == Driver)
			{
				return true;
			}
		}
		return false;
	}

	bool IsTrackedWorld(const UWorld* World) const
	{
		return World && IsTrackedDriver(World->GetNetDriver());
	}

	void PinStaticCacheObjectsForDriver(UNetDriver* Driver, const TCHAR* Reason)
	{
		if (!bActive || !IsTrackedDriver(Driver) || !Driver->GetNetGuidCache().IsValid())
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
		// Only touch caches owned by traveling drivers while observation is active. Outside a
		// travel window the engine's retirement bookkeeping is left completely untouched.
		if (!bActive)
		{
			return;
		}
		if (WorldContext && !IsTrackedWorld(WorldContext))
		{
			return;
		}

		for (const TWeakObjectPtr<UNetDriver>& WeakDriver : Drivers)
		{
			UNetDriver* Driver = WeakDriver.Get();
			if (!Driver || !Driver->GetNetGuidCache().IsValid())
			{
				continue;
			}
			if (WorldContext && Driver != WorldContext->GetNetDriver())
			{
				continue;
			}
			FNetGUIDCache& Cache = *Driver->GetNetGuidCache();
			for (auto& Pair : Cache.ObjectLookup)
			{
				FNetGuidCacheObject& Entry = Pair.Value;
				const double PreviousReadOnly = Entry.ReadOnlyTimestamp;
				if (!RestoreLiveGuidEntry(Cache, Pair.Key, Entry))
				{
					continue;
				}
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
		const int32 TrackedDriverCount = Drivers.Num();
		const int32 HeldPinCount = PinnedStaticObjects.Num();
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
		FCoreUObjectDelegates::GetPostGarbageCollect().Remove(GarbageHandle);
		GarbageHandle.Reset();
		Drivers.Reset();
		ArrivedWorlds.Reset();
		Previous.Reset();
		SelectedKeys.Reset();
		PinnedStaticObjects.Reset();
		PinReleaseTime = 0.0;
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: STOP reason=%s changes=%d sanitized=%d pinned=%d trackedDrivers=%d releasedPins=%d callbacksRemoved=1"),
			Reason, ChangeCount, SanitizedCount, PinnedCount, TrackedDriverCount, HeldPinCount);
#else
		UE_UNUSED(Reason);
		UE_UNUSED(TrackedDriverCount);
		UE_UNUSED(HeldPinCount);
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
#if !UE_BUILD_SHIPPING
				UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: event=release-pins released=%d"), PinnedStaticObjects.Num());
#endif
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
			if (!CurrentWorld || !CurrentWorld->IsGameWorld() || !CurrentWorld->GetNetDriver())
			{
				return;
			}
			Start(45.0, TEXT("seamless-travel-start"));
			TrackTravelingDriver(CurrentWorld, TEXT("OnSeamlessTravelStart"));
			QuiesceOutgoingPlayerStates(CurrentWorld, TEXT("OnSeamlessTravelStart"));
			PinStaticCacheObjectsForDriver(CurrentWorld->GetNetDriver(), TEXT("OnSeamlessTravelStart"));
			Sample(TEXT("seamless-start"));
		});

		SeamlessTransitionHandle = FWorldDelegates::OnSeamlessTravelTransition.AddLambda([this](UWorld* CurrentWorld)
		{
			if (!bActive || !IsTrackedWorld(CurrentWorld))
			{
				return;
			}
			PinStaticCacheObjectsForDriver(CurrentWorld->GetNetDriver(), TEXT("OnSeamlessTravelTransition"));
			SanitizePostTravelGuidCache(CurrentWorld, TEXT("seamless-transition"));
			Sample(TEXT("seamless-transition"));
		});

		CleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda([this](UWorld* World, bool, bool)
		{
			if (!World || !World->IsGameWorld() || !World->GetNetDriver())
			{
				return;
			}
			if (!bActive && World->GetNetMode() == NM_Client && World->GetMapName().Contains(TEXT("HeroSelect")))
			{
				// A client can tear down the staging world before its own travel-start broadcast
				// reaches this module (for example while the travel URL is still pending).
				Start(45.0, TEXT("client-staging-world-cleanup"));
				TrackTravelingDriver(World, TEXT("OnWorldCleanup"));
			}
			if (!bActive || !IsTrackedWorld(World))
			{
				return;
			}
			PinStaticCacheObjectsForDriver(World->GetNetDriver(), TEXT("OnWorldCleanup"));
			Sample(TEXT("world-cleanup"));
		});

		PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddLambda([this](UWorld* World, const UWorld::InitializationValues)
		{
			if (!bActive || !IsTrackedWorld(World))
			{
				return;
			}
			SanitizePostTravelGuidCache(World, TEXT("post-world-init"));
			Sample(TEXT("post-world-init"));
		});

		ActorsInitHandle = FWorldDelegates::OnWorldInitializedActors.AddLambda([this](const UWorld::FActorsInitializedParams& Params)
		{
			UWorld* World = Params.World;
			if (!bActive || !IsTrackedWorld(World))
			{
				return;
			}
			SanitizePostTravelGuidCache(World, TEXT("world-actors-init"));
			Sample(TEXT("world-actors-init"));
			if (World->GetMapName().Contains(TEXT("Dorado")))
			{
				// Same-process PIE shares one observer across peers; keep the window open until
				// every tracked driver (including a slower or delayed-loading client) has finished
				// initializing actors in a destination world during this observation window.
				ArrivedWorlds.AddUnique(World);
				bool bAllTrackedArrived = true;
				for (const TWeakObjectPtr<UNetDriver>& WeakDriver : Drivers)
				{
					const UNetDriver* Driver = WeakDriver.Get();
					UWorld* DriverWorld = Driver ? Driver->GetWorld() : nullptr;
					if (Driver && (!DriverWorld || !ArrivedWorlds.Contains(DriverWorld)))
					{
						bAllTrackedArrived = false;
						break;
					}
				}
#if !UE_BUILD_SHIPPING
				UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: event=destination-arrived world=%s tracked=%d allArrived=%d"),
					*GetPathNameSafe(World), Drivers.Num(), bAllTrackedArrived ? 1 : 0);
#endif
				if (bAllTrackedArrived)
				{
					SetScopedLogNetPackageMap(false);
					PinReleaseTime = FPlatformTime::Seconds() + 5.0;
					Deadline = FMath::Min(Deadline, FPlatformTime::Seconds() + 8.0);
				}
			}
		});
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: hooks installed"));
#endif
	}

	/** Remove every engine callback that captures this observer. Required before the module's
	 * code is unloaded (Game Feature deactivation, live coding module swap, or engine exit). */
	void UninstallTravelHooks(const TCHAR* Reason)
	{
		Stop(Reason);
		if (!bHooksInstalled)
		{
			return;
		}
		bHooksInstalled = false;
		FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessStartHandle);
		FWorldDelegates::OnSeamlessTravelTransition.Remove(SeamlessTransitionHandle);
		FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
		FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitHandle);
		FWorldDelegates::OnWorldInitializedActors.Remove(ActorsInitHandle);
		SeamlessStartHandle.Reset();
		SeamlessTransitionHandle.Reset();
		CleanupHandle.Reset();
		PostWorldInitHandle.Reset();
		ActorsInitHandle.Reset();
#if !UE_BUILD_SHIPPING
		UE_LOG(LogTemp, Log, TEXT("BwayTravelGuid: hooks removed reason=%s"), Reason);
#endif
	}
};

FObserver Observer;
FDelegateHandle PostEngineInitHandle;

void InstallHooks()
{
	if (GEngine)
	{
		Observer.InstallTravelHooks();
		return;
	}
	if (!PostEngineInitHandle.IsValid())
	{
		PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddLambda([]()
		{
			FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
			PostEngineInitHandle.Reset();
			Observer.InstallTravelHooks();
		});
	}
}

void UninstallHooks()
{
	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
	}
	Observer.UninstallTravelHooks(TEXT("module-shutdown"));
}

bool AreHooksInstalled()
{
	return Observer.bHooksInstalled;
}

#if !UE_BUILD_SHIPPING
FAutoConsoleCommand Command(
	TEXT("bway.Test.TravelGuidWatch"),
	TEXT("Observe PlayerState and GameState NetGUID lifetime for 120 seconds on every current game-world driver; pass 0 to stop."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		if (Args.Num() > 0 && Args[0] == TEXT("0"))
		{
			Observer.Stop(TEXT("requested"));
		}
		else
		{
			Observer.Start(120.0, TEXT("console"));
			if (GEngine)
			{
				for (const FWorldContext& Context : GEngine->GetWorldContexts())
				{
					Observer.TrackTravelingDriver(Context.World(), TEXT("console"));
				}
			}
		}
	}));
#endif
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayTravelGuidRetirementTest, "Breakaway.Travel.GuidRetirement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayTravelGuidRetirementTest::RunTest(const FString&)
{
	FNetGUIDCache Cache(nullptr);
	const FNetworkGUID DeadGuid = FNetworkGUID::CreateFromIndex(1, false);
	const FNetworkGUID UnloadedGuid = FNetworkGUID::CreateFromIndex(3, true);
	const FNetworkGUID RetiredGuid = FNetworkGUID::CreateFromIndex(1, true);
	const FNetworkGUID CurrentGuid = FNetworkGUID::CreateFromIndex(2, true);
	const double RetiredTimestamp = FPlatformTime::Seconds() - 91.0;
	TStrongObjectPtr<UObject> LiveObject(NewObject<UBwayMatchFlowConfig>());

	FNetGuidCacheObject DeadEntry;
	DeadEntry.ReadOnlyTimestamp = RetiredTimestamp;
	Cache.ObjectLookup.Add(DeadGuid, DeadEntry);
	Cache.ObjectLookup.Add(UnloadedGuid, DeadEntry);

	FNetGuidCacheObject LiveEntry;
	LiveEntry.Object = LiveObject.Get();
	LiveEntry.ReadOnlyTimestamp = RetiredTimestamp;
	Cache.ObjectLookup.Add(RetiredGuid, LiveEntry);
	Cache.ObjectLookup.Add(CurrentGuid, LiveEntry);
	Cache.NetGUIDLookup.Add(LiveObject.Get(), CurrentGuid);

	TestFalse(TEXT("Dead GUID stays retired"), BwayTravelGuidDiagnostics::RestoreLiveGuidEntry(
		Cache, DeadGuid, Cache.ObjectLookup.FindChecked(DeadGuid)));
	TestEqual(TEXT("Dead GUID keeps its expiry clock"),
		Cache.ObjectLookup.FindChecked(DeadGuid).ReadOnlyTimestamp, RetiredTimestamp);
	TestFalse(TEXT("Unloaded static GUID stays retired"), BwayTravelGuidDiagnostics::RestoreLiveGuidEntry(
		Cache, UnloadedGuid, Cache.ObjectLookup.FindChecked(UnloadedGuid)));
	TestFalse(TEXT("Older alias cannot replace a newer GUID"), BwayTravelGuidDiagnostics::RestoreLiveGuidEntry(
		Cache, RetiredGuid, Cache.ObjectLookup.FindChecked(RetiredGuid)));
	TestTrue(TEXT("Live reciprocal GUID can resume"), BwayTravelGuidDiagnostics::RestoreLiveGuidEntry(
		Cache, CurrentGuid, Cache.ObjectLookup.FindChecked(CurrentGuid)));
	TestEqual(TEXT("Live reciprocal GUID is writable"), Cache.ObjectLookup.FindChecked(CurrentGuid).ReadOnlyTimestamp, 0.0);

	Cache.CleanReferences();
	TestFalse(TEXT("Engine cleanup expires the dead GUID"), Cache.ObjectLookup.Contains(DeadGuid));
	TestFalse(TEXT("Engine cleanup expires the unloaded static GUID"), Cache.ObjectLookup.Contains(UnloadedGuid));
	TestFalse(TEXT("Engine cleanup expires the retired alias"), Cache.ObjectLookup.Contains(RetiredGuid));
	TestTrue(TEXT("Engine cleanup retains the live GUID"), Cache.ObjectLookup.Contains(CurrentGuid));
	TestTrue(TEXT("Reverse lookup retains the current identity"), Cache.NetGUIDLookup.FindRef(LiveObject.Get()) == CurrentGuid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBwayTravelGuidHookLifecycleTest, "Breakaway.Travel.HookLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBwayTravelGuidHookLifecycleTest::RunTest(const FString&)
{
	using namespace BwayTravelGuidDiagnostics;
	TestTrue(TEXT("Module startup installed the seamless-travel hooks"), AreHooksInstalled());
	const bool bStartBound = FWorldDelegates::OnSeamlessTravelStart.IsBound();
	TestTrue(TEXT("Seamless-travel start delegate is bound while installed"), bStartBound);

	UninstallHooks();
	TestFalse(TEXT("Module shutdown path removes every observer hook"), AreHooksInstalled());
	TestFalse(TEXT("Observation is inactive after hook removal"), Observer.bActive);
	TestEqual(TEXT("No pins survive hook removal"), Observer.PinnedStaticObjects.Num(), 0);
	TestEqual(TEXT("No drivers stay tracked after hook removal"), Observer.Drivers.Num(), 0);
	TestFalse(TEXT("Seamless-travel start handle cleared"), Observer.SeamlessStartHandle.IsValid());
	TestFalse(TEXT("World cleanup handle cleared"), Observer.CleanupHandle.IsValid());
	TestFalse(TEXT("Post world init handle cleared"), Observer.PostWorldInitHandle.IsValid());
	TestFalse(TEXT("Actors init handle cleared"), Observer.ActorsInitHandle.IsValid());

	// Inactive observation never acquires pins or touches unrelated drivers.
	const int32 SanitizedBefore = Observer.SanitizedCount;
	Observer.PinStaticCacheObjectsForDriver(nullptr, TEXT("test-inactive"));
	Observer.SanitizePostTravelGuidCache(nullptr, TEXT("test-inactive"));
	TestEqual(TEXT("Inactive observer cannot pin"), Observer.PinnedStaticObjects.Num(), 0);
	TestEqual(TEXT("Inactive observer cannot sanitize"), Observer.SanitizedCount, SanitizedBefore);

	InstallHooks();
	TestTrue(TEXT("Startup path reinstalls hooks"), AreHooksInstalled());
	TestTrue(TEXT("Reinstalled seamless-travel start handle"), Observer.SeamlessStartHandle.IsValid());
	return true;
}
#endif
