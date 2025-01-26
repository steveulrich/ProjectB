// LyraGameplayAbility_BWThrow.h
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "LyraGameplayAbility_BWThrow.generated.h"

UCLASS()
class INTERACTIVEOBJECT_API ULyraGameplayAbility_BWThrow : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	ULyraGameplayAbility_BWThrow();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	float MinThrowSpeed = 500.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	float MaxThrowSpeed = 1500.0f;

	UPROPERTY()
	float CurrentChargeTime;
	
	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	float MaxChargeTime = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Throw")
	float ThrowArc = 0.2f;

	float ChargeStartTime;
	bool bIsCharging;

	UFUNCTION()
	void HandleChargeCompleted();

	UFUNCTION()
	float CalculateThrowSpeed() const;

	UFUNCTION()
	FVector CalculateThrowVelocity(const FVector& StartLocation, const FVector& TargetDirection) const;

	UFUNCTION()
	void ExecuteThrow();
};