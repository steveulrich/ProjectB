#pragma once

#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "BwayKorrynSpiteZone.generated.h"

class ABwayCharacterWithAbilities;
class USphereComponent;
class UGameplayEffect;

/**
 * Circle of Spite ground zone — applies/removes slow + damage amp on enemy enter/exit.
 */
UCLASS()
class HEROMORGANRUNTIME_API ABwayKorrynSpiteZone : public AActor
{
	GENERATED_BODY()

public:
	ABwayKorrynSpiteZone();

	void ConfigureZone(
		ABwayCharacterWithAbilities* InCaster,
		float InRadius,
		float InDuration,
		TSubclassOf<UGameplayEffect> InSlowEffectClass,
		TSubclassOf<UGameplayEffect> InDamageAmpEffectClass);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	bool IsEnemyCharacter(ABwayCharacterWithAbilities* Other) const;
	void ApplyZoneEffects(ABwayCharacterWithAbilities* Enemy);
	void RemoveZoneEffects(ABwayCharacterWithAbilities* Enemy);
	void ClearAllZoneEffects();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Korryn|Circle")
	TObjectPtr<USphereComponent> EffectSphere;

	UPROPERTY(Replicated)
	float ReplicatedRadius = 700.f;

	UPROPERTY(Transient)
	TObjectPtr<ABwayCharacterWithAbilities> CasterCharacter;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> SlowEffectClass;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageAmpEffectClass;

	struct FZoneEffectHandles
	{
		FActiveGameplayEffectHandle SlowHandle;
		FActiveGameplayEffectHandle AmpHandle;
	};

	TMap<TWeakObjectPtr<ABwayCharacterWithAbilities>, FZoneEffectHandles> ActiveEnemyEffects;
};
