// LyraGameplayAbility_InteractiveObjectPickup.h
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "LyraGameplayAbility_InteractiveObjectPickup.generated.h"

class UInteractivePhysicsObject;

UCLASS()
class INTERACTIVEGAMEPLAYPLUGIN_API ULyraGameplayAbility_InteractiveObjectPickup : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	ULyraGameplayAbility_InteractiveObjectPickup();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// Helper to get the interactive object from the event data
	UInteractivePhysicsObject* GetInteractiveObjectFromEventData(const FGameplayEventData* TriggerEventData) const;
};