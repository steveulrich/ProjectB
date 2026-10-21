#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BwayAllKitCombatProbe.generated.h"

class ABwayPlayerState;
class ABwayAllKitCombatWitness;
struct FBwayAllKitCombatState;
struct FBwayAllKitCombatWitnessState;

/** Actual observations, not a client echo of the expected result. */
USTRUCT()
struct FBwayAllKitCombatRow
{
	GENERATED_BODY()
	UPROPERTY() int32 Player = INDEX_NONE;
	UPROPERTY() float Health = 0;
	UPROPERTY() float Maximum = 0;
	UPROPERTY() float Attack = 0;
	UPROPERTY() float Armor = 0;
	UPROPERTY() float Move = 0;
	UPROPERTY() float Incoming = 0;
	UPROPERTY() float Leech = 0;
	UPROPERTY() int32 Tags = 0;
	UPROPERTY() int32 Effects = 0;
	UPROPERTY() int32 Kills = 0;
	UPROPERTY() int32 Deaths = 0;
	UPROPERTY() int32 Assists = 0;
};

USTRUCT()
struct FBwayAllKitCombatFrame
{
	GENERATED_BODY()
	UPROPERTY() int32 Serial = 0;
	UPROPERTY() int32 Case = 0;
	UPROPERTY() int32 Control = 0;
	// 1 baseline, 2 owning activation, 3 outcome, 4 case cleanup, 5 final cleanup, 6 respawn, 7 pass hold, 9 failure.
	UPROPERTY() int32 Stage = 0;
	UPROPERTY() int32 Source = INDEX_NONE;
	UPROPERTY() int32 Target = INDEX_NONE;
	UPROPERTY() FName Action;
	UPROPERTY() bool bExpectActivation = true;
	UPROPERTY() float Duration = 0;
	UPROPERTY() FVector SourcePosition = FVector::ZeroVector;
	UPROPERTY() TArray<FBwayAllKitCombatRow> Rows;
	UPROPERTY() TArray<FVector> Positions;
	UPROPERTY() TArray<FRotator> SavedAims;
};

/** Development-only serial owner/authority/three-peer combat fixture. Inert in Shipping. */
UCLASS(Transient, NotPlaceable, NotBlueprintable)
class BREAKAWAYCORERUNTIME_API ABwayAllKitCombatProbe : public AActor
{
	GENERATED_BODY()
public:
	ABwayAllKitCombatProbe();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	const FBwayAllKitCombatFrame& GetFrame() const { return Frame; }
	void Acknowledge(ABwayAllKitCombatWitness* Witness, int32 Serial, int32 Stage,
		const TArray<FBwayAllKitCombatRow>& Rows, int32 SeenSourceTags, float PeakSourceSpeed, float PeakTargetSpeed);
	void Activated(ABwayAllKitCombatWitness* Witness, int32 Serial, bool bActivated);

private:
	UPROPERTY(Replicated) FBwayAllKitCombatFrame Frame;
	UPROPERTY() TArray<TObjectPtr<ABwayAllKitCombatWitness>> Witnesses;
	TSharedPtr<FBwayAllKitCombatState> State;
	bool Setup();
	bool Prepare();
	bool ValidateOutcome();
	bool CleanCase();
	bool Restore();
	void SnapshotCombat();
	void RestoreCombat();
	UFUNCTION() void OnCreditVictimDeathStarted(AActor* OwningActor);
	void RemoveCreditDeathObserver();
	void SetStage(int32 Stage);
	void Fail(const TCHAR* Reason);
};

/** One actor owned by each real player: all four observers can acknowledge over their own RPC channel. */
UCLASS(Transient, NotPlaceable, NotBlueprintable)
class BREAKAWAYCORERUNTIME_API ABwayAllKitCombatWitness : public AActor
{
	GENERATED_BODY()
public:
	ABwayAllKitCombatWitness();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	UPROPERTY(Replicated) TObjectPtr<ABwayAllKitCombatProbe> Probe;
	UPROPERTY(Replicated) int32 Player = INDEX_NONE;
private:
	TSharedPtr<FBwayAllKitCombatWitnessState> State;
	bool RestoreLocal(bool bFinal);
	UFUNCTION(Server, Reliable) void ServerObserved(int32 Serial, int32 Stage,
		const TArray<FBwayAllKitCombatRow>& Rows, int32 SeenSourceTags, float PeakSourceSpeed, float PeakTargetSpeed);
	UFUNCTION(Server, Reliable) void ServerActivated(int32 Serial, bool bActivated);
};
