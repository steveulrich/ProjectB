// LyraGameplayAbility_InteractiveObjectHolding.h
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "LyraGameplayAbility_InteractiveObjectHolding.generated.h"

class UInteractivePhysicsObject;

UCLASS()
class INTERACTIVEGAMEPLAYPLUGIN_API ULyraGameplayAbility_InteractiveObjectHolding : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	ULyraGameplayAbility_InteractiveObjectHolding();

protected:
	// Helper to get the interactive object we're currently holding
	UFUNCTION(BlueprintCallable, Category = "Interactive Object")
	UInteractivePhysicsObject* GetHeldObject() const;
    
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility, bool bWasCancelled) override;
};