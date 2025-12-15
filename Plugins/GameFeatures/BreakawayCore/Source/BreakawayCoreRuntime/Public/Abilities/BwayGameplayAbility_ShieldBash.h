// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BwayCharacterWithAbilities.h"
#include "BwayCharacterMovementComponent.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Abilities/BwayGameplayAbility_Base.h"
#include "BwayGameplayAbility_ShieldBash.generated.h"

class UGameplayEffect;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_MoveToLocation;

BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_State_Dashing);
BREAKAWAYCORERUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_Ability_Spartacus_ShieldBash);

/**
 * Shield Bash Ability (Q)
 * Dashes forward and stuns the first enemy hit
 */
UCLASS()
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_ShieldBash : public UBwayGameplayAbility_Base
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_ShieldBash(const FObjectInitializer& ObjectInitializer);

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Perform sphere trace during dash to detect enemies */
	UFUNCTION()
	void PerformDashTrace();

	/** Apply stun effect to hit enemy */
	void ApplyStunToEnemy(AActor* EnemyActor);

	/** Tag representing the dashing state */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield Bash")
	FGameplayTag DashingStateTag;

	/** Gameplay effect to apply stun */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield Bash")
	TSubclassOf<UGameplayEffect> StunGameplayEffectClass;

	/** Dash distance in cm */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield Bash", meta = (ClampMin = "0.0"))
	float DashDistance = 800.0f;

	/** Dash duration in seconds */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield Bash", meta = (ClampMin = "0.0"))
	float DashDuration = 0.5f;

	/** Sphere trace radius for enemy detection */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shield Bash", meta = (ClampMin = "0.0"))
	float TraceRadius = 150.0f;

private:
	UPROPERTY(Transient)
	mutable TObjectPtr<UBwayCharacterMovementComponent> CachedMovementComponent;

	/** Timer handle for dash trace */
	FTimerHandle DashTraceTimerHandle;

	/** Start location of dash */
	FVector DashStartLocation;

	/** Dash direction */
	FVector DashDirection;

	/** Whether we've hit an enemy during this dash */
	bool bHasHitEnemy = false;
};

