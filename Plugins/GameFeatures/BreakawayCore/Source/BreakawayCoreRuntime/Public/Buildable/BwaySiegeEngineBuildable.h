#pragma once

#include "CoreMinimal.h"
#include "Buildable/BuildableBase.h"
#include "BwaySiegeEngineBuildable.generated.h"

class USphereComponent;

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

protected:
	virtual void InitializeAbilitySystem() override;
	virtual void FinishBuilding() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void StartRoll();
	void StopRollAndExpire();
	void ApplyDamageToOverlappingEnemyBuildables(float DeltaTime);
	bool IsEnemyBuildable(const ABuildableActor* OtherBuildable) const;
	void ApplyDamageToBuildable(ABuildableActor* TargetBuildable, float DamageAmount);

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
