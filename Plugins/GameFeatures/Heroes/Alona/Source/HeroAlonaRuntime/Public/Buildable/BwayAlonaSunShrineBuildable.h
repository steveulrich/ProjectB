#pragma once

#include "CoreMinimal.h"
#include "Buildable/BuildableBase.h"
#include "BwayAlonaSunShrineBuildable.generated.h"

class ABwayCharacterWithAbilities;
class USphereComponent;
struct FGameplayEffectSpec;

/**
 * Alona Sun Shrine — persistent healing buildable.
 * Sheet: HP 750, heals 35 HP/s to allies in 5m radius; does not stack with other Sun Shrines.
 */
UCLASS()
class HEROALONARUNTIME_API ABwayAlonaSunShrineBuildable : public ABuildableActor
{
	GENERATED_BODY()

public:
	ABwayAlonaSunShrineBuildable();

protected:
	virtual void InitializeAbilitySystem() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FinishBuilding() override;

	UFUNCTION()
	void OnHealSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnHealSphereEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

	void HandleOutOfHealth(
		AActor* EffectInstigator,
		AActor* EffectCauser,
		const FGameplayEffectSpec* EffectSpec,
		float EffectMagnitude,
		float OldValue,
		float NewValue);

	void StartHealAura();
	void StopHealAura();
	void TickHeal();
	void SeedAlliesAlreadyInRadius();
	void ApplyHealToAlly(ABwayCharacterWithAbilities* Ally, float HealAmount);
	bool IsAllyOfShrine(ABwayCharacterWithAbilities* OtherCharacter) const;
	bool IsAliveAlly(ABwayCharacterWithAbilities* Ally) const;
	bool ShouldThisShrineHealAlly(ABwayCharacterWithAbilities* Ally) const;
	bool IsActiveSunShrine() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SunShrine")
	TObjectPtr<USphereComponent> HealSphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunShrine|Config", meta = (ClampMin = "1.0"))
	float MaxHealth = 750.f;

	/** Sheet: 5m radius. Project convention is ~100 uu per meter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunShrine|Config", meta = (ClampMin = "50.0"))
	float HealRadius = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SunShrine|Config", meta = (ClampMin = "0.0"))
	float HealPerSecond = 35.f;

	UPROPERTY(Transient)
	TSet<TObjectPtr<ABwayCharacterWithAbilities>> AlliesInRadius;

	FTimerHandle HealTickTimerHandle;
	bool bHealAuraActive = false;
	bool bDeathHandled = false;
};
