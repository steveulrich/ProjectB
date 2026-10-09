#pragma once

#include "GameFramework/Actor.h"
#include "BwayRelicIndicatorProbe.generated.h"

USTRUCT()
struct FBwayRelicIndicatorProbeFrame
{
	GENERATED_BODY()
	UPROPERTY() int32 Stage = -1;
	UPROPERTY() int32 Requester = -1;
	UPROPERTY() bool Active = false;
	UPROPERTY() int32 ExpectedPlayers = 3;
	UPROPERTY() TArray<int32> Players;
	UPROPERTY() TArray<int32> Teams;
};

/** Development console fixture; never spawned by gameplay. */
UCLASS(NotBlueprintable, Transient)
class ABwayRelicIndicatorProbe : public AActor
{
	GENERATED_BODY()
public:
	ABwayRelicIndicatorProbe();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void SetFrame(const FBwayRelicIndicatorProbeFrame& Value);
	int32 ReportedStage = -1;
	bool ReportFailed = false;
	UFUNCTION(Client, Reliable) void ClientFinish();
private:
	UPROPERTY(Replicated) FBwayRelicIndicatorProbeFrame Frame;
	UFUNCTION(Server, Reliable) void ServerReport(int32 Stage, bool Passed);
	int32 ObservedStage = -1;
	int32 SubmittedStage = -1;
	double StageStarted = 0.0;
	double StableStarted = 0.0;
};
