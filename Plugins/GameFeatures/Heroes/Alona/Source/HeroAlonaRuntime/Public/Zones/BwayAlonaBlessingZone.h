#pragma once

#include "GameFramework/Actor.h"
#include "BwayAlonaBlessingZone.generated.h"

class ABwayCharacterWithAbilities;
class USphereComponent;

/**
 * Temporary ground heal circle for Blessing of the Sun.
 * Server applies initial burst + periodic heal to allies in radius.
 */
UCLASS()
class HEROALONARUNTIME_API ABwayAlonaBlessingZone : public AActor
{
	GENERATED_BODY()

public:
	ABwayAlonaBlessingZone();

	void ConfigureZone(
		ABwayCharacterWithAbilities* InCaster,
		float InRadius,
		float InDuration,
		float InInitialHeal,
		float InHealPerSecond);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void ApplyHealToAlly(ABwayCharacterWithAbilities* Ally, float HealAmount);
	void ApplyInitialHeals();
	void TickHeal();
	bool IsAllyOfCaster(ABwayCharacterWithAbilities* OtherCharacter) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Blessing")
	TObjectPtr<USphereComponent> HealSphere;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blessing", meta = (ClampMin = "50.0"))
	float HealRadius = 400.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blessing", meta = (ClampMin = "0.1"))
	float ZoneDuration = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blessing", meta = (ClampMin = "0.0"))
	float InitialHeal = 70.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blessing", meta = (ClampMin = "0.0"))
	float HealPerSecond = 35.f;

	UPROPERTY(Replicated)
	float ReplicatedRadius = 400.f;

	UPROPERTY(Transient)
	TObjectPtr<ABwayCharacterWithAbilities> CasterCharacter;

	UPROPERTY(Transient)
	TSet<TObjectPtr<ABwayCharacterWithAbilities>> AlliesInZone;

	FTimerHandle HealTickTimerHandle;
};
