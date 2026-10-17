#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BwayRelicFlightProbe.generated.h"

USTRUCT()
struct FBwayRelicFlightFrame
{
	GENERATED_BODY()
	UPROPERTY() int32 Serial = 0;
	UPROPERTY() int32 Stage = 0;
	UPROPERTY() int32 Scenario = 0;
	UPROPERTY() int32 Witness = INDEX_NONE;
	UPROPERTY() int32 Source = INDEX_NONE;
	UPROPERTY() int32 Carrier = INDEX_NONE;
	UPROPERTY() int32 RelicState = 0;
	UPROPERTY() int32 PassingTeam = INDEX_NONE;
	UPROPERTY() int32 PossessingTeam = INDEX_NONE;
	UPROPERTY() bool bPassInFlight = false;
	UPROPERTY() float HoldSeconds = 0.0f;
	UPROPERTY() TArray<int32> Players;
	UPROPERTY() TArray<int32> Interceptions;
};

struct FBwayRelicFlightState;

UCLASS(Transient, NotPlaceable, NotBlueprintable)
class BREAKAWAYCORERUNTIME_API ABwayRelicFlightProbe : public AActor
{
	GENERATED_BODY()
public:
	ABwayRelicFlightProbe();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UPROPERTY(Replicated) FBwayRelicFlightFrame Frame;
	TSharedPtr<FBwayRelicFlightState> State;
	TWeakObjectPtr<ABwayRelicFlightProbe> Coordinator;
	UFUNCTION(Server, Reliable) void ServerReport(int32 Serial, bool bSuccess, const FString& Reason);
	bool Setup();
	void AuthorityTick();
	void LocalTick();
	void Publish(int32 Stage);
	void Report(bool bSuccess, const TCHAR* Reason);
	void Fail(const TCHAR* Reason);
	void Cleanup();
	bool AcquireSource();
	bool Converged(FString& Summary) const;
};
