#pragma once

#include "CoreMinimal.h"
#include "Buildable/BuildableBase.h"
#include "Buildable/BwayRawlinsJailCage.h"
#include "BwayRawlinsJailBuildable.generated.h"

class ABwayCharacterWithAbilities;
class UBwayRawlinsKitConfig;

/**
 * Rawlins Jail floor trap — one-shot ATrapBase.
 * Invulnerable while armed; persists between rounds until first live-enemy trigger.
 * On trigger: snapshot enemies in capture radius, spawn one cage per victim, then destroy self.
 */
UCLASS()
class HERORAWLINSRUNTIME_API ABwayRawlinsJailBuildable : public ATrapBase
{
	GENERATED_BODY()

public:
	ABwayRawlinsJailBuildable();

protected:
	virtual void InitializeAbilitySystem() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FinishBuilding() override;
	virtual void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult) override;

	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

	void HandleOutOfHealth(
		AActor* EffectInstigator,
		AActor* EffectCauser,
		const FGameplayEffectSpec* EffectSpec,
		float EffectMagnitude,
		float OldValue,
		float NewValue);

	void ApplyKitConfigOverrides();
	void ArmTrap();
	void TriggerJailCapture(ABwayCharacterWithAbilities* TriggeringEnemy);
	void CollectEnemiesInCaptureRadius(TArray<ABwayCharacterWithAbilities*>& OutEnemies) const;
	ABwayRawlinsJailCage* SpawnCageForVictim(ABwayCharacterWithAbilities* Victim) const;
	bool IsEnemyOfTrap(ABwayCharacterWithAbilities* OtherCharacter) const;
	bool IsAliveEnemy(ABwayCharacterWithAbilities* Enemy) const;
	AActor* ResolveTeamProxy() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jail|Config")
	TSoftObjectPtr<UBwayRawlinsKitConfig> KitConfig;

	/** Per-victim cage class (BP preferred). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jail|Config")
	TSubclassOf<ABwayRawlinsJailCage> JailCageClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jail|Config", meta = (ClampMin = "50.0"))
	float TriggerRadius = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jail|Config", meta = (ClampMin = "50.0"))
	float CaptureRadius = 450.f;

	bool bHasTriggered = false;
	bool bDeathHandled = false;
};
