#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BwaySlideLifecycleProbe.generated.h"

class UPackageMap;
struct FBwaySlideReviewObservation;

/** One coherent, acknowledged phase; separate property order must not dispatch input. */
USTRUCT()
struct FBwaySlideLifecycleFrame
{
	GENERATED_BODY()
	UPROPERTY() int32 Serial = 0;
	UPROPERTY() int32 Phase = 0;
	UPROPERTY() int32 Case = 0;
	UPROPERTY() int32 Peer = 0;
	UPROPERTY() int32 Source = INDEX_NONE;
	UPROPERTY() bool Passed = true;
	UPROPERTY() bool Trajectory = false;
	UPROPERTY() bool SupportDiagnostic = false;
	/** Ground/air observation is deliberately diagnostic-only and can never pass the trajectory gate. */
	UPROPERTY() bool GroundAirDiagnostic = false;
	UPROPERTY() bool ReviewRegression = false;
	UPROPERTY() FString Token;
	UPROPERTY() float AuthoredMinSpeed = 0.f;
	/** One authority-selected course radius; simulated proxy capsule shrinking must not change the geometry. */
	UPROPERTY() float ReviewCourseRadius = 0.f;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() FVector Direction = FVector::ForwardVector;
	UPROPERTY() FVector RestoreScale = FVector::OneVector;
	UPROPERTY() FVector RestoreVelocity = FVector::ZeroVector;
	UPROPERTY() uint8 RestoreMode = 0;
	UPROPERTY() uint8 RestoreCustom = 0;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FBwaySlideLifecycleFrame> : TStructOpsTypeTraitsBase2<FBwaySlideLifecycleFrame>
{
	enum { WithNetSerializer = true };
};

struct FBwaySlideLifecycleProbeState;

/** Development fixture coordinator or owner-only relay; inert in Shipping. */
UCLASS(Transient, NotPlaceable, NotBlueprintable)
class BREAKAWAYCORERUNTIME_API ABwaySlideLifecycleProbe : public AActor
{
	GENERATED_BODY()
public:
	ABwaySlideLifecycleProbe();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	static bool StartInWorld(UWorld* World, bool bTrajectory = false, const FString& Token = FString(), bool bSupportDiagnostic = false, bool bGroundAirDiagnostic = false, bool bReviewRegression = false);

private:
	UPROPERTY(Replicated) FBwaySlideLifecycleFrame Frame;
	TWeakObjectPtr<ABwaySlideLifecycleProbe> Coordinator;
	TSharedPtr<FBwaySlideLifecycleProbeState> State;
	// Development evidence acknowledgments, never trusted production gameplay results.
	UFUNCTION(Server, Reliable) void ServerReport(int32 Serial, int32 Case, int32 Phase, bool bPassed, const FString& Reason);
	void ReceiveReport(ABwaySlideLifecycleProbe* Relay, int32 Serial, int32 Case, int32 Phase, bool bPassed, const FString& Reason);
	void Publish(int32 Phase);
	bool BindSource();
	void Observe();
	void AuthorityTick();
	void Report(bool bPassed, const TCHAR* Reason);
	void Input(bool bJump, bool bPressed);
	void Sample(const TCHAR* Label) const;
	void Fail(const TCHAR* Category, const TCHAR* Reason);
	void CleanupLocal();
	bool RestoreAuthority();
	bool RestorePeerOrientation();
	void DestroyRelays();
	UFUNCTION() void OnTrajectoryMovement(float DeltaSeconds, FVector OldLocation, FVector OldVelocity);
	void TrajectoryRecord(const TCHAR* Event, const FString& Fields) const;
	void GroundAirRecord(const TCHAR* Event, const FString& Fields) const;
	void ObserveTrajectory();
	void ObserveGroundAirDiagnostic();
	bool PrepareTrajectoryCase();
	bool ValidateTrajectoryGeometry(FVector& OutStart, FVector& OutDirection) const;
	void ObserveReviewRegression();
	void OnReviewStep(const FBwaySlideReviewObservation& Observation);
	void ReviewMovementSample(float DeltaSeconds, const FVector& OldPosition, const FVector& OldVelocity);
};
