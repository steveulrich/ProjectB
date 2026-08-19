// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/BwayGameplayAbility.h"
#include "BwayCharacterWithAbilities.h"
#include "GameplayTagContainer.h"
#include "BwayGameplayAbility_Base.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;

/**
 * Base class for Breakaway gameplay abilities
 * Provides common functionality for team checking, damage application, and effect context creation
 */
UCLASS(Abstract)
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_Base : public UBwayGameplayAbility
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_Base(const FObjectInitializer& ObjectInitializer);

	// Debug logging overrides
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	/** Enable/disable debug logging for this ability class */
	static bool bEnableAbilityDebugLogging;

protected:
	/** Check if character is an enemy (different team) */
	bool IsEnemy(ABwayCharacterWithAbilities* OtherCharacter) const;

	/** Check if character is an ally (same team) */
	bool IsAlly(ABwayCharacterWithAbilities* OtherCharacter) const;

	/** Get enemies in radius around location */
	TArray<ABwayCharacterWithAbilities*> GetEnemiesInRadius(const FVector& Location, float Radius) const;

	/** Get allies in radius around location */
	TArray<ABwayCharacterWithAbilities*> GetAlliesInRadius(const FVector& Location, float Radius) const;

	/** Apply gameplay effect to target actor */
	void ApplyGameplayEffectToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> EffectClass, float EffectLevel = 1.0f);

	/** Apply damage to enemy using Lyra's damage system */
	void ApplyDamageToEnemy(ABwayCharacterWithAbilities* Enemy, float DamageAmount);

	/** Apply healing to ally using Lyra's SetByCaller heal GE */
	void ApplyHealToAlly(ABwayCharacterWithAbilities* Ally, float HealAmount);

	/**
	 * Sheet formula: Final = AbilityBaseDamage + AttackStrength * ScalingCoefficient.
	 * AttackStrength is ULyraCombatSet::BaseDamage (hero DA Attack Str).
	 */
	float CalculateScaledDamage(float AbilityBaseDamage, float ScalingCoefficient) const;

	/** Launch knockback impulse on an enemy character (authority). */
	void ApplyKnockbackToEnemy(ABwayCharacterWithAbilities* Enemy, const FVector& Impulse) const;

	/** Create effect context using the ability's MakeEffectContext method */
	FGameplayEffectContextHandle MakeEffectContextForAbility() const;

	/** Get cached character from actor info */
	ABwayCharacterWithAbilities* GetBwayCharacterFromActorInfo() const;

	/**
	 * Apply CooldownGameplayEffectClass with an optional duration override via FGameplayEffectSpec::SetDuration.
	 * Falls back to UGameplayAbility::ApplyCooldown when DurationSeconds <= 0 or no cooldown GE is set.
	 */
	void ApplyCooldownWithOptionalDuration(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		float DurationSeconds) const;

	/**
	 * One-shot GameplayCue via ASC->ExecuteGameplayCue (same pattern as UGCN_BwayCombatFeedback triggers).
	 * No-ops when CueTag is invalid or ASC is missing.
	 */
	void ExecuteAbilityCue(FGameplayTag CueTag, const FVector& Location = FVector::ZeroVector, float Magnitude = 0.f) const;

	/** Persistent cue via ASC->AddGameplayCue (use for Cue_Loop). */
	void AddAbilityCue(FGameplayTag CueTag, const FVector& Location = FVector::ZeroVector, float Magnitude = 0.f) const;

	/** Remove a previously added persistent cue. */
	void RemoveAbilityCue(FGameplayTag CueTag) const;

	/** Stored ability spec handle for use in callbacks */
	FGameplayAbilitySpecHandle StoredSpecHandle;

	/** Cached character for convenience */
	UPROPERTY(Transient)
	mutable TObjectPtr<ABwayCharacterWithAbilities> CachedCharacter;

	/** Optional one-shot cast / impact / end tags and a loop tag for duration abilities. */
	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag Cue_Cast;

	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag Cue_Loop;

	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag Cue_Impact;

	UPROPERTY(EditDefaultsOnly, Category = "Presentation|Cues", meta = (Categories = "GameplayCue"))
	FGameplayTag Cue_End;
};

