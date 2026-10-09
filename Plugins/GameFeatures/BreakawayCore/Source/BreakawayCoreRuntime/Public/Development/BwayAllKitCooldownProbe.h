#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BwayAllKitCooldownProbe.generated.h"

class UPackageMap;

USTRUCT()
struct FBwayAllKitCooldownFrame
{
	GENERATED_BODY()
	UPROPERTY() int32 Serial = 0;
	UPROPERTY() int32 Case = 0;
	UPROPERTY() int32 Cycle = 1;
	UPROPERTY() int32 Stage = 0; // 1 activation, 2 restored, 3 failure
	UPROPERTY() int32 Source = INDEX_NONE;
	UPROPERTY() float Base = 0;
	UPROPERTY() float Bonus = 0;
	UPROPERTY() FVector Position = FVector::ZeroVector;
	UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY() TArray<int32> Players;
	UPROPERTY() TArray<float> Health;
	UPROPERTY() TArray<float> Maximum;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FBwayAllKitCooldownFrame> : TStructOpsTypeTraitsBase2<FBwayAllKitCooldownFrame>
{
	enum { WithNetSerializer = true };
};

struct FBwayAllKitCooldownState;

/** Development-only owned match probe. Inert in Shipping. */
UCLASS(Transient, NotPlaceable, NotBlueprintable)
class BREAKAWAYCORERUNTIME_API ABwayAllKitCooldownProbe : public AActor
{
	GENERATED_BODY()
public:
	ABwayAllKitCooldownProbe();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Replicated) FBwayAllKitCooldownFrame Frame;
	TSharedPtr<FBwayAllKitCooldownState> State;
	UFUNCTION(Server, Reliable) void ServerOwnerStage(int32 Serial, int32 Stage);
	bool Setup();
	bool Prepare();
	void AuthorityTick();
	void Observe();
	void Cleanup();
	void Fail(const TCHAR* Reason);
};
