#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BwayBarrageTransportProbe.generated.h"

class UPackageMap;

/** One coherent instruction per connection-owned witness. No gameplay state is overwritten. */
USTRUCT()
struct FBwayBarrageTransportFrame
{
	GENERATED_BODY()
	UPROPERTY() int32 Serial = 0;
	UPROPERTY() int32 Stage = 0;
	UPROPERTY() int32 Witness = INDEX_NONE;
	UPROPERTY() int32 Source = INDEX_NONE;
	UPROPERTY() int32 Round = INDEX_NONE;
	UPROPERTY() int32 Lag = 0;
	UPROPERTY() int32 Count = 0;
	UPROPERTY() float Interval = 0;
	UPROPERTY() float Life = 0;
	UPROPERTY() float Cooldown = 0;
	UPROPERTY() float Speed = 0;
	UPROPERTY() float Offset = 0;
	UPROPERTY() float FrameQuantum = 0;
	UPROPERTY() FString KitPath;
	UPROPERTY() TArray<int32> Players;
	UPROPERTY() TArray<int32> Teams;
	UPROPERTY() TArray<float> Health;
	UPROPERTY() TArray<float> Maximum;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FBwayBarrageTransportFrame> : TStructOpsTypeTraitsBase2<FBwayBarrageTransportFrame>
{
	enum { WithNetSerializer = true };
};

struct FBwayBarrageTransportState;

/** Measured first-cast readiness; all timestamps belong to the sending process. */
USTRUCT()
struct FBwayBarrageStartupReady
{
	GENERATED_BODY()
	UPROPERTY() int32 Serial = 0;
	UPROPERTY() int32 Generation = 0;
	UPROPERTY() int32 Sequence = 0;
	UPROPERTY() int32 Observation = 0;
	UPROPERTY() int32 Samples = 0;
	UPROPERTY() uint64 EngineFrame = 0;
	UPROPERTY() uint64 SourceGuid = 0;
	UPROPERTY() double WorldTime = 0;
	UPROPERTY() double Elapsed = 0;
	UPROPERTY() double WallSpan = 0;
	UPROPERTY() double WorldSpan = 0;
	UPROPERTY() double MaxGap = 0;
	UPROPERTY() float WorldDelta = 0;
	UPROPERTY() float PeakDelta = 0;
};

/** Development-only four-process observation; inert in Shipping. */
UCLASS(Transient, NotPlaceable, NotBlueprintable)
class BREAKAWAYCORERUNTIME_API ABwayBarrageTransportProbe : public AActor
{
	GENERATED_BODY()
public:
	ABwayBarrageTransportProbe();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated) FBwayBarrageTransportFrame Frame;
	TSharedPtr<FBwayBarrageTransportState> State;
	TWeakObjectPtr<ABwayBarrageTransportProbe> Coordinator;
	UFUNCTION(Server, Reliable) void ServerReport(int32 Serial, int32 Stage, bool bSuccess, const FString& Reason);
	UFUNCTION(Server, Reliable) void ServerStartupReady(const FBwayBarrageStartupReady& Measurement);
	bool SetupAuthority();
	bool SetupLocal();
	bool Live() const;
	bool SafeLane();
	bool ObserveChangingAim(bool bAllowInput);
	bool VerifyRuntimeTuning();
	bool CheckFrameQuantum();
	void FinalizeShot();
	bool WaitFor(const TCHAR* Reason);
	void Bind();
	void Observe();
	void AuthorityTick();
	void Publish(int32 Stage);
	void Report(bool bSuccess, const TCHAR* Reason);
	void Fail(const TCHAR* Reason);
	void RemoveObservers(bool bCancelCapturedActivation);
	void Sample(const TCHAR* Phase);
	bool StartupIdle() const;
	void ObserveStartup();
	bool HandoffStartup(const TCHAR* Reason, bool bRequireIdle);
	bool CheckStartupActivation(bool bObservedTick);
	void RestoreStartupTick(const TCHAR* Reason);
};
