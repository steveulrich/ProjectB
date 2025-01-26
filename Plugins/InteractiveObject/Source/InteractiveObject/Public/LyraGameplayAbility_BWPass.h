// LyraGameplayAbility_PassObject.h
#pragma once

#include "CoreMinimal.h"
#include "LyraGameplayAbility_BWInteractiveObjectHolding.h"
#include "LyraGameplayAbility_BWPass.generated.h"

class UInputAction;  // Forward declaration

UCLASS()
class INTERACTIVEOBJECT_API ULyraGameplayAbility_BWPass: public ULyraGameplayAbility_BWInteractiveObjectHolding
{
	GENERATED_BODY()

public:
	ULyraGameplayAbility_BWPass();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	APawn* FindPassTarget(const FVector& StartLocation, const FRotator& LookRotation) const;

	UPROPERTY(EditDefaultsOnly, Category="Pass")
	float PassSpeed = 800.0f;

	UPROPERTY(EditDefaultsOnly, Category="Pass")
	float MaxPassDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, Category="Pass")
	float TargetingRadius = 100.0f;

	// Debug visualization
	UPROPERTY(EditDefaultsOnly, Category = "Pass|Debug")
	bool bDebugDrawTargeting = false;

	UPROPERTY(EditDefaultsOnly, Category = "Pass|Debug")
	float DebugDrawDuration = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	const class UInputAction* PassInputAction;
};