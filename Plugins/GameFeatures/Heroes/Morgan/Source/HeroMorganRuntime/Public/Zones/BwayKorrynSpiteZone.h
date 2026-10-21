#pragma once

#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "BwayKorrynSpiteZone.generated.h"

class ABwayCharacterWithAbilities;
class ABwayPlayerState;
class UAbilitySystemComponent;
class UBwayRoundManagementComponent;
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
		TSubclassOf<UGameplayEffect> InDamageAmpEffectClass,
		float InSlowMultiplier = 0.85f,
		float InDamageAmpMultiplier = 1.35f);

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

	UFUNCTION()
	void OnRoundStateChanged(FName NewRoundState);
	void RemoveForRoundTransition();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Korryn|Circle")
	TObjectPtr<USphereComponent> EffectSphere;

	UPROPERTY(Replicated)
	float ReplicatedRadius = 700.f;

	UPROPERTY(Transient)
	TWeakObjectPtr<ABwayPlayerState> CasterPlayerState;

	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> CasterAbilitySystem;

	TWeakObjectPtr<UBwayRoundManagementComponent> RoundManager;
	FDelegateHandle RoundResetHandle;
	bool bRemovingForRoundTransition = false;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> SlowEffectClass;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> DamageAmpEffectClass;

	float SlowMultiplier = 0.85f;
	float DamageAmpMultiplier = 1.35f;

	struct FZoneEffectHandles
	{
		TWeakObjectPtr<UAbilitySystemComponent> TargetAbilitySystem;
		FActiveGameplayEffectHandle SlowHandle;
		FActiveGameplayEffectHandle AmpHandle;
	};
	static void RemoveEffectHandles(const FZoneEffectHandles& Handles);

	TMap<TWeakObjectPtr<ABwayCharacterWithAbilities>, FZoneEffectHandles> ActiveEnemyEffects;
};
