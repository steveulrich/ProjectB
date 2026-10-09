#pragma once

#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "BwayRelicOwnershipProbe.generated.h"

class ARelicActor;

USTRUCT()
struct FBwayRelicOwnershipFrame
{
    GENERATED_BODY()
    UPROPERTY() int32 Stage = -1;
    UPROPERTY() int32 Action = 0; // 0 observe, 1 request, 2 pass, 3 throw.
    UPROPERTY() int32 ActionPlayer = -1;
    UPROPERTY() int32 Carrier = -1;
    UPROPERTY() int32 State = -1;
    UPROPERTY() int32 AbilityCount = 0;
    UPROPERTY() int32 ScorePlayer = -1;
    UPROPERTY() int32 ObjectiveScore = 0;
    UPROPERTY() int32 ScoreTeam = -1;
    UPROPERTY() int32 TeamScore = 0;
    UPROPERTY() TArray<int32> Players;
    UPROPERTY() TObjectPtr<ARelicActor> Relic;
};

/** Owned development observer; never spawned by shipping gameplay. */
UCLASS(NotBlueprintable, Transient)
class ABwayRelicOwnershipProbe : public AActor
{
    GENERATED_BODY()
public:
    ABwayRelicOwnershipProbe();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void SetFrame(const FBwayRelicOwnershipFrame& Value);
    int32 ReportedStage = -1;
    bool ReportFailed = false;
    UFUNCTION(Client, Reliable) void ClientFinish();
private:
    UPROPERTY(Replicated) FBwayRelicOwnershipFrame Frame;
    UFUNCTION(Server, Reliable) void ServerReport(int32 Stage, bool Passed);
    int32 ObservedStage = -1;
    int32 SubmittedStage = -1;
    int32 Observations = 0;
    int32 Actions = 0;
    double StageStarted = 0;
    double StableStarted = 0;
    double ReleaseAt = 0;
    FGameplayTag ReleaseTag;
    FRotator SavedControlRotation;
    bool SavedControl = false;
};
