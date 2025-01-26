#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h" // From LyraGame
#include "LyraGameplayAbility_BWInteract.generated.h"

UCLASS()
class INTERACTIVEOBJECT_API ULyraGameplayAbility_BWInteract : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	ULyraGameplayAbility_BWInteract();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, 
							   const FGameplayAbilityActorInfo* ActorInfo, 
							   const FGameplayAbilityActivationInfo ActivationInfo,
							   const FGameplayEventData* TriggerEventData) override;

	UPROPERTY(EditDefaultsOnly, Category = "Interaction")
	float InteractionRange = 200.0f;

protected:
	UFUNCTION(BlueprintCallable, Category = "Ability")
	void OnInteractionCompleted();

	UFUNCTION(BlueprintCallable, Category = "Ability")
	void OnInteractionCancelled();
	
	// Optional: Add montage-specific events
	UFUNCTION(BlueprintCallable, Category = "Ability")
	void OnMontageCompleted(FGameplayTag EventTag, FGameplayEventData EventData);
};
