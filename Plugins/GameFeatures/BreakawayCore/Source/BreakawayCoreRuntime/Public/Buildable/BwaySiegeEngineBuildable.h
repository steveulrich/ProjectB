#pragma once

#include "CoreMinimal.h"
#include "Buildable/BuildableBase.h"
#include "UObject/ObjectKey.h"
#include "BwaySiegeEngineBuildable.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UAudioComponent;
class UBwaySiegePresentationData;
struct FGameplayEffectSpec;

UENUM()
enum class EBwaySiegePresentationEvent : uint8
{
	Launch,
	BuildableImpact,
	TerminalImpact,
	Expiry
};

/**
 * Argus Siege Engine — one-use rolling barrel that damages enemy buildables.
 * Sheet: HP 250, 200 dmg/s vs buildables, 10s max roll duration.
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API ABwaySiegeEngineBuildable : public ABuildableActor
{
	GENERATED_BODY()

public:
	ABwaySiegeEngineBuildable();

#if !UE_BUILD_SHIPPING
	// Read-only observations for the development lifetime/replication fixture.
	bool IsRollingForDevelopment() const { return bIsRolling; }
	float GetRollElapsedForDevelopment() const { return RollElapsed; }
	float GetMaxRollDurationForDevelopment() const { return MaxRollDuration; }
	float GetRollSpeedForDevelopment() const { return RollSpeed; }
#endif

protected:
	friend class FBwaySiegePresentationLifecycleTest;
	virtual void BeginPlay() override;
	virtual void InitializeAbilitySystem() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FinishBuilding() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void StartRoll();
	void StopRollAndExpire(bool bImpact = false, const FVector& ImpactLocation = FVector::ZeroVector,
		const FVector& ImpactNormal = FVector::UpVector);
	void ApplyDamageToOverlappingEnemyBuildables(float DeltaTime);
	bool IsEnemyBuildable(const ABuildableActor* OtherBuildable) const;
	void ApplyDamageToBuildable(ABuildableActor* TargetBuildable, float DamageAmount);
	void HandleOutOfHealth(AActor* InstigatorActor, AActor* Causer, const FGameplayEffectSpec* Spec,
		float Magnitude, float OldValue, float NewValue);
	void RefreshPresentation();
	void StopPresentation();
	bool CanRenderPresentation() const;
	void DispatchPresentationEvent(EBwaySiegePresentationEvent Event, const FVector& Location,
		const FVector& Normal, const AActor* ContactTarget = nullptr);
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayPresentationEvent(uint16 Sequence, EBwaySiegePresentationEvent Event,
		UBwaySiegePresentationData* Data, FVector_NetQuantize10 Location, FVector_NetQuantizeNormal Normal);
	UFUNCTION()
	void OnRep_PresentationData();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_PresentationData, Category="SiegeEngine|Presentation")
	TObjectPtr<UBwaySiegePresentationData> PresentationData;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SiegeEngine|Presentation")
	TObjectPtr<UStaticMeshComponent> PresentationMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SiegeEngine|Presentation")
	TObjectPtr<UNiagaraComponent> RollingVFX;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SiegeEngine|Presentation")
	TObjectPtr<UAudioComponent> RollingAudio;
	UPROPERTY(Transient)
	TObjectPtr<UBwaySiegePresentationData> AppliedPresentationData;
	TSet<TObjectKey<AActor>> PresentedContactTargets;
	uint16 DispatchedPresentationSequence = 0;
	uint16 ReceivedPresentationSequence = 0;
	bool bLaunchPresentationDispatched = false;
	bool bTerminalPresentationDispatched = false;
	bool bTerminalPresentationReceived = false;
	bool bRollingPresentationActive = false;
	bool bSavedRootVisibility = false;
	bool bRootWasVisible = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SiegeEngine")
	TObjectPtr<USphereComponent> DamageSphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SiegeEngine|Config", meta = (ClampMin = "1.0"))
	float MaxHealth = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SiegeEngine|Config", meta = (ClampMin = "0.0"))
	float BuildableDamagePerSecond = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SiegeEngine|Config", meta = (ClampMin = "0.1"))
	float MaxRollDuration = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SiegeEngine|Config", meta = (ClampMin = "0.0"))
	float RollSpeed = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SiegeEngine|Config", meta = (ClampMin = "0.0"))
	float DamageRadius = 150.f;

	UPROPERTY(ReplicatedUsing = OnRep_IsRolling)
	bool bIsRolling = false;

	UPROPERTY(Replicated)
	FVector RollDirection = FVector::ForwardVector;

	float RollElapsed = 0.f;

	UFUNCTION()
	void OnRep_IsRolling();
};
