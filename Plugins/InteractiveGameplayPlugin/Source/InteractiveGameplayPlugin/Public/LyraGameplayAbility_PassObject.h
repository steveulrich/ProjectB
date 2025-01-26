// LyraGameplayAbility_PassObject.h
#pragma once

#include "CoreMinimal.h"
#include "LyraGameplayAbility_InteractiveObjectHolding.h"
#include "LyraGameplayAbility_PassObject.generated.h"

UCLASS()
class INTERACTIVEGAMEPLAYPLUGIN_API ULyraGameplayAbility_PassObject : public ULyraGameplayAbility_InteractiveObjectHolding
{
	GENERATED_BODY()

public:
	ULyraGameplayAbility_PassObject();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// Settings for pass behavior
	UPROPERTY(EditDefaultsOnly, Category = "Pass")
	float PassSpeed = 800.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pass")
	float MaxPassDistance = 2000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pass")
	float TargetingRadius = 100.0f;

	// Find the best target to pass to
	APawn* FindPassTarget(const FVector& StartLocation, const FRotator& LookRotation) const;
};