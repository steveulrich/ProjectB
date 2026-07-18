#pragma once

#include "CoreMinimal.h"
#include "Buildable/BuildableBase.h"
#include "GameplayEffectTypes.h"
#include "BwayKorrynCursedWardBuildable.generated.h"

class ABwayCharacterWithAbilities;
class UBwayKorrynKitConfig;
class USphereComponent;
class UGameplayEffect;
struct FGameplayEffectSpec;

/**
 * Korryn Cursed Ward — persistent enemy-slow buildable.
 * Sheet: HP 600, 50% slow, 6m radius; once/round free; persists between rounds.
 */
UCLASS()
class HEROMORGANRUNTIME_API ABwayKorrynCursedWardBuildable : public ABuildableActor
{
	GENERATED_BODY()

public:
	ABwayKorrynCursedWardBuildable();

protected:
	virtual void InitializeAbilitySystem() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void FinishBuilding() override;

	UFUNCTION()
	void OnSlowSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnSlowSphereEndOverlap(
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

	void StartSlowAura();
	void StopSlowAura();
	void SeedEnemiesAlreadyInRadius();
	void ApplySlowToEnemy(ABwayCharacterWithAbilities* Enemy);
	void RemoveSlowFromEnemy(ABwayCharacterWithAbilities* Enemy);
	void ClearAllSlowEffects();
	bool IsEnemyOfWard(ABwayCharacterWithAbilities* OtherCharacter) const;
	bool IsAliveEnemy(ABwayCharacterWithAbilities* Enemy) const;
	AActor* ResolveTeamProxy() const;
	void ApplyKitConfigOverrides();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CursedWard")
	TObjectPtr<USphereComponent> SlowSphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CursedWard|Config")
	TSoftObjectPtr<UBwayKorrynKitConfig> KitConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CursedWard|Config", meta = (ClampMin = "1.0"))
	float MaxHealth = 600.f;

	/** Sheet: 6m radius. Project convention is ~100 uu per meter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CursedWard|Config", meta = (ClampMin = "50.0"))
	float SlowRadius = 600.f;

	/** MoveSpeedMultiplier applied while enemies remain in radius (0.5 = 50% slow). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CursedWard|Config", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float SlowMultiplier = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CursedWard|Config")
	TSubclassOf<UGameplayEffect> SlowEffectClass;

	TMap<TWeakObjectPtr<ABwayCharacterWithAbilities>, FActiveGameplayEffectHandle> ActiveEnemySlowHandles;

	bool bSlowAuraActive = false;
	bool bDeathHandled = false;
};
