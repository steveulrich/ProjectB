#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BwayLeechLimitProbe.generated.h"

class UPackageMap;

/** Development fixture inputs. Health remains production ASC state, never probe state. */
USTRUCT()
struct FBwayLeechLimitFrame
{
	GENERATED_BODY()
	UPROPERTY() int32 Serial = 0;
	UPROPERTY() int32 Case = 0;
	UPROPERTY() int32 Stage = 0; // 0 baseline, 1 damage, 2 natural respawn, 3 cleanup
	UPROPERTY() int32 Source = INDEX_NONE;
	UPROPERTY() int32 Target = INDEX_NONE;
	UPROPERTY() int32 FirstVictim = INDEX_NONE;
	UPROPERTY() float Raw = 121.25f;
	UPROPERTY() float Bonus = 0;
	UPROPERTY() float Armor = 0;
	UPROPERTY() float Incoming = 1;
	UPROPERTY() TArray<int32> Players;
	UPROPERTY() TArray<float> Before;
	UPROPERTY() TArray<FString> OriginalPawns;
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template<> struct TStructOpsTypeTraits<FBwayLeechLimitFrame> : TStructOpsTypeTraitsBase2<FBwayLeechLimitFrame>
{
	enum { WithNetSerializer = true };
};

struct FBwayLeechLimitState;
struct FGameplayTag;
struct FLyraVerbMessage;

/** Spawned only by a development console fixture; inert in Shipping. */
UCLASS(Transient, NotPlaceable, NotBlueprintable)
class BREAKAWAYCORERUNTIME_API ABwayLeechLimitProbe : public AActor
{
	GENERATED_BODY()
public:
	ABwayLeechLimitProbe();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(Replicated) FBwayLeechLimitFrame Frame;
	TSharedPtr<FBwayLeechLimitState> State;
	bool Setup();
	bool PrepareCase(int32 Case);
	void ApplyCase();
	bool Observe();
	void Cleanup();
	void Fail(const TCHAR* Reason);
	void OnDamageMessage(FGameplayTag Channel, const FLyraVerbMessage& Message);
	void OnHealMessage(FGameplayTag Channel, const FLyraVerbMessage& Message);
};
