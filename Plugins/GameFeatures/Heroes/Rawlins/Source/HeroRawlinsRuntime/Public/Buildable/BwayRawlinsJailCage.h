#pragma once

#include "CoreMinimal.h"
#include "Buildable/BuildableBase.h"
#include "GameplayEffectTypes.h"
#include "BwayRawlinsJailCage.generated.h"

class ABwayCharacterWithAbilities;
class UBwayRawlinsKitConfig;
class USphereComponent;
class UGameplayEffect;

/**
 * Per-victim Rawlins Jail cage — 450 HP, movement lock only, abilities remain usable.
 * Not owned by the placer PC (so placement-registry once/round counts stay correct).
 * Spawned with null Owner so consuming the floor trap does not destroy cages.
 * Non-persistent: round reset destroys via bPersistsBetweenRounds=false (victim pawn is replaced).
 */
UCLASS()
class HERORAWLINSRUNTIME_API ABwayRawlinsJailCage : public ABuildableActor
{
	GENERATED_BODY()

public:
	ABwayRawlinsJailCage();

	/** Bind victim, attach, apply jailed GE. Call after spawn on authority. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Jail")
	void CaptureVictim(ABwayCharacterWithAbilities* InVictim);

	UFUNCTION(BlueprintPure, Category = "Jail")
	ABwayCharacterWithAbilities* GetCapturedVictim() const { return CapturedVictim.Get(); }

protected:
	virtual void InitializeAbilitySystem() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FinishBuilding() override;

	UFUNCTION()
	void HandleDeathStarted(AActor* OwningActor);

	UFUNCTION()
	void HandleVictimDeathStarted(AActor* OwningActor);

	void HandleOutOfHealth(
		AActor* EffectInstigator,
		AActor* EffectCauser,
		const FGameplayEffectSpec* EffectSpec,
		float EffectMagnitude,
		float OldValue,
		float NewValue);

	void ApplyKitConfigOverrides();
	void ApplyJailedEffect();
	void ReleaseVictim();
	void DestroyCage();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Jail")
	TObjectPtr<USphereComponent> CageHitSphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jail|Config")
	TSoftObjectPtr<UBwayRawlinsKitConfig> KitConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jail|Config", meta = (ClampMin = "1.0"))
	float MaxHealth = 450.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jail|Config", meta = (ClampMin = "50.0"))
	float CageRadius = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jail|Config")
	TSubclassOf<UGameplayEffect> JailedEffectClass;

	TWeakObjectPtr<ABwayCharacterWithAbilities> CapturedVictim;
	FActiveGameplayEffectHandle JailedEffectHandle;

	bool bDeathHandled = false;
	bool bVictimReleased = false;
};
