#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "LyraGameplayAbility_BWInteractiveObjectHolding.generated.h"

class UInteractionComponent;

UCLASS()
class INTERACTIVEOBJECT_API ULyraGameplayAbility_BWInteractiveObjectHolding : public ULyraGameplayAbility
{
   GENERATED_BODY()

public:
   ULyraGameplayAbility_BWInteractiveObjectHolding();

protected:
   // Helper to get the interactive object we're currently holding
   UFUNCTION(BlueprintCallable, Category = "Interactive Object")
   UInteractionComponent* GetHeldObject() const;
   
   // Called when the ability ends to potentially clean up the held object
   virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, 
                         const FGameplayAbilityActorInfo* ActorInfo, 
                         const FGameplayAbilityActivationInfo ActivationInfo, 
                         bool bReplicateCancelAbility, 
                         bool bWasCancelled) override;
};