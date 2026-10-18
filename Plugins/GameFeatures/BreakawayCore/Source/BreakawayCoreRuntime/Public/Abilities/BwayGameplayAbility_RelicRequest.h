#pragma once

#include "CoreMinimal.h"
#include "Abilities/BwayGameplayAbility.h"
#include "BwayGameplayAbility_RelicRequest.generated.h"

class AController;
class APawn;
class UAbilitySystemComponent;
class URelicSettings;

/**
 * Common Request Relic ability (InputTag.Ability.RelicRequest / RMB).
 * Applies RelicSettings::RequestingGameplayEffectClass for the timed 3.0s call-for-relic indicator
 * while maintaining State.Relic.PreparingCatch while the input remains held.
 */
UCLASS(Abstract)
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_RelicRequest : public UBwayGameplayAbility
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_RelicRequest(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Active relic settings, else RelicManager RelicSettingsAsset. */
	const URelicSettings* ResolveRelicSettings() const;

private:
	UFUNCTION()
	void OnInputReleased(float TimeHeld);

	UFUNCTION()
	void OnAbortTagAdded();

	UFUNCTION()
	void OnPawnControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	void HandleRequestTagChanged(const FGameplayTag Tag, int32 NewCount);
	void SetPreparingCatchTag(const FGameplayAbilityActorInfo* ActorInfo, bool bPreparingCatch) const;

	TWeakObjectPtr<UAbilitySystemComponent> TrackedASC;
	TWeakObjectPtr<APawn> PreparedAvatar;
	TWeakObjectPtr<AController> PreparedController;
	FDelegateHandle RequestTagEventHandle;
	FGameplayTag ActiveRequestTag;
	double ActivationWorldTime = 0.0;
	float RequestEffectDuration = 3.0f;
	bool bActivatedWithPressedInput = false;
	bool bClearRequestEffectOnEnd = false;
};
