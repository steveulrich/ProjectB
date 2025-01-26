// LyraGameplayAbility_ThrowObject.h
#pragma once

#include "CoreMinimal.h"
#include "LyraGameplayAbility_InteractiveObjectHolding.h"
#include "LyraGameplayAbility_ThrowObject.generated.h"

UCLASS()
class INTERACTIVEGAMEPLAYPLUGIN_API ULyraGameplayAbility_ThrowObject : public ULyraGameplayAbility_InteractiveObjectHolding
{
	GENERATED_BODY()

public:
	ULyraGameplayAbility_ThrowObject();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category="Throw")
	float ThrowSpeed = 1000.0f;
    
	UPROPERTY(EditDefaultsOnly, Category="Throw")
	float ThrowUpwardAngle = 20.0f;
};