// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BwayCharacterWithAbilities.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"
#include "BwayGameplayAbility_Base.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;

/**
 * Base class for Breakaway gameplay abilities
 * Provides common functionality for team checking, damage application, and effect context creation
 */
UCLASS(Abstract)
class BREAKAWAYCORERUNTIME_API UBwayGameplayAbility_Base : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:
	UBwayGameplayAbility_Base(const FObjectInitializer& ObjectInitializer);

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

	/** Create effect context using the ability's MakeEffectContext method */
	FGameplayEffectContextHandle MakeEffectContextForAbility() const;

	/** Get cached character from actor info */
	ABwayCharacterWithAbilities* GetBwayCharacterFromActorInfo() const;

protected:
	/** Stored ability spec handle for use in callbacks */
	FGameplayAbilitySpecHandle StoredSpecHandle;

	/** Cached character for convenience */
	UPROPERTY(Transient)
	mutable TObjectPtr<ABwayCharacterWithAbilities> CachedCharacter;
};

